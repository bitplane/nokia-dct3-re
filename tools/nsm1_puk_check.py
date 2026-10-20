"""Own NSM-1 physical PIN exhaustion, PUK controls and cold new-PIN check."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_pin_check import check_trace as check_pin
from tools.nsk3_phonebook_check import transmitted
from tools.nsm1_published_record_check import records
from tools.nsm1_v523_static_check import SHA1
from tools.sim_security_trace_check import statuses,ATTEMPTS_OFFSET,CHV_OFFSET,padded_pin


def recovery(trace,wrong=False):
    keys=(1,2,3,4,5,6,7,9 if wrong else 8,0x19)+(4,3,2,1,0x19)*2
    contacts=re.findall(r'nsm1_puk_contact: key=([0-9a-f]+) down=([01]) ',trace)
    if contacts!=[(f'{key:02x}',down) for key in keys for down in ('1','0')]:
        raise ValueError('physical PUK/new-PIN confirmation differs')
    tx=transmitted(trace,4096,'nsm1_sim_tx')
    command=bytes.fromhex('a02c000010')+(b'12345679' if wrong else b'12345678')+padded_pin('4321')
    if tx.count(command)!=1:raise ValueError('UNBLOCK command differs')
    if statuses(trace,0x2c)!=[(0x9804 if wrong else 0x9000,0 if wrong else 3,9 if wrong else 10)]:
        raise ValueError('PUK result differs')
    if any(statuses(trace,ins) for ins in (0x20,0x24,0x26,0x28)):
        raise ValueError('unexpected credential command')


def transition(before,mode):
    expected=bytearray(before)
    if mode=='exhaust':expected[ATTEMPTS_OFFSET]-=1
    elif mode=='wrong':expected[ATTEMPTS_OFFSET+2]-=1
    elif mode=='recover':
        expected[ATTEMPTS_OFFSET]=3
        expected[CHV_OFFSET:CHV_OFFSET+8]=padded_pin('4321')
    elif mode!='cold':raise ValueError('unknown phase')
    return bytes(expected)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','second','exhausted','wrong','recovery','cold'):
        parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        source=(args.source/'nvram/nsm1r4t/sim_card').read_bytes()
        if source[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4]!=bytes((2,3,10,10)):
            raise ValueError('source must retain first physical PIN failure')
        phases=((args.second,args.source,'exhaust'),(args.exhausted,args.second,'exhaust'),
                (args.wrong,args.exhausted,'wrong'),(args.recovery,args.exhausted,'recover'),
                (args.cold,args.recovery,'cold'))
        for run,parent,mode in phases:
            storage=run/'nvram/nsm1r4t';old=parent/'nvram/nsm1r4t'
            before=(old/'sim_card').read_bytes()
            if (run/'initial_sim_card.bin').read_bytes()!=before:
                raise ValueError('cold input card chain differs')
            if (storage/'sim_card').read_bytes()!=transition(before,mode):
                raise ValueError('whole-card transition differs')
            if (run/'initial_eeprom.bin').read_bytes()!=(old/'eeprom').read_bytes():
                raise ValueError('cold EEPROM chain differs')
            if hashlib.sha1((storage/'flash').read_bytes()).hexdigest()!=SHA1:
                raise ValueError('own flash differs')
            trace=(run/'error.log').read_text();records(trace,True)
            if mode in ('wrong','recover'):recovery(trace,mode=='wrong')
            elif mode=='cold':check_pin(trace,True,b'4321',prefix='nsm1',tx_limit=4096)
            else:
                count=before[ATTEMPTS_OFFSET]-1
                if statuses(trace,0x20)!=[(0x9804 if count else 0x9840,count,10)]:
                    raise ValueError('PIN exhaustion result differs')
                keys=re.findall(r'nsm1_pin_contact: correct=0 key=([0-9a-f]+) down=([01]) ',trace)
                if keys!=[(f'{key:02x}',down) for key in (4,3,2,1,0x19) for down in ('1','0')]:
                    raise ValueError('physical wrong PIN contacts differ')
    except (OSError,ValueError) as error:
        print(f'NSM-1 PUK qualification failed: {error}',file=sys.stderr);return 1
    print('NSM-1 physical PIN exhaustion, PUK rejection/recovery and cold new PIN PASS')
    return 0


if __name__=='__main__':raise SystemExit(main())
