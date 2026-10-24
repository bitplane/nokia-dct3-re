"""NSB-7 physical SIP acceptance; HLE media is distinct from native speech."""
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
from tools.radio_call_lifecycle_common import require_ordered,laboratory_incoming_setup

SIP_INCOMING_FRAMES={**INCOMING_FRAMES,
    'nsb7_call_ringing.png':'6d379d1b35231e7e5205018afb27823b43f0e00643913481aaf3a14cad75d7ff'}
# Real localhost INVITE arrival is not phase-locked to emulated display animation.
# Both reviewed full frames display 5551234; one omits Calling/Decline for a phase.
SIP_RINGING_ALTERNATE='b0e2787482bcc037d9d415f390b8fc4199379d8dfa1fd3628777195917d85a44'
PENDING_CALLING='e52eedec3e38b9e485554ba496dd9c27194083916771d7476bb435899b68f72e'
PENDING_CALLING_ALTERNATE='248673ab3db1dabf5d32273cd0191d76ef6264b7b9eb8b9ce7312fe2eb0fd1f7'


def check_sip_frame(path,digest,*,incoming=False,alerting_snapshot=False,pending_snapshot=False):
    try:
        check_frame(path,digest)
    except ValueError:
        if pending_snapshot and path.name in ('nsb7_sip_state_saved.png','nsb7_sip_state_restored.png',
                                               'nsb7_host_outgoing_connected.png'):
            check_frame(path,PENDING_CALLING_ALTERNATE)
            return
        if not incoming or (path.name!='nsb7_call_ringing.png' and not (
                alerting_snapshot and path.name in ('nsb7_sip_state_saved.png','nsb7_sip_state_restored.png'))): raise
        check_frame(path,SIP_RINGING_ALTERNATE)


def check_restored_architecture(text,*,incoming=False,alerting=False):
    check_authentication(text.split('nsb7_sip_state: event=saved',1)[0])
    if '[LUA ERROR]' in text or 'nsb7_sip_state: FAIL' in text:
        raise ValueError('SIP restore harness failed')
    rows=re.findall(r'nsb7_sip_state: event=(saved|restored) pc=([0-9a-f]{8}) '
                    r'sp=([0-9a-f]{8}) ram=([0-9a-f]{8}) cpu=([0-9a-f,]+) t=([0-9.]+)',text)
    if len(rows)!=2 or [row[0] for row in rows]!=['saved','restored'] or rows[0][1:]!=rows[1][1:] or \
            any(not re.fullmatch(r'[0-9a-f]{8}(?:,[0-9a-f]{8}){36}',row[4]) for row in rows):
        raise ValueError('SIP restore must reproduce all ARM/banked registers, RAM and time')
    direction='incoming' if incoming else 'outgoing'
    if rows[0][5]!=('51.000000000' if alerting else '55.000000000' if incoming else '60.000000000') or \
            text.count('state_roundtrip: result=pass scenario=nsb7_sip_'+direction)!=1:
        raise ValueError('missing unique connected save/load boundary')


def check_incoming_restore(text,phase='connected',*,media=False,clocked_unanswered=False):
    if phase not in ('connected','alerting'):raise ValueError('unknown incoming restore phase')
    alerting=phase=='alerting'
    if media and alerting:
        raise ValueError('unanswered incoming cannot qualify connected media')
    if clocked_unanswered and not alerting:
        raise ValueError('unanswered acceptance requires alerting phase')
    check_restored_architecture(text,incoming=True,alerting=alerting)
    setup=laboratory_incoming_setup('5551234')
    rows=list(re.finditer(r'nsb7_call_input: key=(Send|End) state=([01]) t=([0-9.]+)',text))
    expected=[(key,edge,f'{second}.{fraction}') for key,second in ([('End',60)] if alerting else [('Send',52),('End',60)])
              for edge,fraction in [('1','000000000'),('0','200000000')]]
    if [row.groups() for row in rows]!=expected:
        raise ValueError('restored incoming Answer/End sequence differs')
    cleanup=list(re.finditer(r'nsb7_sip_cleanup_input: key=(.*?) state=([01]) t=([0-9.]+)',text))
    if [row.groups() for row in cleanup]!=[(key,edge,f'{second}.{fraction}')
            for key,second in [('Left Softkey / Menu',62),('End',65)]
            for edge,fraction in [('1','000000000'),('0','200000000')]]:
        raise ValueError('restored incoming Menu recovery differs')
    for inputs,codes in ((rows,('0f',) if alerting else ('0e','0f')),(cleanup,('19','0f'))):
        for index,code in enumerate(codes):
            if f'nsb7_key_decode: key={code} observer_writes=0' not in text[inputs[index*2].end():inputs[index*2+1].start()]:
                raise ValueError('restored incoming physical contact did not decode')
    if re.findall(r'gsm_call_adapter: incoming state id=1 epoch=(\d+) phase=(\w+)',text)!=[
            ('1','queued'),('1','paging'),('1','alerting')]+([] if alerting else [('1','connected')])+[
            ('2',phase)]+([] if alerting else [('2','media_closed')])+[('2','ended')]:
        raise ValueError('restored incoming identity/epoch sequence differs')
    require_ordered(text,[
        ('registered',r'gsm_call_adapter: network registered=1 arfcn=600\b'),
        ('readiness',r'nsb7_call_ready: t=45\.000000000'),
        ('admission',r'gsm_call_adapter: incoming id=1 result=accepted'),
        ('paging',r'incoming state id=1 epoch=1 phase=paging'),
        ('SETUP',rf'GSM service downlink kind=9 sapi=0 pd=03 message=05 length={len(setup)}(?:\s|$)'),
        ('task4 caller',rf'nsb7_lapdm_downlink: pc=002e49e6 task=4 selector=80 block=03[0-9a-f]{{2}}{len(setup)*4+1:02x}{setup.hex()}[0-9a-f]* '),
        ('task14 caller',rf'nsb7_lapdm_downlink: pc=002645ec task=14 selector=80 block=03[0-9a-f]{{2}}{len(setup)*4+1:02x}{setup.hex()}[0-9a-f]* '),
        ('Call Confirmed',r'GSM service uplink sapi=0 pd=03 message=08 length=11 data=8308040460020180150101'),
        ('Alerting',r'GSM service uplink sapi=0 pd=03 message=01 length=2 data=8341'),
    ]+([] if alerting else [
        ('physical Answer',r'nsb7_call_input: key=Send state=1 t=52\.'),
        ('Connect',r'GSM service uplink sapi=0 pd=03 message=07 length=2 data=8307'),
        ('Connect ACK',r'GSM service downlink kind=20 sapi=0 pd=03 message=0f length=2'),
        ('connected',r'incoming state id=1 epoch=1 phase=connected'),
    ])+[
        ('save',r'sip_state: saved'),('load',r'sip_state: restored'),
        ('new epoch',rf'incoming state id=1 epoch=2 phase={phase}'),
        ('termination',r'termination id=1 cause=41 result=accepted'),
        ('Disconnect',r'GSM service downlink kind=13 sapi=0 pd=03 message=25'),
        ('Release Complete',r'GSM service uplink sapi=0 pd=03 message=2a'),
        ('RR released',r'LAPDm service Channel Release acknowledged'),
        ('ended',r'incoming state id=1 epoch=2 phase=ended'),
        ('Menu',r'nsb7_sip_cleanup_input: key=Left Softkey / Menu state=1 t=62\.'),
        ('done',r'nsb7_sip_done: direction=incoming t=68\.000000000'),
    ],'NSB-7 restored incoming SIP')
    if alerting and (re.search(r'GSM service uplink sapi=0 pd=03 message=07\b',text) or
                     re.search(r'phase=connected\b',text)):
        raise ValueError('alerting restore answered or connected')
    if alerting and text.index('incoming state id=1 epoch=2 phase=ended')>rows[0].start():
        raise ValueError('network termination waited for physical End')
    if len(re.findall(r'gsm_call_adapter: incoming id=\d+ result=accepted',text))!=1 or \
            'GSM service establish sapi=0 pd=05 message=24 ' in text:
        raise ValueError('restored incoming has duplicate/outgoing admission')
    if media:
        from tools.nsb7_pcm_check import verify_before_restore
        verify_before_restore(text, incoming=True)
    elif clocked_unanswered:
        from tools.nsb7_pcm_check import verify_unanswered_restore
        verify_unanswered_restore(text)
    elif re.search(r'media direction=uplink [^\n]*good=1\b',text) or 'dsp_hle: speech tick' in text:
        raise ValueError('unclocked incoming restoration produced qualified media')


def check_restore(text,connected=True,*,media=False,clocked_unanswered=False):
    if media and not connected:
        raise ValueError('pending call cannot qualify connected media')
    if clocked_unanswered and connected:
        raise ValueError('unanswered acceptance requires pending phase')
    check_restored_architecture(text)
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
    phase='connected' if connected else 'alerting'
    if re.findall(r'gsm_call_adapter: state id=1 epoch=(\d+) phase=(\w+)',text)!=(
            [('1',phase),('2',phase),('2','media_closed'),('2','ended')] if connected else [('2','ended')]):
        raise ValueError('restored host lifecycle differs')
    require_ordered(text,((
        ('predecessor',rf'gsm_call_adapter: state id=1 epoch=1 phase={phase}'),
    ) if connected else ()) + (
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
    if not connected:
        if re.search(r'phase=connected\b',text) or re.search(r'GSM service downlink kind=12 sapi=0 pd=03 message=07\b',text):
            raise ValueError('pending outgoing restore connected')
        if text.index('state id=1 epoch=2 phase=ended')>primary[-2].start():
            raise ValueError('pending outgoing restore waited for physical End')
    if media:
        from tools.nsb7_pcm_check import verify_before_restore
        verify_before_restore(text)
    elif clocked_unanswered:
        from tools.nsb7_pcm_check import verify_unanswered_restore
        verify_unanswered_restore(text)
    elif re.search(r'media direction=uplink [^\n]*good=1\b',text) or 'dsp_hle: speech tick' in text:
        raise ValueError('unclocked restoration produced qualified handset media')


def check(text,*,incoming=False,media=False):
    if incoming:
        check_incoming(text,caller='5551234')
        if media:
            from tools.nsb7_pcm_check import verify
            verify(text, incoming=True)
        elif any(int(value) for value in re.findall(r'media_uplink_sequence=(\d+)',text)) or 'dsp_hle: speech tick' in text:
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
    if media:
        from tools.nsb7_pcm_check import verify
        verify(text)
    elif any(int(value) for value in re.findall(r'media_uplink_sequence=(\d+)',text)) or 'dsp_hle: speech tick' in text:
        raise ValueError('unclocked SIP fixture unexpectedly produced handset media')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('run',type=Path)
    parser.add_argument('--media', action='store_true',
                        help='qualify clocked HLE media or connected outgoing media restoration')
    parser.add_argument('--clocked-unanswered', action='store_true',
                        help='unanswered restoration on PCM-enabled build, no external media')
    modes=parser.add_mutually_exclusive_group()
    modes.add_argument('--incoming',action='store_true')
    modes.add_argument('--restore-outgoing',action='store_true')
    modes.add_argument('--restore-outgoing-pending',action='store_true')
    modes.add_argument('--restore-incoming',action='store_true')
    modes.add_argument('--restore-incoming-alerting',action='store_true')
    args=parser.parse_args()
    incoming=args.incoming or args.restore_incoming or args.restore_incoming_alerting
    restoring=args.restore_outgoing or args.restore_outgoing_pending or args.restore_incoming or args.restore_incoming_alerting
    if args.media and restoring and not (args.restore_outgoing or args.restore_incoming):
        parser.error('--media restoration requires a connected call')
    if args.clocked_unanswered and (args.media or not (
            args.restore_outgoing_pending or args.restore_incoming_alerting)):
        parser.error('--clocked-unanswered requires an unanswered restoration fixture')
    phase='alerting' if args.restore_incoming_alerting else 'connected'
    try:
        contract(args.image.read_bytes())
        text=(args.run/'error.log').read_text()
        if args.restore_incoming or args.restore_incoming_alerting: check_incoming_restore(text,phase=phase,media=args.media,clocked_unanswered=args.clocked_unanswered)
        elif args.restore_outgoing or args.restore_outgoing_pending: check_restore(text,connected=not args.restore_outgoing_pending,media=args.media,clocked_unanswered=args.clocked_unanswered)
        else: check(text,incoming=incoming,media=args.media)
        check_clock(text,finish=68 if incoming else 76)
        storage=args.run/'nvram/nsb7hle'
        initial=(args.run/'initial_sim_card.bin').read_bytes()
        check_source(initial)
        if (storage/'sim_card').read_bytes()!=initial:
            raise ValueError('SIP signaling changed complete SIM card')
        inspect((storage/'flash').read_bytes())
        frames=SIP_INCOMING_FRAMES if incoming else FRAMES
        if restoring:
            frames={**frames,
                'nsb7_sip_state_saved.png':FRAMES['nsb7_host_outgoing_connected.png'],
                'nsb7_sip_state_restored.png':FRAMES['nsb7_host_outgoing_connected.png'],
                ('nsb7_sip_cleanup_62.png' if incoming else 'nsb7_sip_cleanup_70.png'):
                    '76728306449ad0f2ae8cbdbd1fc5e880d3dcff054c955e8b4a2aabc87db8e7e4'}
            if args.restore_incoming_alerting:
                frames.pop('nsb7_call_Send.png')
                frames['nsb7_sip_state_saved.png']=SIP_INCOMING_FRAMES['nsb7_call_ringing.png']
                frames['nsb7_sip_state_restored.png']=SIP_INCOMING_FRAMES['nsb7_call_ringing.png']
            if args.restore_outgoing_pending:
                for name in ('nsb7_host_outgoing_connected.png','nsb7_sip_state_saved.png','nsb7_sip_state_restored.png'):
                    frames[name]=PENDING_CALLING
        for name,digest in frames.items():
            check_sip_frame(args.run/'snap'/name,digest,incoming=incoming,
                            alerting_snapshot=args.restore_incoming_alerting,
                            pending_snapshot=args.restore_outgoing_pending)
        result=json.loads((args.run/'sip-result.json').read_text())
        if result.get('passed') is not True or (not args.media and not args.clocked_unanswered and (
                result.get('speech_tested') is not False or result.get('native_dsp_complete') is not False)) or (not restoring and
                result.get('caller' if incoming else 'dialed_digits')!='5551234'):
            raise ValueError('missing explicit SIP signaling-only result')
        from tools.run_sip_handset_gate import verify_success,verify_outgoing_restore,verify_restore,verify_handset_health
        from types import SimpleNamespace
        remote=(args.run/'sip-remote.log').read_text()
        if restoring:
            verify_handset_health((args.run/'sip-handset.log').read_text())
            direction='incoming' if incoming else 'outgoing'
            if not (args.run/f'sta/nsb7hle/nsb7_sip_{direction}.sta').is_file():
                raise ValueError('actual handset snapshot file absent')
            if incoming: verify_restore(args.run,remote,phase,'8290',signaling_only=not (args.media or args.clocked_unanswered))
            else: verify_outgoing_restore(args.run,remote,not args.restore_outgoing_pending,'8290',signaling_only=not (args.media or args.clocked_unanswered))
        else:
            verify_success(args.run,remote,SimpleNamespace(product='8290',incoming=incoming,
                           signaling_only=not args.media,restore_idle=False))
    except (OSError,ValueError,RuntimeError) as error:
        parser.exit(1,f'NSB-7 SIP signaling: FAIL: {error}\n')
    print('PASS: physical PIN/'+('unanswered ringing' if args.restore_incoming_alerting else 'Answer' if incoming else 'dial')+
          '/End, real local SIP '+('cancellation' if args.restore_outgoing_pending else
                                  'rejection' if args.restore_incoming_alerting else 'confirmation')+' and CC/RR release'+
          ('; exact pending restore, CANCEL/487 and no replay' if args.restore_outgoing_pending else
           '; exact alerting restore, SIP rejection and no replay' if args.restore_incoming_alerting else
           '; exact connected restore, BYE and no replay' if restoring else '')+
          ('; clocked HLE media, native speech unproved' if args.media else '; speech_tested=0'))


if __name__=='__main__': main()
