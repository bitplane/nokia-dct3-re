"""Physical NSM-1 ordinary PIN change and independent cold authentication."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_pin_check import check_authenticated
from tools.nsm1_published_record_check import records
from tools.nsm1_v523_static_check import SHA1
from tools.nsk3_pin_check import check_trace as check_pin
from tools.nsk3_pin_change_check import KEYS,CHANGED
from tools.nsk3_phonebook_check import transmitted
from tools.sim_security_trace_check import CHV_OFFSET,ENABLED_OFFSET,padded_pin,statuses
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH
from tools.nsk1_phonebook_roundtrip import frame_check


def check_change(trace,before,after):
    check_authenticated(trace,KEYS,allowed_mutations=(0x24,))
    if statuses(trace,0x24)!=[(0x9000,3,10)]:raise ValueError('CHANGE result differs')
    if any(statuses(trace,ins) for ins in (0x26,0x28,0x2c)):
        raise ValueError('unexpected credential operation')
    command=bytes.fromhex('a024000110')+padded_pin('1234')+padded_pin('4321')
    if transmitted(trace,4096,'nsm1_sim_tx').count(command)!=1:
        raise ValueError('CHANGE credential payload differs')
    if len(before)!=CURRENT_NVRAM_LENGTH or before[ENABLED_OFFSET]!=1 or before[CHV_OFFSET:CHV_OFFSET+8]!=padded_pin('1234'):
        raise ValueError('initial card differs')
    expected=bytearray(before);expected[CHV_OFFSET:CHV_OFFSET+8]=padded_pin('4321')
    if after!=expected:raise ValueError('card changed outside PIN1')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('changed',type=Path);parser.add_argument('cold',type=Path)
    args=parser.parse_args()
    try:
        saved=(args.changed/'nvram/nsm1r4t/sim_card').read_bytes()
        check_change((args.changed/'error.log').read_text(),(args.changed/'initial_sim_card.bin').read_bytes(),saved)
        cold=(args.cold/'error.log').read_text()
        check_pin(cold,True,b'4321',prefix='nsm1',tx_limit=4096)
        if (args.cold/'initial_sim_card.bin').read_bytes()!=saved or (args.cold/'nvram/nsm1r4t/sim_card').read_bytes()!=saved:
            raise ValueError('cold card not preserved')
        if (args.cold/'initial_eeprom.bin').read_bytes()!=(args.changed/'nvram/nsm1r4t/eeprom').read_bytes():
            raise ValueError('cold EEPROM input differs')
        for run in (args.changed,args.cold):
            records((run/'error.log').read_text(),True)
            if hashlib.sha1((run/'nvram/nsm1r4t/flash').read_bytes()).hexdigest()!=SHA1:
                raise ValueError('own flash changed')
        frame_check(args.changed/'snap/nsm1_pin_change_39.png',CHANGED)
    except (ValueError,OSError) as error:
        print(f'NSM-1 PIN change failed: {error}',file=sys.stderr);return 1
    print('NSM-1 ordinary physical PIN change and unchanged cold authentication PASS')
    return 0


if __name__=='__main__':raise SystemExit(main())
