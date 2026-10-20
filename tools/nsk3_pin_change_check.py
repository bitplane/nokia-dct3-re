"""Physical NSK-3 ordinary CHANGE CHV and cold authentication with its new PIN."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_static_check import inspect
from tools.nsk3_security_check import check_trace as check_phone
from tools.nsk3_pin_check import check_trace as check_pin,PIN,PHONE_PROMPT
from tools.nsk3_boot_check import check_upload_handoffs
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_security_trace_check import CHV_OFFSET,ENABLED_OFFSET,AOC_NVRAM_SIZE,statuses

KEYS=(0x19,4,5,6,0x19,0x18,0x19,1,2,3,4,0x19)+(4,3,2,1,0x19)*2
CHANGED='e3d554448ecdac351002f8a3f94582448fc1f03cc5a9b3071e9b0e60272f018a'


def check_change(trace,before,after):
    check_phone(trace,True,KEYS)
    if statuses(trace,0x20)!=[(0x9000,3,10)] or statuses(trace,0x24)!=[(0x9000,3,10)]:
        raise ValueError('initial VERIFY or CHANGE CHV outcome differs')
    if any(statuses(trace,ins) for ins in (0x26,0x28,0x2c)):
        raise ValueError('unexpected credential operation')
    tx=bytes(int(value,16) for value in re.findall(r'nsk3_sim_tx: index=\d+ value=([0-9a-f]+)',trace))
    if tx.count(bytes.fromhex('a024000110')+b'1234'+b'\xff'*4+b'4321'+b'\xff'*4)!=1:
        raise ValueError('CHANGE CHV credential payload differs')
    if len(before)!=AOC_NVRAM_SIZE or before[ENABLED_OFFSET]!=1 or before[CHV_OFFSET:CHV_OFFSET+8]!=b'1234'+b'\xff'*4:
        raise ValueError('initial card is not the expected request-enabled card')
    expected=bytearray(before)
    expected[CHV_OFFSET:CHV_OFFSET+8]=b'4321'+b'\xff'*4
    if after!=expected:raise ValueError('CHANGE CHV mutated more than PIN1')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','changed','cold'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        _,image=inspect(args.source,'boot4')
        before=(args.changed/'initial_sim_card.bin').read_bytes()
        after=(args.changed/'nvram/nsk3r4t/sim_card').read_bytes()
        trace=(args.changed/'error.log').read_text()
        check_change(trace,before,after)
        check_upload_handoffs(trace,1)
        check_pin((args.cold/'error.log').read_text(),True,b'4321')
        check_upload_handoffs((args.cold/'error.log').read_text(),1)
        if (args.cold/'initial_sim_card.bin').read_bytes()!=after or (args.cold/'nvram/nsk3r4t/sim_card').read_bytes()!=after:
            raise ValueError('cold changed-PIN card not preserved')
        for run in (args.changed,args.cold):
            if (run/'nvram/nsk3r4t/flash').read_bytes()!=image:
                raise ValueError('own executable flash changed')
        frame_check(args.changed/'snap/nsk3_change_pin_37.png',CHANGED)
        frame_check(args.cold/'snap/nsk3_pin_5.png',PIN)
        frame_check(args.cold/'snap/nsk3_pin_17.png',PHONE_PROMPT)
    except (ValueError,OSError) as error:
        print(f'NSK-3 PIN change failed: {error}',file=sys.stderr)
        return 1
    print('NSK-3 ordinary physical CHANGE CHV/cold new-PIN PASS')
    return 0


if __name__=='__main__':raise SystemExit(main())
