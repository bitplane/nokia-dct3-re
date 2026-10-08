"""Own-PMM NPE-3 physical rail-off/restart; not native DSP speech."""
import re
from PIL import Image
from tools.noki6210_staged_check import verify as verify_stage
from tools.power_domain_contract import require_endpoint_silence
from tools.run_noki6210_acceptance import check_registration, check_frame, OPERATOR_SHA256, check_output

FRAMES = {
    '6210_power_idle.png': OPERATOR_SHA256,
    '6210_power_restarted.png': OPERATOR_SHA256,
    '6210_power_off.png': '907c2e3cc0dc7d0dc17827521badb7be0f647b6b945f1bac2e68094fd47568a7',
    # Reviewed Messages animation phase at 63 s after physical Menu at 60 s.
    '6210_power_menu.png': '81b207e77820f80f4844daeeebdbd260c1f2590888de1d0274ac67f7d036b586',
}


def verify(text, storage):
    check_output(text)
    actions = list(re.finditer(r'6210_power_physical: action=(\w+)', text))
    if [event[1] for event in actions] != [
            'shutdown_press', 'shutdown_release', 'restart_press', 'restart_release', 'menu']:
        raise ValueError('NPE-3 physical power sequence differs')
    off = list(re.finditer(r'ccont_power: event=off t=([0-9.]+)', text))
    wake = list(re.finditer(r'ccont_power: event=wake cause=(\w+) t=([0-9.]+)', text))
    if len(off) != 1 or len(wake) != 1 or wake[0][1] != '02':
        raise ValueError('NPE-3 requires one rail-off and PWRONX wake, not charger')
    off, wake = off[0], wake[0]
    if not (actions[1].end() < off.start() < actions[2].start() < wake.start()
            < actions[3].start() < actions[4].start()):
        raise ValueError('NPE-3 rail transitions do not follow physical inputs')
    if float(wake[2]) - float(off[1]) < 8:
        raise ValueError('NPE-3 off interval is too short')
    interval = text[off.end():wake.start()]
    ticks = re.findall(r'ccont_rtc: event=second time=12:00:(\d+) day=0[^\n]*t=([0-9.]+)', interval)
    if len(ticks) < 8 or any(int(second) != float(when) for second, when in ticks) or any(
            float(right[1]) - float(left[1]) != 1 for left, right in zip(ticks, ticks[1:])):
        raise ValueError('NPE-3 always-powered RTC did not keep ticking while off')
    require_endpoint_silence(interval, 'NPE-3 DSP/radio endpoint generated activity while off')
    before, after = text[:off.start()], text[wake.end():]
    cause = re.search(r'ccont_power: event=cause_read data=(\w+)', after)
    if not cause or int(cause[1], 16) & 7 != 3:
        raise ValueError('NPE-3 did not consume ready/PWRONX without charger')
    if not re.search(r'ccont_rtc: event=counter_write reg=07 data=00\b', after):
        raise ValueError('NPE-3 seconds reinitialization is absent')
    for boot in (before, after):
        verify_stage(boot, runtime=True, selftest=True)
    check_registration(before, storage)
    check_registration(after, storage, preserved_location=True)
    if not re.search(r'6210_keypad_decoded: key=19\b', text[actions[4].end():]):
        raise ValueError('NPE-3 physical Menu did not decode after restart')


def check_frames(directory):
    for name, digest in FRAMES.items():
        with Image.open(directory / name) as frame:
            check_frame(frame, digest, 'physical NPE-3 power lifecycle ' + name)
