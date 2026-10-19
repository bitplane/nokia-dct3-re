"""Read-only NAND/file deltas; signatures are not codec or file-validity proof."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import sys


def adts_header(source, offset):
    """Decode a candidate fixed header, not AAC payload or CRC validity."""
    if offset < 0 or offset + 7 > len(source):
        return None
    h = source[offset:offset + 7]
    if h[0] != 0xff or h[1] & 0xf6 != 0xf0:
        return None
    header_bytes = 7 if h[1] & 1 else 9
    frame_bytes = ((h[3] & 3) << 11) | (h[4] << 3) | (h[5] >> 5)
    if frame_bytes < header_bytes or offset + header_bytes > len(source):
        return None
    return {'offset': offset, 'header_hex': h.hex(), 'header_bytes': header_bytes,
            'mpeg_version': 2 if h[1] & 8 else 4, 'profile_bits': h[2] >> 6,
            'sampling_index': (h[2] >> 2) & 15,
            'channel_configuration': ((h[2] & 1) << 2) | (h[3] >> 6),
            'frame_bytes': frame_bytes, 'buffer_fullness': ((h[5] & 31) << 6) | (h[6] >> 2),
            'raw_data_blocks_minus_one': h[6] & 3,
            'frame_within_file': offset + frame_bytes <= len(source)}


def adts_inventory(source):
    headers = {}
    for offset in range(max(0, len(source) - 6)):
        if source[offset] == 0xff:
            header = adts_header(source, offset)
            if header is not None:
                headers[offset] = header
    chain, stop, reason = [], None, 'no_candidate'
    if headers:
        stop = min(headers)
        while stop in headers:
            header = headers[stop]
            chain.append(stop)
            if not header['frame_within_file']:
                reason = 'truncated_frame'
                break
            stop += header['frame_bytes']
        else:
            reason = 'exact_eof' if stop == len(source) else 'missing_header_at_declared_end'
    return {'file_bytes': len(source), 'sha256': hashlib.sha256(source).hexdigest(),
            'candidate_count': len(headers), 'candidates': list(headers.values()),
            'first_candidate_chain': chain, 'chain_stop': stop, 'chain_stop_reason': reason,
            'scope': 'candidate ADTS headers and declared-length chain from first candidate only; '
                     'not correct start offset, fixed frame size, CRC/payload validity, REL identity or decoded audio'}


def file_inventory(source, endpoint):
    """Compare externally extracted files, without interpreting their format."""
    common = min(len(source), len(endpoint))
    ranges = []
    changed = 0
    for offset, (old, new) in enumerate(zip(source, endpoint)):
        if old == new:
            continue
        changed += 1
        if ranges and ranges[-1][1] == offset:
            ranges[-1][1] = offset + 1
        else:
            ranges.append([offset, offset + 1])
    added, removed = endpoint[common:], source[common:]
    return {'source_bytes': len(source), 'endpoint_bytes': len(endpoint),
            'source_sha256': hashlib.sha256(source).hexdigest(),
            'endpoint_sha256': hashlib.sha256(endpoint).hexdigest(),
            'common_extent_bytes': common, 'changed_common_bytes': changed,
            'changed_common_ranges_end_exclusive': ranges,
            'added_bytes': len(added), 'removed_bytes': len(removed),
            'added_byte_counts': {f'{value:02x}': count for value, count in sorted(Counter(added).items())},
            'removed_byte_counts': {f'{value:02x}': count for value, count in sorted(Counter(removed).items())},
            'scope': 'byte deltas of independently extracted files only; no filesystem or codec validation'}


def id3v23_inventory(source):
    """Strict flag-free ID3v2.3 inventory (https://id3.org/id3v2.3.0)."""
    if len(source) < 10 or source[:6] != b'ID3\x03\x00\x00' or any(byte & 0x80 for byte in source[6:10]):
        raise ValueError('requires a complete flag-free ID3v2.3.0 tag')
    size = 0
    for byte in source[6:10]:
        size = (size << 7) | byte
    end,cursor = 10+size,10
    if end > len(source):
        raise ValueError('truncated ID3 tag extent')
    frames,duration = [],None
    while cursor < end:
        if source[cursor] == 0:
            if any(source[cursor:end]):
                raise ValueError('nonzero ID3 padding')
            break
        if cursor+10 > end:
            raise ValueError('truncated ID3 frame header')
        name = source[cursor:cursor+4]
        length = int.from_bytes(source[cursor+4:cursor+8],'big')
        if (not all(65 <= byte <= 90 or 48 <= byte <= 57 for byte in name)
                or source[cursor+8:cursor+10] != b'\0\0' or not length
                or cursor+10+length > end):
            raise ValueError('unsupported or truncated ID3 frame')
        payload = source[cursor+10:cursor+10+length]
        frame = dict(id=name.decode('ascii'),payload_bytes=length,payload_prefix_hex=payload[:32].hex())
        if name == b'TLEN':
            digits = payload[1:].rstrip(b'\0')
            if duration is not None or payload[0] != 0 or not digits or any(byte not in b'0123456789' for byte in digits):
                raise ValueError('unsupported, duplicate or malformed ID3 length frame')
            duration = int(digits)
        frames.append(frame)
        cursor += 10+length
    return dict(tag_payload_bytes=size,tag_end=end,frames=frames,padding_bytes=end-cursor,
                declared_length_milliseconds=duration,post_tag_prefix_hex=source[end:end+16].hex(),
                scope='Flag-free ID3v2.3 tag fields only; no inferred sample count, custom payload interpretation or audio completeness')


def fixture_volume(image):
    """Export the original InitDisk fixture's single partition, not arbitrary NAND FTL."""
    page_bytes, data_bytes = 528, 512
    if len(image) < page_bytes * 2 or len(image) % page_bytes:
        raise ValueError('volume export requires complete 528-byte NAND pages')
    mbr = image[page_bytes:page_bytes + data_bytes]
    if mbr[510:] != b'\x55\xaa':
        raise ValueError('missing fixture MBR signature at physical page 1')
    entries = [mbr[446 + i * 16:462 + i * 16] for i in range(4)]
    populated = [entry for entry in entries if entry[4]]
    if len(populated) != 1 or populated[0] != entries[0] or entries[0][4] != 6:
        raise ValueError('expected the fixture single first partition with type 06')
    lba, partition_sectors = struct.unpack_from('<II', entries[0], 8)
    # Original lower reader 3035 adds one to the logical row address.
    first_page = lba + 1
    if not lba or not partition_sectors or first_page >= len(image) // page_bytes:
        raise ValueError('fixture partition begins outside the NAND capture')
    boot = image[first_page * page_bytes:first_page * page_bytes + data_bytes]
    if boot[510:] != b'\x55\xaa' or struct.unpack_from('<H', boot, 11)[0] != data_bytes:
        raise ValueError('fixture volume needs a signed 512-byte boot sector')
    sectors = struct.unpack_from('<H', boot, 19)[0] or struct.unpack_from('<I', boot, 32)[0]
    if not 0 < sectors <= partition_sectors or first_page + sectors > len(image) // page_bytes:
        raise ValueError('fixture volume extent exceeds its partition or NAND capture')
    volume = b''.join(image[page * page_bytes:page * page_bytes + data_bytes]
                      for page in range(first_page, first_page + sectors))
    return {'mbr_physical_page': 1, 'partition_lba': lba,
            'partition_sectors': partition_sectors, 'first_physical_page': first_page,
            'bpb_hidden_sectors': struct.unpack_from('<I', boot, 28)[0],
            'volume_sectors': sectors, 'volume_bytes': len(volume),
            'volume_sha256': hashlib.sha256(volume).hexdigest(),
            'scope': 'original InitDisk fixture row+1 mapping only; filesystem validation delegated to independent readers'}, volume


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
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--export-fixture-volume', type=Path,
                        help='write a new data-only volume image from the original InitDisk fixture endpoint')
    mode.add_argument('--extracted-files', action='store_true',
                      help='compare independent file readbacks instead of raw NAND captures')
    parser.add_argument('--adts-headers', action='store_true',
                        help='inventory candidate ADTS headers in both independently extracted files')
    parser.add_argument('--id3-tags', action='store_true',help='strictly inventory ID3v2.3 tags in extracted files')
    args = parser.parse_args()
    if args.adts_headers and not args.extracted_files:
        parser.error('--adts-headers requires --extracted-files')
    if args.id3_tags and not args.extracted_files:
        parser.error('--id3-tags requires --extracted-files')
    try:
        source, endpoint = args.source.read_bytes(), args.endpoint.read_bytes()
        report = file_inventory(source, endpoint) if args.extracted_files else inventory(source, endpoint)
        if args.adts_headers:
            report['adts_header_candidates'] = {'source': adts_inventory(source), 'endpoint': adts_inventory(endpoint)}
        if args.id3_tags:
            report['id3_tags'] = {'source':id3v23_inventory(source),'endpoint':id3v23_inventory(endpoint)}
        if args.export_fixture_volume:
            metadata, volume = fixture_volume(endpoint)
            with args.export_fixture_volume.open('xb') as output:
                output.write(volume)
            report['fixture_volume'] = metadata
    except (OSError, ValueError) as error:
        print(f'mu4 recorder media inventory: {error}', file=sys.stderr)
        return 1
    print(json.dumps(report, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
