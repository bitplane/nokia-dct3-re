"""Authenticated NSM-1 offline SMS cancel/delete and cold empty Outbox."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_pin_check import check_authenticated
from tools.nsm1_published_record_check import records
from tools.nsm1_v523_static_check import SHA1
from tools.nsk3_sms_delete_check import deleted_card,CONFIRM,EMPTY,READ_KEYS,READ,DRAFT
from tools.nsk3_phonebook_check import transmitted
from tools.sim_security_trace_check import statuses
from tools.nsk1_phonebook_roundtrip import frame_check


def check_case(trace,mode):
    if mode not in ('cancel','erase','cold'):raise ValueError('unknown SMS erase phase')
    keys=READ_KEYS[:-1] if mode=='cold' else READ_KEYS+((0x19,0x19,0x1a) if mode=='cancel' else (0x19,)*3)
    check_authenticated(trace,keys)
    tx=transmitted(trace,4096,'nsm1_sim_tx')
    if statuses(trace,0xdc)!=([(0x9000,3,10)] if mode=='erase' else []):
        raise ValueError('erase/cancel/read-only result differs')
    command=bytes.fromhex('a0dc0104b0')+bytes(1)+DRAFT[1:]
    if mode=='erase':
        if tx.count(command)!=1:raise ValueError('exact free-status update missing')
    elif bytes.fromhex('a0dc') in tx:raise ValueError('cancel/cold unexpectedly rewrote a card record')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('saved','cancel','erase','cold'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        saved=(args.saved/'nvram/nsm1r4t/sim_card').read_bytes();deleted=deleted_card(saved)
        previous_eeprom=(args.saved/'nvram/nsm1r4t/eeprom').read_bytes()
        for run,mode,initial,final in ((args.cancel,'cancel',saved,saved),
                                      (args.erase,'erase',saved,deleted),(args.cold,'cold',deleted,deleted)):
            trace=(run/'error.log').read_text();check_case(trace,mode);records(trace,True)
            storage=run/'nvram/nsm1r4t'
            if (run/'initial_sim_card.bin').read_bytes()!=initial or (storage/'sim_card').read_bytes()!=final:
                raise ValueError('persistent SMS transition differs outside its free-status byte')
            if (run/'initial_eeprom.bin').read_bytes()!=previous_eeprom:raise ValueError('cold EEPROM chain differs')
            previous_eeprom=(storage/'eeprom').read_bytes()
            if hashlib.sha1((storage/'flash').read_bytes()).hexdigest()!=SHA1:raise ValueError('own flash changed')
        for run in (args.cancel,args.erase):frame_check(run/'snap/nsm1_sms_read_26.png',CONFIRM)
        frame_check(args.cancel/'snap/nsm1_sms_read_27.png',READ)
        frame_check(args.cold/'snap/nsm1_sms_read_24.png',EMPTY)
    except (ValueError,OSError) as error:
        print(f'NSM-1 SMS erase qualification failed: {error}',file=sys.stderr);return 1
    print('NSM-1 physical authenticated SMS cancel/erase and unchanged cold empty Outbox PASS')
    return 0


if __name__=='__main__':raise SystemExit(main())
