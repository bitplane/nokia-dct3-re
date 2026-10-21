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
    # Cold rotation retires the old header and publishes the other sector.
    # Dual-live headers need a separately recovered generation-order contract.
    candidates = [base for base in (BASE, BASE+SECTOR)
                  if flash[base+6:base+12] == b'EEPROM' and
                  flash[base+0x18:base+0x1a] == b'\x00\xc8']
    if len(candidates) != 1:
        raise ValueError('requires exactly one live own EEPROM journal sector')
    selected = candidates[0]
    sector = flash[selected:selected+SECTOR]
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
    prefix_sum = sum(cache[:0x11c]) & 0xffff
    prefix_stored = int.from_bytes(cache[0x11c:0x120], 'big')
    prefix_matched = False
    if trace is not None:
        expected = (f'{checksum(cache):04x}', cache[0x1e0:0x1e2].hex(), cache[0x170:0x172].hex())
        observed = re.findall(r'nsb7_calibration_compare: computed=([0-9a-f]{4}) stored=([0-9a-f]{4}) marker=([0-9a-f]{4})', trace)
        if observed != [expected]:
            raise ValueError('runtime checksum inputs differ from replay')
        pc, value = ('00248942', '00') if valid else ('00248948', '0c')
        if len(re.findall(f'nsb7_fault12: pc={pc} value={value} mask=ff000000', trace)) != 1:
            raise ValueError('runtime checksum branch differs')
        prefix_rows = re.findall(r'nsb7_prefix_compare: computed=([0-9a-f]{8}) stored=([0-9a-f]{8})', trace)
        if prefix_rows and prefix_rows != [(f'{prefix_sum:08x}', f'{prefix_stored:08x}')]:
            raise ValueError('runtime prefix checksum differs from replay')
        if prefix_rows:
            value = '00' if prefix_sum == prefix_stored else 'fd'
            if len(re.findall(f'nsb7_result: index=12 pc=002e8ffc value={value} mask=0000ff00', trace)) != 1:
                raise ValueError('runtime prefix checksum result differs')
            prefix_matched = True
    return cache, stop, {
        'source_sha256': hashlib.sha256(flash).hexdigest(), 'journal_records': len(records),
        'sector_base': hex(selected),
        'stop': hex(stop), 'shadow_bytes': len(cache), 'runtime_shadow_matched': shadow is not None,
        'computed': f'{checksum(cache):04x}', 'stored': cache[0x1e0:0x1e2].hex(),
        'excluded_word': cache[0x154:0x156].hex(), 'marker': cache[0x170:0x172].hex(),
        'prefix_computed': f'{prefix_sum:08x}', 'prefix_stored': f'{prefix_stored:08x}',
        'prefix_valid': prefix_sum == prefix_stored,
        'runtime_prefix_branch_matched': prefix_matched,
        'checksum_valid': valid, 'runtime_branch_matched': trace is not None}


def checksum_fixture(flash, shadow, prefix=False):
    cache, stop, report = inspect(flash, shadow)
    base = int(report['sector_base'], 16)
    destination, width = (0x11c, 4) if prefix else (0x1e0, 2)
    if shadow is None or cache[destination:destination+width] != b'\xff'*width:
        raise ValueError('requires observed unprovisioned checksum field')
    value = (sum(cache[:0x11c]) & 0xffff) if prefix else checksum(cache)
    payload = value.to_bytes(width, 'big')
    record = bytes([width << 2, record_checksum(destination, payload)]) + destination.to_bytes(2, 'big') + payload
    if stop+len(record)+2 > SECTOR or flash[base+stop:base+stop+len(record)+2] != b'\xff'*(len(record)+2):
        raise ValueError('no erased journal append space')
    result = bytearray(flash)
    result[base+stop:base+stop+len(record)] = record
    after, _, _ = inspect(result)
    expected = bytearray(cache)
    expected[destination:destination+width] = payload
    if after != expected:
        raise ValueError('fixture changed logical bytes beyond the checksum')
    report['fixture'] = {'kind': 'research checksum-only, not factory calibration',
                         'logical_offset': hex(destination), 'payload': payload.hex(),
                         'physical_append': hex(base+stop), 'record': record.hex()}
    return bytes(result), report


def security_retry_fixture(flash, shadow):
    cache, stop, report = inspect(flash, shadow)
    base = int(report['sector_base'], 16)
    destination = 0x9d8
    if shadow is None or cache[destination] != 0xff:
        raise ValueError('requires observed erased retry byte')
    payload = b'\x00'
    record = bytes([4, record_checksum(destination, payload)]) + destination.to_bytes(2, 'big') + payload
    if stop+len(record)+2 > SECTOR or flash[base+stop:base+stop+len(record)+2] != b'\xff'*(len(record)+2):
        raise ValueError('no erased journal append space')
    result = bytearray(flash)
    result[base+stop:base+stop+len(record)] = record
    after, _, _ = inspect(result)
    expected = bytearray(cache)
    expected[destination] = 0
    if after != expected:
        raise ValueError('fixture changed bytes beyond the erased retry counter')
    report['fixture'] = {'kind': 'research erased retry byte, not phone-code/identity provisioning',
                         'logical_offset': hex(destination), 'payload': payload.hex(),
                         'physical_append': hex(base+stop), 'record': record.hex()}
    return bytes(result), report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('flash', type=Path)
    parser.add_argument('--shadow', type=Path)
    parser.add_argument('--trace', type=Path)
    fixtures = parser.add_mutually_exclusive_group()
    fixtures.add_argument('--checksum-fixture', type=Path)
    fixtures.add_argument('--prefix-checksum-fixture', type=Path)
    fixtures.add_argument('--security-retry-fixture', type=Path)
    args = parser.parse_args()
    try:
        flash = args.flash.read_bytes()
        shadow = args.shadow.read_bytes() if args.shadow else None
        output_path = args.security_retry_fixture or args.prefix_checksum_fixture or args.checksum_fixture
        if output_path:
            if output_path.exists():
                raise ValueError('fixture output must be a fresh path')
            if args.trace:
                inspect(flash, shadow, args.trace.read_text())
            if output_path.resolve() == args.flash.resolve():
                raise ValueError('fixture must not overwrite its source')
            if args.security_retry_fixture:
                output, report = security_retry_fixture(flash, shadow)
            else:
                output, report = checksum_fixture(flash, shadow, prefix=bool(args.prefix_checksum_fixture))
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_bytes(output)
        else:
            _, _, report = inspect(flash, shadow, args.trace.read_text() if args.trace else None)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 storage: FAIL: {error}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
