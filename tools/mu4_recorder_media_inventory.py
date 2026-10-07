"""Read-only NAND delta inventory; signatures are not codec or file-validity proof."""
import argparse
import hashlib
import json
from pathlib import Path
import sys


def inventory(source, endpoint, page_bytes=528, data_bytes=512):
    if not 0 < data_bytes <= page_bytes:
        raise ValueError('invalid NAND geometry')
    if len(source) != len(endpoint) or not source or len(source) % page_bytes:
        raise ValueError('images must have equal nonzero whole-page lengths')
    changed = []
    spare_changed = []
    runs = []
    signatures = []
    for page in range(len(source) // page_bytes):
        start = page * page_bytes
        old = source[start:start + page_bytes]
        new = endpoint[start:start + page_bytes]
        if old == new:
            continue
        changed.append(page)
        if old[data_bytes:] != new[data_bytes:]:
            spare_changed.append(page)
        if runs and runs[-1][1] == page:
            runs[-1][1] = page + 1
        else:
            runs.append([page, page + 1])
        for signature in (b'REL_001 ', b'ID3', b'POCP'):
            offset = new[:data_bytes].find(signature)
            if offset >= 0:
                signatures.append({'literal': signature.decode('ascii'),
                                   'page': page, 'data_offset': offset,
                                   'image_offset': start + offset})
    return {'image_bytes': len(endpoint), 'page_bytes': page_bytes,
            'data_bytes': data_bytes,
            'source_sha256': hashlib.sha256(source).hexdigest(),
            'endpoint_sha256': hashlib.sha256(endpoint).hexdigest(),
            'changed_pages': changed, 'changed_page_ranges_end_exclusive': runs,
            'spare_changed_pages': spare_changed, 'raw_signatures': signatures,
            'scope': 'raw storage delta only; no filesystem, codec, protection or completed-recording inference'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('endpoint', type=Path)
    args = parser.parse_args()
    try:
        report = inventory(args.source.read_bytes(), args.endpoint.read_bytes())
    except (OSError, ValueError) as error:
        print(f'mu4 recorder media inventory: {error}', file=sys.stderr)
        return 1
    print(json.dumps(report, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
