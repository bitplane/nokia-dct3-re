#!/usr/bin/env python3
"""Check the native C54x add-compare-select loop against the pipeline model.

Runs the lean 6210 call (tools/bench_6210_call.py) with C54X_ACS=check, which
computes every native span and then lets the model run the same clocks; the
core stops on the first difference in its state or in the words written. Then
compares the call's log with one run with the native loop off.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.bench_6210_call import CONNECTED, prepare, run  # noqa: E402

SPANS = re.compile(r'C54x ACS kernel: (\d+) spans checked')


def spans_checked(console):
    match = SPANS.search(console)
    return int(match.group(1)) if match else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mame', type=Path, default=ROOT / 'mame/mame')
    args = parser.parse_args()
    (ROOT / 'tmp').mkdir(exist_ok=True)
    base = Path(tempfile.mkdtemp(prefix='c54x-acs.', dir=ROOT / 'tmp'))
    logs = {}
    for mode in ('check', 'on', 'off'):
        directory = base / mode
        directory.mkdir()
        prepare(directory)
        os.environ['C54X_ACS'] = mode
        _, _, logs[mode] = run(args.mame.resolve(), directory, 30)
        if mode == 'check':
            spans = spans_checked((directory / 'run30/console.log').read_text(errors='replace'))
    if CONNECTED not in logs['check'] or not spans:
        raise SystemExit(f'FAIL - call not connected or no span checked; evidence in {base}')
    if not logs['check'] == logs['on'] == logs['off']:
        raise SystemExit(f'FAIL - the call log changes with C54X_ACS; evidence in {base}')
    shutil.rmtree(base)
    print(f'C54x native ACS loop: PASS ({spans} spans identical to the model, call log identical)')


if __name__ == '__main__':
    main()
