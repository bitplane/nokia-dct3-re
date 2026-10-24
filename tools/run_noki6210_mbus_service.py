"""Fresh read-only MBUS version exchange with a no-query storage control."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile

try:
    from .mbus_input_check import verify as verify_ingress
    from .mbus_service_check import verify
except ImportError:
    from mbus_input_check import verify as verify_ingress
    from mbus_service_check import verify


ROOT = Path(__file__).resolve().parents[1]


def storage(root):
    return {str(path.relative_to(root)): path.read_bytes()
            for path in root.rglob('*') if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame-dir', type=Path, default=ROOT / 'mame')
    args = parser.parse_args()
    try:
        rom = (ROOT / 'roms/noki6210/6210_556c.fls').read_bytes()
        if hashlib.sha1(rom).hexdigest() != '3d9ea319503e78ec69b60d72cda23e461e118ea9':
            raise ValueError('own NPE-3 MCU hash mismatch')
        args.run_directory.mkdir(parents=True, exist_ok=True)
        capture = Path(tempfile.mkdtemp(prefix='mbus-service-', dir=args.run_directory.resolve()))
        for name in ('control', 'client'):
            command = [sys.executable, str(ROOT / 'tools/run_mame_isolated.py'),
                       '--mame-dir', str(args.mame_dir.resolve()), '--run-dir', str(capture / name),
                       '--', 'npe3mbus', '-rompath', str(ROOT / 'roms'), '-video', 'none',
                       '-sound', 'none', '-nothrottle', '-seconds_to_run', '24', '-log', '-verbose']
            if name == 'client':
                command += ['-autoboot_delay', '0', '-autoboot_script',
                            str(ROOT / 'tools/noki6210_mbus_service_client.lua')]
            with (capture / f'{name}.stdout.log').open('w') as output:
                subprocess.run(command, stdout=output, stderr=subprocess.STDOUT, check=True)
        text = (capture / 'client/error.log').read_text()
        verify_ingress(text, version_query=True)
        payload = verify(text)
        expected = b'V 05.56\n25-01-02\nNPE-3\n(c) NMP.\0'
        if payload != b'\x01\x00\x00\x03' + expected or expected not in rom:
            raise ValueError('reply differs from own ROM version text')
        control = storage(capture / 'control/nvram')
        client = storage(capture / 'client/nvram')
        if not control or control != client:
            raise ValueError('service changed storage or storage control is absent')
        print(f'MBUS service PASS: own ROM version, sequence ACK, {len(control)} unchanged NVRAM files; {capture}')
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'MBUS service FAIL: {error}\n')


if __name__ == '__main__':
    main()
