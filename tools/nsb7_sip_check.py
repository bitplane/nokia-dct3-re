"""NSB-7 physical SIP signaling only; fitted DSP and speech remain unproved."""
import argparse
import json
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsb7_outgoing_call_check import check as check_call,check_inputs
from tools.nsb7_pin_services_check import check_authentication,check_source
from tools.nsb7_host_outgoing_call_check import check_clock,FRAMES
from tools.nsb7_host_incoming_call_check import check as check_incoming,FRAMES as INCOMING_FRAMES
from tools.nsb7_radio_check import contract
from tools.nsb7_pin_check import check_frame
from tools.nsb7_storage_check import inspect
from tools.radio_call_lifecycle_common import require_ordered

SIP_INCOMING_FRAMES={**INCOMING_FRAMES,
    'nsb7_call_ringing.png':'6d379d1b35231e7e5205018afb27823b43f0e00643913481aaf3a14cad75d7ff'}


def check(text,*,incoming=False):
    if incoming:
        check_incoming(text,caller='5551234')
        if any(int(value) for value in re.findall(r'media_uplink_sequence=(\d+)',text)) or 'dsp_hle: speech tick' in text:
            raise ValueError('unclocked incoming SIP unexpectedly produced handset media')
        return
    check_authentication(text.split('nsb7_host_outgoing_ready:',1)[0])
    check_call(text,delay=26)
    rows=check_inputs(text,delay=26)
    for index in range(0,len(rows),2):
        key=rows[index][1]
        code='0e' if key=='Send' else '0f' if key=='End' else f'{int(key[-1]):02x}'
        if f'nsb7_key_decode: key={code} observer_writes=0' not in text[rows[index].end():rows[index+1].start()]:
            raise ValueError('SIP dial contact did not decode')
    require_ordered(text,(
        ('ready',r'nsb7_host_outgoing_ready: t=45\.000000000'),
        ('physical Send',r'nsb7_outgoing_call_input: key=Send state=1 t=54\.'),
        ('host request',r'gsm_call_adapter: request id=1 epoch=1 digits=5551234 clients=1'),
        ('SIP-owned decision',r'gsm_call_adapter: decision id=1 outcome=0 result=accepted'),
        ('host connected',r'gsm_call_adapter: state id=1 epoch=1 phase=connected'),
        ('physical End',r'nsb7_outgoing_call_input: key=End state=1 t=66\.'),
        ('host ended',r'gsm_call_adapter: state id=1 epoch=1 phase=ended'),
    ),'NSB-7 outgoing SIP signaling')
    if re.findall(r'gsm_call_adapter: decision id=1 outcome=(\d+) result=accepted',text)!=['0']:
        raise ValueError('SIP must own exactly one correlated accepted decision')
    if re.findall(r'gsm_call_adapter: state id=1 epoch=(\d+) phase=(\w+)',text)!=[
            ('1','connected'),('1','media_closed'),('1','ended')]:
        raise ValueError('SIP host state/epoch sequence differs')
    if any(int(value) for value in re.findall(r'media_uplink_sequence=(\d+)',text)) or 'dsp_hle: speech tick' in text:
        raise ValueError('unclocked SIP fixture unexpectedly produced handset media')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('run',type=Path)
    parser.add_argument('--incoming',action='store_true')
    args=parser.parse_args()
    try:
        contract(args.image.read_bytes())
        text=(args.run/'error.log').read_text()
        check(text,incoming=args.incoming)
        check_clock(text,finish=68 if args.incoming else 76)
        storage=args.run/'nvram/nsb7hle'
        initial=(args.run/'initial_sim_card.bin').read_bytes()
        check_source(initial)
        if (storage/'sim_card').read_bytes()!=initial:
            raise ValueError('SIP signaling changed complete SIM card')
        inspect((storage/'flash').read_bytes())
        frames=SIP_INCOMING_FRAMES if args.incoming else FRAMES
        for name,digest in frames.items(): check_frame(args.run/'snap'/name,digest)
        result=json.loads((args.run/'sip-result.json').read_text())
        if result.get('passed') is not True or result.get('speech_tested') is not False or \
                result.get('native_dsp_complete') is not False or result.get('caller' if args.incoming else 'dialed_digits')!='5551234':
            raise ValueError('missing explicit SIP signaling-only result')
        from tools.run_sip_handset_gate import verify_success
        from types import SimpleNamespace
        verify_success(args.run,(args.run/'sip-remote.log').read_text(),
                       SimpleNamespace(product='8290',incoming=args.incoming,signaling_only=True))
    except (OSError,ValueError,RuntimeError) as error:
        parser.exit(1,f'NSB-7 SIP signaling: FAIL: {error}\n')
    print('PASS: physical PIN/'+('Answer' if args.incoming else 'dial')+
          '/End, real local SIP confirmation and CC/RR release; speech_tested=0')


if __name__=='__main__': main()
