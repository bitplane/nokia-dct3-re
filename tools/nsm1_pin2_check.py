"""Physical NSM-1 PIN2 change, cold restore and retry recovery controls."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_pin_check import check_authenticated
from tools.nsm1_published_record_check import records
from tools.nsm1_v523_static_check import SHA1
from tools.nsk3_pin2_check import PREFIX,ERROR_FRAME
from tools.nsk3_phonebook_check import transmitted
from tools.sim_security_trace_check import CHV_OFFSET,ATTEMPTS_OFFSET,padded_pin,statuses
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH
from tools.nsk1_phonebook_roundtrip import frame_check

FRAME='d145e1eb7e1008b3271c709e01b483fdb017b2eb9e29e80b3f545da93ccff14f'


def changed_card(before,old,new,accepted):
    if len(before)!=CURRENT_NVRAM_LENGTH or not 1<=before[ATTEMPTS_OFFSET+1]<=3:
        raise ValueError('initial card extent/retries differ')
    matches=before[CHV_OFFSET+8:CHV_OFFSET+16]==padded_pin(old)
    if matches!=accepted:raise ValueError('credential control differs')
    expected=bytearray(before)
    if accepted:
        expected[CHV_OFFSET+8:CHV_OFFSET+16]=padded_pin(new)
        expected[ATTEMPTS_OFFSET+1]=3
    else:expected[ATTEMPTS_OFFSET+1]-=1
    return bytes(expected)


def check_change(trace,before,after,old,new,accepted=True):
    keys=PREFIX+tuple(map(int,old))+(0x19,)+(tuple(map(int,new))+(0x19,))*2
    check_authenticated(trace,keys,allowed_mutations=(0x24,))
    result=0x9000 if accepted else 0x9840 if before[ATTEMPTS_OFFSET+1]==1 else 0x9804
    if statuses(trace,0x24)!=[(result,3,10)]:raise ValueError('PIN2 result differs')
    command=bytes.fromhex('a024000210')+padded_pin(old)+padded_pin(new)
    if transmitted(trace,4096,'nsm1_sim_tx').count(command)!=1:raise ValueError('PIN2 command differs')
    if after!=changed_card(before,old,new,accepted):raise ValueError('whole-card PIN2 transition differs')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('changed','restored','wrong','retry'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        cases=((args.changed,None,'5678','8765',True),
               (args.restored,args.changed,'8765','5678',True),
               (args.wrong,None,'5675','8765',False),
               (args.retry,args.wrong,'5678','8765',True))
        for run,parent,old,new,accepted in cases:
            before=(run/'initial_sim_card.bin').read_bytes();storage=run/'nvram/nsm1r4t'
            check_change((run/'error.log').read_text(),before,(storage/'sim_card').read_bytes(),old,new,accepted)
            records((run/'error.log').read_text(),True)
            if parent:
                if before!=(parent/'nvram/nsm1r4t/sim_card').read_bytes():raise ValueError('cold card link differs')
                if (run/'initial_eeprom.bin').read_bytes()!=(parent/'nvram/nsm1r4t/eeprom').read_bytes():
                    raise ValueError('cold EEPROM link differs')
            if hashlib.sha1((storage/'flash').read_bytes()).hexdigest()!=SHA1:raise ValueError('own flash changed')
            frame_check(run/'snap/nsm1_pin2_40.png',FRAME if accepted else ERROR_FRAME)
        if (args.wrong/'initial_sim_card.bin').read_bytes()!=(args.changed/'initial_sim_card.bin').read_bytes():
            raise ValueError('negative control input differs')
        if (args.retry/'nvram/nsm1r4t/sim_card').read_bytes()!=(args.changed/'nvram/nsm1r4t/sim_card').read_bytes():
            raise ValueError('retry did not restore exact positive card')
    except (ValueError,OSError) as error:
        print(f'NSM-1 PIN2 qualification failed: {error}',file=sys.stderr);return 1
    print('NSM-1 physical PIN2 change/cold restore and wrong-PIN retry recovery PASS')
    return 0


if __name__=='__main__':raise SystemExit(main())
