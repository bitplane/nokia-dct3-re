"""Physical PIN2 exhaustion, cold PUK2 prompt/recovery and new-PIN2 authentication."""
import argparse
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_static_check import inspect
from tools.nsk3_pin2_check import check_change,PREFIX,FRAME
from tools.nsk3_security_check import check_trace as check_phone
from tools.nsk3_boot_check import check_upload_handoffs
from tools.nsk3_phonebook_check import transmitted
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_security_trace_check import CHV_OFFSET,ATTEMPTS_OFFSET,AOC_NVRAM_SIZE,statuses,padded_pin

BLOCKED='5e2f7fb2de8b983abd2f4cb3f6dc4591bc810b859df1ce6d5f89e8fe675e68d8'
PROMPT='4714aaf1a23d14b5d10638facad9d9aa4b4296c2e4ff33e8f7707957630230a9'
KEYS=PREFIX+(8,7,6,5,4,3,2,1,0x19)+(8,7,6,5,0x19)*2


def recovery(trace,before,after):
    check_phone(trace,True,KEYS)
    if statuses(trace,0x20)!=[(0x9000,3,10)]*2 or statuses(trace,0x2c)!=[(0x9000,3,10)]:
        raise ValueError('startup PIN1 or UNBLOCK CHV2 result differs')
    if any(statuses(trace,ins) for ins in (0x24,0x26,0x28)):
        raise ValueError('unexpected credential operation during recovery')
    tx=transmitted(trace)
    commands=(bytes.fromhex('a020000108')+padded_pin('1234'),
              bytes.fromhex('a02c000210')+b'87654321'+padded_pin('8765'),
              bytes.fromhex('a020000208')+padded_pin('8765'))
    if any(tx.count(command)!=1 for command in commands):
        raise ValueError('physical PUK2/new-PIN2 and automatic VERIFY payloads differ')
    positions=[tx.index(command) for command in commands]
    if positions!=sorted(positions):raise ValueError('PUK2 recovery/VERIFY ordering differs')
    if len(before)!=AOC_NVRAM_SIZE or before[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4]!=bytes((3,0,10,10)):
        raise ValueError('recovery input is not the exhausted lab PIN2 card')
    expected=bytearray(before)
    expected[CHV_OFFSET+8:CHV_OFFSET+16]=padded_pin('8765')
    expected[ATTEMPTS_OFFSET+1]=3
    if after!=expected:raise ValueError('PUK2 recovery changed other persistent card bytes')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','first','second','blocked','prompt','recovered','cold'):
        parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        _,image=inspect(args.source,'boot4')
        previous=None
        for count,run in zip((3,2,1),(args.first,args.second,args.blocked)):
            before=(run/'initial_sim_card.bin').read_bytes()
            after=(run/'nvram/nsk3r4t/sim_card').read_bytes()
            if len(before)!=AOC_NVRAM_SIZE or before[ATTEMPTS_OFFSET+1]!=count or (previous is not None and before!=previous):
                raise ValueError('physical exhaustion cold chain differs')
            check_change((run/'error.log').read_text(),before,after,'5675','8765',False)
            previous=after
        for run in (args.prompt,args.recovered):
            if (run/'initial_sim_card.bin').read_bytes()!=previous:
                raise ValueError('blocked card not preserved into separate cold process')
        prompt_trace=(args.prompt/'error.log').read_text()
        check_phone(prompt_trace,True,PREFIX)
        if statuses(prompt_trace,0x20)!=[(0x9000,3,10)] or any(statuses(prompt_trace,ins) for ins in (0x24,0x2c,0x26,0x28)):
            raise ValueError('cold PUK2 prompt performed unexpected credential mutation')
        if (args.prompt/'nvram/nsk3r4t/sim_card').read_bytes()!=previous:
            raise ValueError('cold prompt changed blocked card')
        recovered=(args.recovered/'nvram/nsk3r4t/sim_card').read_bytes()
        recovery((args.recovered/'error.log').read_text(),previous,recovered)
        if (args.cold/'initial_sim_card.bin').read_bytes()!=recovered:
            raise ValueError('cold authentication did not receive recovered card')
        check_change((args.cold/'error.log').read_text(),recovered,
                     (args.cold/'nvram/nsk3r4t/sim_card').read_bytes(),'8765','5678')
        for run in (args.first,args.second,args.blocked,args.prompt,args.recovered,args.cold):
            check_upload_handoffs((run/'error.log').read_text(),1)
            if (run/'nvram/nsk3r4t/flash').read_bytes()!=image:raise ValueError('own executable flash changed')
        frame_check(args.blocked/'snap/nsk3_pin2_40.png',BLOCKED)
        frame_check(args.prompt/'snap/nsk3_pin2_30.png',PROMPT)
        frame_check(args.recovered/'snap/nsk3_pin2_47.png',FRAME)
        frame_check(args.cold/'snap/nsk3_pin2_47.png',FRAME)
    except (ValueError,OSError) as error:
        print(f'NSK-3 PUK2 qualification failed: {error}',file=sys.stderr)
        return 1
    print('Physical PIN2 exhaustion, cold PUK2 prompt/recovery and new-PIN2 authentication PASS')
    return 0


if __name__=='__main__':raise SystemExit(main())
