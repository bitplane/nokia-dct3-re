"""Fresh own-PMM clock seeding and retained NSB-6 DISPLAY TEXT acceptance."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.dct3_toolkit_check import verify_display_text
from tools.noki8890_registration_check import verify as verify_registration

FRAMES = {
    '8890_toolkit_display.png': '6a0bd20bfe0a7f57ae82a570eceb3a0f29fdc8a9a1e047fb294e85d61e610f6f',
    '8890_toolkit_after_dismiss.png': '8187cbe68f4b7fe0a15cf10c16b742b3236f3240af3f79556c319d5ed337f2ee',
}


def verify(run):
    from PIL import Image
    text = (run / 'error.log').read_text(errors='replace')
    verify_display_text(text, '8890')
    verify_registration(text, preserved_location=True)
    card = (run / 'nvram/nsb6hle/sim_card').read_bytes()
    if len(card) < 1611 or card[1604:1609] != bytes.fromhex('00f1100001') or card[1610] != 0:
        raise ValueError('retained SIM location differs')
    for name, digest in FRAMES.items():
        with Image.open(run / 'snap' / name) as frame:
            if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != digest:
                raise ValueError('unexpected retained Toolkit frame: ' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    try:
        run.mkdir(parents=True, exist_ok=False)
        seed = run / 'seed'
        command = [sys.executable, str(root / 'tools/run_noki8xxx_supplementary.py'),
                   '8890', str(seed), '--service', 'toolkit-busy']
        if args.mame:
            command += ['--mame', str(args.mame.resolve())]
        subprocess.run(command, check=True)
        warm = run / 'retained'
        warm.mkdir()
        for name in ('nvram', 'cfg'):
            shutil.copytree(seed / name, warm / name)
        (warm / 'snap').mkdir()
        command = json.loads((seed / 'acceptance.json').read_text())['command']
        command[command.index('-autoboot_script') + 1] = str(root / 'tools/noki8890_toolkit_retained_input.lua')
        command[command.index('-seconds_to_run') + 1] = '80'
        with (warm / 'console.log').open('w') as console:
            subprocess.run(command, cwd=warm, stdout=console, stderr=subprocess.STDOUT,
                           check=True, timeout=180)
        verify(warm)
        (run / 'acceptance.json').write_text(json.dumps({
            'product': '8890', 'result': 'pass', 'seed': 'fresh physical clock/date; own acquired PMM',
            'scope': 'retained-clock DISPLAY TEXT and laboratory registration; research HLE, not native speech',
            'command': command,
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'8890 retained Toolkit FAIL: {error}\n')
    print('8890 fresh-seed/retained Toolkit PASS')


if __name__ == '__main__':
    main()
