"""Qualify the acquired NSB-1 v6.71 service package without donor storage."""

import argparse
import configparser
import hashlib
import io
import json
import struct
from pathlib import Path
import sys
from zipfile import BadZipFile, ZipFile

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.extract_dct3_wintesla import decode_records
from tools.dct3_eeprom_descriptor import eeprom_geometry

SHA256 = '7c7167aba12342fa1ef3810c93bf518b0f8aeb79f67e9637e19d76b34e781128'
VERSION = b'V  6.71\n25-01-01\nNSB-1\n(c) NMP.'
MEMBERS = {'nsb1_3.pp', 'nsb1___6.71a', 'nsb1___6.71b',
           'nsb1___6.710', 'nsb1_671.doc', 'nsb-1.ini'}


def security_record_directory(image):
    """Own v6.71 direct-index records, not inferred provisioning contents."""
    directory = 0xcc3a0 + 7 * 8
    if len(image) < directory + 8:
        raise ValueError('truncated security directory')
    pointer, limit, _ = struct.unpack_from('>IHH', image, directory)
    base = pointer - 0x200000
    if limit < 2 or base < 0 or base + (limit + 1) * 8 > len(image):
        raise ValueError('invalid security directory bounds')
    records = []
    for index in (1, 2):
        identifier, offset, length = struct.unpack_from('>IHH', image, base + index * 8)
        if identifier != 0x700 + index or not length or offset + length > 8192:
            raise ValueError('invalid security record identity or storage bounds')
        records.append({'id': hex(identifier), 'offset': hex(offset), 'length': length})
    return {'lookup': '0x2a75e4', 'records': records, 'field_semantics_proved': False}


def post_coverage(calls):
    resolved = [call for call in calls if all(call['arguments'][name] is not None
                                            for name in ('task', 'event'))]
    candidates = [call for call in calls if call['arguments']['event'] == 0x21]
    return {'direct_calls': len(calls), 'both_arguments_recovered': len(resolved),
            'incomplete_arguments': len(calls) - len(resolved),
            'unknown_destination_calls': [call for call in calls if call['arguments']['task'] is None],
            'event_21_candidates': candidates,
            'absence_proved': False,
            'scope': 'direct BL to 0x268a94 only; no indirect or data-driven closure'}


def startup_post_census(image):
    from tools.message_census import decode_image, extract_calls
    # The shared census reads swap16 bytes, whereas this normalizer emits CPU order.
    swapped = bytearray(image)
    swapped[0::2], swapped[1::2] = image[1::2], image[0::2]
    addresses = (0x268a94, 0x269200, 0x268944)
    profile = {'apis': [{'address': hex(address), 'name': f'nsb1_post_{address:x}',
                         'kind': 'task_message', 'arguments': {'task': 'r0', 'event': 'r1'}}
                        for address in addresses]}
    instructions = decode_image(swapped, 0x200000)
    calls = extract_calls(profile, instructions, swapped, 0x200000)
    result = post_coverage([call for call in calls if call['api_address'] == addresses[0]])
    result['other_transports'] = [
        {'address': hex(address), **post_coverage([call for call in calls if call['api_address'] == address])}
        for address in addresses[1:]]
    for item in result['other_transports']:
        item['scope'] = 'direct BL only; r1 may be a scalar or object pointer; no indirect closure'
    return result


def flash_integrity(image):
    """Own 294e70 header grammar: at most two inclusive BE-halfword ranges."""
    if len(image) < 0x30:
        raise ValueError('truncated flash-integrity header')
    ranges, total = [], 0
    for index in range(2):
        offset = 0x24 + 6 * index
        start = int.from_bytes(image[offset:offset + 3], 'big')
        end = int.from_bytes(image[offset + 3:offset + 6], 'big')
        if start == 0xffffff:
            break
        if (start < 0x200000 or end >= 0x200000 + len(image) or start > end
                or start & 1 or not end & 1):
            raise ValueError('invalid own flash-integrity span')
        payload = image[start - 0x200000:end - 0x200000 + 1]
        total = (total + sum(word[0] for word in struct.iter_unpack('>H', payload))) & 0xffff
        ranges.append([hex(start), hex(end)])
    stored = int.from_bytes(image[0x22:0x24], 'big')
    if not ranges or total != stored:
        raise ValueError('own declared flash-integrity sum mismatch')
    return {'ranges': ranges, 'computed': hex(total), 'stored': hex(stored),
            'whole_flash_authentication': False}


def inspect(source, ppm='A'):
    if ppm not in ('A', 'B'):
        raise ValueError('unsupported NSB-1 PPM selection')
    if hashlib.sha256(source).hexdigest() != SHA256:
        raise ValueError('requires the acquired 5190 NSB-1 v6.71 package')
    with ZipFile(io.BytesIO(source)) as archive:
        if len(archive.infolist()) != 6 or set(archive.namelist()) != MEMBERS:
            raise ValueError('unexpected archive member inventory')
        if any(member.file_size > 2 * 1024 * 1024 for member in archive.infolist()):
            raise ValueError('archive member exceeds expected bound')
        config = configparser.ConfigParser(interpolation=None)
        config.read_string(archive.read('nsb-1.ini').decode('ascii'))
        restore = config['NSB-1_RESTOREFILES']
        if (restore['LowEraseLimit'], restore['HighEraseLimit'], restore['ROM4ImageFile']) != (
                '200000', '2FFFFF', 'nsb1___6.710'):
            raise ValueError('unexpected NSB-1 restore map')
        if (config['USA']['ROM4PpmFile'], config['USA_T9']['ROM4PpmFile']) != (
                'nsb1___6.71a', 'nsb1___6.71b'):
            raise ValueError('unexpected NSB-1 market/PPM mapping')
        image = bytearray(b'\xff' * 0x100000)
        streams = []
        for name, start, length, kind in (
                ('nsb1___6.710', 0x200000, 0xd2d00, 'MCU'),
                ('nsb1___6.71a', 0x2e0000, 0x20000, 'A'),
                ('nsb1___6.71b', 0x2e0000, 0x20000, 'B')):
            raw = archive.read(name)
            address, data = decode_records(raw)
            if address != start or len(data) != length or data.count(VERSION) != 1:
                raise ValueError(f'unexpected {kind} source span or product/version')
            if kind != 'MCU' and (not data.startswith(b'PPM\0' + VERSION) or data[40:41] != kind.encode()):
                raise ValueError(f'unexpected {kind} PPM identity')
            if kind in ('MCU', ppm):
                image[start - 0x200000:start - 0x200000 + length] = data
            streams.append({'name': kind, 'address': hex(start), 'bytes': length,
                            'raw_sha1': hashlib.sha1(raw).hexdigest(),
                            'decoded_sha1': hashlib.sha1(data).hexdigest()})
    return bytes(image), {'archive_sha256': SHA256, 'streams': streams,
                         'ppm': ppm, 'normalized_bytes': len(image),
                         'firmware_eeprom_geometry': eeprom_geometry(image[5]),
                         'declared_flash_integrity': flash_integrity(image),
                         'security_record_directory': security_record_directory(image),
                         'normalized_sha1': hashlib.sha1(image).hexdigest(),
                         'absent_erased_gap': ['0x2d2d00', '0x2dffff'],
                         'provisioning_supplied': False, 'boot_claim': False,
                         'resident_rom4_supplied': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('--ppm', choices=('A', 'B'), default='A')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--post-census', action='store_true',
                        help='include bounded direct startup-post argument census')
    args = parser.parse_args()
    try:
        if args.output and args.output.resolve() == args.archive.resolve():
            raise ValueError('output must not overwrite the source archive')
        image, report = inspect(args.archive.read_bytes(), args.ppm)
        if args.post_census:
            report['startup_post_census'] = startup_post_census(image)
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(image)
    except (OSError, ValueError, BadZipFile, KeyError, configparser.Error) as error:
        parser.exit(1, f'NSB-1 package: FAIL: {error}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
