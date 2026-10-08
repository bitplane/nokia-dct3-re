"""Verify physical NSB-6 shutdown/PWRONX restart; not cold RTC persistence."""
import argparse
import hashlib
from pathlib import Path
import re
import sys
from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki8890_staged_check import verify as verify_stage
from tools.noki8890_registration_check import verify as verify_registration
from tools.run_noki8890_host_sms import check_output
from tools.power_domain_contract import require_endpoint_silence

FRAMES = {
    '8890_power_idle.png': ((0, 8, 84, 48), '46520fc623a6b562b2aee5f1c59e6943d4a5c1798eff4064091c38e7b3a285de'),
    '8890_power_off.png': ((0, 0, 84, 48), '7d9978ed11e23fdb98a9251da90ed9a3c299066e104d69c32f161a9b86d119b9'),
    '8890_power_restart_security.png': ((0, 0, 84, 24), '40a542aae8dd09a5fdf015bc9c595f26ef505800f67ac9b19c6f67aae16d5564'),
    '8890_power_restart_86.png': ((0, 8, 84, 48), '46520fc623a6b562b2aee5f1c59e6943d4a5c1798eff4064091c38e7b3a285de'),
}


def verify_off_restore(text):
    states = list(re.finditer(r'8890_power_state: event=(saved|restored) '
                             r'pc=(\w+) sp=(\w+) ram=(\w+) t=([0-9.]+)', text))
    if len(states) != 2 or [state[1] for state in states] != ['saved', 'restored']:
        raise ValueError('missing unique off-state save/load observations')
    if states[0].groups()[1:] != states[1].groups()[1:] or float(states[0][5]) != 53:
        raise ValueError('off-state architecture or save time differs')
    windows = list(re.finditer(r'8890_power_replay: phase=(reference|restored) '
                              r'event=(begin|end) t=([0-9.]+)', text))
    if [(event[1], event[2]) for event in windows] != [
            ('reference', 'begin'), ('reference', 'end'),
            ('restored', 'begin'), ('restored', 'end')]:
        raise ValueError('off-state replay windows absent or unordered')
    if any(float(event[3]) != when for event, when in zip(windows, (53, 54, 53, 54))):
        raise ValueError('off-state replay did not cover matching one-second windows')
    reference = text[windows[0].end():windows[1].start()]
    restored = text[windows[2].end():windows[3].start()]
    rtc = r'ccont_rtc: event=second[^\r\n]+'
    ticks = re.findall(rtc, reference)
    if len(ticks) != 1 or ticks != re.findall(rtc, restored):
        raise ValueError('off-state RTC replay differs or is absent')
    # Validate both branches before removing the abandoned timeline for the
    # ordinary power checker, which requires monotonically advancing RTC ticks.
    require_endpoint_silence(reference + restored,
                             'off-state replay resumed powered endpoint activity')
    if re.search(r'ccont_power: event=wake', reference + restored):
        raise ValueError('off-state replay resumed powered endpoint activity')
    return text[:states[0].start()] + text[states[1].start():]


def verify(text, storage, *, restore_off=False):
    check_output(text)
    if restore_off:
        text = verify_off_restore(text)
    elif '8890_power_state:' in text:
        raise ValueError('unexpected off-state restoration in uninterrupted power gate')
    actions = list(re.finditer(r'8890_power_physical: action=([^\r\n]+)', text))
    if [event[1] for event in actions] != [
            'shutdown_press', 'shutdown_release', 'restart_press', 'restart_release']:
        raise ValueError('physical power sequence differs or uses charger wake')
    off = list(re.finditer(r'ccont_power: event=off t=([0-9.]+)', text))
    wake = list(re.finditer(r'ccont_power: event=wake cause=(\w+) t=([0-9.]+)', text))
    if len(off) != 1 or len(wake) != 1 or wake[0][1] != '02':
        raise ValueError('expected one rail-off and one PWRONX wake, not charger')
    off, wake = off[0], wake[0]
    if not (actions[1].end() < off.start() < actions[2].start() < wake.start() < actions[3].start()):
        raise ValueError('rail transitions do not follow physical inputs')
    interval = text[off.end():wake.start()]
    if float(wake[2]) - float(off[1]) < 6:
        raise ValueError('digital rail-off interval is too short to test sustained silence')
    rtc = re.search(r'ccont_rtc: event=second time=12:00:52 day=0[^\n]*t=([0-9.]+)', interval)
    if not rtc or not float(off[1]) < float(rtc[1]) < float(wake[2]):
        raise ValueError('RTC did not tick with the digital rail off')
    ticks = re.findall(r'ccont_rtc: event=second time=12:00:(\d+) day=0[^\n]*t=([0-9.]+)', interval)
    if len(ticks) < 6 or any(int(second) != float(when) for second, when in ticks) or any(
            float(right[1]) - float(left[1]) != 1 for left, right in zip(ticks, ticks[1:])):
        raise ValueError('always-powered RTC did not continue across the sustained off interval')
    require_endpoint_silence(interval, 'DSP/radio endpoint generated activity while its rail was off')
    after = text[wake.end():]
    cause = re.search(r'ccont_power: event=cause_read data=(\w+)', after)
    if not cause or int(cause[1], 16) & 7 != 3:
        raise ValueError('firmware did not consume ready/PWRONX without charger cause')
    # This ROM deliberately resets seconds through GENSIO after waking.
    # Retained hardware counters are not a claim of untouched software time.
    if not re.search(r'ccont_rtc: event=counter_write reg=07 data=00\b', after):
        raise ValueError('missing firmware-owned seconds reinitialization')
    verify_stage(text[:wake.start()], runtime=True, selftest=True)
    verify_stage(after, runtime=True, selftest=True)
    verify_registration(text[:wake.start()], configured_gsm900=True)
    verify_registration(after, configured_gsm900=True, preserved_location=True)
    events = list(re.finditer(r'8890_power_security: key=([^\r\n]+)', after))
    keys = [('Keypad ' + str(key), f'{key:02x}') for key in range(1, 6)] + [('Menu', '19')]
    if [event[1] for event in events] != [key for key, _ in keys]:
        raise ValueError('post-restart security input sequence differs')
    for index, (event, (_, code)) in enumerate(zip(events, keys)):
        end = events[index + 1].start() if index + 1 < len(events) else len(after)
        if not re.search(rf'8890_keypad_decoded: key={code}\b', after[event.end():end]):
            raise ValueError('post-restart physical security key did not decode')
    if len(storage) < 1611 or storage[1604:1609] != bytes.fromhex('00f1100001') or storage[1610] != 0:
        raise ValueError('persistent SIM location lost laboratory registration')


def check_frames(directory, *, restore_off=False):
    frames = dict(FRAMES)
    if restore_off:
        for phase in ('reference', 'restored'):
            frames[f'8890_power_off_{phase}.png'] = FRAMES['8890_power_off.png']
    for name, (crop, expected) in frames.items():
        with Image.open(directory / name) as frame:
            actual = hashlib.sha256(frame.convert('L').crop(crop).tobytes()).hexdigest()
            if frame.size != (84, 48) or actual != expected:
                raise ValueError('reviewed physical power frame mismatch: ' + name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--restore-off', action='store_true')
    args = parser.parse_args()
    try:
        run = args.run_directory
        check_output((run / 'console.log').read_text(errors='replace'))
        verify((run / 'error.log').read_text(errors='replace'),
               (run / 'nvram/nsb6hle/sim_card').read_bytes(), restore_off=args.restore_off)
        check_frames(run / 'snap', restore_off=args.restore_off)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8890 physical power cycle FAIL: {error}\n')
    print('8890 physical shutdown/PWRONX restart/registered idle PASS; cold RTC/native speech unproved')
