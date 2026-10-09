"""Own-ROM physical Calendar entry and independent cold-process persistence."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.run_noki6210_acceptance import check_frame, check_output, check_registration
from tools.noki6210_upload_contract import assess
from tools.noki6210_radio_contract import verify as verify_radio
from tools.noki6210_staged_check import verify as verify_stage

CALENDAR_SHA256 = 'fcf0327cc0cc4edb952d0dc37621e467c0810b867738de6c91f39354a9c3a3b3'


def check_entry(text, rtc):
    actions = ['menu'] + [f'menu_{i}' for i in range(2, 9)] + ['selected']
    actions += [f'time_{i}' for i in range(1, 5)] + ['time_confirm']
    actions += [f'date_{i}' for i in range(1, 9)] + ['date_confirm']
    cursor = 0
    for action in actions:
        token = f'6210_calendar_probe: action={action}\n'
        position = text.find(token, cursor)
        if position < 0:
            raise ValueError('missing ordered physical Calendar action: ' + action)
        cursor = position + len(token)
    if not re.search(r'event=alarm_write reg=0b data=2f.*\n.*event=alarm_write reg=0c data=8d', text):
        raise ValueError('physical time entry did not program CCONT 13:47')
    check_rtc(rtc)


def check_rtc(rtc):
    if len(rtc) != 9 or rtc[1:3] != bytes((47, 13)):
        raise ValueError('retained CCONT snapshot does not contain 13:47')


def check_cold(text, rtc):
    if '6210_calendar_probe:' in text or 'action=time_' in text or 'action=date_' in text:
        raise ValueError('cold Calendar fixture replaced time/date')
    cursor = 0
    for action in ['menu'] + [f'menu_{i}' for i in range(2, 9)] + ['selected']:
        token = f'6210_calendar_cold: action={action}\n'
        position = text.find(token, cursor)
        if position < 0:
            raise ValueError('missing ordered cold Calendar action: ' + action)
        cursor = position + len(token)
    check_rtc(rtc)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    try:
        image = (root / 'roms/noki6210/6210_556c.fls').read_bytes()
        assess(image, (root / 'roms/noki6210/6210 virgin eeprom 005fa000.fls').read_bytes())
        verify_radio(image)
        run.mkdir(parents=True, exist_ok=False)
        from PIL import Image
        for phase, seconds, script, frame in (
            ('entry', 52, 'calendar_input', '6210_calendar_after_date.png'),
            ('cold', 34, 'calendar_cold_input', '6210_calendar_cold_result.png'),
        ):
            directory = run / phase
            directory.mkdir()
            if phase == 'cold':
                shutil.copytree(run / 'entry/nvram', directory / 'nvram')
            command = [str((args.mame or root / 'mame/mame').resolve()), 'npe3hle',
                       '-rompath', str(root / 'roms'), '-nvram_directory', 'nvram',
                       '-cfg_directory', 'cfg', '-noreadconfig', '-debug', '-debugger', 'none',
                       '-autoboot_script', str(root / f'tools/noki6210_{script}.lua'),
                       '-autoboot_delay', '0', '-seconds_to_run', str(seconds),
                       '-video', 'none', '-sound', 'none', '-nothrottle', '-log', '-verbose']
            with (directory / 'console.log').open('w') as output:
                subprocess.run(command, cwd=directory, stdout=output,
                               stderr=subprocess.STDOUT, check=True, timeout=180)
            check_output((directory / 'console.log').read_text(errors='replace'))
            text = (directory / 'error.log').read_text(errors='replace')
            verify_stage(text, runtime=True, selftest=True)
            storage = directory / 'nvram/npe3hle'
            check_registration(text, (storage / 'sim_card').read_bytes(),
                               preserved_location=phase == 'cold')
            (check_entry if phase == 'entry' else check_cold)(text, (storage / 'ccont').read_bytes())
            with Image.open(directory / 'snap' / frame) as rendered:
                check_frame(rendered, CALENDAR_SHA256, '7 October 2026 Wednesday Calendar')
        print('6210 Calendar: PASS physical entry, CCONT time, cold date pixels and retained-location registration; research HLE only')
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        print(f'6210 Calendar: FAIL: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
