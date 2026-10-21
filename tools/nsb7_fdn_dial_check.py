"""Registered NSB-7 listed/unlisted/Off FDN call-permission controls."""
import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_outgoing_call_check import check as check_call, check_inputs
from tools.nsb7_pin_services_check import check_authentication
from tools.nsb7_fdn_check import check_trace as check_disable
from tools.nsb7_fdn_record_check import edited_record
from tools.nsb7_storage_check import inspect
from tools.nsb7_pin_check import check_frame
from tools.sim_fdn_profile import EXTENDED_LENGTH
from tools import radio_outgoing_call_trace_check as call

DENIED_HASH='aea8c689d8cae96e12dd77020d02a58b0bfc0f31e00134c88bc218bb91eeb55f'
# Own retained clock has advanced through the physical FDN cold lifecycle.
FDN_IDLE_HASH='29dcc2200f6f5c170afea80e751d1d3afefd83332964c4bdc0f2a843be5e2567'


def check_trace(text, denied=False, digits=None, setup_transfer=False):
    digits = digits if digits is not None else '12347' if denied else '12346'
    check_authentication(text.split('nsb7_outgoing_call_input:',1)[0])
    rows = check_inputs(text,8,digits)
    for index in range(0,len(rows),2):
        key = rows[index][1]
        code = 0x0e if key == 'Send' else 0x0f if key == 'End' else int(key[-1])
        if f'nsb7_key_decode: key={code:02x} observer_writes=0' not in text[rows[index].end():rows[index+1].start()]:
            raise ValueError('FDN dial physical contact did not decode')
    if denied:
        if any(pattern.search(text) for pattern in (call.CM_SERVICE_REQUEST,call.SETUP,call.TRAFFIC_ASSIGNMENT,call.CONNECT)):
            raise ValueError('FDN-denied number reached network call establishment')
        if 'nsb7_outgoing_call_done: t=58.000000000' not in text:
            raise ValueError('FDN rejection observation incomplete')
    else:
        check_call(text,delay=8,digits=digits,setup_transfer=setup_transfer)


def check_cards(source, listed, denied, disabled, off):
    if len(source)!=EXTENDED_LENGTH or source[3570:3578]!=b'FDN\x01\x01\x00\x00\x00' or source[3578:3610]!=edited_record():
        raise ValueError('own edited FDN-On card required')
    expected = bytearray(source)
    expected[3575]=1
    if listed!=source or denied!=source or disabled!=bytes(expected) or off!=disabled:
        raise ValueError('FDN call controls changed unrelated card bytes')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','listed','denied','disable','off'):
        parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        source=(args.source/'nvram/nsb7hle/sim_card').read_bytes()
        cards=[]
        for run in (args.listed,args.denied,args.disable,args.off):
            storage=run/'nvram/nsb7hle'
            before=(run/'initial_sim_card.bin').read_bytes()
            if before!=(cards[-1] if run==args.off else source):
                raise ValueError('FDN dial input/predecessor card differs')
            cards.append((storage/'sim_card').read_bytes())
            inspect((storage/'flash').read_bytes())
        check_cards(source,*cards)
        check_trace((args.listed/'error.log').read_text())
        check_trace((args.denied/'error.log').read_text(),True)
        check_disable((args.disable/'error.log').read_text(),2)
        check_trace((args.off/'error.log').read_text(),digits='12347',setup_transfer=True)
        for run in (args.listed,args.denied,args.off):
            check_frame(run/'snap/nsb7_outgoing_call_final.png',FDN_IDLE_HASH)
        check_frame(args.denied/'snap/nsb7_outgoing_call_36.png',DENIED_HASH)
    except (OSError,ValueError) as error:
        parser.exit(1,f'NSB-7 FDN dial: FAIL: {error}\n')
    print('PASS: listed FDN call connects, unlisted is locally denied, physical Off permits the same unlisted number; no speech claim')


if __name__ == '__main__':
    main()
