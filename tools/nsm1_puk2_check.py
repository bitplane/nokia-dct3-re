"""Physical NSM-1 PIN2 exhaustion, PUK2 controls and cold restoration."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_pin2_check import check_change,PREFIX
from tools.nsm1_security_check import check_trace as check_phone
from tools.nsm1_published_record_check import records
from tools.nsm1_v523_static_check import SHA1
from tools.nsk3_phonebook_check import transmitted
from tools.sim_security_trace_check import ATTEMPTS_OFFSET,CHV_OFFSET,padded_pin,statuses
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH


def recovered_card(before,accepted):
    if len(before)!=CURRENT_NVRAM_LENGTH or before[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+3]!=bytes((3,0,10)):
        raise ValueError('PIN2 must be physically exhausted')
    if not 1<=before[ATTEMPTS_OFFSET+3]<=10:raise ValueError('PUK2 retry count differs')
    expected=bytearray(before)
    if accepted:
        expected[CHV_OFFSET+8:CHV_OFFSET+16]=padded_pin('8765')
        expected[ATTEMPTS_OFFSET+1]=3;expected[ATTEMPTS_OFFSET+3]=10
    else:expected[ATTEMPTS_OFFSET+3]-=1
    return bytes(expected)


def recovery(trace,before,after,accepted):
    puk='87654321' if accepted else '87654322'
    keys=PREFIX+tuple(map(int,puk))+(0x19,)+(8,7,6,5,0x19)*2
    check_phone(trace,True,keys)
    pin_contacts=re.findall(r'nsm1_pin_contact: correct=1 key=([0-9a-f]+) down=([01]) ',trace)
    if pin_contacts!=[(f'{key:02x}',down) for key in (1,2,3,4,0x19) for down in ('1','0')]:
        raise ValueError('startup physical PIN differs')
    if statuses(trace,0x20)!=[(0x9000,3,10)]*(2 if accepted else 1):
        raise ValueError('startup/automatic PIN verification differs')
    result=0x9000 if accepted else 0x9840 if before[ATTEMPTS_OFFSET+3]==1 else 0x9804
    if statuses(trace,0x2c)!=[(result,3,10)]:
        raise ValueError('PUK2 result differs')
    if any(statuses(trace,ins) for ins in (0x24,0x26,0x28)):
        raise ValueError('unexpected credential operation')
    tx=transmitted(trace,4096,'nsm1_sim_tx')
    commands=[bytes.fromhex('a020000108')+padded_pin('1234'),
              bytes.fromhex('a02c000210')+puk.encode()+padded_pin('8765')]
    automatic=bytes.fromhex('a020000208')+padded_pin('8765')
    if accepted:commands.append(automatic)
    elif automatic in tx:raise ValueError('failed recovery verified PIN2')
    if any(tx.count(command)!=1 for command in commands):raise ValueError('recovery APDU differs')
    positions=[tx.index(command) for command in commands]
    if positions!=sorted(positions):raise ValueError('recovery command order differs')
    if trace.index('SIM status ins=20')>trace.index('nsm1_security_submit:') or trace.index('nsm1_security_accepted:')>trace.index('SIM status ins=2c'):
        raise ValueError('authentication order differs')
    if after!=recovered_card(before,accepted):raise ValueError('whole-card recovery differs')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('first','second','exhausted','wrong','recovered','cold'):
        parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        cases=((args.second,args.first,'exhaust'),(args.exhausted,args.second,'exhaust'),
               (args.wrong,args.exhausted,'wrong'),(args.recovered,args.exhausted,'recover'),
               (args.cold,args.recovered,'cold'))
        for run,parent,mode in cases:
            before=(run/'initial_sim_card.bin').read_bytes();storage=run/'nvram/nsm1r4t'
            if before!=(parent/'nvram/nsm1r4t/sim_card').read_bytes():raise ValueError('cold card link differs')
            if (run/'initial_eeprom.bin').read_bytes()!=(parent/'nvram/nsm1r4t/eeprom').read_bytes():
                raise ValueError('cold EEPROM link differs')
            trace=(run/'error.log').read_text();after=(storage/'sim_card').read_bytes()
            if mode=='exhaust':check_change(trace,before,after,'5675','8765',False)
            elif mode=='cold':check_change(trace,before,after,'8765','5678',True)
            else:recovery(trace,before,after,mode=='recover')
            records(trace,True)
            if hashlib.sha1((storage/'flash').read_bytes()).hexdigest()!=SHA1:raise ValueError('own flash changed')
        if (args.exhausted/'nvram/nsm1r4t/sim_card').read_bytes()[ATTEMPTS_OFFSET+1]!=0:
            raise ValueError('PIN2 did not reach zero')
    except (ValueError,OSError) as error:
        print(f'NSM-1 PUK2 qualification failed: {error}',file=sys.stderr);return 1
    print('NSM-1 physical PIN2 exhaustion, PUK2 rejection/recovery and cold PIN2 restore PASS')
    return 0


if __name__=='__main__':raise SystemExit(main())
