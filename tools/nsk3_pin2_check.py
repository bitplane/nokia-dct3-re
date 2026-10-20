"""Physical NSK-3 PIN2 change and independent cold new-PIN2 authentication."""
import argparse
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_static_check import inspect
from tools.nsk3_security_check import check_trace as check_phone
from tools.nsk3_boot_check import check_upload_handoffs
from tools.nsk3_phonebook_check import transmitted
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_security_trace_check import CHV_OFFSET,ATTEMPTS_OFFSET,ENABLED_OFFSET,AOC_NVRAM_SIZE,statuses,padded_pin

PREFIX=(0x19,4,5,6,0x19,0x18,0x18,0x19)
FRAME='e3aa8fdedd0634e762c3b42e469b3b57e50f5870952e37518e9b63df4f1d4926'
ERROR_FRAME='fd5b8fb1ab0c3397bf958d567a46aee84a72f75123802a00d5bc802a1814543e'


def check_change(trace,before,after,old,new,accepted=True):
    if len(before)!=AOC_NVRAM_SIZE or not 1<=before[ATTEMPTS_OFFSET+1]<=3:
        raise ValueError('initial PIN2 attempts/extent differs')
    keys=PREFIX+tuple(int(d) for d in old)+(0x19,)+2*(tuple(int(d) for d in new)+(0x19,))
    check_phone(trace,True,keys)
    result=0x9000 if accepted else 0x9840 if before[ATTEMPTS_OFFSET+1]==1 else 0x9804
    if statuses(trace,0x20)!=[(0x9000,3,10)] or statuses(trace,0x24)!=[(result,3,10)]:
        raise ValueError('PIN1 startup or PIN2 change result differs')
    if any(statuses(trace,ins) for ins in (0x26,0x28,0x2c)):
        raise ValueError('unexpected credential operation')
    command=bytes.fromhex('a024000210')+padded_pin(old)+padded_pin(new)
    if transmitted(trace).count(command)!=1:raise ValueError('PIN2 CHANGE CHV payload differs')
    if (len(before)!=AOC_NVRAM_SIZE or before[ENABLED_OFFSET]!=1
            or before[CHV_OFFSET:CHV_OFFSET+8]!=padded_pin('1234')
            or (accepted and before[CHV_OFFSET+8:CHV_OFFSET+16]!=padded_pin(old))):
        raise ValueError('initial laboratory card credentials differ')
    expected=bytearray(before)
    if accepted:
        expected[CHV_OFFSET+8:CHV_OFFSET+16]=padded_pin(new)
        expected[ATTEMPTS_OFFSET+1]=3
    else:
        if before[CHV_OFFSET+8:CHV_OFFSET+16]==padded_pin(old) or not 1<=before[ATTEMPTS_OFFSET+1]<=3:
            raise ValueError('wrong-PIN2 fixture is not a rejectable credential')
        expected[ATTEMPTS_OFFSET+1]-=1
    if after!=expected:raise ValueError('PIN2 change altered other persistent card bytes')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','changed','cold'):parser.add_argument(name,type=Path)
    parser.add_argument('--wrong',type=Path)
    parser.add_argument('--retry-cold',type=Path)
    args=parser.parse_args()
    try:
        _,image=inspect(args.source,'boot4')
        changed=(args.changed/'nvram/nsk3r4t/sim_card').read_bytes()
        if (args.cold/'initial_sim_card.bin').read_bytes()!=changed:
            raise ValueError('cold card is not the saved changed-PIN2 card')
        for run,old,new in ((args.changed,'5678','8765'),(args.cold,'8765','5678')):
            trace=(run/'error.log').read_text()
            check_change(trace,(run/'initial_sim_card.bin').read_bytes(),
                         (run/'nvram/nsk3r4t/sim_card').read_bytes(),old,new)
            check_upload_handoffs(trace,1)
            if (run/'nvram/nsk3r4t/flash').read_bytes()!=image:raise ValueError('own flash changed')
            frame_check(run/'snap/nsk3_pin2_47.png',FRAME)
        if bool(args.wrong)!=bool(args.retry_cold):raise ValueError('wrong and retry-cold require both captures')
        if args.wrong:
            rejected=(args.wrong/'nvram/nsk3r4t/sim_card').read_bytes()
            if (args.wrong/'initial_sim_card.bin').read_bytes()!=(args.changed/'initial_sim_card.bin').read_bytes():
                raise ValueError('wrong-PIN2 control card differs')
            if (args.retry_cold/'initial_sim_card.bin').read_bytes()!=rejected:
                raise ValueError('cold retry card is not saved rejected card')
            for run,old,accepted in ((args.wrong,'5675',False),(args.retry_cold,'5678',True)):
                trace=(run/'error.log').read_text()
                check_change(trace,(run/'initial_sim_card.bin').read_bytes(),
                             (run/'nvram/nsk3r4t/sim_card').read_bytes(),old,'8765',accepted)
                check_upload_handoffs(trace,1)
                if (run/'nvram/nsk3r4t/flash').read_bytes()!=image:raise ValueError('retry executable flash changed')
            if (args.retry_cold/'nvram/nsk3r4t/sim_card').read_bytes()!=changed:
                raise ValueError('cold retry did not recover exact changed-PIN2 card')
            frame_check(args.wrong/'snap/nsk3_pin2_40.png',ERROR_FRAME)
            frame_check(args.retry_cold/'snap/nsk3_pin2_47.png',FRAME)
    except (ValueError,OSError) as error:
        print(f'NSK-3 PIN2 qualification failed: {error}',file=sys.stderr)
        return 1
    print('Physical PIN2 change and cold new-PIN2 authentication PASS; PUK2 has a separate gate')
    return 0


if __name__=='__main__':raise SystemExit(main())
