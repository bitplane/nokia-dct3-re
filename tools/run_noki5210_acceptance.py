"""Isolated fresh own-product NSM-5 SIM acceptance."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_profile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--scenario', choices=('toolkit-interactive', 'pin-registration'),
                        default='toolkit-interactive')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    try:
        run.mkdir(parents=True, exist_ok=False)
        for name in ('cfg', 'nvram', 'snap'):
            (run / name).mkdir()
        pin = args.scenario == 'pin-registration'
        if pin:
            card = run / 'nvram/noki5210_3/sim_card'
            card.parent.mkdir()
            card.write_bytes(make_profile(pin_enabled=True))
        else:
            shutil.copyfile(root / 'fixtures/sim_toolkit_5210_interactive/noki5210.cfg',
                            run / 'cfg/noki5210.cfg')
        script = 'noki5210_pin_input.lua' if pin else 'noki5210_toolkit_interactive_input.lua'
        checker = 'noki5210_pin_registration_check.py' if pin else 'noki5210_toolkit_interactive_check.py'
        command = [str((args.mame or root / 'mame/mame').resolve()), 'noki5210',
                   '-bios', '540e', '-rompath', str(root / 'roms'), '-cfg_directory', 'cfg',
                   '-nvram_directory', 'nvram', '-snapshot_directory', 'snap', '-noreadconfig',
                   '-debug', '-debugger', 'none', '-verbose', '-log', '-video', 'none',
                   '-sound', 'none', '-nothrottle', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools' / script),
                   '-seconds_to_run', '45' if pin else '65']
        with (run / 'console.log').open('w') as console:
            subprocess.run(command, cwd=run, stdout=console, stderr=subprocess.STDOUT, check=True)
        subprocess.run([sys.executable, str(root / 'tools' / checker),
                        str(run)], check=True)
    except (OSError, subprocess.SubprocessError) as error:
        parser.exit(1, f'5210 isolated {args.scenario} FAIL: {error}\n')


if __name__ == '__main__':
    main()
