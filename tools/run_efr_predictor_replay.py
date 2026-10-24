"""Compare original adaptive-predictor arithmetic on captured native histories."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.c54x_rom4_efr_predictor_check import native_records
from tools.c54x_rom4_efr_excitation_check import native_records as excitation_records
from tools.efr_reference_calculator import calculate, compare_vectors


def prepare(text):
    inputs = native_records(text)
    _, outputs = excitation_records(text)
    vectors, expected = bytearray(), []
    for key, (lag, fraction, history) in inputs.items():
        vectors.extend(struct.pack('<156H', lag, fraction, *history))
        expected.append((key, outputs[key[0], key[1], 'ADAPT']))
    return bytes(vectors), expected


def compare(output, expected):
    return compare_vectors(output, expected, scope='captured-argument predictor arithmetic only')


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
        result, evidence = calculate(args.source.resolve(), output, vectors, mode='predictor')
        report = compare(result, expected)
        report.update(evidence, native_log_sha256=hashlib.sha256(native).hexdigest())
        (output / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'EFR predictor arithmetic FAIL: {error}\n')
    print(f"reference predictor comparison: calls={report['calls']} differing_samples={report['differing_samples']}; no full-decoder conformance claim")


if __name__ == '__main__':
    main()
