"""Fresh read-only MBUS version exchange with a no-query storage control.

--after-sleep queries at 40 s instead of 20 s, after the handset has stopped
its clock and woken organically; the reply must still arrive, and the run
must show that sleep and wake happened before the query."""
import argparse
import os
import hashlib
import re
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
    parser.add_argument('--after-sleep', action='store_true')
    args = parser.parse_args()
    start = 40 if args.after_sleep else 20
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
                       '-sound', 'none', '-nothrottle', '-seconds_to_run', str(start + 4), '-log', '-verbose']
            if name == 'client':
                command += ['-autoboot_delay', '0', '-autoboot_script',
                            str(ROOT / 'tools/noki6210_mbus_service_client.lua')]
            environment = dict(os.environ, NOKIA_MBUS_PROBE_START=str(start))
            with (capture / f'{name}.stdout.log').open('w') as output:
                subprocess.run(command, stdout=output, stderr=subprocess.STDOUT, check=True, env=environment)
        text = (capture / 'client/error.log').read_text()
        if args.after_sleep:
            wakes = [float(t) for t in re.findall(r'mad2_sleep: event=wake .*? t=([0-9.]+)', text)]
            if not any(t < start for t in wakes):
                raise ValueError('the handset did not stop its clock and wake before the query')
        verify_ingress(text, version_query=True, start=start)
        payload = verify(text)
        expected = b'V 05.56\n25-01-02\nNPE-3\n(c) NMP.\0'
        if payload != b'\x01\x00\x00\x03' + expected or expected not in rom:
            raise ValueError('reply differs from own ROM version text')
        control = storage(capture / 'control/nvram')
        client = storage(capture / 'client/nvram')
        if not control or control != client:
            raise ValueError('service changed storage or storage control is absent')
        print(f'MBUS service PASS: query at {start} s, own ROM version, sequence ACK, {len(control)} unchanged NVRAM files; {capture}')
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'MBUS service FAIL: {error}\n')


if __name__ == '__main__':
    main()
