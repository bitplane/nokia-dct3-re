"""Read-only inventory of acquired MU4 A00 containers; no load-address claims."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


MARKERS = {0xaa55, 0xaa22, 0xaa44, 0xaa88, 0xaabb, 0xaadd, 0xaa99}


def inventory(image):
    offset = 0
    segments = []
    while offset < len(image):
        if len(image) - offset < 10:
            raise ValueError('truncated segment header/trailer')
        marker, length = struct.unpack_from('>HI', image, offset)
        if marker not in MARKERS:
            raise ValueError(f'unknown segment marker at {offset:x}')
        end = offset + 6 + length
        if end + 4 > len(image):
            raise ValueError('segment length exceeds container')
        payload = image[offset + 6:end]
        segments.append({'offset': offset, 'marker': f'{marker:04x}',
                         'payload_bytes': length,
                         'payload_sha256': hashlib.sha256(payload).hexdigest(),
                         'first_bytes': payload[:16].hex(),
                         'opaque_trailer': image[end:end + 4].hex()})
        offset = end + 4
    if not segments or segments[0]['marker'] != 'aa55':
        raise ValueError('missing initial aa55 segment')
    return {'decoded_bytes': len(image),
            'decoded_sha256': hashlib.sha256(image).hexdigest(),
            'segments': segments, 'coverage_bytes': offset,
            'scope': 'container extents only; trailer checks, load addresses and execution unvalidated'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    args = parser.parse_args()
    try:
        source = args.image.read_bytes()
        report = inventory(bytes.fromhex(source.decode('ascii')))
    except (ValueError, OSError) as exc:
        parser.exit(1, f'A00 inventory: {exc}\n')
    report['source_sha256'] = hashlib.sha256(source).hexdigest()
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
