"""Compare captured unsmoothed enhancement gain control with original reference AGC."""
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
from tools.c54x_rom4_efr_filter_check import native_records as filter_records
from tools.c54x_rom4_efr_lost_sid_state_check import parse_native
from tools.efr_reference_calculator import calculate, compare_vectors
from tools.run_efr_mixing_replay import total_records

CALL = re.compile(r'efr_agc_call: return=([0-9a-f]{4}) input=([0-9a-f]{4}) output=([0-9a-f]{4}) mode=([0-9a-f]{4})')
VECTOR = re.compile(r'efr_agc: kind=(INPUT|PRE|POST) values=([0-9a-f]{160})')
# Saturating pitch<<3 exceeds 0x4000 exactly when pitch exceeds 2048.
ENHANCEMENT_PITCH_THRESHOLD = 2048


def agc_records(text, states):
    groups = []
    for line in text.splitlines():
        if line == 'efr_boundary: kind=entry':
            groups.append([])
        elif line.startswith(('efr_agc_call:', 'efr_agc:')):
            if not groups:
                raise ValueError('unowned enhancement AGC observation')
            groups[-1].append(line)
    if len(groups) != len(states) * 2:
        raise ValueError('incomplete enhancement AGC frames')
    records = {}
    for frame, state in enumerate(states):
        group = groups[frame * 2]
        if group != groups[frame * 2 + 1]:
            raise ValueError('enhancement AGC replay differs')
        subs = [sub for sub in range(4) if state['pitch'][sub] > ENHANCEMENT_PITCH_THRESHOLD]
        if len(group) != len(subs) * 4:
            raise ValueError('enhancement AGC calls disagree with pitch admission')
        for index, sub in enumerate(subs):
            match = CALL.fullmatch(group[index * 4])
            if not match or tuple(int(x, 16) for x in match.groups()) != (
                    0xc885, 0x1514 + sub * 40, 0xaf2, 1):
                raise ValueError('enhancement AGC argument ownership differs')
            fields = {}
            for offset, kind in enumerate(('INPUT', 'PRE', 'POST'), 1):
                match = VECTOR.fullmatch(group[index * 4 + offset])
                if not match or match[1] != kind:
                    raise ValueError('enhancement AGC vector order/width differs')
                fields[kind] = tuple(int(match[2][i:i + 4], 16) for i in range(0, 160, 4))
            records[frame, sub] = fields
    return records


def check_chain(records, filters, totals):
    keys = {(frame, sub) for frame in range(len(totals)) for sub in range(4)}
    if not records.keys() <= keys:
        raise ValueError('AGC record lies outside the synthesis chain')
    for frame, sub in sorted(keys):
        total = totals[frame][sub]
        if (frame, sub) in records:
            fields = records[frame, sub]
            if fields['INPUT'] != total:
                raise ValueError('AGC input differs from captured total excitation')
            expected = fields['POST']
        else:
            expected = total
        if filters[frame, sub, 'EXC'] != expected:
            raise ValueError('synthesis input differs from AGC output/bypass')
    return dict(normalized_calls=len(records), bypass_calls=len(keys) - len(records))


def prepare(text):
    states = parse_native(text, frames=518, synthesis=True)
    filters = filter_records(text)
    records = agc_records(text, states)
    chain = check_chain(records, filters, total_records(text))
    vectors, expected = bytearray(), []
    for key, fields in records.items():
        vectors.extend(struct.pack('<80H', *(fields['INPUT'] + fields['PRE'])))
        expected.append((key, fields['POST']))
    return bytes(vectors), expected, chain


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        native = args.native.read_bytes()
        vectors, expected, chain = prepare(native.decode('utf-8-sig'))
        output = args.output.resolve()
        result, evidence = calculate(args.source.resolve(), output, vectors, mode='agc')
        report = compare_vectors(result, expected, scope='captured-argument enhancement AGC only')
        report.update(evidence, chain=chain, native_log_sha256=hashlib.sha256(native).hexdigest())
        (output / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'EFR enhancement AGC FAIL: {error}\n')
    print(f"reference enhancement AGC: calls={report['calls']} differing_samples={report['differing_samples']}; no full-decoder conformance claim")


if __name__ == '__main__':
    main()
