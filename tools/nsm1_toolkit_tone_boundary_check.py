"""Bound own NSM-1 PLAY TONE before the native DSP publication interface."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_toolkit_check import check as check_hold,prepare
from tools.nsm1_v523_static_check import literal,read32,direct_call_candidates,instruction,SHA1
from capstone.arm import ARM_OP_IMM,ARM_OP_REG,ARM_REG_R0,ARM_REG_R1,ARM_REG_R5


def immediate(image,address,mnemonic,register):
    insn=instruction(image,address)
    if (insn.mnemonic!=mnemonic or len(insn.operands)!=2
            or insn.operands[0].type!=ARM_OP_REG or insn.operands[0].reg!=register
            or insn.operands[1].type!=ARM_OP_IMM):
        raise ValueError('control argument instruction differs')
    return insn.operands[1].imm


def contract(image):
    if hashlib.sha1(image).hexdigest()!=SHA1:
        raise ValueError('own NSM-1 executable differs')
    entries={field:read32(image,0x2a4aa0+4*field) for field in range(47)}
    if not all(0x2a4b5c<=target<=0x2a4d16 and target%2==0 for target in entries.values()):
        raise ValueError('control jump table extent differs')
    fields={field:literal(image,entries[field]) for field in range(0x20,0x26)}
    if fields!={0x20:0x100ae,0x21:0x100b0,0x22:0x100b2,0x23:0x100b4,0x24:0x100b6,0x25:0x100ac}:
        raise ValueError('own tone-control fields differ')
    if literal(image,0x2a4a80)!=0x10000:
        raise ValueError('compiler shared base differs')
    callers=direct_call_candidates(image,0x2a4a6c)
    if callers!=[0x2404f2,0x240e4c,0x240e56,0x2a2bf0,0x2a2c24,0x2a2c4c,0x2a4d5a]:
        raise ValueError('direct BL candidate census differs')
    direct_fields={site:immediate(image,site-4,'movs',ARM_REG_R0)
                   for site in (0x2404f2,0x240e4c,0x240e56)}
    if direct_fields!={0x2404f2:0x12,0x240e4c:8,0x240e56:9}:
        raise ValueError('direct candidate fields differ')
    references=[]
    start=0
    while (start:=image.find(bytes.fromhex('002a4d57'),start))!=-1:
        references.append(0x200000+start)
        start+=1
    if references!=[0x2dcefc]:
        raise ValueError('literal wrapper-pointer inventory differs')
    if (literal(image,0x2b2ca8)!=0x2dcea8 or read32(image,0x2dcefc)!=0x2a4d57
            or immediate(image,0x2b2f1a,'movs',ARM_REG_R1)!=0xd0
            or immediate(image,0x2b2f22,'subs',ARM_REG_R1)!=1
            or immediate(image,0x2b2e74,'movs',ARM_REG_R0)!=0xd2):
        raise ValueError('command table or D1/D2 route differs')
    branch,call,indirect=(instruction(image,p) for p in (0x2b2f26,0x2b2f5e,0x2b2d8e))
    if (branch.mnemonic!='beq' or branch.operands[0].type!=ARM_OP_IMM or branch.operands[0].imm!=0x2b2f5c
            or call.mnemonic!='bl' or call.operands[0].type!=ARM_OP_IMM or call.operands[0].imm!=0x2b2c9a
            or indirect.mnemonic!='bx' or indirect.operands[0].type!=ARM_OP_REG or indirect.operands[0].reg!=ARM_REG_R5):
        raise ValueError('command indirect call chain differs')
    return dict(compiler=0x2a4a6c,table_records=47,fields=fields,
                direct_bl_candidates=callers,direct_candidate_fields=direct_fields,
                command_table=dict(base=0x2dcea8,index=0x15,entry=0x2dcefc,
                                   wrapper=0x2a4d56,request=0xd1,reply=0xd2,
                                   literal_wrapper_references=references,
                                   ordinary_tone_producer_proved=False),
                indirect_callers_closed=False)


def check_trace(text):
    summary=re.findall(r'nsm1_tone_boundary_summary: start=85 end=91.5 mcu_early=(\d+) mcu=(\d+) dsp=(\d+) io=(\d+) compiler_early=(\d+) compiler=(\d+) observer_writes=0',text)
    if summary!=[('70514','0','0','2816','7','0')] or text.count('nsm1_tone_boundary_summary:')!=1:
        raise ValueError('scoped publication census or positive controls differ')
    for kind in ('mcu','dsp'):
        if f'nsm1_tone_boundary_{kind}:' in text:
            raise ValueError('unexpected late publication')
    early=re.findall(r'nsm1_tone_boundary_early: address=([0-9a-f]{8}) value=([0-9a-f]{8}) mask=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0',text)
    if len(early)!=8 or text.count('nsm1_tone_boundary_early:')!=8 or any(not 0<=float(t)<5 or not int(m,16) or not 0x10000<=(int(a,16)&~0x8f000)<=0x10fff for a,v,m,t in early):
        raise ValueError('shared-aperture observation lacks early positives')
    compiler=re.findall(r'nsm1_tone_boundary_compiler: field=([0-9a-f]{8}) value=([0-9a-f]{8}) commit=([0-9a-f]{8}) caller=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0',text)
    if (len(compiler)!=7 or text.count('nsm1_tone_boundary_compiler:')!=7
            or [int(row[0],16) for row in compiler]!=[8,9,0x28,0x2d,0x2e,0x2d,0x2e]
            or any(not 0<=float(row[-1])<5 for row in compiler)):
        raise ValueError('compiler entry lacks early positives or executes late')
    io=re.findall(r'nsm1_tone_boundary_io: address=([0-9a-f]{4}) value=([0-9a-f]{4}) mask=([0-9a-f]{4}) pc=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
    if (len(io)!=2816 or text.count('nsm1_tone_boundary_io:')!=2816
            or any(not 85<=float(t)<91.5 or m!='ffff' for a,v,m,p,t in io)
            or Counter((a,v) for a,v,m,p,t in io)!=Counter({('0021','0e82'):1408,('0021','0682'):1408})):
        raise ValueError('native I/O activity or observation horizon differs')


def check(source,run):
    check_hold(source,run,tone='hold')
    result=contract((run/'nvram/nsm1r4t/flash').read_bytes())
    check_trace((run/'error.log').read_text())
    return result


def execute(binary,source,run):
    prepare(source,run,tone='hold')
    script=Path(__file__).with_suffix('.lua').with_name('nsm1_toolkit_tone_boundary.lua')
    command=[str(binary),'nsm1r4t','-rompath',str(Path(__file__).resolve().parents[1]/'roms'),
             '-noreadconfig','-video','none','-sound','none','-nothrottle',
             '-joystickprovider','none','-midiprovider','none','-debug','-debugger','none',
             '-log','-verbose','-seconds_to_run','92','-autoboot_delay','0','-autoboot_script',str(script.resolve())]
    (run/'invocation.json').write_text(json.dumps(dict(command=command,
        mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        script_sha256=hashlib.sha256(script.read_bytes()).hexdigest()),indent=2)+'\n')
    with (run/'console.log').open('w') as output:
        result=subprocess.run(command,cwd=run,stdout=output,stderr=subprocess.STDOUT,timeout=180)
    (run/'exit_status.json').write_text(json.dumps(dict(returncode=result.returncode))+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path);parser.add_argument('run',type=Path)
    parser.add_argument('--mame',type=Path)
    args=parser.parse_args()
    try:
        source,run=args.source.resolve(),args.run.resolve()
        if args.mame:
            execute(args.mame.resolve(),source,run)
        check(source,run)
    except (OSError,ValueError,KeyError,subprocess.TimeoutExpired) as error:
        parser.exit(1,f'NSM-1 tone publication boundary FAIL: {error}\n')
    print('PASS: no observed late compiler entry or HPI publication; early positives and native I/O retained; audio unproved')


if __name__=='__main__':
    main()
