#!/usr/bin/env python3
"""Run native ROM4 Location Updating on a laboratory BTS machine and check it."""
import argparse
import os
import shutil
from pathlib import Path
import sys
import tempfile

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_5110_eeprom_profile import build_profile
from tools.process_runner import run_emulator
from tools.rom4_lab_registration_check import NSE1_LCD_SHA256, check, check_lcd

HEADLESS = ['-noreadconfig', '-midiprovider', 'none', '-keyboardprovider', 'none',
            '-mouseprovider', 'none', '-lightgunprovider', 'none', '-joystickprovider', 'none',
            '-video', 'none', '-sound', 'none', '-log', '-verbose', '-skip_gameinfo', '-nothrottle']


def command(root, run, machine, mame):
    nvram = run / 'nvram/nse1ap5'
    nvram.mkdir(parents=True)
    (run / 'snap').mkdir()
    nvram.joinpath('eeprom').write_bytes(build_profile(
        (root / 'roms/noki5110/nse-1.bin').read_bytes(),
        (root / 'roms/noki5110/5110f530.fls').read_bytes()))
    # The staged menu script adds the LCD mirror and the staged-CPU end marker.
    script = root / 'tools/c54x_rom4_staged_menu.lua'
    return [str(mame), machine, '-rompath', str(root / 'mame/roms'), '-nvram_directory', str(run / 'nvram'),
            '-cfg_directory', str(run / 'cfg'), '-seconds_to_run', '22', *HEADLESS,
            '-autoboot_script', str(script)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('machine', choices=('nse1ap5',))
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    scratch = root / 'tmp/native-gates'
    scratch.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix=f'{args.machine}-registration.', dir=scratch))
    try:
        cmd = command(root, run, args.machine, (args.mame or root / 'mame/mame').resolve())
        run.mkdir(exist_ok=True)
        os.environ['NOKIA_DCT3_SNAPSHOT_DIR'] = str(run / 'snap')
        with open(run / 'console.log', 'wb') as output:
            status = run_emulator(cmd, run, output, timeout=1200)
        if status:
            raise ValueError(f'emulator exited {status}')
        text = (run / 'error.log').read_text(errors='replace')
        result = check(text)
        if args.machine == 'nse1ap5':
            if 'rom4_staged_end:' not in text or 'illegal=0' not in text.split('rom4_staged_end:', 1)[1].split('\n', 1)[0]:
                raise ValueError('staged ROM4 execution did not end cleanly')
            check_lcd(run / 'snap', NSE1_LCD_SHA256)
    except Exception as error:
        raise SystemExit(f'FAIL - {error}; evidence retained: {run}')
    print(' '.join(f'{key}={value}' for key, value in result.items()))
    print(f'NSE ROM4 {args.machine} native laboratory Location Updating: PASS')
    shutil.rmtree(run)


if __name__ == '__main__':
    main()
