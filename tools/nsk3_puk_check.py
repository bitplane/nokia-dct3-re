"""Physical NSK-3 PUK positive/negative control and cold new-PIN authentication."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import statuses,CHV_OFFSET,ATTEMPTS_OFFSET,AOC_NVRAM_SIZE,NVRAM_SIZE
from tools.nsk3_pin_check import check_trace as check_pin,PIN,PHONE_PROMPT
from tools.nsk3_static_check import inspect
from tools.nsk3_boot_check import check_upload_handoffs
from tools.nsk1_phonebook_roundtrip import frame_check

PUK='642bb9aa764fc34ad31c1eb32611724b5c3902d56c127cfe5e0a447e37d58f7c'
PUK_RETRY='bb522918e8d4e86e6f8b7602f4ed7f44385f17675d4a939b4fba50a5ac33d68f'


def check_recovery(trace,card,wrong=False,initial=None):
    if initial is None:
        initial=bytearray(make_profile(True).ljust(AOC_NVRAM_SIZE,b'\xff'))
        initial[ATTEMPTS_OFFSET]=0
        # Legacy profile records acquire the laboratory subscriber's preferred PLMN.
        initial[NVRAM_SIZE:NVRAM_SIZE+3]=bytes.fromhex('00f110')
    if (len(initial)!=AOC_NVRAM_SIZE or initial[ATTEMPTS_OFFSET]!=0
            or not 1<=initial[ATTEMPTS_OFFSET+2]<=10):
        raise ValueError('requires a blocked PIN1 with live PUK1 retries')
    remaining=initial[ATTEMPTS_OFFSET+2]-1 if wrong else 10
    digits=(1,2,3,4,5,6,7,9 if wrong else 8,0x19)+(4,3,2,1,0x19)*2
    contacts=re.findall(r'nsk3_puk_contact: key=([0-9a-f]+) down=([01]) ',trace)
    if contacts!=[(f'{key:02x}',down) for key in digits for down in ('1','0')]:
        raise ValueError('physical PUK/new-PIN/confirmation contacts differ')
    tx=bytes(int(value,16) for value in re.findall(r'nsk3_sim_tx: index=\d+ value=([0-9a-f]+)',trace))
    command=bytes.fromhex('a02c000010')+(b'12345679' if wrong else b'12345678')+b'4321'+b'\xff'*4
    if tx.count(command)!=1:raise ValueError('UNBLOCK CHV APDU differs')
    status=0x9840 if wrong and remaining==0 else 0x9804 if wrong else 0x9000
    if statuses(trace,0x2c)!=[(status,0 if wrong else 3,remaining)]:
        raise ValueError('UNBLOCK result/retry transition differs')
    if any(statuses(trace,ins) for ins in (0x20,0x24,0x26,0x28)):
        raise ValueError('unexpected credential APDU')
    expected=bytearray(initial)
    if wrong:
        expected[ATTEMPTS_OFFSET]=0
        expected[ATTEMPTS_OFFSET+2]=remaining
    else:
        expected[CHV_OFFSET:CHV_OFFSET+8]=b'4321'+b'\xff'*4
        expected[ATTEMPTS_OFFSET]=3
        expected[ATTEMPTS_OFFSET+2]=10
    if card!=expected:
        raise ValueError('credential/retry storage differs')
    if re.search(r'illegal|unsupported|unhandled|fatal|\[LUA ERROR\]',trace,re.I):
        raise ValueError('execution/observer error')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','recovery','wrong','cold'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        _,image=inspect(args.source,'boot4')
        for run,wrong in ((args.recovery,False),(args.wrong,True)):
            trace=(run/'error.log').read_text()
            check_recovery(trace,(run/'nvram/nsk3r4t/sim_card').read_bytes(),wrong)
            check_upload_handoffs(trace,1)
            frame_check(run/'snap/nsk3_pin_5.png',PUK)
        cold=(args.cold/'error.log').read_text()
        check_pin(cold,True,b'4321')
        check_upload_handoffs(cold,1)
        saved=(args.recovery/'nvram/nsk3r4t/sim_card').read_bytes()
        if (args.cold/'initial_sim_card.bin').read_bytes()!=saved or (args.cold/'nvram/nsk3r4t/sim_card').read_bytes()!=saved:
            raise ValueError('cold new-PIN card not preserved byte-for-byte')
        for run in (args.recovery,args.wrong,args.cold):
            if (run/'nvram/nsk3r4t/flash').read_bytes()!=image:
                raise ValueError('own executable flash changed')
        frame_check(args.recovery/'snap/nsk3_pin_24.png',PHONE_PROMPT)
        frame_check(args.wrong/'snap/nsk3_pin_24.png',PUK_RETRY)
        frame_check(args.cold/'snap/nsk3_pin_5.png',PIN)
        frame_check(args.cold/'snap/nsk3_pin_17.png',PHONE_PROMPT)
    except (ValueError,OSError) as error:
        print(f'NSK-3 PUK gate failed: {error}',file=sys.stderr)
        return 1
    print('NSK-3 physical PUK/rejection/cold new-PIN PASS; phone lock remains distinct')
    return 0


if __name__=='__main__':raise SystemExit(main())
