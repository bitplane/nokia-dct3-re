"""Physical NSM-1 PIN1 retry persistence and cold authentication, not phone unlock."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_pin_check import check_trace,PIN,ERROR
from tools.nsm1_analog_check import FRAME as PHONE_PROMPT
from tools.nsm1_published_record_check import records
from tools.nsm1_v523_static_check import SHA1
from tools.sim_security_trace_check import ATTEMPTS_OFFSET,CHV_OFFSET,ENABLED_OFFSET,padded_pin
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH
from tools.nsk1_phonebook_roundtrip import frame_check


def check_cards(before,wrong,cold,correct):
    if (len(before)!=CURRENT_NVRAM_LENGTH or before[CHV_OFFSET:CHV_OFFSET+8]!=padded_pin('1234')
            or before[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4]!=bytes((3,3,10,10)) or before[ENABLED_OFFSET]!=1):
        raise ValueError('PIN-enabled lab card input differs')
    expected=bytearray(before);expected[ATTEMPTS_OFFSET]=2
    if wrong!=expected or cold!=wrong:raise ValueError('wrong PIN/cold input changed other card bytes')
    if correct!=before:raise ValueError('correct PIN did not restore exactly the initial card')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('wrong',type=Path);parser.add_argument('correct',type=Path)
    args=parser.parse_args()
    try:
        for run,correct in ((args.wrong,False),(args.correct,True)):
            trace=(run/'error.log').read_text()
            check_trace(trace,correct,prefix='nsm1',tx_limit=4096);records(trace,True)
            if 'nsm1_security_accepted:' in trace:raise ValueError('PIN-only fixture performed phone unlock')
            if hashlib.sha1((run/'nvram/nsm1r4t/flash').read_bytes()).hexdigest()!=SHA1:
                raise ValueError('own executable flash changed')
            frame_check(run/'snap/nsm1_pin_5.png',PIN)
        check_cards((args.wrong/'initial_sim_card.bin').read_bytes(),
                    (args.wrong/'nvram/nsm1r4t/sim_card').read_bytes(),
                    (args.correct/'initial_sim_card.bin').read_bytes(),
                    (args.correct/'nvram/nsm1r4t/sim_card').read_bytes())
        if (args.correct/'initial_eeprom.bin').read_bytes()!=(args.wrong/'nvram/nsm1r4t/eeprom').read_bytes():
            raise ValueError('cold EEPROM input differs')
        frame_check(args.wrong/'snap/nsm1_pin_12.png',ERROR)
        frame_check(args.correct/'snap/nsm1_pin_17.png',PHONE_PROMPT)
    except (ValueError,OSError) as error:
        print(f'NSM-1 SIM PIN qualification failed: {error}',file=sys.stderr);return 1
    print('NSM-1 physical wrong PIN, cold retry persistence and correct PIN PASS; phone lock remains separate')
    return 0


if __name__=='__main__':raise SystemExit(main())
