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
MIDNIGHT_SHA256 = 'a04551d523cb8e0adf4efe4da7e6bdcc6ec683dc264a8e791aed57ae798d4d01'
BOUNDARIES = {
    'leap-day': {
        'script': 'calendar_leap_input',
        'before': 'ed6aad596773e123f80db40fe8034ea55cc8fb78b1346766437814658b12b7db',
        'after': '4dbca58bfc5f7a97df617fbffa70686eead09c0b9c4bc9bfcedf878b52aaa45e',
        'description': '28 February -> 29 February 2024',
    },
    'year-end': {
        'script': 'calendar_year_input',
        'before': 'af17a183794279bf85b189fc8c1ccd11680e5d79f56d91cc845757ca1c53cd0a',
        'after': '4624ec9f948d58f2d2e25ade39d988ebde69cb996dc5c77240249d7d711e5d8e',
        'description': '31 December 2026 -> 1 January 2027',
    },
}


def check_entry(text, rtc, *, midnight=False):
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
    minute, hour = ('3b', '97') if midnight else ('2f', '8d')
    if not re.search(rf'event=alarm_write reg=0b data={minute}.*\n.*event=alarm_write reg=0c data={hour}', text):
        raise ValueError('physical time entry did not program the expected CCONT latch')
    if midnight:
        check_midnight(text)
    check_rtc(rtc, expected=(0, 0) if midnight else (47, 13))


def check_rtc(rtc, *, expected=(47, 13)):
    if len(rtc) != 9 or rtc[1:3] != bytes(expected):
        raise ValueError('retained CCONT snapshot does not contain the expected hour/minute')


def check_midnight(text):
    cursor = 0
    for token in ('event=second time=00:00:00 day=1 ',
                  '6210_calendar_probe: action=midnight_back\n',
                  'event=read reg=0a data=01 ',
                  'event=counter_write reg=0a data=00 ',
                  '6210_calendar_probe: action=midnight_reopen\n'):
        position = text.find(token, cursor)
        if position < 0:
            raise ValueError('missing ordered midnight/day-consumption evidence: ' + token)
        cursor = position + len(token)


def check_cold(text, rtc, *, expected=(47, 13)):
    if '6210_calendar_probe:' in text or 'action=time_' in text or 'action=date_' in text:
        raise ValueError('cold Calendar fixture replaced time/date')
    cursor = 0
    for action in ['menu'] + [f'menu_{i}' for i in range(2, 9)] + ['selected']:
        token = f'6210_calendar_cold: action={action}\n'
        position = text.find(token, cursor)
        if position < 0:
            raise ValueError('missing ordered cold Calendar action: ' + action)
        cursor = position + len(token)
    check_rtc(rtc, expected=expected)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--midnight', action='store_true',
                        help='enter 23:59, cross midnight organically and cold-read the next day')
    parser.add_argument('--boundary', choices=BOUNDARIES,
                        help='select an independently reviewed boundary-date fixture; implies --midnight')
    args = parser.parse_args()
    args.midnight = args.midnight or args.boundary is not None
    boundary = BOUNDARIES.get(args.boundary)
    before_hash = boundary['before'] if boundary else CALENDAR_SHA256
    after_hash = boundary['after'] if boundary else MIDNIGHT_SHA256 if args.midnight else CALENDAR_SHA256
    entry_script = boundary['script'] if boundary else 'calendar_midnight_input' if args.midnight else 'calendar_input'
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    try:
        image = (root / 'roms/noki6210/6210_556c.fls').read_bytes()
        assess(image, (root / 'roms/noki6210/6210 virgin eeprom 005fa000.fls').read_bytes())
        verify_radio(image)
        run.mkdir(parents=True, exist_ok=False)
        from PIL import Image
        for phase, seconds, script, frame in (
            ('entry', 135 if args.midnight else 52,
             entry_script,
             '6210_calendar_midnight_result.png' if args.midnight else '6210_calendar_after_date.png'),
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
            rtc = (storage / 'ccont').read_bytes()
            if phase == 'entry':
                check_entry(text, rtc, midnight=args.midnight)
            else:
                check_cold(text, rtc, expected=(0, 0) if args.midnight else (47, 13))
            if args.midnight and phase == 'entry':
                with Image.open(directory / 'snap/6210_calendar_after_date.png') as rendered:
                    check_frame(rendered, before_hash, 'original fixture Calendar date')
            with Image.open(directory / 'snap' / frame) as rendered:
                check_frame(rendered, after_hash, boundary['description'] if boundary else
                            '8 October 2026 Thursday Calendar' if args.midnight else
                            '7 October 2026 Wednesday Calendar')
        print('6210 Calendar: PASS physical entry, CCONT time, ' +
              ('organic midnight and cold next-day pixels' if args.midnight else 'cold date pixels') +
              ' and retained-location registration; research HLE only')
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        print(f'6210 Calendar: FAIL: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
