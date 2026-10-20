"""Qualify the two acquired NSK-3 v5.61 MCU/PPM-A pairs, not a handset."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.extract_dct3_wintesla import extract_records
from tools.nsk1_static_check import read_catalogue

PROFILES={
    'boot4':('nsk3__05.61',0xc7e00,0x2ae384,4,
             'a585b1c7e39b5dd612856d64b49405f4d38a51f321ffe22739589f1e4ea90ab4',
             '33204770d13bb97666462c457b483d5e11723cde3f52e542ae65d2c80355e9cb'),
    'boot3':('nsk3___5.61',0xc8b00,0x2ae374,3,
             'b5f1f652cd7d3591418bc23187dbe5cc1c287992e58921658bcb082bca6e48a5',
             '25bd572968c6e61718b50f68b5f32549ade7b921f18b64c0a222ddbc636acc99'),
}


def bootstrap_identity(payload):
    # Own C54x STM immediate instruction: data[0803] = identity.
    if len(payload)!=372 or payload[24:28] != bytes.fromhex('76f8 0803'):
        raise ValueError('NSK-3 bootstrap identity instruction differs')
    value=int.from_bytes(payload[28:30],'big')
    if value not in (3,4):
        raise ValueError('unclassified NSK-3 bootstrap identity')
    return value


def inspect(directory,variant):
    stem,size,table,identity,mcu_hash,ppm_hash=PROFILES[variant]
    mcu_address,mcu=extract_records(directory/(stem+'0'))
    ppm_address,ppm=extract_records(directory/(stem+'a'))
    if (mcu_address!=0x200000 or len(mcu)!=size
            or ppm_address!=0x2d0000 or len(ppm)!=0x30000
            or hashlib.sha256(mcu).hexdigest()!=mcu_hash
            or hashlib.sha256(ppm).hexdigest()!=ppm_hash):
        raise ValueError('requires the matched acquired NSK-3 v5.61 pair')
    catalogue=read_catalogue(mcu,table)
    bootstrap=catalogue['entries'][-1]
    if bootstrap['slot']!=25 or bootstrap['header']!=[0x0f00,0,186,0x0700,180,0]:
        raise ValueError('NSK-3 bootstrap descriptor differs')
    offset=int(bootstrap['descriptor'],16)-mcu_address+12
    if bootstrap_identity(mcu[offset:offset+372])!=identity:
        raise ValueError('NSK-3 bootstrap identity/profile disagreement')
    calls=[]
    for relative in (0x44,0x7c,0xf4):
        if mcu[offset+relative:offset+relative+2] != bytes.fromhex('f074'):
            raise ValueError('NSK-3 bootstrap direct CALL differs')
        calls.append(int.from_bytes(mcu[offset+relative+2:offset+relative+4],'big'))
    if calls != ([0x8023,0x8023,0x802b] if identity==4 else [0x8008,0x8008,0x8010]):
        raise ValueError('NSK-3 bootstrap resident CALL targets differ')
    version=b'V 05.61' if identity==4 else b'V  5.61'
    marker=version+b'\n22-11-99\nNSK-3\n(c) NMP.\0'
    if mcu.count(marker)!=1 or b'NSK-3' not in ppm[:64]:
        raise ValueError('own MCU/PPM identity marker differs')
    flash=mcu+b'\xff'*(ppm_address-mcu_address-len(mcu))+ppm
    return {'variant':variant,'mcu_member':stem+'0','ppm_member':stem+'a',
            'mcu_address':hex(mcu_address),'mcu_bytes':len(mcu),'mcu_sha256':mcu_hash,
            'ppm_address':hex(ppm_address),'ppm_bytes':len(ppm),'ppm_sha256':ppm_hash,
            'unprogrammed_gap_bytes':ppm_address-mcu_address-len(mcu),
            'flash_bytes':len(flash),'flash_sha1':hashlib.sha1(flash).hexdigest(),
            'flash_sha256':hashlib.sha256(flash).hexdigest(),
            'catalogue_address':hex(table),'catalogue':catalogue,
            'bootstrap_data_0803':identity,'bootstrap_resident_calls':[hex(x) for x in calls],
            'bootstrap_mask_compatibility_proved':False,
            'external_eeprom_acquired':False,'runtime_acceptance':False},flash


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    try:
        reports=[]
        for variant in PROFILES:
            report,flash=inspect(args.source,variant)
            reports.append(report)
            if args.output:
                args.output.mkdir(parents=True,exist_ok=True)
                (args.output/f'6130-v561-{variant}-a.fls').write_bytes(flash)
        print(json.dumps(reports,indent=2))
    except (ValueError,OSError) as error:
        print(f'NSK-3 static qualification failed: {error}',file=sys.stderr)
        return 1
    return 0


if __name__=='__main__':
    raise SystemExit(main())
