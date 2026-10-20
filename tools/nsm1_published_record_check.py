"""Qualify NSM-1 diagnostic record validation/update/cold persistence, not boot."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.dct3_rom4_record_codec import decode_record,retained_record_transform
from tools.dct3_lock_record_example import EXAMPLE_RECORDS
from tools.nsm1_v523_static_check import SHA1


def records(trace,cold=False):
    packets=[bytes.fromhex(raw) for raw in re.findall(r'nsm1_restart_packet: .*?bytes=([0-9a-f]+)',trace)]
    replies=[p for p in packets if p[4:6] in (b'\x35\x32',b'\x35\x34')]
    if [p[5] for p in replies]!=([0x32] if cold else [0x32,0x34,0x32]):
        raise ValueError('record update dialogue differs')
    for p in replies:
        update=p[5]==0x34
        if len(p)!=(58 if update else 56) or p[3]!=0x74 or p[6:8]!=bytes(2):
            raise ValueError('record response envelope differs')
        encoded=p[32:56]
        plain=b''.join(decode_record(encoded[i:i+12],bytes.fromhex('00160010')) for i in (0,12))
        for i in (0,12):
            if plain[i+10:i+12]!=EXAMPLE_RECORDS[i+10:i+12] or p[8+i:18+i]!=plain[i:i+10]:
                raise ValueError('native record arithmetic differs')
            if p[18+i:20+i]!=(b'\xa5\xa5' if update else bytes(2)):
                raise ValueError('record marker differs')
        if update and ((sum(int.from_bytes(p[i:i+2],'big') for i in range(6,56,2))^65535)&65535)!=int.from_bytes(p[56:58],'big'):
            raise ValueError('native update checksum differs')
    finals=re.findall(r'nsm1_record_validation: pc=0027e798 .*?r5=([0-9a-f]+)',trace)
    if finals!=['00000001']*len(replies) or 'nsm1_record_validation: pc=0027e796 ' in trace:
        raise ValueError('record validator did not retain validity')
    resets=re.findall(r'nsm1_restart_request: reason=([0-9a-f]+) caller=([0-9a-f]+)',trace)
    if resets!=([] if cold else [('00000004','00242c91')]):
        raise ValueError('record restart lifecycle differs')
    return replies[-1][32:56]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('initial',type=Path);parser.add_argument('cold',type=Path)
    args=parser.parse_args()
    try:
        first=records((args.initial/'error.log').read_text())
        second=records((args.cold/'error.log').read_text(),True)
        a=(args.initial/'nvram/nsm1r4t/eeprom').read_bytes()
        b=(args.cold/'nvram/nsm1r4t/eeprom').read_bytes()
        if len(a)!=0x4000 or a!=b or first!=second or retained_record_transform(a[0x20:0x38],a[:12])!=second:
            raise ValueError('cold persisted record/echo differs')
        for run in (args.initial,args.cold):
            if hashlib.sha1((run/'nvram/nsm1r4t/flash').read_bytes()).hexdigest()!=SHA1:
                raise ValueError('own executable flash changed')
        if int.from_bytes(a[0x3c:0x40],'big')!=sum(a[:0x3c])&65535:
            raise ValueError('saved low-block checksum differs')
    except (ValueError,OSError) as error:
        print(f'NSM-1 record qualification failed: {error}',file=sys.stderr);return 1
    print('NSM-1 native diagnostic record validation and cold persistence PASS; UI/radio unaccepted')
    return 0


if __name__=='__main__':raise SystemExit(main())
