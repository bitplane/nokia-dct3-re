"""Run synthetic completed frames through the actual native DSP backend."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

MARKER = 'C54x native backend BSP: PASS cases=3 save_replay=3 handset_audio_claim=0'
COMPLETION = 'Fatal error: C54x backend BSP tests complete'


def check_result(text, returncode):
    lines = text.splitlines()
    if returncode != 3 or lines.count(MARKER) != 1:
        raise ValueError('missing unique successful backend BSP completion')
    if not lines or lines[-1] != COMPLETION:
        raise ValueError('missing terminal backend BSP completion')
    if any(line.startswith('Fatal error:') and line != COMPLETION for line in lines):
        raise ValueError('unexpected backend BSP fatal error')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mame', type=Path, required=True)
    parser.add_argument('--output-root', type=Path, default=Path('tmp'))
    args = parser.parse_args()
    args.output_root.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='c54x-backend-bsp-', dir=args.output_root.resolve()))
    command = [str(args.mame.resolve()), 'tms54bsp', '-video', 'none',
               '-sound', 'none', '-nothrottle', '-noreadconfig', '-log',
               '-seconds_to_run', '1']
    print(f'Native backend BSP evidence: {run}', flush=True)
    with (run / 'console.log').open('w') as output:
        result = subprocess.run(command, cwd=run, stdout=output, stderr=subprocess.STDOUT)
    text = (run / 'console.log').read_text()
    print(text, end='')
    try:
        check_result(text, result.returncode)
    except ValueError as error:
        print(f'Native backend BSP: FAIL: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
