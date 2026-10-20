"""Physical NSM-1 PIN2-protected FDN save, wrong-PIN control and cold read."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_security_check import check_trace as check_phone
from tools.nsm1_published_record_check import records
from tools.nsm1_v523_static_check import SHA1
from tools.nsk3_phonebook_check import transmitted
from tools.sim_security_trace_check import statuses,ATTEMPTS_OFFSET,padded_pin
from tools.sim_fdn_profile import EXTENDED_LENGTH
from tools.nsk1_phonebook_roundtrip import frame_check

PREFIX=(0x19,4,5,3,0x19,0x18,0x19,0x18,0x19)
RECORD=bytes.fromhex('41ffffffffffffffffffffffffffffffffff04812143f5ffffffffffffffffff')
READ_KEYS=(0x19,4,5,3,0x19,0x18,0x19,0x19,0x19,0x19)
SAVED='fadcc0b3b9dbabd90d23602e2b3d698ddb88606a2bdd25748a009ae499d2f402'
LIST='dc8f66d7ea525fefa7cae88569faa87e6fc1cabb98d59f1700aac6e1be851fd3'
DETAILS='a089a5b358dd43b2f090d972dadb617c935180f95923efa43f99e9e330c055d0'
EDITED_DETAILS='a05b707808f144e8ba04fada27a7ed5557521a51c73497beb2ef0756f053836e'
ERROR='fd5b8fb1ab0c3397bf958d567a46aee84a72f75123802a00d5bc802a1814543e'


def changed_card(before,wrong=False):
    if len(before)!=EXTENDED_LENGTH or before[3578:3610]!=b'\xff'*32 or before[ATTEMPTS_OFFSET+1]!=3:
        raise ValueError('fresh FDN slot and PIN2 retries required')
    expected=bytearray(before)
    if wrong:expected[ATTEMPTS_OFFSET+1]=2
    else:expected[3578:3610]=RECORD
    return bytes(expected)


def edited_card(before):
    if len(before)!=EXTENDED_LENGTH or before[3578:3610]!=RECORD:raise ValueError('saved A/12345 FDN record required')
    expected=bytearray(before);expected[3600]=0xf6
    return bytes(expected)


def check_authentication(trace,keys,results):
    check_phone(trace,True,keys)
    contacts=re.findall(r'nsm1_pin_contact: correct=1 key=([0-9a-f]+) down=([01]) ',trace)
    if contacts!=[(f'{key:02x}',down) for key in (1,2,3,4,0x19) for down in ('1','0')]:
        raise ValueError('physical startup PIN differs')
    if trace.index('SIM status ins=20 sw=9000')>trace.index('nsm1_security_submit:'):
        raise ValueError('startup authentication order differs')
    application=list(re.finditer(r'nsm1_security_contact: key=[0-9a-f]+ down=1 ',trace))
    if application[6].start()<trace.index('nsm1_security_accepted:'):raise ValueError('early application input')
    if statuses(trace,0x20)!=results:raise ValueError('PIN1/PIN2 results differ')


def check_trace(trace,mode):
    if mode not in ('write','wrong','cold'):raise ValueError('unknown FDN record phase')
    wrong=mode=='wrong'
    keys=READ_KEYS if mode=='cold' else PREFIX+(5,6,7,5 if wrong else 8,0x19)
    if mode=='write':keys+=(2,0x19,1,2,3,4,5,0x19)
    results=[(0x9000,3,10)]
    if mode!='cold':results.append((0x9804 if wrong else 0x9000,3,10))
    check_authentication(trace,keys,results)
    if any(statuses(trace,ins) for ins in (4,0x44,0x24,0x26,0x28,0x2c)):
        raise ValueError('unexpected validity/credential operation')
    tx=transmitted(trace,4096,'nsm1_sim_tx')
    if tx.count(bytes.fromhex('a0a40000026f3b'))<1:raise ValueError('FDN selection absent')
    if tx.count(bytes.fromhex('a020000108')+padded_pin('1234'))!=1:raise ValueError('startup PIN APDU differs')
    if mode!='cold':
        pin=bytes.fromhex('a020000208')+padded_pin('5675' if wrong else '5678')
        if tx.count(pin)!=1:raise ValueError('PIN2 payload differs')
    if statuses(trace,0xdc)!=([(0x9000,3,10)] if mode=='write' else []):raise ValueError('record update result differs')
    command=bytes.fromhex('a0dc010420')+RECORD
    if mode=='write':
        if tx.count(command)!=1 or tx.index(command)<tx.index(pin):raise ValueError('protected FDN update differs')
    elif bytes.fromhex('a0dc') in tx:raise ValueError('read/rejection rewrote a record')


def check_edit(trace,cold=False):
    keys=READ_KEYS if cold else READ_KEYS+(0x19,5,6,7,8,0x19,0x19,0x19,0x1a,6,0x19)
    check_authentication(trace,keys,[(0x9000,3,10)]*(1 if cold else 2))
    if any(statuses(trace,ins) for ins in (4,0x44,0x24,0x26,0x28,0x2c)):
        raise ValueError('unexpected edit validity/credential operation')
    tx=transmitted(trace,4096,'nsm1_sim_tx')
    if statuses(trace,0xdc)!=([] if cold else [(0x9000,3,10)]):raise ValueError('edit update result differs')
    if tx.count(bytes.fromhex('a020000108')+padded_pin('1234'))!=1:raise ValueError('edit startup PIN differs')
    pin2=bytes.fromhex('a020000208')+padded_pin('5678')
    if tx.count(pin2)!=(0 if cold else 1):raise ValueError('edit PIN2 differs')
    if cold:
        if bytes.fromhex('a0dc') in tx:raise ValueError('cold edited record was rewritten')
    else:
        body=bytearray(RECORD);body[22]=0xf6
        command=bytes.fromhex('a0dc010420')+body
        if tx.count(command)!=1 or tx.index(command)<tx.index(pin2):raise ValueError('in-place edit APDU differs')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('write','cold','wrong'):parser.add_argument(name,type=Path)
    parser.add_argument('--edit',type=Path);parser.add_argument('--edit-cold',type=Path)
    args=parser.parse_args()
    try:
        initial=(args.write/'initial_sim_card.bin').read_bytes();saved=changed_card(initial)
        for run,mode in ((args.write,'write'),(args.cold,'cold'),(args.wrong,'wrong')):
            before=(run/'initial_sim_card.bin').read_bytes();storage=run/'nvram/nsm1r4t'
            if before!=(saved if mode=='cold' else initial):raise ValueError('independent card input differs')
            expected=saved if mode!='wrong' else changed_card(initial,True)
            if (storage/'sim_card').read_bytes()!=expected:raise ValueError('whole-card transition differs')
            trace=(run/'error.log').read_text();check_trace(trace,mode);records(trace,True)
            if hashlib.sha1((storage/'flash').read_bytes()).hexdigest()!=SHA1:raise ValueError('own flash differs')
        if (args.cold/'initial_eeprom.bin').read_bytes()!=(args.write/'nvram/nsm1r4t/eeprom').read_bytes():raise ValueError('cold EEPROM differs')
        if (args.wrong/'initial_eeprom.bin').read_bytes()!=(args.write/'initial_eeprom.bin').read_bytes():raise ValueError('negative EEPROM input differs')
        frame_check(args.write/'snap/nsm1_fdn_write_39.png',SAVED)
        frame_check(args.cold/'snap/nsm1_fdn_read_28.png',LIST)
        frame_check(args.cold/'snap/nsm1_fdn_read_30.png',DETAILS)
        frame_check(args.wrong/'snap/nsm1_fdn_write_33.png',ERROR)
        if bool(args.edit)!=bool(args.edit_cold):raise ValueError('edit and edit-cold must be paired')
        if args.edit:
            updated=edited_card(saved);previous=args.cold
            for run,cold in ((args.edit,False),(args.edit_cold,True)):
                storage=run/'nvram/nsm1r4t'
                if (run/'initial_sim_card.bin').read_bytes()!=(saved if not cold else updated):raise ValueError('edit cold card input differs')
                if (storage/'sim_card').read_bytes()!=updated:raise ValueError('edit changed other card bytes')
                if (run/'initial_eeprom.bin').read_bytes()!=(previous/'nvram/nsm1r4t/eeprom').read_bytes():raise ValueError('edit cold EEPROM link differs')
                trace=(run/'error.log').read_text();check_edit(trace,cold);records(trace,True)
                if hashlib.sha1((storage/'flash').read_bytes()).hexdigest()!=SHA1:raise ValueError('edit flash differs')
                previous=run
            frame_check(args.edit/'snap/nsm1_fdn_read_38.png',SAVED)
            frame_check(args.edit_cold/'snap/nsm1_fdn_read_30.png',EDITED_DETAILS)
    except (ValueError,OSError) as error:
        print(f'NSM-1 FDN record qualification failed: {error}',file=sys.stderr);return 1
    print('NSM-1 physical PIN2-protected FDN save, cold read and wrong-PIN preservation PASS')
    if args.edit:print('NSM-1 physical in-place FDN edit and unchanged cold12346 read PASS')
    return 0


if __name__=='__main__':raise SystemExit(main())
