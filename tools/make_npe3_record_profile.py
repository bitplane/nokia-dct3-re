"""Explicit synthetic NPE-3 record fixture; never replace the acquired PMM.

The record locations and unpadded opcode16 inputs follow the own MCU capture.
The example IMEI/FAID/unlocked policy follows the existing NSE-1 factory
fixture convention, checked through the native NPE-3 validators at runtime.
This does not identify the donor phone's COBBA or claim factory provenance.
"""
import hashlib
import re

from tools.dct3_rom6_record_codec import encode_record
from tools.nse5_pmm_journal import replay
from tools.noki6250_pmm_check import record_checksum

MCU_SHA1='3d9ea319503e78ec69b60d72cda23e461e118ea9'
PMM_SHA1='b3a527ede1be87bd715fb3741a81eef5bd422efa'
DROM_SHA1='845133638256ed1f07e014437fdee3ececc4508f'
EXAMPLE_IMEI14='49015420621001'
OBSERVED_CHIP=bytes.fromhex('00160010')


def low_records(seed,chip,imei14,*,faid_table,identity_table,lock_table):
    if not isinstance(seed,bytes) or len(seed)!=4:
        raise ValueError('record fixture requires the explicit four-byte flash seed')
    if not isinstance(chip,bytes) or len(chip)!=4 or chip[0]!=0:
        raise ValueError('record fixture requires an explicit 24-bit COBBA identity')
    if not isinstance(imei14,str) or len(imei14)!=14 or any(digit not in '0123456789' for digit in imei14):
        raise ValueError('record fixture requires fourteen ASCII example IMEI digits')
    bcd=bytes(int(imei14[at:at+2],16) for at in range(0,14,2))
    identity_plain=bytes.fromhex('7929')+bcd+bytes.fromhex('ff0000')
    identity=encode_record(identity_plain,chip,identity_table)
    crc=(int.from_bytes(seed,'big')^0x17ca6089).to_bytes(4,'big')
    faid_plain=crc+bytes(value^255 for value in crc)+bytes(4)
    faid=encode_record(faid_plain,chip,faid_table)
    locks=b''.join(encode_record(bytes.fromhex(plain),chip,lock_table) for plain in
                   ('ffffffffff0f0000007854c2','0000000000000000087c54c2'))
    return identity+bcd+bytes([255])+faid+locks


def replace_low_records(pmm,records):
    if not isinstance(records,bytes) or len(records)!=0x38:
        raise ValueError('record fixture requires the complete56-byte low record family')
    cache,rows,_=replay(pmm,0x9c4)
    if not rows or rows[0]!={'record':0x20,'source':0x26,'destination':0,'length':0x9c4}:
        raise ValueError('unexpected NPE-3 base record layout')
    if any(row['destination']<0x38 for row in rows[1:]):
        raise ValueError('later low-record writes need a separate fixture contract')
    result=bytearray(pmm)
    result[0x26:0x5e]=records
    # Own387fb4 checks sum16(cache[0:11c]) against a BE32 field at11c.
    # Changing the encoded records must update this outer storage checksum,
    # independently of the DSP's decrypted-record validation.
    result[0x26+0x11c:0x26+0x120]=(sum(result[0x26:0x26+0x11c])&0xffff).to_bytes(4,'big')
    result[0x21]=record_checksum(0,result[0x26:0x26+0x9c4])
    after,after_rows,_=replay(result,0x9c4)
    if (after[:0x38]!=records or after[0x38:0x11c]!=cache[0x38:0x11c] or
            after[0x120:]!=cache[0x120:] or rows!=after_rows):
        raise ValueError('record fixture changed unrelated journal semantics')
    return bytes(result)


def make_profile(mcu,pmm,drom):
    for name,data,digest in (('MCU',mcu,MCU_SHA1),('PMM',pmm,PMM_SHA1),('DROM',drom,DROM_SHA1)):
        if hashlib.sha1(data).hexdigest()!=digest:
            raise ValueError(f'record fixture requires the pinned own {name} input')
    records=low_records(mcu[-4:],OBSERVED_CHIP,EXAMPLE_IMEI14,
        faid_table=drom[0xb74f*2:0xb755*2],
        identity_table=drom[0xb755*2:0xb75b*2],lock_table=drom[0xb749*2:0xb74f*2])
    return replace_low_records(pmm,records)


def assess_runtime(text):
    rows=re.findall(r'npe3_integrity_flags: sequence=(\d+) t=[\d.]+ value=([0-9a-f]{4}) mask=ffff owner=([0-9a-f]{6}) observer_writes=0',text)
    expected=[(0,0xf0f),(0,0x45b4),(1,0x45b7),(1,0x45bc),(3,0x45bf),(3,0x45e6),(7,0x45e9)]
    if (len(rows)<7 or len(rows)>=32 or len(rows)!=text.count('npe3_integrity_flags:') or
            [int(row[0]) for row in rows]!=list(range(1,len(rows)+1)) or
            [(int(value,16),int(owner,16)) for _,value,owner in rows[:7]]!=expected):
        raise ValueError('synthetic records did not pass all three initial native validators')
    return dict(initial_native_validations_qualified=True,
                later_validation_writes=[dict(value=int(value,16),owner=owner) for _,value,owner in rows[7:]],
                complete_record_family_qualified=False,fitted_identity_claim=False)
