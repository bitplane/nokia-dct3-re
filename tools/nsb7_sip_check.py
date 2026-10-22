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


def check_restore(text):
    check_authentication(text.split('nsb7_sip_state: event=saved',1)[0])
    if '[LUA ERROR]' in text or 'nsb7_sip_state: FAIL' in text:
        raise ValueError('SIP restore harness failed')
    rows=re.findall(r'nsb7_sip_state: event=(saved|restored) pc=([0-9a-f]{8}) '
                    r'sp=([0-9a-f]{8}) ram=([0-9a-f]{8}) cpu=([0-9a-f,]+) t=([0-9.]+)',text)
    if len(rows)!=2 or [row[0] for row in rows]!=['saved','restored'] or rows[0][1:]!=rows[1][1:] or \
            any(not re.fullmatch(r'[0-9a-f]{8}(?:,[0-9a-f]{8}){36}',row[4]) for row in rows):
        raise ValueError('SIP restore must reproduce all ARM/banked registers, RAM and time')
    if rows[0][5]!='60.000000000' or text.count('state_roundtrip: result=pass scenario=nsb7_sip_outgoing')!=1:
        raise ValueError('missing unique connected save/load boundary')
    primary=check_inputs(text,delay=26)
    cleanup=list(re.finditer(r'nsb7_sip_cleanup_input: key=(.*?) state=([01]) t=([0-9.]+)',text))
    if [row.groups() for row in cleanup]!=[(key,state,f'{time}.{fraction}')
            for key,time in (('Left Softkey / Menu',70),('End',73))
            for state,fraction in (('1','000000000'),('0','200000000'))]:
        raise ValueError('post-restore menu recovery differs')
    for inputs in (primary,cleanup):
        for index in range(0,len(inputs),2):
            key=inputs[index][1]
            code='19' if key=='Left Softkey / Menu' else '0e' if key=='Send' else '0f' if key=='End' else f'{int(key[-1]):02x}'
            if f'nsb7_key_decode: key={code} observer_writes=0' not in text[inputs[index].end():inputs[index+1].start()]:
                raise ValueError('SIP restore physical contact did not decode')
    if re.findall(r'gsm_call_adapter: request id=1 epoch=(\d+) digits=5551234\b',text)!=['1','2']:
        raise ValueError('restore must republish one existing request under the new epoch')
    if re.findall(r'gsm_call_adapter: state id=1 epoch=(\d+) phase=(\w+)',text)!=[
            ('1','connected'),('2','connected'),('2','media_closed'),('2','ended')]:
        raise ValueError('restored host lifecycle differs')
    require_ordered(text,(
        ('connected predecessor',r'gsm_call_adapter: state id=1 epoch=1 phase=connected'),
        ('saved',r'sip_state: saved'),('restored',r'sip_state: restored'),
        ('new identity',r'gsm_call_adapter: request id=1 epoch=2 digits=5551234'),
        ('termination',r'termination id=1 cause=41 result=accepted'),
        ('network Disconnect',r'GSM service downlink kind=13 sapi=0 pd=03 message=25'),
        ('handset Release',r'GSM service uplink sapi=0 pd=03 message=2d'),
        ('network Release Complete',r'GSM service downlink kind=26 sapi=0 pd=03 message=2a'),
        ('RR released',r'LAPDm service Channel Release acknowledged'),
        ('host ended',r'gsm_call_adapter: state id=1 epoch=2 phase=ended'),
        ('Menu',r'nsb7_sip_cleanup_input: key=Left Softkey / Menu state=1 t=70\.'),
        ('done',r'nsb7_outgoing_call_done: t=76\.000000000'),
    ),'NSB-7 restored SIP cleanup')
    if re.search(r'media direction=uplink [^\n]*good=1\b',text) or 'dsp_hle: speech tick' in text:
        raise ValueError('unclocked restoration produced qualified handset media')


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
    modes=parser.add_mutually_exclusive_group()
    modes.add_argument('--incoming',action='store_true')
    modes.add_argument('--restore-outgoing',action='store_true')
    args=parser.parse_args()
    try:
        contract(args.image.read_bytes())
        text=(args.run/'error.log').read_text()
        if args.restore_outgoing: check_restore(text)
        else: check(text,incoming=args.incoming)
        check_clock(text,finish=68 if args.incoming else 76)
        storage=args.run/'nvram/nsb7hle'
        initial=(args.run/'initial_sim_card.bin').read_bytes()
        check_source(initial)
        if (storage/'sim_card').read_bytes()!=initial:
            raise ValueError('SIP signaling changed complete SIM card')
        inspect((storage/'flash').read_bytes())
        frames=SIP_INCOMING_FRAMES if args.incoming else FRAMES
        if args.restore_outgoing:
            frames={**FRAMES,
                'nsb7_sip_state_saved.png':FRAMES['nsb7_host_outgoing_connected.png'],
                'nsb7_sip_state_restored.png':FRAMES['nsb7_host_outgoing_connected.png'],
                'nsb7_sip_cleanup_70.png':'76728306449ad0f2ae8cbdbd1fc5e880d3dcff054c955e8b4a2aabc87db8e7e4'}
        for name,digest in frames.items(): check_frame(args.run/'snap'/name,digest)
        result=json.loads((args.run/'sip-result.json').read_text())
        if result.get('passed') is not True or result.get('speech_tested') is not False or \
                result.get('native_dsp_complete') is not False or (not args.restore_outgoing and
                result.get('caller' if args.incoming else 'dialed_digits')!='5551234'):
            raise ValueError('missing explicit SIP signaling-only result')
        from tools.run_sip_handset_gate import verify_success,verify_outgoing_restore,verify_handset_health
        from types import SimpleNamespace
        remote=(args.run/'sip-remote.log').read_text()
        if args.restore_outgoing:
            verify_handset_health((args.run/'sip-handset.log').read_text())
            if not (args.run/'sta/nsb7hle/nsb7_sip_outgoing.sta').is_file():
                raise ValueError('actual handset snapshot file absent')
            verify_outgoing_restore(args.run,remote,True,'8290',signaling_only=True)
        else:
            verify_success(args.run,remote,SimpleNamespace(product='8290',incoming=args.incoming,signaling_only=True))
    except (OSError,ValueError,RuntimeError) as error:
        parser.exit(1,f'NSB-7 SIP signaling: FAIL: {error}\n')
    print('PASS: physical PIN/'+('Answer' if args.incoming else 'dial')+
          '/End, real local SIP confirmation and CC/RR release'+
          ('; exact connected restore, BYE and no redial' if args.restore_outgoing else '')+
          '; speech_tested=0')


if __name__=='__main__': main()
