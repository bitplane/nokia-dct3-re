"""Self-contained physical NPE-3 clock/alarm expiry and Stop acceptance."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.noki6210_staged_check import verify as verify_stage
from tools.run_noki6210_acceptance import (
    OPERATOR_SHA256, check_frame, check_output, check_registration,
)

FRAMES = {
    'confirm': '753c5b7d172c9e418a4c9689e463c94bbaba0d2522c9972784f6e59716f0e0fd',
    'elapsed': '52a1c70ae301457fc0022e76b754d561cc8f46b9f58b41171ae82211ffd919a6',
    'stopped': OPERATOR_SHA256,
}
COLD_IDLE_SHA256 = 'e5b41cc3e22487969140eeb44414d1e47c32721c39938e3c3592efe163872904'
SNOOZE_FRAMES = {
    'snooze': 'eabf1aaefdeba24b26ef44a8c4873004544d9d742b96a82265b7d94461176896',
    'repeated': '1c5b09187d9fd5df3e66f36fc6029730848da6a9ecc512f5d0644e3c03744d61',
}
POWER_OFF_FRAMES = {
    'powered_off': '907c2e3cc0dc7d0dc17827521badb7be0f647b6b945f1bac2e68094fd47568a7',
    'woke': 'd1c7925c3a9dd1c116cc09133fcab3e32e7bf871597055965ac15e2ccea858e7',
    'stopped': '66e40d0bd8e655b0ac6400b6e83c1acd01c58b0247d51ed4f341ccf6f7cab615',
}


def check_alarm_set(text):
    actions = ['menu', 'menu_2', 'menu_3', 'menu_4', 'settings', 'alarm']
    actions += [f'time_{i}' for i in range(1, 5)] + ['confirm']
    cursor = 0

    def require(token):
        nonlocal cursor
        position = text.find(token, cursor)
        if position < 0:
            raise ValueError('missing ordered alarm evidence: ' + token)
        cursor = position + len(token)

    for action in actions:
        require(f'6210_alarm_probe: action={action}\n')
    require('event=alarm_write reg=0b data=30 armed=1 ')
    require('event=alarm_write reg=0c data=0d armed=1 ')
    require('6210_alarm_probe: action=idle\n')
    return cursor


def check_alarm(text, *, cold=False):
    if cold:
        if 'action=confirm\n' in text or 'action=time_' in text:
            raise ValueError('cold alarm process entered a replacement alarm')
        cursor = text.find('6210_alarm_probe: cold_observe=1\n')
        if cursor < 0:
            raise ValueError('missing independent cold alarm observation')
        startup = text[:cursor]
        first_tick = re.search(r'event=second time=([^ ]+) day=([^ ]+) ', startup)
        if first_tick is None or first_tick.groups() != ('13:47:01', '0'):
            raise ValueError('cold alarm did not restore the retained clock before its first tick')
        for token in ['event=read reg=0b data=30 ', 'event=read reg=0c data=0d ']:
            if token not in startup:
                raise ValueError('cold boot did not read retained alarm registers: ' + token)
    else:
        cursor = check_alarm_set(text)

    def require(token):
        nonlocal cursor
        position = text.find(token, cursor)
        if position < 0:
            raise ValueError('missing ordered alarm evidence: ' + token)
        cursor = position + len(token)

    require('event=second time=13:48:00 day=0 ')
    # CCONT status bit 7 is the independently modeled alarm cause.
    event = re.search(r'event=read reg=0e data=([0-9a-f]{2}) ', text[cursor:])
    if event is None or not int(event[1], 16) & 0x80:
        raise ValueError('firmware did not read the CCONT alarm cause after expiry')
    cursor += event.end()
    require('event=status_ack data=81 ')
    require('buzzer: enabled=1 ')
    require('6210_alarm_probe: action=stop\n')
    require('buzzer: enabled=0 ')
    states = re.findall(r'buzzer: enabled=([01]) ', text[cursor:])
    if states and states[-1] != '0':
        raise ValueError('alarm buzzer remains enabled after physical Stop')
    if '6210_calendar_probe:' in text:
        raise ValueError('alarm process replaced the clock through Calendar input')


def check_snooze(text):
    cursor = check_alarm_set(text)
    events = [
        'event=second time=13:48:00 day=0 ',
        'event=status_ack data=81 ',
        'buzzer: enabled=1 ',
        '6210_alarm_probe: action=snooze\n',
        'event=alarm_write reg=0b data=35 armed=1 ',
        'event=alarm_write reg=0c data=0d armed=1 ',
        'buzzer: enabled=0 ',
        'event=second time=13:53:00 day=0 ',
        'event=read reg=0e data=b1 ',
        'event=status_ack data=a1 ',
        'buzzer: enabled=1 ',
        '6210_alarm_probe: action=stop\n',
        'buzzer: enabled=0 ',
    ]
    for event in events:
        position = text.find(event, cursor)
        if position < 0:
            raise ValueError('missing ordered Snooze evidence: ' + event)
        cursor = position + len(event)
    if '6210_calendar_probe:' in text:
        raise ValueError('Snooze process replaced the clock through Calendar input')
    states = re.findall(r'buzzer: enabled=([01]) ', text[cursor:])
    if states and states[-1] != '0':
        raise ValueError('Snooze buzzer remains enabled after physical Stop')


def check_power_off_alarm(text, choice):
    from tools.power_domain_contract import require_endpoint_silence
    cursor = check_alarm_set(text)

    def require(token):
        nonlocal cursor
        position = text.find(token, cursor)
        if position < 0:
            raise ValueError('missing ordered powered-off alarm evidence: ' + token)
        cursor = position + len(token)
        return position

    require('6210_alarm_probe: action=power_off\n')
    require('6210_alarm_probe: action=power_release\n')
    off = require('ccont_power: event=off ')
    wake = require('ccont_power: event=wake cause=80 ')
    require_endpoint_silence(text[off:wake], 'alarm rail-off domain generated activity')
    require('event=second time=13:48:00 day=0 ')
    wake_time = re.search(r'ccont_power: event=wake cause=80 t=([0-9.]+)', text[wake:])
    deadline = re.search(r'event=second time=13:48:00 day=0 [^\n]*t=([0-9.]+)', text[wake:])
    if wake_time is None or deadline is None or float(wake_time[1]) != float(deadline[1]):
        raise ValueError('rail wake did not occur at the natural RTC deadline')
    require('event=cause_read data=b1 ')
    require('event=status_ack data=81 ')
    require('buzzer: enabled=1 ')
    require('6210_alarm_probe: action=stop\n')
    require('buzzer: enabled=0 ')
    decision = require(f'6210_alarm_probe: action=activate_{choice}\n')
    if re.findall(r'ccont_power: event=wake cause=(\w+)', text) != ['80']:
        raise ValueError('alarm wake used an extra power-key/charger wake')
    if choice == 'no':
        final_off = require('ccont_power: event=off ')
        require_endpoint_silence(text[final_off:], 'No activation did not leave endpoints off')
        if len(re.findall(r'ccont_power: event=off ', text)) != 2:
            raise ValueError('No activation requires exactly two rail-off transitions')
    else:
        if 'ccont_power: event=off ' in text[decision:]:
            raise ValueError('Yes activation unexpectedly removed the rails')
    if '6210_calendar_probe:' in text:
        raise ValueError('alarm wake process replaced the clock through Calendar input')
    return off, wake, decision


def check_off_restore(text):
    from tools.power_domain_contract import require_endpoint_silence
    states = list(re.finditer(r'6210_alarm_state: event=(saved|restored) '
                             r'pc=(\w+) sp=(\w+) ram=(\w+) t=([0-9.]+)', text))
    if len(states) != 2 or [state[1] for state in states] != ['saved', 'restored']:
        raise ValueError('missing unique alarm off-state save/load observations')
    if states[0].groups()[1:] != states[1].groups()[1:] or float(states[0][5]) != 45:
        raise ValueError('alarm off-state architecture or checkpoint time differs')
    windows = list(re.finditer(r'6210_alarm_replay: phase=(reference|restored) '
                              r'event=(begin|end) t=([0-9.]+)', text))
    if [(event[1], event[2]) for event in windows] != [
            ('reference', 'begin'), ('reference', 'end'),
            ('restored', 'begin'), ('restored', 'end')]:
        raise ValueError('alarm off-state replay windows absent or unordered')
    if any(float(event[3]) != when for event, when in zip(windows, (45, 46.25, 45, 46.25))):
        raise ValueError('alarm off-state replay windows differ')
    reference = text[windows[0].end():windows[1].start()]
    restored = text[windows[2].end():windows[3].start()]
    rtc = r'ccont_rtc: event=second[^\r\n]+'
    ticks = re.findall(rtc, reference)
    if len(ticks) != 1 or ticks != re.findall(rtc, restored):
        raise ValueError('alarm off-state RTC replay differs or is absent')
    require_endpoint_silence(reference + restored, 'alarm off-state replay resumed endpoint activity')
    if 'ccont_power: event=wake' in reference + restored:
        raise ValueError('alarm off-state replay woke before its deadline')
    # Keep the restored timeline for the ordinary monotonic lifecycle checks.
    return text[:states[0].start()] + text[states[1].start():]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--cold', action='store_true',
                        help='exit with an armed alarm, then verify expiry in a separate cold boot')
    mode.add_argument('--snooze', action='store_true',
                      help='physically Snooze and verify natural five-minute recurrence and Stop')
    mode.add_argument('--power-off', choices=('yes', 'no'),
                      help='physically shut down, verify autonomous alarm wake and activation choice')
    mode.add_argument('--restore-off', choices=('yes', 'no'), nargs='?', const='no',
                      help='restore the powered-off countdown, then verify natural alarm wake and activation')
    args = parser.parse_args()
    if args.restore_off:
        args.power_off = args.restore_off
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    mame = (args.mame or root / 'mame/mame').resolve()
    try:
        run.mkdir(parents=True, exist_ok=False)
        # This fresh, independently checked physical fixture is the only seed.
        subprocess.run([sys.executable, str(root / 'tools/run_noki6210_calendar.py'),
                        str(run / 'clock'), '--mame', str(mame)], check=True, timeout=400)
        alarm = run / ('armed' if args.cold else 'alarm')
        alarm.mkdir()
        shutil.copytree(run / 'clock/entry/nvram', alarm / 'nvram')
        command = [str(mame), 'npe3hle', '-rompath', str(root / 'roms'),
                   '-nvram_directory', 'nvram', '-cfg_directory', 'cfg', '-noreadconfig',
                   '-debug', '-debugger', 'none', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools' / (
                       'noki6210_alarm_arm.lua' if args.cold else
                       'noki6210_alarm_power_restore_yes.lua' if args.restore_off == 'yes' else
                       'noki6210_alarm_power_restore.lua' if args.restore_off else
                       f'noki6210_alarm_power_{args.power_off}.lua' if args.power_off else
                       'noki6210_alarm_snooze.lua' if args.snooze else 'noki6210_alarm_input.lua')),
                   '-seconds_to_run', '385' if args.snooze else '115' if args.power_off else '85',
                   '-video', 'none', '-sound', 'none',
                   '-nothrottle', '-log', '-verbose']
        with (alarm / 'console.log').open('w') as output:
            subprocess.run(command, cwd=alarm, stdout=output, stderr=subprocess.STDOUT,
                           check=True, timeout=400 if args.snooze else 180)
        check_output((alarm / 'console.log').read_text(errors='replace'))
        text = (alarm / 'error.log').read_text(errors='replace')
        if args.restore_off:
            text = check_off_restore(text)
        elif '6210_alarm_state:' in text or '6210_alarm_replay:' in text:
            raise ValueError('unexpected alarm replay in an uninterrupted fixture')
        if args.power_off:
            off, wake, decision = check_power_off_alarm(text, args.power_off)
            verify_stage(text[:off], runtime=True, selftest=True)
            # Alarm-only startup is not normal telephony/analogue initialization.
            if args.power_off == 'yes':
                restart = re.search(r'mad2_clock: event=W off=01 data=05 ', text[decision:])
                if restart is None:
                    raise ValueError('Yes did not request a firmware-owned software restart')
                boundary = decision + restart.start()
                verify_stage(text[wake:boundary], runtime=True)
                verify_stage(text[boundary:], runtime=True, selftest=True)
            else:
                verify_stage(text[wake:], runtime=True)
        else:
            verify_stage(text, runtime=True, selftest=True)
        check_registration(text, (alarm / 'nvram/npe3hle/sim_card').read_bytes(),
                           preserved_location=True)
        if args.cold:
            check_alarm_set(text)
            if 'armed_checkpoint=1' not in text or 'time=13:48:00' in text:
                raise ValueError('armed checkpoint absent or alarm expired before cold boot')
            cold = run / 'cold'
            cold.mkdir()
            shutil.copytree(alarm / 'nvram', cold / 'nvram')
            command[command.index('-autoboot_script') + 1] = str(
                root / 'tools/noki6210_alarm_cold.lua')
            with (cold / 'console.log').open('w') as output:
                subprocess.run(command, cwd=cold, stdout=output, stderr=subprocess.STDOUT,
                               check=True, timeout=180)
            alarm = cold
            check_output((alarm / 'console.log').read_text(errors='replace'))
            text = (alarm / 'error.log').read_text(errors='replace')
            verify_stage(text, runtime=True, selftest=True)
            check_registration(text, (alarm / 'nvram/npe3hle/sim_card').read_bytes(),
                               preserved_location=True)
        if args.snooze:
            check_snooze(text)
        elif not args.power_off:
            check_alarm(text, cold=args.cold)
        from PIL import Image
        if args.restore_off:
            for name in ('off_reference', 'off_restored'):
                with Image.open(alarm / f'snap/6210_alarm_{name}.png') as frame:
                    check_frame(frame, POWER_OFF_FRAMES['powered_off'], 'off-state replay ' + name)
        if args.cold:
            with Image.open(alarm / 'snap/6210_alarm_cold_idle.png') as frame:
                check_frame(frame, COLD_IDLE_SHA256, 'cold registered idle with armed-alarm icon')
        for name, digest in FRAMES.items():
            if args.power_off and name != 'confirm':
                continue
            if args.cold and name == 'confirm':
                continue
            with Image.open(alarm / f'snap/6210_alarm_{name}.png') as frame:
                check_frame(frame, digest, 'physical alarm ' + name)
        if args.snooze:
            for name, digest in SNOOZE_FRAMES.items():
                with Image.open(alarm / f'snap/6210_alarm_{name}.png') as frame:
                    check_frame(frame, digest, 'physical Snooze ' + name)
        if args.power_off:
            for name, digest in POWER_OFF_FRAMES.items():
                with Image.open(alarm / f'snap/6210_alarm_{name}.png') as frame:
                    check_frame(frame, digest, 'powered-off alarm ' + name)
            digest = (OPERATOR_SHA256 if args.power_off == 'yes'
                      else POWER_OFF_FRAMES['powered_off'])
            with Image.open(alarm / 'snap/6210_alarm_power_choice.png') as frame:
                check_frame(frame, digest, 'physical activation ' + args.power_off)
        print('6210 alarm: PASS ' + ('armed cold retention, ' if args.cold else '') +
              ('exact off-state restore and RTC replay, ' if args.restore_off else '') +
              ('physical Snooze, natural five-minute recurrence, ' if args.snooze else '') +
              (f'autonomous rail wake, physical activation {args.power_off}, '
               if args.power_off else '') +
              'physical set, natural RTC expiry, reviewed pixels, '
              'buzzer control and physical Stop; research HLE, not audible-output acceptance')
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        print(f'6210 alarm: FAIL: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
