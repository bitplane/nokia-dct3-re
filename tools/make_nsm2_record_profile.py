"""Explicit synthetic NSM-2 record fixture; never replace the acquired PMM.

The 8850's own type-70 inputs (13 seed, 14 FAID, 15 identity, 16 locks) carry
the same low-record layout as NPE-3, so this reuses that fixture's records
with the 8850's own seed. The example identity and modelled COBBA chip make no
claim about the donor phone. The base record's header check byte does not
satisfy the rule the other 379 records do, even in the acquired image, so it
is left as acquired.
"""
import hashlib

from tools.make_npe3_record_profile import DROM_SHA1, OBSERVED_CHIP, low_records
from tools.nse5_pmm_journal import replay

MCU_SHA1 = '9f966787403b68a09530680ad911302403eb1521'
PMM_SHA1 = 'b09455302d98fbedf35072c9ecfd7721a04924b0'
# Own 3002a8 stores the security code (NV item 0701) as its packed digits XOR
# ASCII IMEI digits 11..14 XOR 33f888. The acquired value d73339dc is 12345
# only for an IMEI ending 076 with check digit 0, so the example ends that way.
EXAMPLE_IMEI14 = '49015420620076'
EEPROM = 0x10000      # journal offset in the acquired PMM image
BASE = {'record': 0x20, 'source': 0x26, 'destination': 0, 'length': 0x8000}


def journal(pmm):
    return replay(pmm[EEPROM:EEPROM + 0x10000], 0x8000, sector_size=0x10000, version=3)


def replace_low_records(pmm, records):
    if not isinstance(records, bytes) or len(records) != 0x110:
        raise ValueError('record fixture requires identity, FAID and ten lock slots')
    cache, rows, _ = journal(pmm)
    if not rows or rows[0] != BASE or any(row['destination'] < 0x120 for row in rows[1:]):
        raise ValueError('unexpected NSM-2 low-record layout')
    if (sum(cache[:0x11c]) & 0xffff).to_bytes(4, 'big') != cache[0x11c:0x120]:
        raise ValueError('NSM-2 low-record sum differs from its storage field')
    result = bytearray(pmm)
    at = EEPROM + BASE['source']
    result[at:at + 0x110] = records
    result[at + 0x11c:at + 0x120] = (sum(result[at:at + 0x11c]) & 0xffff).to_bytes(4, 'big')
    after, after_rows, _ = journal(bytes(result))
    if after[0x110:0x11c] != cache[0x110:0x11c] or after[0x120:] != cache[0x120:] or rows != after_rows:
        raise ValueError('record fixture changed unrelated journal semantics')
    return bytes(result)


def make_profile(mcu, pmm, drom):
    for name, data, digest in (('MCU', mcu, MCU_SHA1), ('PMM', pmm, PMM_SHA1), ('DROM', drom, DROM_SHA1)):
        if hashlib.sha1(data).hexdigest() != digest:
            raise ValueError(f'record fixture requires the pinned {name} input')
    records = low_records(mcu[-4:], OBSERVED_CHIP, EXAMPLE_IMEI14,
                          faid_table=drom[0xb74f * 2:0xb755 * 2],
                          identity_table=drom[0xb755 * 2:0xb75b * 2],
                          lock_table=drom[0xb749 * 2:0xb74f * 2])
    return replace_low_records(pmm, records)
