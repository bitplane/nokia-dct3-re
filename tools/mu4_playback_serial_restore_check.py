"""Bounded native MU4 pending-control replay, not audio completion."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.mu4_playback_frame_check import rows
from tools.mu4_context_contract import contract
from tools.mu4_playback_serial_control_check import COMPLETION,response_frames,check as check_cold
from tools.noki5510_bootstrap_check import verify_resident_memory
from tools.radio_call_lifecycle_common import require_ordered
from tools.run_sip_handset_gate import verify_handset_health

REGISTERS='PC XPC A B T TRN RTN RC SP ST0 ST1 PMST AR0 AR1 AR2 AR3 AR4 AR5 AR6 AR7 BRC RSA REA BK IFR IMR SWWSR BSCR SWCR TIM PRD TCR IDLE ILLEGAL GENPC CURPC'.split()
CAPTURES=('mu4_playback_tx.le16','mu4_playback_din.le16','mu4_playback_dma.le16',
          'mu4_playback_din.slots','mu4_playback_tx.times','mu4_playback_dxr.writes',
          'mu4_playback_tx.events','mu4_decoded_returns.le16','mu4_headphone.frames')


def check_captures(run,text):
    rewinds=rows(text,'mu4_playback_capture_rewind')
    if len(rewinds)!=len(CAPTURES):raise ValueError('all nine diagnostic rewinds are required')
    total=0
    for index,(row,name) in enumerate(zip(rewinds,CAPTURES)):
        reference=name+'.before-load-1'
        if (row.get('index')!=str(index) or row.get('reference')!=reference or
                row.get('same_process')!='1' or row.get('firmware_state_forcing')!='0' or row.get('audio_acceptance')!='0'):
            raise ValueError('capture rewind scope/name/index differs')
        before=(run/reference).read_bytes()
        after=(run/name).read_bytes()
        if not 0<=int(row['bytes'])<=len(before)<=len(after) or not before or after[:len(before)]!=before:
            raise ValueError('preserved native capture differs from restored prefix')
        total+=len(before)
    captures=[row for row in text.splitlines() if 'save_restore=same_process_rewind ' in row]
    if len(captures)!=9:raise ValueError('same-process capture metadata absent')
    return dict(reference_bytes=total,exact_prefixes=9)


def check_independent_captures(run,cold):
    total=0
    for name in CAPTURES:
        reference=(cold/name).read_bytes()
        if not reference or reference!=(run/name).read_bytes():
            raise ValueError('restored capture differs from independent cold control: '+name)
        total+=len(reference)
    return dict(cold_capture_bytes=total,exact_cold_files=9)


def check(text,console,resident_status=False):
    endpoint='Fatal error: '+COMPLETION
    if console.count(endpoint)!=1:raise ValueError('strict native diagnostic endpoint absent')
    verify_handset_health(console.replace(endpoint,'').replace('Ignoring MAME exception: '+COMPLETION,''))
    if any(word in text for word in ('illegal=1','unsupported staged','[LUA ERROR]','mu4_state: FAIL')):
        raise ValueError('native control execution failed')
    verify_resident_memory(text)
    states=rows(text,'mu4_control_state')
    if len(states)!=2 or [state['phase'] for state in states]!=['saved','restored']:
        raise ValueError('one saved/restored pair required')
    saved,restored=states
    if {key:value for key,value in saved.items() if key!='phase'}!={key:value for key,value in restored.items() if key!='phase'}:
        raise ValueError('CPU/RAM checksum/time restoration differs')
    if saved.get('observer_writes')!='0' or saved.get('data_words')!='65408' or saved.get('program_bytes')!='16777216':
        raise ValueError('snapshot scope differs')
    if any(not re.fullmatch('[0-9a-f]{8}',saved.get(field,'')) for field in ('ram','program')):
        raise ValueError('RAM checksums absent or malformed')
    registers=[field.split(':') for field in saved['cpu'].split(',')]
    if [name for name,value in registers]!=REGISTERS or any(not re.fullmatch('[0-9a-f]{10}',value) for name,value in registers):
        raise ValueError('exposed CPU register inventory differs')
    if dict(registers)['ILLEGAL']!='0000000000' or (not resident_status and dict(registers)['IDLE']!='0000000000'):
        raise ValueError('checkpoint is not active clean native execution')
    lower,upper=(11.204,11.206) if resident_status else (1.04,1.07)
    if not lower<=float(saved['t'])<=upper:raise ValueError('pending-control checkpoint time differs')
    begin=text.index('mu4_control_state: phase=saved')
    loaded=text.index('mu4_control_state: phase=restored')
    if resident_status:
        if ('phase=resident_status49' not in text[:begin] or
                'mu4_native_recorder_control_ack: token=04' in text[:begin] or
                'mu4_native_recorder_response: index=6 ' in text[:begin]):
            raise ValueError('resident status must be pending before ACK/response at save')
        ingress=re.findall(r'mu4_native_command_input: cursor=(\d+) total=37\b',text[:begin])
        if ingress!=['30','31','32','33','34']:
            raise ValueError('resident checkpoint must retain five of eight command bytes')
    elif 'phase=status49' not in text[:begin] or 'selector=71 ' in text[:begin] or 'phase=control30_04' in text[:begin]:
        raise ValueError('status must be pending, before response/control at save')
    branches={'reference':[],'restored':[]}
    for event in rows(text,'mu4_control_replay_event'):
        phase=event['phase']
        if phase not in branches or int(event['index'])!=len(branches[phase]) or event.get('observer_writes')!='0':
            raise ValueError('replay branch/index/scope differs')
        if event['address']!='0033' or event['mask']!='ffff':raise ValueError('native TX register write scope differs')
        if not float(saved['t'])<=float(event['t'])<float(saved['t'])+0.15:
            raise ValueError('replay event outside bounded window')
        branches[phase].append(tuple(event[key] for key in ('t','address','value','mask')))
    reference=branches['reference']
    if not reference or len(reference)>=4096 or reference!=branches['restored']:
        raise ValueError('native serial TX sequence/time differs or is capped')
    terminals=rows(text,'mu4_control_replay')
    if len(terminals)!=1:raise ValueError('one replay result required')
    result=terminals[0]
    if (result.get('result')!='pass' or int(result['reference_events'])!=len(reference) or
            int(result['restored_events'])!=len(reference) or result.get('window')!='0.150000000' or
            result.get('cap')!='4096' or result.get('observer_writes')!='0' or result.get('audio_acceptance')!='0'):
        raise ValueError('replay terminal evidence differs')
    # Discard the reference continuation only for parsing the restored branch's
    # complete wire frames. Keep it above for independent ordered comparison.
    frames=response_frames(text[:begin]+text[loaded:])
    token=2 if resident_status else 3
    status=[frame for frame in frames if (frame['selector'],frame['token'],frame['class'])==(0x71,token,1)]
    if len(status)!=1:
        raise ValueError('restored correlated status response absent')
    expected=([0x7f,4,0x55]+status[0]['data']) if resident_status else (status[0]['data']+[0x7f,3,0x55])
    if [int(event[2],16) for event in reference]!=expected:
        raise ValueError('replayed native TX differs from the status frame/control ACK')
    if resident_status:
        require_ordered(text[loaded:],[
            ('command ACK',r'mu4_native_recorder_control_ack: token=04'),
            ('status response',r'mu4_native_recorder_response: index=6 selector=71 words=11 token=02 class=1'),
            ('clean capture',r'mu4_playback_capture_terminal: closed=1 illegal=0'),
            ('final native control',r'mu4_playback_control_terminal: status_reply=1 input_cursor=37 decoded_calls=5 idle=1 pc=028923'),
            ('restore terminal',r'mu4_control_restore_terminal: complete=1 observer_writes=0 audio_acceptance=0'),
        ],'restored resident serial status')
    else:
        require_ordered(text[loaded:],[
            ('status response',r'mu4_native_recorder_response: index=3 selector=71 words=11 token=03 class=1'),
            ('control',r'phase=control30_04'),('control ACK',r'mu4_native_recorder_control_ack: token=03'),
            ('pending sender',r'kind=send16 pc=020727[^\n]*pending=0003'),
            ('resident idle',r'kind=resident_idle3 pc=028922 opcode=f5e1'),
            ('clean capture',r'mu4_playback_capture_terminal: closed=1 illegal=0'),
            ('final native control',r'mu4_playback_control_terminal: status_reply=1 input_cursor=29 decoded_calls=5 idle=1 pc=028923'),
            ('restore terminal',r'mu4_control_restore_terminal: complete=1 observer_writes=0 audio_acceptance=0'),
        ],'restored native serial control')
    return dict(registers=len(registers),tx_events=len(reference),ram_checksum_equal=True,
                program_checksum_equal=True,audio_completion=False)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run',type=Path)
    parser.add_argument('--console',type=Path,required=True)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--input-nand',type=Path,required=True)
    parser.add_argument('--cold-control',type=Path,required=True)
    parser.add_argument('--cold-console',type=Path,required=True)
    parser.add_argument('--resident-status',action='store_true',help='post-wake partial serial command restoration')
    args=parser.parse_args()
    try:
        contract(args.source.read_bytes())
        original=args.input_nand.read_bytes()
        if len(original)!=69206016 or original!=(args.run/'nvram/mu4nand_30/nand').read_bytes():
            raise ValueError('complete input NAND differs or is truncated')
        if original!=(args.cold_control/'nvram/mu4nand_30/nand').read_bytes():
            raise ValueError('cold control uses different input NAND')
        if args.resident_status:
            from tools.mu4_resident_status_check import check as check_status
            check_status((args.cold_control/'error.log').read_text(),args.cold_console.read_text(),True)
        else:
            check_cold((args.cold_control/'error.log').read_text(),args.cold_console.read_text())
        text=(args.run/'error.log').read_text()
        result=check(text,args.console.read_text(),args.resident_status)
        result.update(check_captures(args.run,text))
        result.update(check_independent_captures(args.run,args.cold_control))
    except (OSError,ValueError,RuntimeError,KeyError) as error:parser.exit(1,f'MU4 control restore: FAIL: {error}\n')
    print(f'PASS: native pending-control restoration {result}; input_nand_sha256={hashlib.sha256(original).hexdigest()}; no audio/EOF acceptance')


if __name__=='__main__':main()
