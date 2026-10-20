"""Physical NSM-1 A/12345 SIM save and independent cold Details readback."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_security_check import check_trace
from tools.nsm1_published_record_check import records
from tools.nsm1_v523_static_check import SHA1
from tools.nsk3_phonebook_check import check_card,expected_record,transmitted,DETAIL,SAVE_KEYS,READ_KEYS
from tools.nsk1_phonebook_roundtrip import frame_check


def check_streams(write,cold):
    update=bytes.fromhex('a0dc010420')
    streams=[transmitted(trace,4096,'nsm1_sim_tx') for trace in (write,cold)]
    if streams[0].count(update+expected_record())!=1 or update in streams[1]:
        raise ValueError('physical write/read-only cold APDU contrast differs')
    if write.count('SIM status ins=dc sw=9000')!=1 or 'SIM status ins=dc ' in cold:
        raise ValueError('UPDATE RECORD result differs')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('write',type=Path);parser.add_argument('cold',type=Path)
    args=parser.parse_args()
    try:
        traces=[];cards=[]
        for run,keys in ((args.write,SAVE_KEYS),(args.cold,READ_KEYS)):
            trace=(run/'error.log').read_text();traces.append(trace)
            check_trace(trace,True,keys);records(trace,True)
            storage=run/'nvram/nsm1r4t'
            if hashlib.sha1((storage/'flash').read_bytes()).hexdigest()!=SHA1:
                raise ValueError('own flash changed')
            card=(storage/'sim_card').read_bytes();check_card(card);cards.append(card)
        check_streams(*traces)
        if cards[0]!=cards[1] or (args.cold/'initial_sim_card.bin').read_bytes()!=cards[0]:
            raise ValueError('cold card input/storage differs')
        if (args.cold/'initial_eeprom.bin').read_bytes()!=(args.write/'nvram/nsm1r4t/eeprom').read_bytes():
            raise ValueError('cold EEPROM input differs')
        frame_check(args.cold/'snap/nsm1_phonebook_read_20.png',DETAIL)
    except (ValueError,OSError) as error:
        print(f'NSM-1 phonebook qualification failed: {error}',file=sys.stderr);return 1
    print('NSM-1 physical A/12345 SIM save and cold Details readback PASS; selected-name/Phone-row coverage has a separate gate')
    return 0


if __name__=='__main__':raise SystemExit(main())
