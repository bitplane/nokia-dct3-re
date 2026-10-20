"""Physical NSK-3 PIN-request setting and corresponding cold challenge behavior."""
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
OFF='6820183b2ba811bde90ea0252422a0d5d612f1eee0c0a87c9d170b79f5ff451f'


def check_enable(trace,before,after):
    check_setting(trace,before,after,True)


def check_setting(trace,before,after,enabled):
    if len(before)!=AOC_NVRAM_SIZE or len(after)!=len(before):
        raise ValueError('card storage layout differs')
    keys=KEYS if enabled else KEYS[:-2]+(0x18,0x19)
    instruction=0x28 if enabled else 0x26
    check_phone(trace,True,keys)
    if statuses(trace,instruction)!=[(0x9000,3,10)]:
        raise ValueError('CHV setting status differs')
    if any(statuses(trace,ins) for ins in (0x24,0x26 if enabled else 0x28,0x2c)):
        raise ValueError('unexpected credential operation')
    tx=bytes(int(value,16) for value in re.findall(r'nsk3_sim_tx: index=\d+ value=([0-9a-f]+)',trace))
    if tx.count(bytes((0xa0,instruction,0,1,8))+b'1234'+b'\xff'*4)!=1:
        raise ValueError('CHV setting credential payload differs')
    expected=bytearray(before)
    if expected[ENABLED_OFFSET]!=int(not enabled):raise ValueError('initial request flag differs')
    expected[ENABLED_OFFSET]=int(enabled)
    if after!=expected:raise ValueError('setting changed more than the card request flag')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','seed','setting','cold'):parser.add_argument(name,type=Path)
    parser.add_argument('--disabled',action='store_true',help='check physical Off and a cold boot without SIM VERIFY')
    args=parser.parse_args()
    try:
        _,image=inspect(args.source,'boot4')
        before=(args.seed/'nvram/nsk3r4t/sim_card').read_bytes()
        after=(args.setting/'nvram/nsk3r4t/sim_card').read_bytes()
        trace=(args.setting/'error.log').read_text()
        check_setting(trace,before,after,not args.disabled)
        check_upload_handoffs(trace,1)
        cold=(args.cold/'error.log').read_text()
        if args.disabled:
            if statuses(cold,0x20) or re.search(r'nsk3_pin_contact:|SIM status ins=(24|26|28|2c)',cold):
                raise ValueError('cold disabled card unexpectedly authenticated or mutated credentials')
            if re.search(r'illegal|unsupported|unhandled|fatal|\[LUA ERROR\]',cold,re.I):
                raise ValueError('cold execution/observer error')
        else:check_pin(cold,True)
        check_upload_handoffs(cold,1)
        if (args.cold/'initial_sim_card.bin').read_bytes()!=after or (args.cold/'nvram/nsk3r4t/sim_card').read_bytes()!=after:
            raise ValueError('cold request-enabled card changed')
        for run in (args.setting,args.cold):
            if (run/'nvram/nsk3r4t/flash').read_bytes()!=image:
                raise ValueError('own executable flash changed')
        if args.disabled:
            frame_check(args.setting/'snap/nsk3_pin_request_off_31.png',OFF)
            for time in (5,15):frame_check(args.cold/f'snap/nsk3_boot_{time}.png',PHONE_PROMPT)
        else:
            frame_check(args.setting/'snap/nsk3_settings_24.png',ON)
            frame_check(args.cold/'snap/nsk3_pin_5.png',PIN)
            frame_check(args.cold/'snap/nsk3_pin_17.png',PHONE_PROMPT)
    except (ValueError,OSError) as error:
        print(f'NSK-3 PIN request failed: {error}',file=sys.stderr)
        return 1
    print('NSK-3 physical PIN request setting/cold challenge behavior PASS')
    return 0


if __name__=='__main__':raise SystemExit(main())
