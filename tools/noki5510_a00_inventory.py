"""Read-only MU4 A00 container and C54x serial-boot inventory; no execution."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from functools import reduce
from operator import xor


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
        stored, sync = struct.unpack_from('>HH', image, end)
        calculated = reduce(xor, payload, 0)
        segments[-1]['receiver_integrity'] = {
            'payload_byte_xor': calculated, 'stored_word': stored,
            'checksum_matches': stored == calculated,
            'following_word': f'{sync:04x}',
            'scope': 'InitDisk 3229/34a5/32a7 payload-only XOR; following word is not part of checksum'}
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
            'scope': 'container and section extents plus InitDisk payload checksum; following trailer word semantics, overlay selection and DA150 execution unvalidated'}
    if all('serial_boot' in segment or 'section_stream' in segment for segment in segments):
        sections = []
        for segment in segments:
            stream = segment.get('serial_boot', segment.get('section_stream'))
            sections.extend({**section, 'offset': segment['offset'] + 6 + section['offset']}
                            for section in stream['sections'])
        report['all_segment_destinations'] = destination_inventory(image, sections)
    return report


def extract_segment(image, marker):
    """Export one unchanged wire segment, including length and original trailer."""
    selected = [s for s in inventory(image)['segments'] if s['marker'] == marker]
    if len(selected) != 1:
        raise ValueError('segment marker must select exactly one segment')
    segment = selected[0]
    if not segment['receiver_integrity']['checksum_matches']:
        raise ValueError('original segment payload checksum mismatch')
    start = segment['offset']
    return image[start:start + 10 + segment['payload_bytes']]


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


def uploaded_word_image(image, marker):
    """Final logical writes from one checked segment, without cross-overlay merging."""
    payload = extract_segment(image, marker)[6:-4]
    words = {}
    for section in section_inventory(payload)['sections']:
        address = section['destination_word_address']
        offset = section['offset'] + 6
        for index, (word,) in enumerate(struct.iter_unpack('>H', payload[offset:offset + section['words'] * 2])):
            target = address + index
            words[target] = word
    return words


def program_call_census(image, start, end, marker):
    """Find immediate FCALL encodings; instruction boundaries remain unproven."""
    if not 0x10000 <= start < end <= 0x800000:
        raise ValueError('program range must be nonempty extended word addresses')
    words = uploaded_word_image(image, marker)
    candidates = []
    missing_extensions = page_end_candidates = 0
    for address in sorted(a for a in words if start <= a < end):
        opcode = words[address]
        if opcode & 0xff80 != 0xf980:
            continue
        if address & 0xffff == 0xffff:
            page_end_candidates += 1
            continue
        if address + 1 not in words or address + 1 >= end:
            missing_extensions += 1
            continue
        candidates.append({'source_word_address': address,
                           'target_word_address': ((opcode & 0x7f) << 16) | words[address + 1]})
    return {'segment': marker, 'range_word_addresses': [start, end],
            'covered_words': sum(start <= a < end for a in words),
            'requested_words': end - start, 'candidates': candidates,
            'missing_extensions': missing_extensions,
            'page_end_candidates': page_end_candidates,
            'scope': 'immediate FCALL encoding candidates only; data/operand false positives, '
                     'indirect calls, other call encodings and physical aliasing unresolved'}


def extract_program_range(image, start, end, marker, little_endian=False):
    """Reassemble logical uploaded words in record order; never infer aliases or fill gaps."""
    if not 0x10000 <= start < end <= 0x800000:
        raise ValueError('program range must be nonempty extended word addresses')
    words = {a: v for a, v in uploaded_word_image(image, marker).items() if start <= a < end}
    if len(words) != end - start:
        raise ValueError('program range contains missing words')
    return struct.pack(('<' if little_endian else '>') + f'{end - start}H',
                       *(words[address] for address in range(start, end)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--extract-section', type=lambda value: int(value, 0),
                        help='exact destination word address; export original big-endian bytes')
    parser.add_argument('--cinit-section', type=lambda value: int(value, 0),
                        help='inspect one exact section as a recovered C initialization table')
    parser.add_argument('--segment', help='container marker, e.g. aa55; omit for InitDisk')
    parser.add_argument('--extract-segment', action='store_true', help='export exact original wire segment')
    parser.add_argument('--extract-container', action='store_true', help='export decoded original segment container')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--extract-program-range', nargs=2, type=lambda value: int(value, 0),
                        metavar=('START', 'END'), help='reassemble extended word range, end exclusive, last record wins')
    parser.add_argument('--program-call-census', nargs=2, type=lambda value: int(value, 0),
                        metavar=('START', 'END'), help='inspect immediate FCALL candidates in one segment, end exclusive')
    parser.add_argument('--disassembler-little-endian', action='store_true',
                        help='encode a reconstructed range for little-endian disassemblers, not an original wire export')
    args = parser.parse_args()
    if args.program_call_census is not None:
        if not args.segment or args.output or args.extract_program_range or args.extract_section is not None or args.extract_segment or args.extract_container or args.cinit_section is not None or args.disassembler_little_endian:
            parser.error('--program-call-census requires --segment and excludes export operations')
        try:
            source = args.image.read_bytes()
            report = program_call_census(bytes.fromhex(source.decode('ascii')), *args.program_call_census, args.segment)
            report['source_sha256'] = hashlib.sha256(source).hexdigest()
            print(json.dumps(report, indent=2))
            return
        except (OSError, ValueError) as error:
            parser.exit(1, f'A00 call census: {error}\n')
    if args.extract_program_range is not None:
        if not args.segment or not args.output or args.extract_section is not None or args.extract_segment or args.extract_container or args.cinit_section is not None:
            parser.error('--extract-program-range requires --segment/--output and excludes other exports')
        try:
            image = bytes.fromhex(args.image.read_text(encoding='ascii'))
            data = extract_program_range(image, *args.extract_program_range, args.segment, args.disassembler_little_endian)
            with args.output.open('xb') as output:
                output.write(data)
            print(json.dumps({'range_word_addresses': args.extract_program_range, 'bytes': len(data),
                              'encoding': 'little-endian' if args.disassembler_little_endian else 'big-endian',
                              'sha256': hashlib.sha256(data).hexdigest(), 'scope': 'logical final record writes, no physical alias inference'}))
            return
        except (OSError, ValueError) as error:
            parser.exit(1, f'{error}\n')
    if args.disassembler_little_endian:
        parser.error('--disassembler-little-endian requires --extract-program-range')
    if args.extract_container and (args.segment is not None or args.extract_segment or args.extract_section is not None or args.cinit_section is not None):
        parser.error('--extract-container excludes segment and section operations')
    if args.extract_segment and (args.segment is None or args.extract_section is not None or args.cinit_section is not None):
        parser.error('--extract-segment requires --segment and excludes section operations')
    if args.cinit_section is not None and (args.extract_section is not None or args.output is not None):
        parser.error('--cinit-section cannot be combined with section export')
    if (args.extract_section is None and not args.extract_segment and not args.extract_container) != (args.output is None):
        parser.error('--extract-section and --output must be used together')
    if args.segment is not None and args.output is None and args.cinit_section is None:
        parser.error('--segment requires section extraction or C initialization inspection')
    try:
        source = args.image.read_bytes()
        image = bytes.fromhex(source.decode('ascii'))
        if args.output is not None:
            if args.extract_container:
                report = inventory(image)
                if not all(s['receiver_integrity']['checksum_matches'] for s in report['segments']):
                    raise ValueError('original container payload checksum mismatch')
            data = (image if args.extract_container else extract_segment(image, args.segment) if args.extract_segment else
                    extract_section(image, args.extract_section, args.segment))
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
