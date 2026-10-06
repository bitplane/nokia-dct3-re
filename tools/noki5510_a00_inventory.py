"""Read-only MU4 A00 container and C54x serial-boot inventory; no execution."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


MARKERS = {0xaa55, 0xaa22, 0xaa44, 0xaa88, 0xaabb, 0xaadd, 0xaa99}


def serial_boot_inventory(image):
    """Decode SPRA602F figure 11; never execute register or memory writes."""
    if len(image) < 16 or len(image) % 2:
        raise ValueError('truncated or odd-length serial boot table')
    words = struct.unpack_from('>7H', image)
    if words[0] not in (0x08aa, 0x10aa):
        raise ValueError('unknown serial boot signature')
    if words[5] > 0x7f:
        raise ValueError('entry XPC exceeds seven bits')
    report = section_inventory(image, 14)
    return {'format': 'C54x serial boot, SPRA602F figure 11',
            'compatibility_words': [f'{w:04x}' for w in words[1:5]],
            'entry_word_address': (words[5] << 16) | words[6],
            **report,
            'scope': 'static word addresses; DA150 execution and peripheral map unvalidated'}


def section_inventory(image, offset=0):
    """Inventory count/XPC/PC records, retaining extended word addresses."""
    if len(image) % 2:
        raise ValueError('odd-length section stream')
    sections = []
    while offset + 2 <= len(image):
        start = offset
        count = struct.unpack_from('>H', image, offset)[0]
        offset += 2
        if not count:
            if offset != len(image):
                raise ValueError('bytes follow serial boot terminator')
            return {'sections': sections, 'coverage_bytes': offset}
        if offset + 4 + count * 2 > len(image):
            raise ValueError('serial boot section exceeds input')
        xpc, pc = struct.unpack_from('>HH', image, offset)
        if xpc > 0x7f:
            raise ValueError('section XPC exceeds seven bits')
        offset += 4
        payload = image[offset:offset + count * 2]
        sections.append({'offset': start, 'words': count,
                         'destination_word_address': (xpc << 16) | pc,
                         'payload_sha256': hashlib.sha256(payload).hexdigest()})
        offset += count * 2
    raise ValueError('missing serial boot terminator')


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
        if offset == 0 and payload[:2] in (b'\x08\xaa', b'\x10\xaa'):
            segments[-1]['serial_boot'] = serial_boot_inventory(payload)
        elif offset and 'serial_boot' in segments[0]:
            segments[-1]['section_stream'] = section_inventory(payload)
        offset = end + 4
    if not segments or segments[0]['marker'] != 'aa55':
        raise ValueError('missing initial aa55 segment')
    return {'decoded_bytes': len(image),
            'decoded_sha256': hashlib.sha256(image).hexdigest(),
            'segments': segments, 'coverage_bytes': offset,
            'scope': 'container and section extents; trailer integrity, overlay selection and DA150 execution unvalidated'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    args = parser.parse_args()
    try:
        source = args.image.read_bytes()
        image = bytes.fromhex(source.decode('ascii'))
        report = (serial_boot_inventory(image)
                  if image[:2] in (b'\x08\xaa', b'\x10\xaa') else inventory(image))
    except (ValueError, OSError) as exc:
        parser.exit(1, f'A00 inventory: {exc}\n')
    report['source_sha256'] = hashlib.sha256(source).hexdigest()
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
