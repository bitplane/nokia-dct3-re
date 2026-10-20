"""Physical local FDN dial enforcement; Calling is not network connection."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_fdn_record_check import check_authentication,RECORD
from tools.nsm1_fdn_check import check_trace as check_setting,validity_targets
from tools.nsm1_published_record_check import records
from tools.nsm1_v523_static_check import SHA1
from tools.nsk3_phonebook_check import transmitted
from tools.sim_security_trace_check import statuses,padded_pin
from tools.sim_fdn_profile import EXTENDED_LENGTH
from tools.nsk1_phonebook_roundtrip import frame_check

FRAMES=('1a99e24f91bdc2719463376e88c46824d4062f1e599a2e4c999f894327ec8aef',
        'f77c80d0e7545bd4d1c233f7d3564f9bbed1704ed5ee0362f25fb9b724294f3d',
        '3836dc38ee302adb064092035fe05016f79fe83a434e33b699b116efb25ce12b')


def disabled_card(enabled):
    record=bytearray(RECORD);record[22]=0xf6
    if len(enabled)!=EXTENDED_LENGTH or enabled[3570:3576]!=b'FDN\x01\x01\x00' or enabled[3578:3610]!=record:
        raise ValueError('enabled FDN card with A/12346 required')
    result=bytearray(enabled);result[3575]=1
    return bytes(result)


def check_call(trace,listed,enabled):
    keys=((1,2,3,4,6) if listed else (5,4,3,2,1))+(0x0e,)
    check_authentication(trace,keys,[(0x9000,3,10)])
    if any(statuses(trace,ins) for ins in (4,0x24,0x26,0x28,0x2c,0xdc)):
        raise ValueError('dial attempt mutated SIM state')
    expected=[0x6f7e,0x6f07] if enabled else []
    tx=transmitted(trace,4096,'nsm1_sim_tx')
    if validity_targets(tx,0x44)!=expected or statuses(trace,0x44)!=[(0x9000,3,10)]*len(expected):
        raise ValueError('cold identity handshake differs')
    if tx.count(bytes.fromhex('a020000108')+padded_pin('1234'))!=1:raise ValueError('startup PIN payload differs')
    if enabled and bytes.fromhex('a0a40000026f3b') not in tx:raise ValueError('FDN file was not selected')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','listed','unlisted','disabled','off_call'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        source=args.source/'nvram/nsm1r4t';enabled=(source/'sim_card').read_bytes();disabled=disabled_card(enabled)
        setting=(args.disabled/'error.log').read_text();check_setting(setting,2)
        if (args.disabled/'initial_sim_card.bin').read_bytes()!=enabled or (args.disabled/'nvram/nsm1r4t/sim_card').read_bytes()!=disabled:
            raise ValueError('physical Off changed more than validity')
        if (args.disabled/'initial_eeprom.bin').read_bytes()!=(source/'eeprom').read_bytes():raise ValueError('Off EEPROM input differs')
        for index,(run,listed,active,parent) in enumerate(((args.listed,True,True,args.source),
                (args.unlisted,False,True,args.source),(args.off_call,False,False,args.disabled))):
            card=enabled if active else disabled;storage=run/'nvram/nsm1r4t'
            if (run/'initial_sim_card.bin').read_bytes()!=card or (storage/'sim_card').read_bytes()!=card:
                raise ValueError('dial complete-card input/output differs')
            if (run/'initial_eeprom.bin').read_bytes()!=(parent/'nvram/nsm1r4t/eeprom').read_bytes():raise ValueError('matched EEPROM input differs')
            trace=(run/'error.log').read_text();check_call(trace,listed,active);records(trace,True)
            frame_check(run/'snap/nsm1_fdn_dial_26.png',FRAMES[index])
        for run in (args.disabled,args.listed,args.unlisted,args.off_call):
            records((run/'error.log').read_text(),True)
            if hashlib.sha1((run/'nvram/nsm1r4t/flash').read_bytes()).hexdigest()!=SHA1:raise ValueError('own flash differs')
    except (ValueError,OSError) as error:
        print(f'NSM-1 local FDN dial gate failed: {error}',file=sys.stderr);return 1
    print('NSM-1 physical FDN listed/unlisted/Off local dial enforcement PASS; network_connection=0 speech_claim=0')
    return 0


if __name__=='__main__':raise SystemExit(main())
