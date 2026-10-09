"""Isolated own-PMM midnight rollover and cold Calendar acceptance."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki8890_calendar_check import verify, verify_restore
from tools.noki8890_registration_check import verify as verify_registration
from tools.run_noki8xxx_supplementary import PROFILES, verify_inputs

BOUNDARIES = {
    'non-leap': ('noki8890_calendar_nonleap_input.lua', '1 March 2025 Saturday',
                 '4cebc9032f010a5dcf2b5388a3b0f9f6ffd617a750beca0e79c04f657d54a91f'),
    'ordinary': ('noki8890_calendar_rollover_input.lua', '8 October 2026 Thursday',
                 '33a22a0e0d1072996f2d4bbac4f1f67b54aa8eb919d23ad70b736d7502124c05'),
    'leap-day': ('noki8890_calendar_leap_input.lua', '29 February 2024 Thursday',
                 '8d4c81f3534b43d590855d1112bc38ffc7426f3aa967f775db2b0252146b5f41'),
    'year-end': ('noki8890_calendar_year_input.lua', '1 January 2027 Friday',
                 'b78f184f04f8931745eb7e201e109cd2fc4d8a887cc7a38c17e7506891b3ff00'),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--boundary', choices=BOUNDARIES, default='ordinary')
    parser.add_argument('--restore', action='store_true',
                        help='replay the ordinary midnight from an exact pre-midnight save')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    profile = PROFILES['8890']
    script, expected_date, expected_frame = BOUNDARIES[args.boundary]
    if args.restore:
        if args.boundary != 'ordinary':
            parser.error('--restore currently requires the independently tested ordinary boundary')
        script = 'noki8890_calendar_restore.lua'
    try:
        from PIL import Image
        roms = root / 'roms/noki8890'
        verify_inputs('8890', (roms / profile[1]).read_bytes(),
                      (roms / profile[3]).read_bytes())
        run.mkdir(parents=True, exist_ok=False)
        commands = []
        for name, script, duration in (
                ('seed', script, 110),
                ('cold', 'noki8890_calendar_cold_input.lua', 35)):
            leg = run / name
            leg.mkdir()
            for directory in ('cfg', 'snap'):
                (leg / directory).mkdir()
            if name == 'cold':
                shutil.copytree(run / 'seed/nvram', leg / 'nvram')
            else:
                (leg / 'nvram').mkdir()
            command = [str((args.mame or root / 'mame/mame').resolve()), profile[0],
                       '-rompath', str(root / 'roms'), '-nvram_directory', 'nvram',
                       '-cfg_directory', 'cfg', '-snapshot_directory', 'snap',
                       '-noreadconfig', '-debug', '-debugger', 'none', '-verbose',
                       '-log', '-video', 'none', '-sound', 'none', '-nothrottle',
                       '-autoboot_delay', '0', '-autoboot_script', str(root / 'tools' / script),
                       '-seconds_to_run', str(duration)]
            commands.append(command)
            with (leg / 'console.log').open('w') as console:
                subprocess.run(command, cwd=leg, stdout=console, stderr=subprocess.STDOUT,
                               check=True, timeout=180)
        logs = [(run / leg / 'error.log').read_text(errors='replace') +
                (run / leg / 'console.log').read_text(errors='replace')
                for leg in ('seed', 'cold')]
        verify(*logs, boundary=args.boundary)
        if args.restore:
            verify_restore(logs[0])
            pixels = []
            for name in ('reference', 'replayed'):
                with Image.open(run / 'seed/snap' / ('8890_calendar_restore_' + name + '.png')) as frame:
                    if frame.size != (84, 48):
                        raise ValueError('Calendar restoration frame geometry differs')
                    pixels.append(frame.convert('L').tobytes())
            if pixels[0] != pixels[1] or not any(pixels[0]):
                raise ValueError('Calendar restoration pixels differ or are blank')
        verify_registration(logs[0])
        verify_registration(logs[1], preserved_location=True)
        with Image.open(run / 'cold/snap/8890_calendar_cold.png') as frame:
            digest = hashlib.sha256(frame.convert('L').tobytes()).hexdigest()
            if frame.size != (84, 48) or digest != expected_frame:
                raise ValueError('reviewed ' + expected_date + ' Calendar frame differs')
        (run / 'acceptance.json').write_text(json.dumps({
            'product': '8890', 'result': 'pass', 'commands': commands,
            'mcu_sha1': profile[2], 'pmm_sha1': profile[4],
            'scope': 'physical midnight, own-journal cold date, laboratory registration',
            'calendar_boundary': args.boundary, 'expected_date': expected_date,
            'midnight_state_restore': args.restore,
            'offline_elapsed_time': False, 'native_dsp_speech': False,
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'8890 calendar FAIL: {error}\n')
    print('8890 physical midnight and cold ' + expected_date + ' Calendar PASS')


if __name__ == '__main__':
    main()
