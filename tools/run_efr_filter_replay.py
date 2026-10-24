"""Compare original reference filter arithmetic on captured native arguments."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.c54x_rom4_efr_filter_check import native_records
from tools.c54x_rom4_efr_lost_sid_state_check import parse_native
from tools.efr_reference_calculator import calculate


def check_history(records, frames):
    previous = None
    for frame, state in enumerate(frames):
        for sub in range(4):
            if previous is not None and records[frame, sub, 'MEM'] != previous[-10:]:
                raise ValueError('native synthesis history is not the preceding output tail')
            previous = tuple(state['RAW'][sub * 40:(sub + 1) * 40])


def prepare(text):
    records = native_records(text)
    frames = parse_native(text, frames=518, synthesis=True)
    check_history(records, frames)
    vectors, expected = bytearray(), []
    for frame in range(518):
        for sub in range(4):
            vectors.extend(struct.pack('<61H', *(records[frame, sub, 'A'] +
                records[frame, sub, 'EXC'] + records[frame, sub, 'MEM'])))
            expected.append(tuple(frames[frame]['RAW'][sub * 40:(sub + 1) * 40]))
    return bytes(vectors), expected


def compare(output, expected):
    if not expected or any(len(row) != 40 for row in expected):
        raise ValueError('expected complete native filter output vectors')
    if len(output) != len(expected) * 100:
        raise ValueError('incomplete reference filter output')
    differences = []
    for index, (result, actual) in enumerate(zip(struct.iter_unpack('<50H', output), expected)):
        if result[40:] != result[30:40]:
            raise ValueError('reference filter history update differs from its output tail')
        count = sum(a != b for a, b in zip(result[:40], actual))
        if count:
            differences.append(dict(frame=index // 4, subframe=index % 4, samples=count))
    return dict(scope='captured-argument filter arithmetic only', calls=len(expected),
                samples=len(expected) * 40, differences=differences,
                differing_samples=sum(row['samples'] for row in differences),
                reference_equal=not differences, full_decoder_conformance=False,
                handset_audio_claim=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source, output = args.source.resolve(), args.output.resolve()
    try:
        native = args.native.read_bytes()
        vectors, expected = prepare(native.decode('utf-8-sig'))
        result, evidence = calculate(source, output, vectors)
        report = compare(result, expected)
        report.update(evidence, native_history_transitions=2071,
                      native_log_sha256=hashlib.sha256(native).hexdigest())
        (output / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'EFR filter arithmetic FAIL: {error}\n')
    print(f"reference filter comparison: calls={report['calls']} differing_samples={report['differing_samples']}; no full-decoder conformance claim")


if __name__ == '__main__':
    main()
