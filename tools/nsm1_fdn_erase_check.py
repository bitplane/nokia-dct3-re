"""Physical PIN2-protected NSM-1 FDN cancel/erase and cold empty search."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_fdn_record_check import check_authentication,RECORD,LIST
from tools.nsm1_published_record_check import records
from tools.nsm1_v523_static_check import SHA1
from tools.nsk3_phonebook_check import transmitted
from tools.sim_security_trace_check import statuses,padded_pin
from tools.sim_fdn_profile import EXTENDED_LENGTH
from tools.nsk1_phonebook_roundtrip import frame_check

READ_KEYS=(0x19,4,5,3,0x19,0x18,0x19,0x19,0x19,0x19)
CONFIRM='2501be7ff54218d5a541e0ae7c0d2a19430bfefe4b550b3d46942d172d88606c'
ERASED='1d50b5d87e6aa4851f3734f00495aa9b91298c3418166d9c952359fca1d44d27'
EMPTY='ad7a1d617d2b9646c17bdcddec9e3a313dc2acc9932ac6e24c455beb33a8267d'


def erased_card(card):
    if len(card)!=EXTENDED_LENGTH or card[3578:3610]!=RECORD:raise ValueError('saved A FDN record required')
    expected=bytearray(card);expected[3578:3610]=b'\xff'*32
    return bytes(expected)


def check_case(trace,mode):
    if mode not in ('cancel','erase','cold'):raise ValueError('unknown erase phase')
    keys=READ_KEYS
    if mode!='cold':keys+=(0x19,5,6,7,8,0x19,0x18,0x19,0x1a if mode=='cancel' else 0x19)
    check_authentication(trace,keys,[(0x9000,3,10)]*(1 if mode=='cold' else 2))
    if any(statuses(trace,ins) for ins in (4,0x44,0x24,0x26,0x28,0x2c)):
        raise ValueError('unexpected validity/credential operation')
    tx=transmitted(trace,4096,'nsm1_sim_tx')
    if tx.count(bytes.fromhex('a020000108')+padded_pin('1234'))!=1:raise ValueError('startup PIN APDU differs')
    pin2=bytes.fromhex('a020000208')+padded_pin('5678')
    if tx.count(pin2)!=(0 if mode=='cold' else 1):raise ValueError('PIN2 APDU differs')
    if statuses(trace,0xdc)!=([(0x9000,3,10)] if mode=='erase' else []):raise ValueError('erase outcome differs')
    command=bytes.fromhex('a0dc010420')+b'\xff'*32
    if mode=='erase':
        if tx.count(command)!=1 or tx.index(command)<tx.index(pin2):raise ValueError('protected erase command differs')
    elif bytes.fromhex('a0dc') in tx:raise ValueError('cancel/cold rewrote a record')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('saved','cancel','erase','cold'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        saved=(args.saved/'nvram/nsm1r4t/sim_card').read_bytes();empty=erased_card(saved)
        previous=args.saved
        for run,mode in ((args.cancel,'cancel'),(args.erase,'erase'),(args.cold,'cold')):
            storage=run/'nvram/nsm1r4t'
            if (run/'initial_sim_card.bin').read_bytes()!=(previous/'nvram/nsm1r4t/sim_card').read_bytes():raise ValueError('cold card link differs')
            if (run/'initial_eeprom.bin').read_bytes()!=(previous/'nvram/nsm1r4t/eeprom').read_bytes():raise ValueError('cold EEPROM link differs')
            if (storage/'sim_card').read_bytes()!=(saved if mode=='cancel' else empty):raise ValueError('whole-card erase/cancel transition differs')
            trace=(run/'error.log').read_text();check_case(trace,mode);records(trace,True)
            if hashlib.sha1((storage/'flash').read_bytes()).hexdigest()!=SHA1:raise ValueError('own flash differs')
            previous=run
        for run in (args.cancel,args.erase):frame_check(run/'snap/nsm1_fdn_read_35.png',CONFIRM)
        frame_check(args.cancel/'snap/nsm1_fdn_read_36.png',LIST)
        frame_check(args.erase/'snap/nsm1_fdn_read_36.png',ERASED)
        frame_check(args.cold/'snap/nsm1_fdn_read_28.png',EMPTY)
    except (ValueError,OSError) as error:
        print(f'NSM-1 FDN erase failed: {error}',file=sys.stderr);return 1
    print('NSM-1 physical PIN2-protected FDN cancel/erase and unchanged cold empty search PASS')
    return 0


if __name__=='__main__':raise SystemExit(main())
