"""Bounded save/load replay of a declared native MU4 wake input."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.mu4_context_contract import contract
from tools.mu4_playback_frame_check import rows
from tools.mu4_playback_serial_control_check import check as serial_check
from tools.mu4_playback_serial_restore_check import REGISTERS, check_captures


def check(text, console):
    serial_check(text, console)
    states=rows(text,'mu4_wake_restore_state')
    if len(states)!=2 or [row['phase'] for row in states]!=['saved','restored']:
        raise ValueError('one wake saved/restored pair required')
    saved,restored=states
    if {key:value for key,value in saved.items() if key!='phase'}!={key:value for key,value in restored.items() if key!='phase'}:
        raise ValueError('wake CPU/RAM/time snapshot differs')
    if saved.get('data_words')!='65408' or saved.get('program_bytes')!='16777216':
        raise ValueError('wake snapshot memory scope differs')
    if any(not re.fullmatch('[0-9a-f]{8}',saved.get(key,'')) for key in ('ram','program')):
        raise ValueError('wake memory checksum missing')
    registers=[field.split(':') for field in saved['cpu'].split(',')]
    if [name for name,value in registers]!=REGISTERS or any(not re.fullmatch('[0-9a-f]{10}',value) for name,value in registers):
        raise ValueError('wake CPU register inventory differs')
    values=dict(registers)
    if any(int(values[name],16)!=value for name,value in [('IDLE',1),('IMR',4),('GENPC',0x28923),('ILLEGAL',0)]):
        raise ValueError('wake checkpoint is not clean resident idle')
    anchor=float(saved['t'])
    if not 10.94<=anchor<=10.97:
        raise ValueError('wake checkpoint time differs')
    events={phase:[] for phase in ('reference','restored')}
    for row in rows(text,'mu4_wake_replay_event'):
        branch=events.get(row['phase'])
        if (branch is None or int(row['index'])!=len(branch) or row['address'] not in ('0000','0001') or
                row['mask']!='ffff' or not re.fullmatch('[0-9a-f]{4}',row['value']) or
                not anchor<=float(row['t'])<anchor+0.25):
            raise ValueError('wake replay event identity/window differs')
        branch.append(tuple(row[key] for key in ('t','address','value','mask')))
    reference=events['reference']
    if not reference or len(reference)>=2048 or reference!=events['restored']:
        raise ValueError('wake register publication sequence/time differs or capped')
    if not any(address=='0001' and value=='0438' for _,address,value,_ in reference):
        raise ValueError('native IFR stack acknowledgement absent')
    inputs={phase:[] for phase in events}
    pattern=r'mu4_wake_replay_input: phase=(reference|restored) asserted=([01]) t=([0-9.]+) source=bench_pin firmware_state_forcing=0'
    matches=re.findall(pattern,text)
    if len(matches)!=4 or text.count('mu4_wake_replay_input:')!=4:
        raise ValueError('wake replay input inventory differs')
    for phase,asserted,time in matches:
        inputs[phase].append((asserted,time))
    if inputs['reference']!=inputs['restored'] or [value for value,_ in inputs['reference']]!=['1','0']:
        raise ValueError('wake replay pin sequence/time differs')
    asserted,released=[float(time) for _,time in inputs['reference']]
    if not (anchor+0.05-1e-8<=asserted<anchor+0.07 and
            anchor+0.10-1e-8<=released<anchor+0.12):
        raise ValueError('wake replay pulse outside declared scheduling window')
    actual=re.findall(r'mu4_mcu_int2_input: asserted=([01]) net=UC_DSP_INT cpu_irq=2 t=([0-9.]+) source=bench_pin firmware_state_forcing=0',text)
    if (text.count('mu4_mcu_int2_input:')!=4 or
            actual!=inputs['reference']+inputs['restored']):
        raise ValueError('actual board pin edges differ from replay inputs')
    result=rows(text,'mu4_wake_replay')
    if (len(result)!=1 or result[0].get('result')!='pass' or
            result[0].get('reference_events')!=str(len(reference)) or
            result[0].get('restored_events')!=str(len(reference)) or
            result[0].get('window')!='0.250000000' or result[0].get('cap')!='2048' or
            result[0].get('normal_mcu_waveform')!='unproved' or result[0].get('audio_acceptance')!='0'):
        raise ValueError('wake replay result scope differs')
    terminal=rows(text,'mu4_wake_restore_terminal')
    if len(terminal)!=1 or terminal[0].get('complete')!='1' or terminal[0].get('audio_acceptance')!='0':
        raise ValueError('wake replay terminal absent')
    return dict(registers=len(registers),events=len(reference),same_process=True,audio_completion=False)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    parser.add_argument('input_nand',type=Path)
    parser.add_argument('run',type=Path)
    args=parser.parse_args()
    try:
        contract(args.source.read_bytes())
        original=args.input_nand.read_bytes()
        if len(original)!=69206016 or original!=(args.run/'nvram/mu4nand_30/nand').read_bytes():
            raise ValueError('complete retained input NAND differs')
        text=(args.run/'error.log').read_text()
        result=check(text,Path(str(args.run)+'-launch.log').read_text())
        result.update(check_captures(args.run,text))
    except (OSError,ValueError,RuntimeError,KeyError) as error:
        parser.exit(1,f'MU4 native wake restore: FAIL: {error}\n')
    print(f'PASS: native wake restore {result}; declared pin only, not normal MCU/audio/EOF acceptance')


if __name__=='__main__':
    main()
