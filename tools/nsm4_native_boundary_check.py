"""Execute NSM-4's own uploaded verifier and require its missing-code boundary."""
import argparse
import hashlib
from pathlib import Path
import re
import struct
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsm4_static_check import inspect


def check(source, output, trace, returncode):
    if hashlib.sha1(source).hexdigest() != '8f740f901f51ea60dcca560de011891eb8026f8a':
        raise ValueError('requires own MCU/PPM-P prefix')
    inspect(source[:0x290000])
    verifier = source[0x2019e4:0x2019e4 + 492]
    if struct.unpack_from('>HH', verifier, 0x1ac) != (0xf074, 0x861f):
        raise ValueError('own verifier does not CALL 861f at the recovered site')
    check_trace(output, trace, returncode)


def check_trace(output, trace, returncode):
    if returncode != 1:
        raise ValueError('native execution did not stop at a controlled boundary')
    if not re.search(r'release entry=0f00 words=246 prom_input=0006 clock=13000000 stage=verifier', trace):
        raise ValueError('own 246-word verifier release missing')
    if 'readonly_program_write address=ff87 data=0006 pc=0f12' not in trace:
        raise ValueError('immutable fragment probe missing')
    marker = 'address=861f pc=8620 stage=verifier'
    if f'unavailable_program {marker}' not in trace or marker not in output:
        raise ValueError('missing resident CALL frontier differs')
    if any(value in trace for value in ('staged_dsp: publication', 'runtime_hle_handoff', 'loader2_verified')):
        raise ValueError('unexpected completion beyond absent resident code')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('rom_root', type=Path)
    parser.add_argument('run_dir', type=Path)
    args = parser.parse_args()
    binary, root, run = args.binary.resolve(), args.rom_root.resolve(), args.run_dir.resolve()
    try:
        source = (root / 'noki8855/8855f518p.fls').read_bytes()
        inspect(source[:0x290000])
        run.mkdir(parents=True, exist_ok=False)
        result = subprocess.run([str(binary), 'nsm4stage', '-rompath', str(root),
                                 '-noreadconfig', '-video', 'none', '-sound', 'none',
                                 '-nothrottle', '-seconds_to_run', '2', '-log'],
                                cwd=run, capture_output=True, text=True, timeout=30)
        output = result.stdout + result.stderr
        (run / 'native_output.log').write_text(output)
        check(source, output, (run / 'error.log').read_text(errors='replace'), result.returncode)
    except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
        parser.exit(1, f'NSM-4 native boundary: {exc}\n')
    print('NSM-4 native boundary PASS: own verifier CALL 861f; resident program unavailable, no completion')


if __name__ == '__main__':
    main()
