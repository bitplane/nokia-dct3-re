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


def check_alarm(text):
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    mame = (args.mame or root / 'mame/mame').resolve()
    try:
        run.mkdir(parents=True, exist_ok=False)
        # This fresh, independently checked physical fixture is the only seed.
        subprocess.run([sys.executable, str(root / 'tools/run_noki6210_calendar.py'),
                        str(run / 'clock'), '--mame', str(mame)], check=True, timeout=400)
        alarm = run / 'alarm'
        alarm.mkdir()
        shutil.copytree(run / 'clock/entry/nvram', alarm / 'nvram')
        command = [str(mame), 'npe3hle', '-rompath', str(root / 'roms'),
                   '-nvram_directory', 'nvram', '-cfg_directory', 'cfg', '-noreadconfig',
                   '-debug', '-debugger', 'none', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools/noki6210_alarm_input.lua'),
                   '-seconds_to_run', '85', '-video', 'none', '-sound', 'none',
                   '-nothrottle', '-log', '-verbose']
        with (alarm / 'console.log').open('w') as output:
            subprocess.run(command, cwd=alarm, stdout=output, stderr=subprocess.STDOUT,
                           check=True, timeout=180)
        check_output((alarm / 'console.log').read_text(errors='replace'))
        text = (alarm / 'error.log').read_text(errors='replace')
        verify_stage(text, runtime=True, selftest=True)
        check_registration(text, (alarm / 'nvram/npe3hle/sim_card').read_bytes(),
                           preserved_location=True)
        check_alarm(text)
        from PIL import Image
        for name, digest in FRAMES.items():
            with Image.open(alarm / f'snap/6210_alarm_{name}.png') as frame:
                check_frame(frame, digest, 'physical alarm ' + name)
        print('6210 alarm: PASS physical set, natural RTC expiry, reviewed pixels, '
              'buzzer control and physical Stop; research HLE, not audible-output acceptance')
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        print(f'6210 alarm: FAIL: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
