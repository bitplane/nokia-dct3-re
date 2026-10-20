"""Own NSM-1 physical PIN request Off/On and cold challenge cycle."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_pin_check import check_authenticated
from tools.nsm1_security_check import check_trace as check_phone
from tools.nsm1_published_record_check import records
from tools.nsm1_v523_static_check import SHA1
from tools.nsk3_pin_check import check_trace as check_pin,PIN
from tools.nsk3_pin_request_check import KEYS,ON,OFF
from tools.nsk3_phonebook_check import transmitted
from tools.sim_security_trace_check import ENABLED_OFFSET,statuses,padded_pin
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH
from tools.nsk1_phonebook_roundtrip import frame_check

# Earlier no-PIN startup reaches a different Messages icon animation phase.
DISABLED_MENU='69a8ca954d651cb69b776ed3f0ddf7149d893832a0b8e4414b08795cc64176d8'


def card_transition(before,enabled):
    if len(before)!=CURRENT_NVRAM_LENGTH or before[ENABLED_OFFSET]!=int(not enabled):
        raise ValueError('initial card/request state differs')
    expected=bytearray(before);expected[ENABLED_OFFSET]=int(enabled)
    return bytes(expected)


def setting(trace,enabled):
    instruction=0x28 if enabled else 0x26
    keys=KEYS if enabled else KEYS[:-2]+(0x18,0x19)
    if enabled:
        check_phone(trace,True,keys)
        if statuses(trace,0x20):raise ValueError('disabled input unexpectedly verified PIN')
    else:check_authenticated(trace,keys,allowed_mutations=(instruction,))
    if statuses(trace,instruction)!=[(0x9000,3,10)]:raise ValueError('setting result differs')
    if any(statuses(trace,ins) for ins in (0x24,0x26,0x28,0x2c) if ins!=instruction):
        raise ValueError('unexpected credential operation')
    command=bytes((0xa0,instruction,0,1,8))+padded_pin('1234')
    if transmitted(trace,4096,'nsm1_sim_tx').count(command)!=1:
        raise ValueError('setting APDU differs')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('off','off_cold','on','on_cold'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        previous=None
        for run,enabled,change in ((args.off,False,True),(args.off_cold,False,False),
                                   (args.on,True,True),(args.on_cold,True,False)):
            before=(run/'initial_sim_card.bin').read_bytes()
            storage=run/'nvram/nsm1r4t'
            if previous:
                if before!=(previous/'nvram/nsm1r4t/sim_card').read_bytes():raise ValueError('cold card link differs')
                if (run/'initial_eeprom.bin').read_bytes()!=(previous/'nvram/nsm1r4t/eeprom').read_bytes():
                    raise ValueError('cold EEPROM link differs')
            expected=card_transition(before,enabled) if change else before
            if (storage/'sim_card').read_bytes()!=expected:raise ValueError('whole-card transition differs')
            trace=(run/'error.log').read_text();records(trace,True)
            if change:setting(trace,enabled)
            elif enabled:check_pin(trace,True,prefix='nsm1',tx_limit=4096)
            else:
                check_phone(trace,True)
                if any(statuses(trace,ins) for ins in (0x20,0x24,0x26,0x28,0x2c)):
                    raise ValueError('disabled cold boot authenticated SIM')
            if hashlib.sha1((storage/'flash').read_bytes()).hexdigest()!=SHA1:raise ValueError('own flash changed')
            previous=run
        frame_check(args.off/'snap/nsm1_pin_request_off_33.png',OFF)
        frame_check(args.on/'snap/nsm1_pin_request_on_27.png',ON)
        frame_check(args.off_cold/'snap/nsm1_security_20.png',DISABLED_MENU)
        frame_check(args.on_cold/'snap/nsm1_pin_5.png',PIN)
    except (ValueError,OSError) as error:
        print(f'NSM-1 PIN request failed: {error}',file=sys.stderr);return 1
    print('NSM-1 physical PIN-request Off/On and unchanged cold challenge cycle PASS')
    return 0


if __name__=='__main__':raise SystemExit(main())
