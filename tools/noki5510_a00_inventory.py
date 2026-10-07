"""Read-only MU4 A00 container and C54x serial-boot inventory; no execution."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


MARKERS = {0xaa55, 0xaa22, 0xaa44, 0xaa88, 0xaabb, 0xaadd, 0xaa99}


def cinit_inventory(image):
    """Decode the count/destination/words loop recovered at InitData 0xe5b."""
    if len(image) % 2:
        raise ValueError('odd-length C initialization table')
    records = []
    offset = 0
    while offset + 2 <= len(image):
        start = offset
        count = struct.unpack_from('>H', image, offset)[0]
        offset += 2
        if not count:
            if offset != len(image):
                raise ValueError('bytes follow C initialization terminator')
            return {'records': records, 'coverage_bytes': offset,
                    'scope': 'static startup data writes; no execution or physical memory mapping inferred'}
        if offset + 2 + count * 2 > len(image):
            raise ValueError('C initialization record exceeds input')
        destination = struct.unpack_from('>H', image, offset)[0]
        offset += 2
        if destination + count > 0x10000:
            raise ValueError('C initialization destination wraps 16-bit data space')
        payload = image[offset:offset + count * 2]
        record = {'offset': start, 'words': count, 'destination_data_word_address': destination,
                  'payload_sha256': hashlib.sha256(payload).hexdigest()}
        if count <= 32:
            record['values'] = [f'{word:04x}' for word in struct.unpack(f'>{count}H', payload)]
        records.append(record)
        offset += count * 2
    raise ValueError('missing C initialization terminator')


def destination_inventory(image, sections):
    """Describe linear record extents; do not choose an overwrite policy."""
    seen = {}
    pages = {}
    same = changed = crossing = 0
    for section in sections:
        address = section['destination_word_address']
        count = section['words']
        if address + count > 0x800000:
            raise ValueError('section extent exceeds 23-bit program address space')
        crossing += (address >> 16) != ((address + count - 1) >> 16)
        start = section['offset'] + 6
        for index, (word,) in enumerate(struct.iter_unpack('>H', image[start:start + count * 2])):
            destination = address + index
            page = destination >> 16
            row = pages.setdefault(page, {'page': page, 'write_words': 0,
                                         'unique_words': 0, 'first_word_address': destination,
                                         'last_word_address': destination})
            row['write_words'] += 1
            row['first_word_address'] = min(row['first_word_address'], destination)
            row['last_word_address'] = max(row['last_word_address'], destination)
            if destination in seen:
                same += seen[destination] == word
                changed += seen[destination] != word
            else:
                row['unique_words'] += 1
            seen[destination] = word
    return {'write_words': sum(section['words'] for section in sections),
            'unique_words': len(seen), 'repeated_same_words': same,
            'repeated_changed_words': changed, 'cross_page_records': crossing,
            'pages': [pages[page] for page in sorted(pages)],
            'scope': 'linear destination extents; repeated values compared with preceding record write; no flattened image or physical aliasing inferred'}


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
            return {'sections': sections, 'coverage_bytes': offset,
                    'destination_inventory': destination_inventory(image, sections)}
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
    report = {'decoded_bytes': len(image),
            'decoded_sha256': hashlib.sha256(image).hexdigest(),
            'segments': segments, 'coverage_bytes': offset,
            'scope': 'container and section extents; trailer integrity, overlay selection and DA150 execution unvalidated'}
    if all('serial_boot' in segment or 'section_stream' in segment for segment in segments):
        sections = []
        for segment in segments:
            stream = segment.get('serial_boot', segment.get('section_stream'))
            sections.extend({**section, 'offset': segment['offset'] + 6 + section['offset']}
                            for section in stream['sections'])
        report['all_segment_destinations'] = destination_inventory(image, sections)
    return report


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
    parser.add_argument('--cinit-section', type=lambda value: int(value, 0),
                        help='inspect one exact section as a recovered C initialization table')
    parser.add_argument('--segment', help='container marker, e.g. aa55; omit for InitDisk')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.cinit_section is not None and (args.extract_section is not None or args.output is not None):
        parser.error('--cinit-section cannot be combined with section export')
    if (args.extract_section is None) != (args.output is None):
        parser.error('--extract-section and --output must be used together')
    if args.segment is not None and args.output is None and args.cinit_section is None:
        parser.error('--segment requires section extraction or C initialization inspection')
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
        report = (cinit_inventory(extract_section(image, args.cinit_section, args.segment))
                  if args.cinit_section is not None else serial_boot_inventory(image)
                  if image[:2] in (b'\x08\xaa', b'\x10\xaa') else inventory(image))
    except (ValueError, OSError) as exc:
        parser.exit(1, f'A00 inventory: {exc}\n')
    report['source_sha256'] = hashlib.sha256(source).hexdigest()
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
