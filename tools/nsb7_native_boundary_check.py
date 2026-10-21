"""Qualify own NSB-7 uploads ending at unavailable resident code, not idle."""
import argparse
import json
from pathlib import Path
import re
import struct
import subprocess
import hashlib

try:
    from tools.extract_nsm3_verifier import extract, extract_loader, extract_program_fragment
except ModuleNotFoundError:
    from extract_nsm3_verifier import extract, extract_loader, extract_program_fragment


def check(image, output, trace, returncode):
    extract(image, '8290')
    extract_loader(image, '8290')
    extract_program_fragment(image, '8290')
    if struct.unpack_from('>6H', image, 0x11ddcc) != (0x0a00, 0x1000, 623, 0x200, 1000, 0):
        raise ValueError('second-loader descriptor differs')
    if hashlib.sha1(image[0x11ddd8:0x11ddd8+1246]).hexdigest() != '89080bea5785eb944586574a3e1463145d8d4460':
        raise ValueError('second-loader own payload differs')
    if returncode != 1 or 'Staged DSP escaped uploaded program: pc=2c75' not in output:
        raise ValueError('missing-code boundary is absent or differs')
    required = [
        r'release entry=0f00 words=223 prom_input=0006 clock=13000000 stage=verifier',
        r'publication word0=0000 word1=0006 word2=0006 word3=0006 pc=0f64',
        r'release entry=0f00 words=126 prom_input=0006 clock=13000000 stage=loader',
        r'loader2_verified words=623 entry=0a00',
    ]
    positions = []
    for pattern in required:
        found = list(re.finditer(pattern, trace))
        if len(found) != (2 if pattern == required[0] else 1):
            raise ValueError('native stage inventory differs: ' + pattern)
        positions.append(found[-1].start())
    if positions != sorted(positions):
        raise ValueError('native stages are out of order')
    if 'runtime_hle_handoff' in trace or 'c54x_failure:' in trace:
        raise ValueError('unexpected HLE substitution or CPU failure')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('rom_root', type=Path)
    parser.add_argument('run_dir', type=Path)
    args = parser.parse_args()
    run, root = args.run_dir.resolve(), args.rom_root.resolve()
    try:
        image = (root / 'noki8290/8290f522a.fls').read_bytes()
        extract(image, '8290')
        if run.exists():
            raise ValueError('use a fresh run directory')
        run.mkdir(parents=True)
        result = subprocess.run([
            str(args.binary.resolve()), 'nsb7stage', '-rompath', str(root),
            '-noreadconfig', '-video', 'none', '-sound', 'none', '-nothrottle',
            '-seconds_to_run', '2', '-log'], cwd=run, capture_output=True,
            text=True, timeout=30)
        output = result.stdout + result.stderr
        (run / 'native_output.log').write_text(output)
        (run / 'exit_status.json').write_text(json.dumps({'returncode': result.returncode}) + '\n')
        check(image, output, (run / 'error.log').read_text(), result.returncode)
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSB-7 native boundary: FAIL: {error}\n')
    print('PASS: own NSB-7 verifier/loaders; resident 2c75 unavailable; no idle/native speech claim')


if __name__ == '__main__':
    main()
