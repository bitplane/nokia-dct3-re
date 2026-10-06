"""Read-only replay of the acquired NSM-3 low-record journal."""

import argparse
from pathlib import Path


def checksum(cache):
    return (sum(cache[0x120:0x254]) - sum(cache[0x154:0x156])) & 0xffff


def base_record_fixture(image):
    """Restore only the acquired low-record snapshot; never invent fields."""
    _, records, _ = replay(image)
    if not records or records[0] != (0x10020, 0, 0x8000, False):
        raise ValueError('missing complete acquired base record')
    base = image[0x10026:0x18026]
    if checksum(base) != int.from_bytes(base[0x254:0x256], 'big'):
        raise ValueError('acquired base record is not checksum-valid')
    fixture = bytearray(image)
    fixture[0x18026:0x20000] = b'\xff' * (0x20000 - 0x18026)
    return bytes(fixture)


def replay(image):
    if len(image) != 0x30000:
        raise ValueError('expected a 0x30000-byte product-local PMM tail')
    # Own 2c9d40 reads the sector header and interprets records at +0x20.
    if image[0x10018:0x1001a] != b'\x00\x64':
        raise ValueError('unexpected low-record sector type')
    cache = bytearray(0x8000)
    cursor = 0x10020
    records = []
    while cursor < 0x20000:
        start = cursor
        header = int.from_bytes(image[cursor:cursor + 2], 'big')
        cursor += 2
        if header == 0xffff or header & 0x200:
            break
        length = header >> 10
        if not length:
            length = int.from_bytes(image[cursor:cursor + 2], 'big')
            cursor += 2
        if not length:
            raise ValueError(f'zero-length record at {start:x}')
        deleted = (header & 0x300) == 0x100
        offset = None
        if not deleted:
            offset = int.from_bytes(image[cursor:cursor + 2], 'big')
            cursor += 2
            if offset + length > len(cache):
                raise ValueError(f'out-of-range record at {start:x}')
        if cursor + length > 0x20000:
            raise ValueError(f'truncated record at {start:x}')
        if not deleted:
            cache[offset:offset + length] = image[cursor:cursor + length]
        records.append((start, offset, length, deleted))
        cursor += (length + 1) & ~1
    return bytes(cache), records, cursor


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pmm', type=Path)
    parser.add_argument('--cache', type=Path)
    parser.add_argument('--base-record-flash', type=Path,
                        help='write diagnostic flash using unchanged acquired base record')
    parser.add_argument('--mcu', type=Path,
                        help='own normalized MCU/PPM image for diagnostic flash')
    args = parser.parse_args()
    try:
        image = args.pmm.read_bytes()
        cache, records, end = replay(image)
        base = image[0x10026:0x18026]
        for name, data in [('base', base), ('journal', cache)]:
            stored = int.from_bytes(data[0x254:0x256], 'big')
            print(f'{name}: computed={checksum(data):04x} stored={stored:04x}')
        print(f'records={len(records)} end={end:05x}')
        if args.base_record_flash:
            if not args.mcu or args.base_record_flash.resolve() in (
                    args.pmm.resolve(), args.mcu.resolve()):
                raise ValueError('fixture requires own MCU and a separate output path')
            mcu = args.mcu.read_bytes()
            if len(mcu) != 0x1d0000:
                raise ValueError('unexpected normalized MCU/PPM extent')
            args.base_record_flash.write_bytes(mcu + base_record_fixture(image))
            print('wrote diagnostic base-record flash; archive remains unchanged')
        if args.cache:
            observed = args.cache.read_bytes()
            if observed != cache:
                raise ValueError('runtime cache differs from journal replay')
            print('runtime cache matches complete journal replay')
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 PMM check FAIL: {error}\n')


if __name__ == '__main__':
    main()
