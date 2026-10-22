"""Physical NSK-3 wrong PIN and cold correct PIN, distinct from phone lock."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import statuses,ATTEMPTS_OFFSET,AOC_NVRAM_SIZE,NVRAM_SIZE
from tools.nsk3_static_check import inspect
from tools.nsk3_boot_check import check_upload_handoffs
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.nsk3_menu_check import PROMPT as PHONE_PROMPT
from tools.nsk3_phonebook_check import transmitted

PIN='a12b3c3fdb4a3d1e8ffeb5fc8986c3bc008bb86b1f2e927440848fe0420ffc0c'
ERROR='fd5b8fb1ab0c3397bf958d567a46aee84a72f75123802a00d5bc802a1814543e'


def check_trace(trace,correct,pin=None,prefix='nsk3',tx_limit=1024,allowed_mutations=(),attempts_before=3):
    if not 1<=attempts_before<=3:
        raise ValueError('requires live PIN1 attempts')
    pin=pin if pin is not None else (b'1234' if correct else b'4321')
    keys=tuple(int(chr(digit)) for digit in pin)+(0x19,)
    rows=re.findall(re.escape(prefix)+r'_pin_contact: correct=(\d) key=([0-9a-f]+) down=([01]) ',trace)
    if rows!=[(str(int(correct)),f'{key:02x}',down) for key in keys for down in ('1','0')]:
        raise ValueError('physical PIN contacts differ')
    remaining=3 if correct else attempts_before-1
    status=0x9000 if correct else 0x9840 if remaining==0 else 0x9804
    if statuses(trace,0x20)!=[(status,remaining,10)]:
        raise ValueError('VERIFY status/retry transition differs')
    if any(statuses(trace,ins) for ins in (0x24,0x26,0x28,0x2c) if ins not in allowed_mutations):
        raise ValueError('unexpected credential mutation')
    tx=transmitted(trace,tx_limit,prefix+'_sim_tx')
    command=bytes.fromhex('a020000108')+pin+b'\xff'*(8-len(pin))
    if tx.count(command)!=1:raise ValueError('physical VERIFY APDU differs')
    if re.search(r'illegal|unsupported|unhandled|fatal|\[LUA ERROR\]',trace,re.I):
        raise ValueError('execution/observer error')


def check_cards(wrong,initial,correct):
    expected=bytearray(make_profile(True).ljust(AOC_NVRAM_SIZE,b'\xff'))
    expected[ATTEMPTS_OFFSET]=2
    # The shared generator emits the legacy prefix; the device appends its
    # later PLMN/SMSS/AoC storage defaults on serialization.
    expected[NVRAM_SIZE:NVRAM_SIZE+3]=bytes.fromhex('00f110')
    if wrong!=expected or initial!=wrong:
        raise ValueError('wrong retry or cold input persistence differs')
    expected=bytearray(wrong)
    expected[ATTEMPTS_OFFSET]=3
    if correct!=expected:
        raise ValueError('correct PIN changed more than restoring the retry count')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','wrong','correct'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        _,image=inspect(args.source,'boot4')
        for run,correct in ((args.wrong,False),(args.correct,True)):
            trace=(run/'error.log').read_text()
            check_trace(trace,correct)
            check_upload_handoffs(trace,1)
            if (run/'nvram/nsk3r4t/flash').read_bytes()!=image:
                raise ValueError('own executable flash changed')
            frame_check(run/'snap/nsk3_pin_5.png',PIN)
        check_cards((args.wrong/'nvram/nsk3r4t/sim_card').read_bytes(),
                    (args.correct/'initial_sim_card.bin').read_bytes(),
                    (args.correct/'nvram/nsk3r4t/sim_card').read_bytes())
        frame_check(args.wrong/'snap/nsk3_pin_12.png',ERROR)
        frame_check(args.correct/'snap/nsk3_pin_17.png',PHONE_PROMPT)
    except (ValueError,OSError) as error:
        print(f'NSK-3 SIM PIN failed: {error}',file=sys.stderr)
        return 1
    print('NSK-3 physical wrong PIN/cold retry/correct PIN PASS; phone lock separate')
    return 0


if __name__=='__main__':raise SystemExit(main())
