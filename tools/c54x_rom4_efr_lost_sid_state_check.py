"""Locate the strict lost-SID prefix difference without claiming conformance."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.c54x_rom4_efr_state_check import parse, POLICY

BOUNDARY = re.compile(r'efr_boundary: kind=(R2|LSF|pitch|code) values=([0-9a-f]+)')
NATIVE_POLICY = re.compile(r'efr_gain_policy: previous=([0-9a-f]{4}) gain=([0-9a-f]{4}) candidate_b=([0-9a-f]{10})')


def parse_native(text, *, frames=98):
    groups = []
    for line in text.splitlines():
        if line == 'efr_boundary: kind=entry':
            groups.append([])
        elif line.startswith(('efr_boundary:', 'efr_gain_policy:')):
            match = BOUNDARY.fullmatch(line) or NATIVE_POLICY.fullmatch(line)
            if not match or not groups:
                raise ValueError('malformed or unowned native boundary')
            groups[-1].append(line)
    if len(groups) != frames * 2:
        raise ValueError(f'expected {frames} original/replay frame pairs')
    frames = []
    for index in range(0, len(groups), 2):
        group = groups[index]
        if group != groups[index + 1]:
            raise ValueError('native boundaries differ after restoration')
        fields = {kind: [] for kind in ('R2', 'LSF', 'pitch', 'code', 'policy')}
        order = []
        for line in group:
            match = BOUNDARY.fullmatch(line)
            if match:
                kind, values = match.groups()
                if len(values) != (40 if kind in ('R2', 'LSF') else 4):
                    raise ValueError('wrong native field width')
                fields[kind].extend(int(values[i:i + 4], 16) for i in range(0, len(values), 4))
            else:
                kind = 'policy'
                fields[kind].append(tuple(int(value, 16) for value in NATIVE_POLICY.fullmatch(line).groups()))
            order.append(kind)
        with_policy = ['R2', 'LSF'] + ['pitch', 'policy', 'code'] * 4
        without_policy = ['R2', 'LSF'] + ['pitch', 'code'] * 4
        if order not in (with_policy, without_policy):
            raise ValueError('native semantic boundary order differs')
        frames.append(fields)
    return frames


def compare(native, reference):
    return compare_prefix(parse_native(native), reference)


def compare_prefix(frames, reference):
    states, gains = parse(reference, frames=518)
    for frame, actual in enumerate(frames):
        for kind in ('R2', 'LSF'):
            if tuple(actual[kind]) != states[frame, kind]:
                raise ValueError(f'earlier {kind} difference at frame {frame}')
        for kind in ('pitch', 'code'):
            expected = [gains[frame, kind, sub] for sub in range(4)]
            if actual[kind] != expected and (kind, frame) != ('code', 97):
                raise ValueError(f'unclassified {kind} gain difference at frame {frame}')
    last = frames[97]
    if last['pitch'] != [0] * 4 or last['code'] != [8, 9, 12, 84]:
        raise ValueError('native recovery gains differ')
    policies = []
    for line in reference.splitlines():
        if line.startswith('EFRPOLICY frame=97 '):
            match = POLICY.fullmatch(line)
            if not match:
                raise ValueError('malformed reference recovery policy')
            policies.append(tuple(int(match[i], 16 if i >= 3 else 10) for i in (2, 3, 4, 5)))
    if policies != [(i, 1, 10 if i == 0 else 8, 8) for i in range(4)]:
        raise ValueError('reference previous-bad gain cap differs')
    native_policy = last['policy']
    if len(native_policy) != 4:
        raise ValueError('missing native gain-clamp observations')
    for subframe, (previous, gain, accumulator) in enumerate(native_policy):
        candidate = (accumulator >> 16) & 0xffff
        if previous or gain != [10, 8, 9, 12][subframe] or candidate != last['code'][subframe]:
            raise ValueError('native unclamped candidate does not explain gain')
        if min(candidate, policies[subframe][2]) != gains[97, 'code', subframe]:
            raise ValueError('reference cap does not reproduce gain')
    return dict(compared_prefix_frames=98, matched_r2_words=980, matched_lsf_words=980,
                matched_pitch_gains=392, first_code_gain_difference_frame=97,
                native_code_gains=last['code'], reference_code_gains=[8] * 4,
                reference_cap_reproduced=True, conformance_claim=False,
                complete_fixture_claim=False, handset_audio_claim=False)


def characterize(native, reference):
    frames = parse_native(native, frames=518)
    prefix = compare_prefix(frames[:98], reference)
    states, gains = parse(reference, frames=518)
    boundaries = {}
    for kind in ('R2', 'LSF', 'pitch', 'code'):
        differences = []
        for index, actual in enumerate(frames):
            expected = (states[index, kind] if kind in ('R2', 'LSF') else
                        [gains[index, kind, sub] for sub in range(4)])
            count = sum(a != b for a, b in zip(actual[kind], expected))
            if count:
                differences.append(dict(frame=index, words=count))
        boundaries[kind] = dict(
            compared_words=518 * (10 if kind in ('R2', 'LSF') else 4),
            differing_frames=differences,
            differing_words=sum(row['words'] for row in differences),
            first_difference=differences[0]['frame'] if differences else None,
            last_difference=differences[-1]['frame'] if differences else None)
    return dict(status='characterized', frames=518, replayed_frames=518,
                prefix=prefix, boundaries=boundaries, conformance_claim=False,
                handset_audio_claim=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native', type=Path)
    parser.add_argument('reference', type=Path)
    parser.add_argument('--complete', action='store_true',
                        help='characterize all 518 frames; never promotes reference conformance')
    parser.add_argument('--output', type=Path, help='retain the report with input hashes')
    args = parser.parse_args()
    try:
        report = (characterize if args.complete else compare)(
            args.native.read_text(), args.reference.read_text())
        report['input_sha256'] = {
            name: hashlib.sha256(path.read_bytes()).hexdigest()
            for name, path in (('native_log', args.native), ('reference_log', args.reference))}
        rendered = json.dumps(report, indent=2) + '\n'
        if args.output:
            args.output.write_text(rendered)
    except (OSError, ValueError) as error:
        parser.exit(1, f'FAIL: {error}\n')
    print(rendered, end='')


if __name__ == '__main__':
    main()
