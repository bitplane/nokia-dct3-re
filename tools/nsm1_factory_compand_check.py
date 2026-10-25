"""Bound observed native codec formatting against an independent A-law encoder."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_factory_clock_check import check as check_clock,execute

ENCODER='7716b3d7fd476d96f485f48e4a08490efd44e817f587f520890e8a08f784f48ff47dff4df47fe901fe00f2c41c86'
FORMAT_SETUP='61f81f2b0040f820a4a57722c00c7722c0ccf073a4a97722c0087722c0c8'
REFERENCE_ARGS=('-hide_banner','-loglevel','error','-f','s16le','-ar','8000','-ac','1',
                '-i','pipe:0','-c:a','pcm_alaw','-f','alaw','pipe:1')


def contract(program,data):
    if len(program)!=0x1fffe or len(data)!=0x20000:
        raise ValueError('acquired compatibility mask extents differ')
    for address,raw in ((0x9435,ENCODER),(0x24d5,'f0739435'),
            (0x357f,'61f81f2b0040f82035894a16f07424d58a16f463f47d8821'),
            (0xa49a,FORMAT_SETUP),(0xa53b,'7714085e71941f2b')):
        raw=bytes.fromhex(raw)
        if program[2*address:2*address+len(raw)]!=raw:
            raise ValueError('native formatting sequence differs')
    if data[2*0xb3d7:2*0xb3d9]!=bytes.fromhex('ffc50045'):
        raise ValueError('native sign-mask pair differs')


def check_trace(text):
    if len(re.findall(r'nsm1_tone_boundary_compiler: field=00000028 value=00000dca commit=00000000 caller=002a2c29 t=0\.[0-9]+ observer_writes=0',text))!=1:
        raise ValueError('own MCU output-format publication differs')
    if (re.findall(r'nsm1_compand_window: frequency=([0-9a-f]{4}) flags=([0-9a-f]{4}) xpc=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
            !=[('1b58','0dca','0000','86.140000000')] or text.count('nsm1_compand_window:')!=1):
        raise ValueError('formatting window/conditional gate differs')
    if (re.findall(r'nsm1_compand_summary: input=(\d+) result=(\d+) serial=(\d+) retained=32 observer_writes=0',text)
            !=[('80','80','80')] or text.count('nsm1_compand_summary:')!=1):
        raise ValueError('formatting coverage differs')
    rows=re.findall(r'nsm1_compand_fetch: address=([0-9a-f]{4}) index=(\d+) pc=([0-9a-f]{8}) a=([0-9a-f]{10}) b=([0-9a-f]{10}) t=([0-9.]+) observer_writes=0',text)
    if len(rows)!=96 or text.count('nsm1_compand_fetch:')!=96:
        raise ValueError('formatting retained rows differ')
    inputs,outputs=[],[]
    for index in range(32):
        group=rows[index*3:index*3+3]
        if ([r[:3] for r in group]!=[(a,str(index+1),pc) for a,pc in (
                ('9435','00009436'),('3587','00003588'),('358a','0000358b'))]
                or not 86.14<float(group[0][5])<float(group[1][5])<float(group[2][5])<86.15):
            raise ValueError('formatter read-time sequence differs')
        before,result,serial=(int(r[3],16) for r in group)
        if before not in (3019,15099) or result not in (0xff92,0xffb8) or result!=serial:
            raise ValueError('native formatting values or serial preservation differ')
        inputs.append(before)
        outputs.append(result&255)
    if set(inputs)!={3019,15099} or set(outputs)!={0x92,0xb8}:
        raise ValueError('formatter positive controls lack both observed values')
    return inputs,bytes(outputs)


def reference(inputs,ffmpeg):
    linear=b''.join(struct.pack('<h',value) for value in inputs)
    command=[str(ffmpeg),*REFERENCE_ARGS]
    result=subprocess.run(command,input=linear,stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True,timeout=10)
    if len(result.stdout)!=len(inputs):
        raise ValueError('A-law reference extent differs')
    return result.stdout


def check(seed,reset,run,ffmpeg):
    check_clock(seed,reset,run)
    root=Path(__file__).resolve().parents[1]
    program=root/'roms/noki5110/nse1_rom4_dsp_program.bin'
    data=root/'roms/noki5110/nse1_rom4_dsp_data.bin'
    contract(program.read_bytes(),data.read_bytes())
    inputs,outputs=check_trace((run/'error.log').read_text())
    if outputs!=reference(inputs,ffmpeg):
        raise ValueError('observed native bytes differ from independent A-law reference')
    return dict(retained_samples=32,distinct_linear_inputs=sorted(set(inputs)),
                reference_codec='pcm_alaw',reference_linear_format='s16le',input_scaling=1,
                reference_command=[str(ffmpeg),*REFERENCE_ARGS],
                reference_binary_sha256=hashlib.sha256(ffmpeg.read_bytes()).hexdigest(),
                compatibility_program_sha256=hashlib.sha256(program.read_bytes()).hexdigest(),
                compatibility_data_sha256=hashlib.sha256(data.read_bytes()).hexdigest(),
                full_domain_conformance_proved=False,physical_codec_law_proved=False)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('seed','reset','run'):
        parser.add_argument(name,type=Path)
    parser.add_argument('--mame',type=Path)
    parser.add_argument('--ffmpeg',type=Path,default=shutil.which('ffmpeg'))
    args=parser.parse_args()
    try:
        if args.ffmpeg is None:
            raise ValueError('FFmpeg is required for the independent reference')
        seed,reset,run=(getattr(args,name).resolve() for name in ('seed','reset','run'))
        if args.mame:
            execute(args.mame.resolve(),seed,reset,run,observer=Path(__file__).with_name('nsm1_factory_compand.lua'))
        result=check(seed,reset,run,args.ffmpeg.resolve())
        (run/'companding_result.json').write_text(json.dumps(result,indent=2)+'\n')
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as exc:
        parser.exit(1,f'NSM-1 native formatting: FAIL: {exc}\n')
    print('NSM-1 observed native formatting matches A-law reference; full domain/physical codec law unproved')


if __name__=='__main__':
    main()
