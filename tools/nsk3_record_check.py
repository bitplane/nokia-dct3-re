"""Verify diagnostic NSK-3 native record update and cold persistence, not idle."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_boot_check import check_run,check_upload_handoffs
from tools.nsk3_startup_check import check_record_math,BLANK
from tools.make_nsk3_record_profile import build
from tools.dct3_rom4_record_codec import decode_record,retained_record_transform
from tools.dct3_lock_record_example import EXAMPLE_RECORDS
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.nsk3_static_check import inspect


def replies(trace,chip,cold=False):
    rows=re.findall(r'nsk3_record_reply: .*?data=([0-9a-f]+) cycles=',trace)
    if len(rows)!=trace.count('nsk3_record_reply: '):
        raise ValueError('malformed record reply')
    decoded=[]
    for raw in rows:
        capture=bytes.fromhex(raw)
        if len(capture)!=64:raise ValueError('full update capture required')
        size=int.from_bytes(capture[4:6],'big')+6
        if size>len(capture):raise ValueError('reply truncated')
        packet=capture[:size]
        if packet[:4]!=bytes.fromhex('00000074') or packet[6:8]!=bytes.fromhex('0100'):
            raise ValueError('reply transport header differs')
        body=packet[8:]
        if body[:4]==bytes.fromhex('340e0082') and len(body)==16:
            continue
        update=body[:4]==bytes.fromhex('35340000')
        if len(body)!=(54 if update else 52) or body[:4] not in (
                bytes.fromhex('35320000'),bytes.fromhex('35340000')):
            raise ValueError('unclassified record reply')
        if update and ((sum(int.from_bytes(packet[i:i+2],'big') for i in range(10,60,2))^0xffff)&0xffff)!=int.from_bytes(packet[60:62],'big'):
            raise ValueError('update checksum differs')
        encoded=body[28:52]
        plain=b''.join(decode_record(encoded[i:i+12],chip) for i in (0,12))
        for i in (0,12):
            if plain[i:i+10]!=body[4+i:14+i] or plain[i+10:i+12]!=EXAMPLE_RECORDS[i+10:i+12]:
                raise ValueError('native record decode differs')
            if body[14+i:16+i]!=(b'\xa5\xa5' if update else bytes(2)):
                raise ValueError('record status marker differs')
        decoded.append((update,encoded,body[4:28]))
    if [row[0] for row in decoded]!=([False] if cold else [False,True,False]):
        raise ValueError('record update lifecycle differs')
    copies=re.findall(r'nsk3_record_accept: context=0010c018 copied=([0-9a-f]+) cycles=',trace)
    expected=[]
    for _,_,plain in (decoded if cold else decoded[1:]):
        copy=bytearray(plain)
        copy[20]&=~8
        expected.append(copy.hex())
    if copies!=expected or len(copies)!=trace.count('nsk3_record_accept: '):
        raise ValueError('firmware accepted-copy contract differs')
    calls=re.findall(r'nsk3_restart_entry: reason=([0-9a-f]+) caller=([0-9a-f]+)',trace)
    if calls!=([] if cold else [('00000004','0023d83b')]):
        raise ValueError('record lifecycle restart differs')
    if re.search(r'illegal|unsupported|unhandled|fatal|\[LUA ERROR\]',trace,re.I):
        raise ValueError('execution/observer error')
    check_upload_handoffs(trace,1 if cold else 2)
    return decoded[-1][1]


def check(source,control,native_roms,initial,cold):
    check_run(source,control,'boot4',native_roms)
    records=check_record_math((control/'error.log').read_text())
    chips={record['chip_field'] for record in records}
    if len(chips)!=1:raise ValueError('unstable own chip identity')
    chip=bytes.fromhex(chips.pop())
    seed=(control/'nvram/nsk3r4t/eeprom').read_bytes()
    expected=bytearray(build(seed,chip))
    updated=replies((initial/'error.log').read_text(),chip)
    if replies((cold/'error.log').read_text(),chip,True)!=updated:
        raise ValueError('cold record differs from native update')
    expected[0x20:0x38]=retained_record_transform(updated,seed[:12])
    expected[0x3c:0x40]=(sum(expected[:0x3c])&0xffff).to_bytes(4,'big')
    expected[0x73]=0xe0
    expected[0x7a]=0x4e
    expected[0x11e:0x120]=bytes.fromhex('da54')
    _,image=inspect(source,'boot4')
    for run in (initial,cold):
        if (run/'nvram/nsk3r4t/eeprom').read_bytes()!=expected:
            raise ValueError('EEPROM differs beyond firmware-owned record/checksum writes')
        if (run/'nvram/nsk3r4t/flash').read_bytes()!=image:
            raise ValueError('own executable flash changed')
        for time in (5,15):frame_check(run/f'snap/nsk3_boot_{time}.png',BLANK)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','control','native_roms','initial','cold'):
        parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:check(args.source,args.control,args.native_roms,args.initial,args.cold)
    except (ValueError,OSError) as error:
        print(f'NSK-3 record persistence failed: {error}',file=sys.stderr)
        return 1
    print('NSK-3 diagnostic record update/cold persistence PASS; blank display, usable boot unproved')
    return 0


if __name__=='__main__':raise SystemExit(main())
