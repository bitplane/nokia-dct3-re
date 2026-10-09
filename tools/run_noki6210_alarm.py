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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--cold', action='store_true',
                        help='exit with an armed alarm, then verify expiry in a separate cold boot')
    mode.add_argument('--snooze', action='store_true',
                      help='physically Snooze and verify natural five-minute recurrence and Stop')
    args = parser.parse_args()
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
                       'noki6210_alarm_snooze.lua' if args.snooze else 'noki6210_alarm_input.lua')),
                   '-seconds_to_run', '385' if args.snooze else '85', '-video', 'none', '-sound', 'none',
                   '-nothrottle', '-log', '-verbose']
        with (alarm / 'console.log').open('w') as output:
            subprocess.run(command, cwd=alarm, stdout=output, stderr=subprocess.STDOUT,
                           check=True, timeout=400 if args.snooze else 180)
        check_output((alarm / 'console.log').read_text(errors='replace'))
        text = (alarm / 'error.log').read_text(errors='replace')
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
        else:
            check_alarm(text, cold=args.cold)
        from PIL import Image
        if args.cold:
            with Image.open(alarm / 'snap/6210_alarm_cold_idle.png') as frame:
                check_frame(frame, COLD_IDLE_SHA256, 'cold registered idle with armed-alarm icon')
        for name, digest in FRAMES.items():
            if args.cold and name == 'confirm':
                continue
            with Image.open(alarm / f'snap/6210_alarm_{name}.png') as frame:
                check_frame(frame, digest, 'physical alarm ' + name)
        print('6210 alarm: PASS ' + ('armed cold retention, ' if args.cold else '') +
              ('physical Snooze, natural five-minute recurrence, ' if args.snooze else '') +
              'physical set, natural RTC expiry, reviewed pixels, '
              'buzzer control and physical Stop; research HLE, not audible-output acceptance')
        if args.snooze:
            print('6210 Snooze scope: original alarm and final idle pixels verified; '
                  'repeated alarm title/time pixels remain unresolved')
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        print(f'6210 alarm: FAIL: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
