"""Read-only NAND delta inventory; signatures are not codec or file-validity proof."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


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
    parser.add_argument('--export-fixture-volume', type=Path,
                        help='write a new data-only volume image from the original InitDisk fixture endpoint')
    args = parser.parse_args()
    try:
        source, endpoint = args.source.read_bytes(), args.endpoint.read_bytes()
        report = inventory(source, endpoint)
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
