"""Physical NSK-3 PIN-request enable and cold challenge, not disable parity."""
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
from tools.sim_security_trace_check import ENABLED_OFFSET,statuses,AOC_NVRAM_SIZE

KEYS=(0x19,4,5,1,0x19,1,2,3,4,0x19,0x17,0x19)
ON='ad0b9ff01765b3a2d0be7f31d31125a3316c7cc6c255d0b8864ce7ffef5ca001'


def check_enable(trace,before,after):
    if len(before)!=AOC_NVRAM_SIZE or len(after)!=len(before):
        raise ValueError('card storage layout differs')
    check_phone(trace,True,KEYS)
    if statuses(trace,0x28)!=[(0x9000,3,10)]:
        raise ValueError('ENABLE CHV status differs')
    if any(statuses(trace,ins) for ins in (0x24,0x26,0x2c)):
        raise ValueError('unexpected credential operation')
    tx=bytes(int(value,16) for value in re.findall(r'nsk3_sim_tx: index=\d+ value=([0-9a-f]+)',trace))
    if tx.count(bytes.fromhex('a028000108')+b'1234'+b'\xff'*4)!=1:
        raise ValueError('ENABLE CHV credential payload differs')
    expected=bytearray(before)
    if expected[ENABLED_OFFSET]!=0:raise ValueError('initial request was not disabled')
    expected[ENABLED_OFFSET]=1
    if after!=expected:raise ValueError('enable changed more than the card request flag')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','seed','enabled','cold'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        _,image=inspect(args.source,'boot4')
        before=(args.seed/'nvram/nsk3r4t/sim_card').read_bytes()
        after=(args.enabled/'nvram/nsk3r4t/sim_card').read_bytes()
        trace=(args.enabled/'error.log').read_text()
        check_enable(trace,before,after)
        check_upload_handoffs(trace,1)
        cold=(args.cold/'error.log').read_text()
        check_pin(cold,True)
        check_upload_handoffs(cold,1)
        if (args.cold/'initial_sim_card.bin').read_bytes()!=after or (args.cold/'nvram/nsk3r4t/sim_card').read_bytes()!=after:
            raise ValueError('cold request-enabled card changed')
        for run in (args.enabled,args.cold):
            if (run/'nvram/nsk3r4t/flash').read_bytes()!=image:
                raise ValueError('own executable flash changed')
        frame_check(args.enabled/'snap/nsk3_settings_24.png',ON)
        frame_check(args.cold/'snap/nsk3_pin_5.png',PIN)
        frame_check(args.cold/'snap/nsk3_pin_17.png',PHONE_PROMPT)
    except (ValueError,OSError) as error:
        print(f'NSK-3 PIN request failed: {error}',file=sys.stderr)
        return 1
    print('NSK-3 physical PIN request On/cold challenge PASS; disable/change not yet accepted')
    return 0


if __name__=='__main__':raise SystemExit(main())
