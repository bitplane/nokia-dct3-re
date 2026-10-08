"""Fresh own-ROM physical power restart with retained CCONT and storage."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.run_noki8xxx_supplementary import PROFILES, verify_inputs
from tools.noki8890_power_check import verify, check_frames, check_output


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
        verify_inputs('8890', (roms / profile[1]).read_bytes(), (roms / profile[3]).read_bytes())
        run.mkdir(parents=True, exist_ok=False)
        (run / 'cfg').mkdir()
        shutil.copyfile(root / 'fixtures/noki8890_power/nsb6hle.cfg', run / 'cfg/nsb6hle.cfg')
        command = [str((args.mame or root / 'mame/mame').resolve()), 'nsb6hle',
                   '-rompath', str(root / 'roms'), '-nvram_directory', 'nvram',
                   '-cfg_directory', 'cfg', '-snapshot_directory', 'snap', '-noreadconfig',
                   '-debug', '-debugger', 'none', '-verbose', '-log', '-video', 'none',
                   '-sound', 'none', '-nothrottle', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools/noki8890_power_cycle_observe.lua'),
                   '-seconds_to_run', '89']
        with (run / 'console.log').open('w') as console:
            subprocess.run(command, cwd=run, stdout=console, stderr=subprocess.STDOUT,
                           check=True, timeout=240)
        check_output((run / 'console.log').read_text(errors='replace'))
        verify((run / 'error.log').read_text(errors='replace'),
               (run / 'nvram/nsb6hle/sim_card').read_bytes())
        check_frames(run / 'snap')
        (run / 'acceptance.json').write_text(json.dumps({
            'machine': 'nsb6hle', 'scenario': 'physical-power-restart',
            'mcu_sha1': profile[2], 'pmm_sha1': profile[4],
            'provisioning': 'own acquired PMM unchanged', 'laboratory_carrier': 60,
            'native_dsp_complete': False, 'cold_rtc_persistence': False,
            'command': command, 'result': 'pass',
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'8890 physical power cycle FAIL: {error}; inspect {run}\n')
    print('8890 physical power restart PASS; cold RTC/native speech unproved')


if __name__ == '__main__':
    main()
