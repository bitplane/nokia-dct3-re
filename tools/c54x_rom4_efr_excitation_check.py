"""Characterize fixed/adaptive excitation before gain mixing; no conformance claim."""
import argparse
import hashlib
import json
from pathlib import Path
import re

CALL = re.compile(r'efr_excitation_call: ctrl=([0-9a-f]{4}) base=([0-9a-f]{4}) offset=([0-9a-f]{4})')
VECTOR = re.compile(r'efr_excitation: kind=(FIXED|ADAPT) values=([0-9a-f]{160})')
REF_CALL = re.compile(r'EFREXCCALL frame=(\d+) subframe=(\d+) ctrl=([0-9a-f]{4}) length=40')
REF_PRED = re.compile(r'EFREXCPRED frame=(\d+) subframe=(\d+) length=40')
REF_VECTOR = re.compile(r'EFREXC frame=(\d+) subframe=(\d+) kind=(FIXED|ADAPT) values=([0-9a-f]{160})')


def words(text):
    return tuple(int(text[i:i + 4], 16) for i in range(0, len(text), 4))


def reference_records(text, frames=518):
    controls, vectors, events = {}, {}, []
    for line in text.splitlines():
        if line.startswith('EFREXCCALL '):
            match = REF_CALL.fullmatch(line)
            if not match:
                raise ValueError('malformed reference excitation call')
            key = (int(match[1]), int(match[2]))
            if key in controls:
                raise ValueError('duplicate reference excitation call')
            controls[key] = int(match[3], 16)
            events.append((*key, 'CALL'))
        elif line.startswith('EFREXCPRED '):
            match = REF_PRED.fullmatch(line)
            if not match:
                raise ValueError('malformed reference prediction call')
            events.append((int(match[1]), int(match[2]), 'PRED'))
        elif line.startswith('EFREXC '):
            match = REF_VECTOR.fullmatch(line)
            if not match:
                raise ValueError('malformed reference excitation vector')
            key = (int(match[1]), int(match[2]), match[3])
            if key in vectors:
                raise ValueError('duplicate reference excitation vector')
            vectors[key] = words(match[4])
            events.append(key)
    keys = [(frame, sub) for frame in range(frames) for sub in range(4)]
    if list(controls) != keys:
        raise ValueError('incomplete reference excitation calls')
    expected = []
    for frame, sub in keys:
        if controls[frame, sub] & 1:
            expected.extend(((frame, sub, 'PRED'), (frame, sub, 'ADAPT')))
        expected.extend(((frame, sub, 'CALL'), (frame, sub, 'FIXED')))
    if events != expected:
        raise ValueError('reference prediction/excitation coverage or order differs')
    return controls, vectors


def native_records(text, frames=518):
    groups = []
    for line in text.splitlines():
        if line == 'efr_boundary: kind=entry':
            groups.append([])
        elif line.startswith(('efr_excitation_call:', 'efr_excitation:')):
            if not groups:
                raise ValueError('unowned native excitation observation')
            groups[-1].append(line)
    if len(groups) != frames * 2:
        raise ValueError('incomplete native excitation frames')
    controls, vectors = {}, {}
    for frame in range(frames):
        group = groups[frame * 2]
        if group != groups[frame * 2 + 1]:
            raise ValueError('native excitation replay differs')
        if len(group) != 12:
            raise ValueError('incomplete native excitation calls')
        for sub in range(4):
            match = CALL.fullmatch(group[sub * 3])
            if not match or (int(match[2], 16), int(match[3], 16)) != (0x1514, sub * 40):
                raise ValueError('native excitation argument ownership differs')
            controls[frame, sub] = int(match[1], 16)
            for offset, kind in enumerate(('FIXED', 'ADAPT'), 1):
                match = VECTOR.fullmatch(group[sub * 3 + offset])
                if not match or match[1] != kind:
                    raise ValueError('native excitation vector order/width differs')
                vectors[frame, sub, kind] = words(match[2])
    return controls, vectors


def compare(native, reference, *, frames=518):
    control, actual = native_records(native, frames)
    expected_control, expected = reference_records(reference, frames)
    if any((control[key] & 1) != (value & 1) for key, value in expected_control.items()):
        raise ValueError('native/reference speech-admission bit differs')
    result = {}
    for kind in ('FIXED', 'ADAPT'):
        differences, compared = [], 0
        for key, values in expected.items():
            if key[2] != kind:
                continue
            compared += 40
            count = sum(a != b for a, b in zip(actual[key], values))
            if count:
                differences.append(dict(frame=key[0], subframe=key[1], words=count))
        result[kind] = dict(compared_words=compared, differences=differences,
                            differing_words=sum(row['words'] for row in differences),
                            first=differences[0] if differences else None,
                            last=differences[-1] if differences else None)
    return dict(status='characterized', frames=frames, boundaries=result,
                adaptive_scope='original speech-admitted prediction calls only',
                conformance_claim=False, handset_audio_claim=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native', type=Path)
    parser.add_argument('reference', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        native, reference = args.native.read_bytes(), args.reference.read_bytes()
        result = compare(native.decode('utf-8-sig'), reference.decode())
        result['input_sha256'] = dict(native=hashlib.sha256(native).hexdigest(),
                                     reference=hashlib.sha256(reference).hexdigest())
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    except (OSError, ValueError) as error:
        parser.exit(1, f'EFR excitation observation FAIL: {error}\n')
    print(json.dumps({kind: {key: value for key, value in row.items() if key != 'differences'}
                      for kind, row in result['boundaries'].items()}, indent=2))


if __name__ == '__main__':
    main()
