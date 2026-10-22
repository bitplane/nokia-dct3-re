"""Native resident status after a declared wake; paired sleeping control."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.mu4_context_contract import contract
from tools.mu4_playback_serial_control_check import response_frames
from tools.mu4_resident_int2_check import check_branch as check_wake

STATUS=[0x1e,5,0xaa,1,0x71,1,0,0,2,0xc2,0x55]


def check(text,console,wake):
    check_wake(text,console,wake,resident_status='answered' if wake else 'blocked')
    manual='mu4_manual_status_input: request=1 token=04 t=11.200000000 source=bench_serial_command firmware_state_forcing=0'
    if text.count(manual)!=1 or text.count('mu4_manual_status_input:')!=1:
        raise ValueError('one declared resident status command required')
    complete='mu4_resident_status_fixture: complete=1 token=04 source=bench_serial_command normal_mcu_policy=unproved firmware_state_forcing=0'
    if text.count(complete)!=1:
        raise ValueError('resident status fixture scope differs')
    request=re.findall(r'mu4_playback_control_input: phase=resident_status49 decoded_calls=(\d+) replies=(\d+) t=([0-9.]+) firmware_state_forcing=0 audio_acceptance=0',text)
    if request!=[('5','6','11.201000000')]:
        raise ValueError('resident query ingress boundary differs')
    frames=response_frames(text)
    after=[frame for frame in frames if frame['start']>text.index(manual)]
    acknowledgements=re.findall(r'mu4_native_recorder_control_ack: token=([0-9a-f]{2}) pin_output=1',text)
    if wake:
        if len(frames)!=7 or len(after)!=1 or after[0]['data']!=STATUS or acknowledgements!=['02','03','04']:
            raise ValueError('native post-wake status frame/acknowledgement differs')
        if text.index('mu4_native_recorder_control_ack: token=04')>after[0]['start']:
            raise ValueError('resident status response preceded command acknowledgement')
    elif len(frames)!=6 or after or acknowledgements!=['02','03']:
        raise ValueError('sleeping control unexpectedly consumed/responded to query')
    return dict(wake=wake,query_answered=wake,input_bytes=37 if wake else 30,
                request_token=4,response_token=2 if wake else None,audio_completion=False)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    parser.add_argument('input_nand',type=Path)
    parser.add_argument('control',type=Path)
    parser.add_argument('wake',type=Path)
    args=parser.parse_args()
    try:
        contract(args.source.read_bytes())
        original=args.input_nand.read_bytes()
        if len(original)!=69206016:
            raise ValueError('complete original NAND required')
        result=[]
        for directory,wake in ((args.control,False),(args.wake,True)):
            if (directory/'nvram/mu4nand_30/nand').read_bytes()!=original:
                raise ValueError('retained input NAND differs; no durability claim')
            result.append(check((directory/'error.log').read_text(),
                                Path(str(directory)+'-launch.log').read_text(),wake))
    except (OSError,ValueError,RuntimeError,KeyError) as error:
        parser.exit(1,f'MU4 resident status: FAIL: {error}\n')
    print(f'PASS: paired native resident status {result}; not normal MCU policy or EOF/audio acceptance')


if __name__=='__main__':
    main()
