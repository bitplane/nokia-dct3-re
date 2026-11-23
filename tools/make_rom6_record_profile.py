"""Explicit synthetic low-record fixtures for products trialled on ROM6.

Never replaces an acquired PMM. The 8850, 6250 and 8890 send the same type-70
inputs as NPE-3 (13 seed, 14 FAID, 15 identity, 16 locks) from the same
low-record layout, so this reuses that fixture's records with each product's
own seed, the last four bytes of its MCU image. The example identities and
the modelled COBBA chip make no claim about any donor phone.
"""
from dataclasses import dataclass
import hashlib

from tools.make_npe3_record_profile import DROM_SHA1, EXAMPLE_IMEI14, OBSERVED_CHIP, low_records
from tools.nse5_pmm_journal import replay
from tools.noki6250_pmm_check import record_checksum


@dataclass(frozen=True)
class Product:
    mcu: str            # roms/ path and SHA-1
    mcu_sha1: str
    pmm: str
    pmm_sha1: str
    flash: int          # offset of the PMM in the flash image
    eeprom: int         # offset of the EEPROM journal in the PMM
    sector: int
    version: int
    length: int         # base record
    imei14: str = EXAMPLE_IMEI14


PRODUCTS = {
    # Own 3002a8 stores the security code (NV item 0701) as its packed digits
    # XOR ASCII IMEI digits 11..14 XOR 33f888. The acquired value d73339dc is
    # 12345 only for an IMEI ending 076 with check digit 0, so the example
    # ends that way.
    'nsm2': Product('noki8850/8850v531.fls', '9f966787403b68a09530680ad911302403eb1521',
                    'noki8850/8850 virgin eeprom 003d0000.fls', 'b09455302d98fbedf35072c9ecfd7721a04924b0',
                    0x1d0000, 0x10000, 0x10000, 3, 0x8000, '49015420620076'),
    'nhm3': Product('noki6250/6250-503mcuppmc.fls', '95607ce39c383bda75f1e6aeae67a214b787b0a1',
                    'noki6250/6250 virgin eeprom 005fa000.fls', '57c29c8387caf864603d94a22bfb63ace427b7f9',
                    0x3fa000, 0, 0x2000, 1, 0xa28),
    # The 8210 keeps the same encoding and key; d03438d4 is 12345 for 707/8.
    'nsm3': Product('noki8210/8210_5.31ppm_c.fls', 'c1a0fe95cedb89a92b19654208cc4855e1a4988e',
                    'noki8210/8210 virgin eeprom 003d0000.fls', '1c01ad3948ff9919890498a84f31052369d93e1d',
                    0x1d0000, 0x10000, 0x10000, 100, 0x8000, '49015420622707'),
    'nsb6': Product('noki8890/8890_12.20_ppmc.fls', 'a214a0d69760ecd8eeca0b9d82f95c94bdfe70ed',
                    'noki8890/8890 virgin eeprom 003d0000.fls', 'cc0924cfd4c0ce796fca157c640fc3183c2b5f2c',
                    0x1d0000, 0, 0x10000, 50, 0x8000),
}


def replace_low_records(product, pmm, records):
    if not isinstance(records, bytes) or len(records) != 0x110:
        raise ValueError('record fixture requires identity, FAID and ten lock slots')

    def journal(image):
        return replay(bytes(image[product.eeprom:product.eeprom + product.sector]), product.length,
                      sector_size=product.sector, version=product.version)
    cache, rows, _ = journal(pmm)
    base = {'record': 0x20, 'source': 0x26, 'destination': 0, 'length': product.length}
    if not rows or rows[0] != base or any(row['destination'] < 0x120 for row in rows[1:]):
        raise ValueError('unexpected low-record layout')
    if (sum(cache[:0x11c]) & 0xffff).to_bytes(4, 'big') != cache[0x11c:0x120]:
        raise ValueError('low-record sum differs from its storage field')
    result = bytearray(pmm)
    at = product.eeprom + 0x26
    payload = slice(at, at + product.length)
    # The acquired 8850 and 8890 base records do not satisfy the envelope
    # rule their other records do; keep those as acquired.
    enveloped = record_checksum(0, result[payload]) == result[product.eeprom + 0x21]
    result[at:at + 0x110] = records
    result[at + 0x11c:at + 0x120] = (sum(result[at:at + 0x11c]) & 0xffff).to_bytes(4, 'big')
    if enveloped:
        result[product.eeprom + 0x21] = record_checksum(0, result[payload])
    after, after_rows, _ = journal(result)
    if after[0x110:0x11c] != cache[0x110:0x11c] or after[0x120:] != cache[0x120:] or rows != after_rows:
        raise ValueError('record fixture changed unrelated journal semantics')
    return bytes(result)


def make_profile(product, mcu, pmm, drom):
    for name, data, digest in (('MCU', mcu, product.mcu_sha1), ('PMM', pmm, product.pmm_sha1),
                               ('DROM', drom, DROM_SHA1)):
        if hashlib.sha1(data).hexdigest() != digest:
            raise ValueError(f'record fixture requires the pinned {name} input')
    if product is PRODUCTS['nhm3']:
        from tools.noki6250_pmm_check import initial_record_fixture
        pmm = initial_record_fixture(pmm)
    elif product is PRODUCTS['nsm3']:
        from tools.noki8210_pmm_check import base_record_fixture
        pmm = base_record_fixture(pmm)
    records = low_records(mcu[-4:], OBSERVED_CHIP, product.imei14,
                          faid_table=drom[0xb74f * 2:0xb755 * 2],
                          identity_table=drom[0xb755 * 2:0xb75b * 2],
                          lock_table=drom[0xb749 * 2:0xb74f * 2])
    return replace_low_records(product, pmm, records)
