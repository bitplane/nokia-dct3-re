#!/usr/bin/env python3
"""Differential oracle for restructuring the C54x pipeline model.

Runs the native nse1ap5 laboratory traffic-channel call (boot, registration,
a paged TCH/F call with speech both ways and clearing: about 343 million DSP
clocks) with the core's C54X_STATE_TRACE hash enabled, then compares it with
the banked reference. The hash folds every clock's architectural registers
and every data, I/O and program write, so any change in what the DSP does or
when it does it shows up in the first window (2^20 clocks) where it happens.

A change to the laboratory driver or the firmware fixture also changes the
trace; re-bank with --rebank only when the model is meant to change.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

REFERENCE = Path('fixtures/c54x_state_trace_traffic.txt')


def parse(text):
    """Rows of (clock, hash, time) and the final (clock, hash) marker."""
    rows, end = [], None
    for line in text.splitlines():
        clock, digest, tail = line.split(' ', 2)
        if tail == 'end':
            end = (int(clock), digest)
        else:
            rows.append((int(clock), digest, tail))
    if end is None:
        raise ValueError('trace has no end marker; the run did not finish')
    return rows, end


def first_divergence(reference, candidate):
    """None if the traces agree, otherwise a description of the first difference."""
    expected, expected_end = parse(reference)
    actual, actual_end = parse(candidate)
    for index, (want, got) in enumerate(zip(expected, actual)):
        if want[:2] != got[:2]:
            after = expected[index - 1] if index else (0, '', '0')
            return (f'first divergence between clocks {after[0]} and {want[0]} '
                    f'(emulated {after[2]}..{want[2]} s)')
    if len(expected) != len(actual) or expected_end != actual_end:
        return f'traces agree to clock {min(len(expected), len(actual)) << 20} but end differently: {expected_end} != {actual_end}'
    return None


def run(root, mame, output):
    environment = dict(os.environ, C54X_STATE_TRACE=str(output), C54X_STATE_TRACE_TAG=':dsp_c54x:cpu')
    command = [sys.executable, 'tools/run_rom4_lab_registration.py', 'nse1ap5', '--traffic-channel']
    if mame:
        command += ['--mame', str(mame)]
    result = subprocess.run(command, cwd=root, env=environment, capture_output=True, text=True)
    if result.returncode:
        raise SystemExit(f'FAIL - traffic-channel scenario failed:\n{result.stdout[-2000:]}{result.stderr[-2000:]}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--rebank', action='store_true', help='replace the reference with this run')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(dir=root / 'tmp') as scratch:
        output = Path(scratch) / 'trace.txt'
        run(root, args.mame, output)
        candidate = output.read_text()
    parse(candidate)
    if args.rebank:
        (root / REFERENCE).write_text(candidate)
        print(f'C54x state trace re-banked: {len(candidate.splitlines())} lines')
        return
    difference = first_divergence((root / REFERENCE).read_text(), candidate)
    if difference:
        raise SystemExit(f'FAIL - C54x state trace: {difference}')
    print(f'C54x state trace: PASS ({parse(candidate)[1][0]} clocks identical to the reference)')


if __name__ == '__main__':
    main()
