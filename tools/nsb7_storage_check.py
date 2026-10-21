"""Replay own NSB-7 EEPROM storage and optionally correct only its checksum."""
import argparse
import hashlib
import json
from pathlib import Path
import re

try:
    from tools.extract_nsm3_verifier import NSB7_FLASH_SHA1
    from tools.nse5_pmm_journal import replay
except ModuleNotFoundError:
    from extract_nsm3_verifier import NSB7_FLASH_SHA1
    from nse5_pmm_journal import replay

BASE = 0x1d0000
SECTOR = 0x10000
CACHE = 0x8000


def checksum(cache):
    if len(cache) != CACHE:
        raise ValueError('logical EEPROM must be 32768 bytes')
    return (sum(cache[0x120:0x1e0]) - sum(cache[0x154:0x156])) & 0xffff


def record_checksum(destination, payload):
    return ((destination >> 8) + (destination & 255) + sum(payload)) & 255


def inspect(flash, shadow=None, trace=None):
    if len(flash) != 0x200000 or hashlib.sha1(flash[:BASE]).hexdigest() != NSB7_FLASH_SHA1:
        raise ValueError('requires own NSB-7 executable prefix and 2 MiB flash')
    sector = flash[BASE:BASE+SECTOR]
    cache, records, stop = replay(sector, CACHE, sector_size=SECTOR, version=0xc8)
    if not records or records[0]['destination'] != 0 or records[0]['length'] != CACHE:
        raise ValueError('complete own initial shadow record required')
    for row in records:
        payload = sector[row['source']:row['source']+row['length']]
        if sector[row['record']+1] != record_checksum(row['destination'], payload):
            raise ValueError('journal record checksum differs')
    if shadow is not None and shadow != cache:
        raise ValueError('runtime shadow differs from independent journal replay')
    valid = checksum(cache) == int.from_bytes(cache[0x1e0:0x1e2], 'big') and (
        checksum(cache) | int.from_bytes(cache[0x170:0x172], 'big')) != 0
    if trace is not None:
        expected = (f'{checksum(cache):04x}', cache[0x1e0:0x1e2].hex(), cache[0x170:0x172].hex())
        observed = re.findall(r'nsb7_calibration_compare: computed=([0-9a-f]{4}) stored=([0-9a-f]{4}) marker=([0-9a-f]{4})', trace)
        if observed != [expected]:
            raise ValueError('runtime checksum inputs differ from replay')
        pc, value = ('00248942', '00') if valid else ('00248948', '0c')
        if len(re.findall(f'nsb7_fault12: pc={pc} value={value} mask=ff000000', trace)) != 1:
            raise ValueError('runtime checksum branch differs')
    return cache, stop, {
        'source_sha256': hashlib.sha256(flash).hexdigest(), 'journal_records': len(records),
        'stop': hex(stop), 'shadow_bytes': len(cache), 'runtime_shadow_matched': shadow is not None,
        'computed': f'{checksum(cache):04x}', 'stored': cache[0x1e0:0x1e2].hex(),
        'excluded_word': cache[0x154:0x156].hex(), 'marker': cache[0x170:0x172].hex(),
        'checksum_valid': valid, 'runtime_branch_matched': trace is not None}


def checksum_fixture(flash, shadow):
    cache, stop, report = inspect(flash, shadow)
    if shadow is None or cache[0x1e0:0x1e2] != b'\xff\xff':
        raise ValueError('requires observed unprovisioned checksum field')
    payload = checksum(cache).to_bytes(2, 'big')
    record = bytes([8, record_checksum(0x1e0, payload)]) + b'\x01\xe0' + payload
    if stop+len(record)+2 > SECTOR or flash[BASE+stop:BASE+stop+len(record)+2] != b'\xff'*(len(record)+2):
        raise ValueError('no erased journal append space')
    result = bytearray(flash)
    result[BASE+stop:BASE+stop+len(record)] = record
    after, _, _ = inspect(result)
    expected = bytearray(cache)
    expected[0x1e0:0x1e2] = payload
    if after != expected:
        raise ValueError('fixture changed logical bytes beyond the checksum')
    report['fixture'] = {'kind': 'research checksum-only, not factory calibration',
                         'logical_offset': '0x1e0', 'payload': payload.hex(),
                         'physical_append': hex(BASE+stop), 'record': record.hex()}
    return bytes(result), report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('flash', type=Path)
    parser.add_argument('--shadow', type=Path)
    parser.add_argument('--trace', type=Path)
    parser.add_argument('--checksum-fixture', type=Path)
    args = parser.parse_args()
    try:
        flash = args.flash.read_bytes()
        shadow = args.shadow.read_bytes() if args.shadow else None
        if args.checksum_fixture:
            if args.checksum_fixture.exists():
                raise ValueError('fixture output must be a fresh path')
            if args.trace:
                inspect(flash, shadow, args.trace.read_text())
            if args.checksum_fixture.resolve() == args.flash.resolve():
                raise ValueError('fixture must not overwrite its source')
            output, report = checksum_fixture(flash, shadow)
            args.checksum_fixture.parent.mkdir(parents=True, exist_ok=True)
            args.checksum_fixture.write_bytes(output)
        else:
            _, _, report = inspect(flash, shadow, args.trace.read_text() if args.trace else None)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 storage: FAIL: {error}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
