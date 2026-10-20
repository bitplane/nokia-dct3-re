"""Physical NSM-1 PIN1 retry persistence and cold authentication, not phone unlock."""
import argparse
import hashlib
import re
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
from tools.nsm1_security_check import check_trace as check_phone,MENU


def check_authenticated(trace,following_keys=(0x19,),allowed_mutations=()):
    check_trace(trace,True,prefix='nsm1',tx_limit=4096,allowed_mutations=allowed_mutations)
    check_phone(trace,True,following_keys)
    positions=[trace.index(marker) for marker in ('SIM status ins=20 sw=9000',
               'nsm1_security_submit:','nsm1_security_accepted:')]
    if positions!=sorted(positions):raise ValueError('PIN and phone-code authentication ordering differs')
    presses=[match.start() for match in re.finditer(r'nsm1_security_contact: key=[0-9a-f]+ down=1 ',trace)]
    if len(presses)<7 or presses[6]<=positions[-1]:raise ValueError('application input preceded phone unlock')


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
    parser.add_argument('--menu',type=Path)
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
        if args.menu:
            run=args.menu;trace=(run/'error.log').read_text()
            check_authenticated(trace);records(trace,True)
            storage=run/'nvram/nsm1r4t'
            initial=(run/'initial_sim_card.bin').read_bytes()
            if initial!=(args.correct/'nvram/nsm1r4t/sim_card').read_bytes() or (storage/'sim_card').read_bytes()!=initial:
                raise ValueError('authenticated-menu card input/storage differs')
            if (run/'initial_eeprom.bin').read_bytes()!=(args.correct/'nvram/nsm1r4t/eeprom').read_bytes():
                raise ValueError('authenticated-menu EEPROM input differs')
            if hashlib.sha1((storage/'flash').read_bytes()).hexdigest()!=SHA1:raise ValueError('menu flash changed')
            frame_check(run/'snap/nsm1_pin_menu.png',MENU)
    except (ValueError,OSError) as error:
        print(f'NSM-1 SIM PIN qualification failed: {error}',file=sys.stderr);return 1
    print('NSM-1 physical wrong PIN, cold retry persistence and correct PIN PASS; phone lock remains separate')
    if args.menu:print('NSM-1 composed PIN1/phone-code authentication and Messages menu PASS')
    return 0


if __name__=='__main__':raise SystemExit(main())
