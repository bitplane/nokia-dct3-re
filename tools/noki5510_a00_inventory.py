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


def extract_section(image, address, marker=None):
    """Return one original big-endian record payload, without filling holes."""
    if image[:2] in (b'\x08\xaa', b'\x10\xaa'):
        if marker is not None:
            raise ValueError('standalone boot stream has no segment marker')
        report = serial_boot_inventory(image)
        payload = image
    else:
        container = inventory(image)
        selected = [s for s in container['segments'] if s['marker'] == marker]
        if len(selected) != 1:
            raise ValueError('select exactly one container segment marker')
        segment = selected[0]
        report = segment.get('serial_boot', segment.get('section_stream'))
        if report is None:
            raise ValueError('segment section grammar is unvalidated')
        start = segment['offset'] + 6
        payload = image[start:start + segment['payload_bytes']]
    selected = [s for s in report['sections']
                if s['destination_word_address'] == address]
    if len(selected) != 1:
        raise ValueError('select exactly one section destination word address')
    section = selected[0]
    start = section['offset'] + 6
    data = payload[start:start + section['words'] * 2]
    if hashlib.sha256(data).hexdigest() != section['payload_sha256']:
        raise ValueError('extracted section hash mismatch')
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--extract-section', type=lambda value: int(value, 0),
                        help='exact destination word address; export original big-endian bytes')
    parser.add_argument('--segment', help='container marker, e.g. aa55; omit for InitDisk')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if (args.extract_section is None) != (args.output is None):
        parser.error('--extract-section and --output must be used together')
    if args.segment is not None and args.output is None:
        parser.error('--segment requires section extraction')
    try:
        source = args.image.read_bytes()
        image = bytes.fromhex(source.decode('ascii'))
        if args.output is not None:
            data = extract_section(image, args.extract_section, args.segment)
            # A research export must never overwrite an existing artifact.
            with args.output.open('xb') as output:
                output.write(data)
            print(json.dumps({'destination_word_address': args.extract_section,
                              'bytes': len(data),
                              'sha256': hashlib.sha256(data).hexdigest()}))
            return
        report = (serial_boot_inventory(image)
                  if image[:2] in (b'\x08\xaa', b'\x10\xaa') else inventory(image))
    except (ValueError, OSError) as exc:
        parser.exit(1, f'A00 inventory: {exc}\n')
    report['source_sha256'] = hashlib.sha256(source).hexdigest()
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
