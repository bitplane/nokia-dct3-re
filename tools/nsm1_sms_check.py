"""Authenticated NSM-1 offline A-draft save and independent cold Outbox read."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_pin_check import check_authenticated
from tools.nsm1_published_record_check import records
from tools.nsm1_v523_static_check import SHA1
from tools.nsk3_sms_check import check_card,check_streams,SMS_OFFSET,SMS_RECORD_SIZE,DRAFT,SAVE_KEYS,READ_KEYS,EDITOR,READ
from tools.nsk3_phonebook_check import transmitted
from tools.nsk1_phonebook_roundtrip import frame_check


def check_transition(before,after):
    check_card(after)
    if len(before)!=len(after) or before[SMS_OFFSET:SMS_OFFSET+SMS_RECORD_SIZE]!=b'\0'+b'\xff'*(SMS_RECORD_SIZE-1):
        raise ValueError('draft input is not a fresh empty SMS slot')
    expected=bytearray(before);expected[SMS_OFFSET:SMS_OFFSET+SMS_RECORD_SIZE]=DRAFT
    if after!=expected:raise ValueError('draft save changed other card bytes')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('write',type=Path);parser.add_argument('cold',type=Path)
    args=parser.parse_args()
    try:
        traces=[];streams=[];cards=[]
        for run,keys in ((args.write,SAVE_KEYS),(args.cold,READ_KEYS)):
            trace=(run/'error.log').read_text();traces.append(trace)
            check_authenticated(trace,keys);records(trace,True)
            streams.append(transmitted(trace,4096,'nsm1_sim_tx'))
            storage=run/'nvram/nsm1r4t'
            if hashlib.sha1((storage/'flash').read_bytes()).hexdigest()!=SHA1:
                raise ValueError('own executable changed')
            card=(storage/'sim_card').read_bytes();check_card(card);cards.append(card)
        check_streams(*traces,*streams)
        check_transition((args.write/'initial_sim_card.bin').read_bytes(),cards[0])
        if cards[0]!=cards[1] or (args.cold/'initial_sim_card.bin').read_bytes()!=cards[0]:
            raise ValueError('cold SMS card input/storage differs')
        if (args.cold/'initial_eeprom.bin').read_bytes()!=(args.write/'nvram/nsm1r4t/eeprom').read_bytes():
            raise ValueError('cold EEPROM input differs')
        frame_check(args.write/'snap/nsm1_sms_1.png',EDITOR)
        frame_check(args.cold/'snap/nsm1_sms_read_25.png',READ)
    except (ValueError,OSError) as error:
        print(f'NSM-1 offline SMS qualification failed: {error}',file=sys.stderr);return 1
    print('NSM-1 physical PIN/phone unlock, offline A draft and unchanged cold Outbox read PASS; network SMS unproved')
    return 0


if __name__=='__main__':raise SystemExit(main())
