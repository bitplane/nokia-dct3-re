"""Compare observed excitation mixing with original reference fixed-point primitives."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.c54x_rom4_efr_excitation_check import native_records as excitation_records
from tools.c54x_rom4_efr_filter_check import native_records as filter_records
from tools.c54x_rom4_efr_lost_sid_state_check import parse_native
from tools.efr_reference_calculator import calculate, compare_vectors

TOTAL = re.compile(r'efr_mixing: kind=TOTAL values=([0-9a-f]{160})')


def total_records(text, *, frames=518):
    groups = []
    for line in text.splitlines():
        if line == 'efr_boundary: kind=entry':
            groups.append([])
        elif line.startswith('efr_mixing:'):
            if not groups:
                raise ValueError('unowned total excitation')
            match = TOTAL.fullmatch(line)
            if not match:
                raise ValueError('malformed total excitation vector')
            groups[-1].append(tuple(int(match[1][i:i + 4], 16) for i in range(0, 160, 4)))
    if len(groups) != frames * 2:
        raise ValueError('incomplete total-excitation frames')
    for frame in range(frames):
        if len(groups[frame * 2]) != 4 or groups[frame * 2] != groups[frame * 2 + 1]:
            raise ValueError('total excitation is incomplete or differs after replay')
    return [groups[frame * 2] for frame in range(frames)]


def prepare(text):
    _, components = excitation_records(text)
    filter_records(text)
    states = parse_native(text, frames=518, synthesis=True)
    totals = total_records(text)
    vectors, expected = bytearray(), []
    for frame in range(518):
        for sub in range(4):
            vectors.extend(struct.pack('<82H', states[frame]['pitch'][sub], states[frame]['code'][sub],
                                       *components[frame, sub, 'ADAPT'], *components[frame, sub, 'FIXED']))
            expected.append(((frame, sub), totals[frame][sub]))
    return bytes(vectors), expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        native = args.native.read_bytes()
        vectors, expected = prepare(native.decode('utf-8-sig'))
        output = args.output.resolve()
        result, evidence = calculate(args.source.resolve(), output, vectors, mode='mix')
        report = compare_vectors(result, expected, scope='captured-argument gain mixing only')
        report.update(evidence, native_log_sha256=hashlib.sha256(native).hexdigest())
        (output / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'EFR mixing arithmetic FAIL: {error}\n')
    print(f"reference mixing comparison: calls={report['calls']} differing_samples={report['differing_samples']}; no full-decoder conformance claim")


if __name__ == '__main__':
    main()
