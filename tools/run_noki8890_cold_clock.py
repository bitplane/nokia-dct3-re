"""Fresh physical 13:47 entry followed by an own-storage cold process."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.run_noki8xxx_supplementary import PROFILES, verify_inputs
from tools.noki8890_cold_clock_check import verify, check_frame


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    profile = PROFILES['8890']
    try:
        roms = root / 'roms/noki8890'
        verify_inputs('8890', (roms / profile[1]).read_bytes(),
                      (roms / profile[3]).read_bytes())
        run.mkdir(parents=True, exist_ok=False)
        commands = []
        for phase, script, duration in (
                ('seed', 'noki8890_clock_retention_input.lua', 45),
                ('cold', 'noki8890_clock_nv_read.lua', 32)):
            directory = run / phase
            (directory / 'cfg').mkdir(parents=True)
            shutil.copyfile(root / 'fixtures/noki8890_power/nsb6hle.cfg',
                            directory / 'cfg/nsb6hle.cfg')
            if phase == 'cold':
                shutil.copytree(run / 'seed/nvram', directory / 'nvram')
            command = [str((args.mame or root / 'mame/mame').resolve()), 'nsb6hle',
                       '-rompath', str(root / 'roms'), '-nvram_directory', 'nvram',
                       '-cfg_directory', 'cfg', '-snapshot_directory', 'snap',
                       '-noreadconfig', '-debug', '-debugger', 'none', '-verbose',
                       '-log', '-video', 'none', '-sound', 'none', '-nothrottle',
                       '-autoboot_delay', '0', '-autoboot_script',
                       str(root / 'tools' / script), '-seconds_to_run', str(duration)]
            commands.append(command)
            with (directory / 'console.log').open('w') as console:
                subprocess.run(command, cwd=directory, stdout=console,
                               stderr=subprocess.STDOUT, check=True, timeout=180)
            text = (directory / 'error.log').read_text(errors='replace')
            if 'LUA ERROR' in text or 'LUA error' in text:
                raise ValueError(f'{phase}: runtime Lua failure')
        seed_text = (run / 'seed/error.log').read_text(errors='replace')
        expected = ['Keypad 1', 'Keypad 3', 'Keypad 4', 'Keypad 7', 'Menu',
                    'Keypad 0', 'Keypad 7', 'Keypad 1', 'Keypad 0', 'Keypad 2',
                    'Keypad 0', 'Keypad 2', 'Keypad 6', 'Menu']
        if re.findall(r'8890_clock_physical: key=([^\r\n]+)', seed_text) != expected:
            raise ValueError('physical time/date seed sequence mismatch')
        stored = (run / 'seed/nvram/nsb6hle/ccont').read_bytes()
        verify((run / 'cold/error.log').read_text(errors='replace'), stored)
        check_frame(run / 'seed/snap/8890_date_after.png')
        check_frame(run / 'cold/snap/8890_clock_cold_30.png')
        (run / 'acceptance.json').write_text(json.dumps({
            'result': 'pass', 'machine': 'nsb6hle', 'mcu_sha1': profile[2],
            'pmm_sha1': profile[4], 'provisioning': 'own acquired PMM unchanged',
            'commands': commands, 'retained_ccont': stored.hex(),
            'cold_user_clock': '13:47', 'offline_calendar_advance': False,
            'native_dsp_complete': False,
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'8890 cold clock FAIL: {error}; inspect {run}\n')
    print('8890 physical clock entry and cold 13:47 idle PASS')


if __name__ == '__main__':
    main()
