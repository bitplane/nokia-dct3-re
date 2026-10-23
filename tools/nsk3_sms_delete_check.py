"""Physical NSK-3 offline draft erase/cancel and cold empty Outbox."""
import argparse
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_sms_check import check_card,SMS_OFFSET,DRAFT,READ_KEYS,READ
from tools.nsk3_phonebook_check import transmitted
from tools.nsk3_security_check import check_trace as check_phone
from tools.nsk3_static_check import inspect
from tools.nsk3_boot_check import check_upload_handoffs
from tools.sim_security_trace_check import statuses
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_sms_draft import deleted_card

CONFIRM='0efe3c5798b90f0653bb227f9cce5afca2af17cdca71701c6b6b1e31b0aeb129'
EMPTY='cdff157a2a10f5c1528e9085b1981401a9714ae37cded35e3eddd43fd8a83fb0'


def check_runs(source,saved_run,erased,cancel,cold):
    _,image=inspect(source,'boot4')
    saved=(saved_run/'nvram/nsk3r4t/sim_card').read_bytes()
    deleted=deleted_card(saved)
    for run,keys,initial,final,write in (
            (erased,READ_KEYS+(0x19,)*3,saved,deleted,True),
            (cancel,READ_KEYS+(0x19,0x19,0x1a),saved,saved,False),
            (cold,READ_KEYS[:-1],deleted,deleted,False)):
        trace=(run/'error.log').read_text()
        check_phone(trace,True,keys)
        check_upload_handoffs(trace,1)
        tx=transmitted(trace,4096)
        if statuses(trace,0xdc)!=([(0x9000,3,10)] if write else []):
            raise ValueError('erase/cancel/cold update result differs')
        command=bytes.fromhex('a0dc0104b0')+bytes(1)+DRAFT[1:]
        if write:
            if tx.count(command)!=1:raise ValueError('exact free-status UPDATE RECORD missing')
        elif bytes.fromhex('a0dc') in tx:raise ValueError('cancel/cold unexpectedly writes a record')
        if (run/'initial_sim_card.bin').read_bytes()!=initial or (run/'nvram/nsk3r4t/sim_card').read_bytes()!=final:
            raise ValueError('persistent SMS transition changes unexpected bytes')
        if (run/'nvram/nsk3r4t/flash').read_bytes()!=image:
            raise ValueError('own executable flash changed')
    for run in (erased,cancel):frame_check(run/'snap/nsk3_sms_read_19.png',CONFIRM)
    frame_check(cancel/'snap/nsk3_sms_read_21.png',READ)
    frame_check(cold/'snap/nsk3_sms_read_16.5.png',EMPTY)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','saved','erased','cancel','cold'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:check_runs(args.source,args.saved,args.erased,args.cancel,args.cold)
    except (ValueError,OSError) as error:
        print(f'NSK-3 SMS erase failed: {error}',file=sys.stderr)
        return 1
    print('NSK-3 physical offline erase/cancel/cold empty Outbox PASS')
    return 0


if __name__=='__main__':raise SystemExit(main())
