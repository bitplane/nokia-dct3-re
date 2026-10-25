"""Classify native click formatter/reference differences without hiding them."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_factory_compand_check import contract,reference,REFERENCE_ARGS
from tools.nsm1_factory_clock_check import check as check_clock,execute,nanoseconds


def samples(text):
    marker='nsm1_click_summary: input=2000 result=2000 serial=2000 observer_writes=0'
    if text.count(marker)!=1 or text.count('nsm1_click_summary:')!=1:
        raise ValueError('click formatter coverage differs')
    rows=re.findall(r'nsm1_click_format: address=([0-9a-f]{4}) index=(\d+) a=([0-9a-f]{10}) b=([0-9a-f]{10}) t=([0-9.]+) observer_writes=0',text)
    if len(rows)!=6000 or text.count('nsm1_click_format:')!=6000:
        raise ValueError('click formatter rows missing or malformed')
    result=[]
    previous=0
    for i in range(2000):
        group=rows[i*3:i*3+3]
        times=[nanoseconds(r[4]) for r in group]
        if ([r[:2] for r in group]!=[(a,str(i+1)) for a in ('9435','3587','358a')]
                or not 84_000_000_000<=times[0]<times[1]<times[2]<84_250_000_000
                or times[0]<=previous or group[1][2]!=group[2][2]):
            raise ValueError('click formatter sequence or serial preservation differs')
        previous=times[2]
        value=int(group[0][2],16)
        signed=value-(1<<40) if value&(1<<39) else value
        result.append((signed,int(group[1][2],16)&255))
    return result


def compare(rows,expected):
    bounded=[(v,b) for v,b in rows if -32768<=v<=32767]
    if len(expected)!=len(bounded):
        raise ValueError('reference coverage differs')
    mismatches=[(v,b,r) for (v,b),r in zip(bounded,expected) if b!=r]
    nonzero=[r for r in mismatches if r[0]]
    if (len(rows)!=2000 or len(bounded)!=1984 or len({v for v,_ in bounded})!=538
            or sum(v<0 for v,_ in bounded)!=1 or sum(v==0 for v,_ in bounded)!=833
            or len(mismatches)!=838
            or any((v,b,r)!=(0,0x55,0xd5) for v,b,r in mismatches if not v)
            or nonzero!=[(2063,0x95,0xea),(2059,0x95,0xea),
                         (8254,0xb5,0x8a),(8237,0xb5,0x8a),(8217,0xb5,0x8a)]
            or [(v,b,r) for (v,b),r in zip(bounded,expected) if v<0]!=[(-13485,0x3f,0x3f)]):
        raise ValueError('bounded click formatting discrepancy changed')
    return dict(observed_samples=2000,signed16_samples=1984,distinct_signed16_inputs=538,
                out_of_range_excluded=16,zero_mismatches=833,
                nonzero_mismatches=nonzero,negative_match=[-13485,63],
                full_domain_conformance_proved=False,physical_codec_law_proved=False)


def check(seed,reset,run,ffmpeg):
    check_clock(seed,reset,run)
    root=Path(__file__).resolve().parents[1]
    program=(root/'roms/noki5110/nse1_rom4_dsp_program.bin').read_bytes()
    data=(root/'roms/noki5110/nse1_rom4_dsp_data.bin').read_bytes()
    contract(program,data)
    rows=samples((run/'error.log').read_text())
    bounded=[v for v,_ in rows if -32768<=v<=32767]
    result=compare(rows,reference(bounded,ffmpeg))
    result.update(reference_command=[str(ffmpeg),*REFERENCE_ARGS],
                  reference_binary_sha256=hashlib.sha256(ffmpeg.read_bytes()).hexdigest(),
                  compatibility_program_sha256=hashlib.sha256(program).hexdigest(),
                  compatibility_data_sha256=hashlib.sha256(data).hexdigest())
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('seed','reset','run'):
        parser.add_argument(name,type=Path)
    parser.add_argument('--mame',type=Path)
    parser.add_argument('--ffmpeg',type=Path,default=shutil.which('ffmpeg'))
    args=parser.parse_args()
    try:
        if args.ffmpeg is None:
            raise ValueError('FFmpeg reference required')
        seed,reset,run=(getattr(args,n).resolve() for n in ('seed','reset','run'))
        if args.mame:
            execute(args.mame.resolve(),seed,reset,run,
                    observer=Path(__file__).with_name('nsm1_factory_click_format.lua'))
        result=check(seed,reset,run,args.ffmpeg.resolve())
        (run/'click_format_result.json').write_text(json.dumps(result,indent=2)+'\n')
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as exc:
        parser.exit(1,f'NSM-1 native click formatting: FAIL: {exc}\n')
    print('NSM-1 bounded click discrepancies reproduced; not full A-law conformance')


if __name__=='__main__':
    main()
