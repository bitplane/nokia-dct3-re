"""Native explicit serial playback control; never an automatic EOF/audio gate."""
import argparse
from functools import reduce
import hashlib
from operator import xor
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.mu4_context_contract import contract
from tools.mu4_playback_frame_check import rows
from tools.noki5510_bootstrap_check import verify_resident_memory
from tools.radio_call_lifecycle_common import require_ordered
from tools.run_sip_handset_gate import verify_handset_health

COMPLETION='MU4 playback coverage diagnostic: audio completion is not validated'
SITES={'serial30':(0x38d03,0x7105),'control30_04':(0x28498,0xeeaa),
       'build03':(0x2905d,0x4a11),'request':(0x20780,0x7102),
       'input03':(0x208ce,0xe800),'pending3':(0x208e5,0x76f8),
       'send16':(0x20727,0x7003),'receive16':(0x29458,0xe781),
       'hook16':(0x291d3,0xf495),'resident_idle3':(0x28922,0xf5e1)}


def response_frames(text):
    result=[]
    header=re.compile(r'mu4_native_recorder_response: index=(\d+) selector=([0-9a-f]{2}) words=(\d+) token=([0-9a-f]{2}) class=(\d+)')
    word=re.compile(r'mu4_native_recorder_response_word: index=(\d+) value=([0-9a-f]{4})')
    current=None
    position=0
    def finish(frame):
        values=frame['data']
        if (len(values)!=frame['words'] or not 6<=len(values)<=64 or values[0]!=0x1e or
                values[1]+6!=len(values) or values[2]!=0xaa or values[-1]!=0x55 or
                reduce(xor,values[:-2],0)!=values[-2] or values[4]!=frame['selector'] or
                values[-3]!=frame['token'] or values[3]!=frame['class']):
            raise ValueError('native response framing, checksum or metadata differs')
        result.append(frame)
    for line in text.splitlines(keepends=True):
        if match:=header.search(line):
            if current is not None:finish(current)
            if int(match[1])!=len(result):raise ValueError('native response ordinal differs')
            current=dict(selector=int(match[2],16),words=int(match[3]),token=int(match[4],16),
                         **{'class':int(match[5])},data=[],start=position,end=position+len(line))
        elif match:=word.search(line):
            if current is None or int(match[1])!=len(current['data']) or int(match[2],16)>255:
                raise ValueError('native response byte index/value differs')
            current['data'].append(int(match[2],16))
            current['end']=position+len(line)
        position+=len(line)
    if current is not None:finish(current)
    return result


def check(text,console,*,resident_status=None):
    if resident_status not in (None,'answered','blocked'):
        raise ValueError('unknown resident status fixture scope')
    expected='Fatal error: '+COMPLETION
    if console.count(expected)!=1:raise ValueError('strict audio diagnostic endpoint absent')
    verify_handset_health(console.replace(expected,'').replace('Ignoring MAME exception: '+COMPLETION,''))
    if any(token in text for token in ('unsupported staged','illegal=1','[LUA ERROR]','mu4_state: FAIL')):
        raise ValueError('native control execution failed')
    verify_resident_memory(text)
    frames=response_frames(text)
    # Request ACK tokens and DSP response tokens belong to opposite directions.
    status=[frame for frame in frames if (frame['selector'],frame['token'],frame['class'])==(0x71,3,1)]
    if len(status)!=1:raise ValueError('one correlated status49 reply is required')
    phases=list(re.finditer(r'mu4_playback_control_input: phase=(\w+) decoded_calls=(\d+) replies=(\d+) t=([0-9.]+) firmware_state_forcing=0 audio_acceptance=0',text))
    expected_phases=['play30_02','status49','control30_04']
    if resident_status is not None:expected_phases.append('resident_status49')
    if [phase[1] for phase in phases]!=expected_phases:
        raise ValueError('native control peer phase sequence differs')
    if not 0<int(phases[1][2])<=int(phases[2][2]) or status[0]['end']>phases[2].start():
        raise ValueError('control preceded completed native decoding/correlated status reply')
    events=rows(text,'mu4_play_control')
    if not events or len(events)>=1024:raise ValueError('native control observation empty or capped')
    for ordinal,event in enumerate(events,1):
        if (int(event['ordinal'])!=ordinal or event.get('identity')!='Execute_entry' or
                event.get('operands')!='context_only' or event.get('observer_writes')!='0'):
            raise ValueError('native control observation identity differs')
        if (int(event['pc'],16),int(event['opcode'],16))!=SITES.get(event['kind']):
            raise ValueError('native control source instruction differs')
    expected_cursor=37 if resident_status=='answered' else 30 if resident_status=='blocked' else 29
    require_ordered(text,[
        ('upload',r'mu4_original_bootstrap_upload: records=7 words=10760 program_only=1'),
        ('AA99',r'mu4_native_playback_overlay: PASS marker=aa99 records=3127 words=49843 checked_ram_words=49839 excluded_register_words=4 before_execution=1'),
        ('status input',r'mu4_playback_control_input: phase=status49'),
        ('status acknowledgement',r'mu4_native_recorder_control_ack: token=02 pin_output=1'),
        ('correlated status reply',r'mu4_native_recorder_response: index=\d+ selector=71 words=\d+ token=03 class=1'),
        ('control input',r'mu4_playback_control_input: phase=control30_04'),
        ('control acknowledgement',r'mu4_native_recorder_control_ack: token=03 pin_output=1'),
        ('selector30 parameter04',r'kind=serial30 pc=038d03[^\n]* parameter=0004'),
        ('controller action',r'kind=control30_04 pc=028498'),
        ('builder',r'kind=build03 pc=02905d'),
        ('input03',r'kind=input03 pc=0208ce[^\n]* f2=0003 f3=0000 f4=0000 f5=0001'),
        ('pending store',r'kind=pending3 pc=0208e5'),
        ('input16 sender',r'kind=send16 pc=020727[^\n]* pending=0003'),
        ('resident IDLE3',r'kind=resident_idle3 pc=028922 opcode=f5e1'),
        ('clean capture',r'mu4_playback_capture_terminal: closed=1 illegal=0'),
        ('final control',rf'mu4_playback_control_terminal: status_reply=1 input_cursor={expected_cursor} decoded_calls=[1-9]\d* idle=\d+ pc=[0-9a-f]{{6}} t=20\.000000000 firmware_state_forcing=0 audio_acceptance=0'),
    ],'native explicit serial playback control')
    if re.findall(r'mu4_native_command_input: cursor=(\d+) total=29',text)!=[str(index) for index in range(1,30)]:
        raise ValueError('physical serial input byte inventory differs')
    suffix=re.findall(r'mu4_native_command_input: cursor=(\d+) total=37',text)
    if suffix!=([str(index) for index in range(30,expected_cursor+1)] if resident_status else []):
        raise ValueError('resident query byte inventory differs')
    if text.count('mu4_native_command_input:')!=expected_cursor:
        raise ValueError('unclassified or duplicated serial input record')
    terminal=rows(text,'mu4_play_control_terminal')
    if (len(terminal)!=1 or int(terminal[0]['events'])!=len(events) or
            terminal[0].get('cap')!='1024' or terminal[0].get('observer_writes')!='0' or
            terminal[0].get('audio_acceptance')!='0'):
        raise ValueError('native control observation terminal differs')
    return dict(events=len(events),responses=len(frames),audio_completion=False,
                receive16_observed=any(event['kind']=='receive16' for event in events),
                hook16_observed=any(event['kind']=='hook16' for event in events))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    parser.add_argument('run',type=Path)
    parser.add_argument('input_nand',type=Path)
    parser.add_argument('--console',type=Path,required=True)
    args=parser.parse_args()
    try:
        contract(args.source.read_bytes())
        original=args.input_nand.read_bytes()
        retained=(args.run/'nvram/mu4nand_30/nand').read_bytes()
        if len(original)!=69206016 or original!=retained:
            raise ValueError('complete input NAND was changed or is truncated')
        result=check((args.run/'error.log').read_text(),args.console.read_text())
    except (OSError,ValueError,RuntimeError,KeyError) as error:
        parser.exit(1,f'MU4 native serial control: FAIL: {error}\n')
    print(f'PASS: native serial control {result}; input_nand_sha256={hashlib.sha256(original).hexdigest()}; no EOF/audio acceptance')


if __name__=='__main__':main()
