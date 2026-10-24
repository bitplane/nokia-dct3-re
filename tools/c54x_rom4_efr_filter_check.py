"""Compare read-only decoder synthesis inputs, not codec conformance."""
import argparse
import hashlib
import json
from pathlib import Path
import re

WIDTHS = dict(A=11, EXC=40, MEM=10)
CALL = re.compile(r'efr_filter_call: return=([0-9a-f]{4}) coefficients=([0-9a-f]{4}) input=([0-9a-f]{4}) output=([0-9a-f]{4}) brc=([0-9a-f]{4})')
VECTOR = re.compile(r'efr_filter: kind=(A|EXC|MEM) values=([0-9a-f]+)')
REF_CALL = re.compile(r'EFRFILTERCALL frame=(\d+) subframe=(\d+) length=(\d+) update=(\d+)')
REF_VECTOR = re.compile(r'EFRFILTER frame=(\d+) subframe=(\d+) kind=(A|EXC|MEM) values=([0-9a-f]+)')


def words(kind, text):
    if len(text) != WIDTHS[kind] * 4:
        raise ValueError('incomplete filter vector')
    return tuple(int(text[i:i + 4], 16) for i in range(0, len(text), 4))


def native_records(text, frames=518):
    groups = []
    for line in text.splitlines():
        if line == 'efr_boundary: kind=entry':
            groups.append([])
        elif line.startswith(('efr_filter_call:', 'efr_filter:')):
            if not groups:
                raise ValueError('unowned filter observation')
            groups[-1].append(line)
    if len(groups) != frames * 2:
        raise ValueError('incomplete original/replay filter frames')
    records = {}
    for frame in range(frames):
        group = groups[frame * 2]
        if group != groups[frame * 2 + 1]:
            raise ValueError('filter replay differs')
        if len(group) != 16:
            raise ValueError('expected four complete filter calls per frame')
        for sub in range(4):
            match = CALL.fullmatch(group[sub * 4])
            if not match:
                raise ValueError('malformed native filter call')
            ret, coefficients, source, output, repeat = (int(x, 16) for x in match.groups())
            if (ret not in (0xc890, 0xc89c) or coefficients != 0x1317 + sub * 11 or
                    output != 0x1388 + sub * 40 or repeat != 19 or
                    (ret == 0xc890 and source != 0xaf2)):
                raise ValueError('native filter argument ownership differs')
            for index, kind in enumerate(WIDTHS, 1):
                match = VECTOR.fullmatch(group[sub * 4 + index])
                if not match or match[1] != kind:
                    raise ValueError('unordered native filter vectors')
                records[frame, sub, kind] = words(kind, match[2])
    return records


def reference_records(text, frames=518):
    lines = [line for line in text.splitlines() if line.startswith(('EFRFILTER ', 'EFRFILTERCALL '))]
    if len(lines) != frames * 16:
        raise ValueError('incomplete reference filter coverage')
    records = {}
    for frame in range(frames):
        for sub in range(4):
            offset = (frame * 4 + sub) * 4
            match = REF_CALL.fullmatch(lines[offset])
            if not match or tuple(map(int, match.groups())) != (frame, sub, 40, 1):
                raise ValueError('reference filter call order/contract differs')
            for index, kind in enumerate(WIDTHS, 1):
                match = REF_VECTOR.fullmatch(lines[offset + index])
                if not match or (int(match[1]), int(match[2]), match[3]) != (frame, sub, kind):
                    raise ValueError('unordered reference filter vectors')
                records[frame, sub, kind] = words(kind, match[4])
    return records


def compare(native, reference, *, frames=518):
    actual, expected = native_records(native, frames), reference_records(reference, frames)
    result = {}
    for kind, width in WIDTHS.items():
        differences = []
        for frame in range(frames):
            for sub in range(4):
                count = sum(a != b for a, b in zip(actual[frame, sub, kind], expected[frame, sub, kind]))
                if count:
                    differences.append(dict(frame=frame, subframe=sub, words=count))
        result[kind] = dict(compared_words=frames * 4 * width, differences=differences,
                            differing_words=sum(row['words'] for row in differences),
                            first_frame=differences[0]['frame'] if differences else None,
                            last_frame=differences[-1]['frame'] if differences else None)
    return dict(status='characterized', frames=frames, filter_calls=frames * 4,
                boundaries=result, conformance_claim=False, handset_audio_claim=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('native', type=Path)
    parser.add_argument('reference', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    try:
        native, reference = args.native.read_bytes(), args.reference.read_bytes()
        result = compare(native.decode('utf-8-sig'), reference.decode())
        result['input_sha256'] = dict(native=hashlib.sha256(native).hexdigest(),
                                     reference=hashlib.sha256(reference).hexdigest())
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    except (OSError, ValueError) as error:
        parser.exit(1, f'EFR filter observation FAIL: {error}\n')
    print(json.dumps({kind: {key: value for key, value in row.items() if key != 'differences'}
                      for kind, row in result['boundaries'].items()}, indent=2))


if __name__ == '__main__':
    main()
