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
from tools.dct3_eeprom_descriptor import eeprom_geometry

PROFILES={
    'boot4':('nsk3__05.61',0xc7e00,0x2ae384,4,
             'a585b1c7e39b5dd612856d64b49405f4d38a51f321ffe22739589f1e4ea90ab4',
             '33204770d13bb97666462c457b483d5e11723cde3f52e542ae65d2c80355e9cb'),
    'boot3':('nsk3___5.61',0xc8b00,0x2ae374,3,
             'b5f1f652cd7d3591418bc23187dbe5cc1c287992e58921658bcb082bca6e48a5',
             '25bd572968c6e61718b50f68b5f32549ade7b921f18b64c0a222ddbc636acc99'),
}

PERIPHERAL_FRAGMENTS={
    'eeprom_stop':(0x2a772a,'22012380029b33207819431170197919431171192004781943017019'),
    'eeprom_start':(0x2a779c,'22012380029b33207819439170197919431171192004781943017019'),
    'eeprom_input':(0x2a765e,'25802601270446a6200002ab3320791a781c43b2711a'),
    'ccont':(0x2a8a62,'222821225411222a21044321541121295c0908c9d3fb212d5c08'),
    'lcd_command':(0x2a8fa0,'21295c890849d3fb212c548846f7'),
    'lcd_data':(0x2a9006,'21295c890849d3fb212b548846f7'),
    'keypad_scan':(0x2aa788,'2031231f5503202f26e0550620305d0006c00ec006000e0125ff291f'),
    'simi_init':(0x29916c,'213820ff550820c07220'),
    'power_raw_compare':(0x2ac6ea,'2884d1f9'),
}


def peripheral_contract(image,variant):
    shift=0 if variant=='boot4' else -0x10
    sites={}
    for name,(address,encoded) in PERIPHERAL_FRAGMENTS.items():
        address+=shift
        expected=bytes.fromhex(encoded)
        if image[address-0x200000:address-0x200000+len(expected)] != expected:
            raise ValueError(f'own NSK-3 {name} instruction contract differs')
        sites[name]=hex(address)
    if image[0xe4:0xec] != bytes.fromhex('e28f0001 e12fff10'):
        raise ValueError('own NSK-3 ARM/Thumb interworking differs')
    if image[0x16c:0x170] != bytes.fromhex('0010fa5c'):
        raise ValueError('own NSK-3 stack literal differs')
    geometry=eeprom_geometry(image[5])
    if geometry['descriptor']!='0xf4':
        raise ValueError('own NSK-3 EEPROM descriptor differs')
    return {'instruction_sites':sites,'thumb_entry':'0x2000ec','stack_literal':'0x10fa5c',
            'eeprom_geometry':geometry,'eeprom_sda_bit':0,'eeprom_scl_bit':2,
            'gensio':{'select':'0x28','ccont_select_value':'0x22','command':'0x2a',
                      'receive':'0x2d','status':'0x29','ready_bit':2,
                      'lcd_command':'0x2c','lcd_data':'0x2b'},
            'keypad':{'row_signal':'0x31','row_direction':'0x2f','column_input':'0x30','rows':5},
            'simi_iir':'0x38','simi_control':'0x39',
            'physical_wiring_or_timing_proved':False}


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
            'peripheral_contract':peripheral_contract(flash,variant),
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
