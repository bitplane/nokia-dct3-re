"""Qualify physical Phone erase acknowledgement and preserved SIM cold readback."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_security_check import check_trace
from tools.nsm1_published_record_check import records
from tools.nsm1_v523_static_check import SHA1
from tools.nsm1_phonebook_name_check import CLEAN
from tools.nsk3_phonebook_check import check_card,transmitted,READ_KEYS,DETAIL
from tools.nsk1_phonebook_roundtrip import frame_check

KEYS=(0x1a,0x18,0x18,0x19,0x18,0x19,0x19,0x19,1,2,3,4,5,0x19)
ERASED='9a7d831591e1f782e46910bdcdb0bc4d80ccf1d969a040925968b6037a7dade8'


def check_erase(trace):
    check_trace(trace,True,KEYS,('313233343500ffff',))
    if bytes.fromhex('a0dc') in transmitted(trace,4096,'nsm1_sim_tx') or 'SIM status ins=dc ' in trace:
        raise ValueError('Phone erase attempted a SIM UPDATE RECORD')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('erase',type=Path);parser.add_argument('cold',type=Path)
    args=parser.parse_args()
    try:
        for run in (args.erase,args.cold):
            trace=(run/'error.log').read_text();records(trace,True)
            if run==args.erase:check_erase(trace)
            else:check_trace(trace,True,READ_KEYS)
            storage=run/'nvram/nsm1r4t'
            card=(storage/'sim_card').read_bytes();check_card(card)
            if card!=(run/'initial_sim_card.bin').read_bytes():raise ValueError('SIM card changed')
            if hashlib.sha1((storage/'flash').read_bytes()).hexdigest()!=SHA1:
                raise ValueError('own flash changed')
        before=(args.erase/'initial_eeprom.bin').read_bytes()
        after=(args.erase/'nvram/nsm1r4t/eeprom').read_bytes()
        if before[:64]!=after[:64] or before[0x3cc:0x3d4]!=after[0x3cc:0x3d4]:
            raise ValueError('identity/record/security inputs changed')
        if before[0x19d4:0x19d4+37]==after[0x19d4:0x19d4+37]:
            raise ValueError('selected EEPROM-backed record did not change')
        if (args.cold/'initial_eeprom.bin').read_bytes()!=after:
            raise ValueError('cold EEPROM input differs')
        if (args.cold/'initial_sim_card.bin').read_bytes()!=(args.erase/'nvram/nsm1r4t/sim_card').read_bytes():
            raise ValueError('cold SIM input differs')
        frame_check(args.erase/'snap/nsm1_erase_phone_30.png',ERASED)
        frame_check(args.cold/'snap/nsm1_phonebook_read_17.png',CLEAN)
        frame_check(args.cold/'snap/nsm1_phonebook_read_20.png',DETAIL)
    except (ValueError,OSError) as error:
        print(f'NSM-1 Phone erase qualification failed: {error}',file=sys.stderr);return 1
    print('NSM-1 physical Phone erase, clean cold A list and unchanged SIM Details PASS')
    return 0


if __name__=='__main__':raise SystemExit(main())
