"""Physical NSK-3 offline SMS draft save/cold Outbox read, not network SMS."""
import argparse
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_static_check import inspect
from tools.nsk3_security_check import check_trace as check_phone
from tools.nsk3_phonebook_check import transmitted
from tools.nsk3_boot_check import check_upload_handoffs
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.make_sim_card_profile import ADN_SIZE,LOCI_SIZE,KC_SIZE,BCCH_SIZE,SMS_RECORD_SIZE,SMS_RECORDS
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH
from tools.sim_security_trace_check import statuses
from tools.sim_sms_draft import SMS_OFFSET,DRAFT,check_card

SAVE_KEYS=(0x19,0x19,0x18,0x18,0x19,2,0x19,0x18,0x18,0x19)
READ_KEYS=(0x19,0x19,0x18,0x19,0x19)
EDITOR='5e1c7514b1bbee44ee0acc3563cf74595023ed6cacc203147c0b4ed78c47b5f0'
READ='126383b4fff9fc2d2e776b939ad18b3ae6d7194e9370a41a38d7313c7eca79cb'


def check_streams(write_trace,cold_trace,write_stream,cold_stream):
    if statuses(write_trace,0xdc)!=[(0x9000,3,10)] or statuses(cold_trace,0xdc):
        raise ValueError('write/cold read-only status contrast differs')
    command=bytes.fromhex('a0dc0104b0')+DRAFT
    if write_stream.count(command)!=1 or bytes.fromhex('a0a40000026f3c') not in write_stream[:write_stream.find(command)]:
        raise ValueError('EF_SMS selection/exact UPDATE RECORD differs')
    if bytes.fromhex('a0dc') in cold_stream:raise ValueError('cold read rewrote a SIM record')


def check_runs(source,write,cold):
    _,image=inspect(source,'boot4')
    cards,streams=[],[]
    for run,keys in ((write,SAVE_KEYS),(cold,READ_KEYS)):
        trace=(run/'error.log').read_text()
        check_phone(trace,True,keys)
        check_upload_handoffs(trace,1)
        stream=transmitted(trace,4096)
        streams.append(stream)
        if (run/'nvram/nsk3r4t/flash').read_bytes()!=image:
            raise ValueError('own executable flash changed')
        card=(run/'nvram/nsk3r4t/sim_card').read_bytes()
        check_card(card)
        cards.append(card)
    check_streams((write/'error.log').read_text(),(cold/'error.log').read_text(),*streams)
    if (cold/'initial_sim_card.bin').read_bytes()!=cards[0] or cards[1]!=cards[0]:
        raise ValueError('cold SMS card did not preserve saved bytes')
    frame_check(write/'snap/nsk3_sms_23.png',EDITOR)
    frame_check(cold/'snap/nsk3_sms_read_19.png',READ)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','write','cold'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:check_runs(args.source,args.write,args.cold)
    except (ValueError,OSError) as error:
        print(f'NSK-3 offline SMS failed: {error}',file=sys.stderr)
        return 1
    print('NSK-3 physical offline A draft/cold Outbox read PASS; network SMS unproved')
    return 0


if __name__=='__main__':raise SystemExit(main())
