"""Isolated native ROM6 soft-input SCH decoder; not a boot or RF gate."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
if __package__ in (None,''): sys.path.insert(0,str(ROOT))
from tools.gsm_sch_signal import sch_information_bits,sch_coded_bits,sch_uncoded_bits,sch_burst_bits,gmsk_burst_samples
from tools.rom6_adc_filter_check import COEFFICIENTS,fir_word
from tools.run_rom6_native import prepare,execute


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--entry-dir',type=Path,help='reuse a passive entry capture instead of making a fresh one')
    parser.add_argument('--frame',type=int,default=1)
    parser.add_argument('--bsic',type=int,default=42)
    parser.add_argument('--capture-frontend',action='store_true',help='also retain the preceding native SCH sample-stage entries')
    parser.add_argument('--frontend',action='store_true',help='exercise the native sample frontend before Viterbi/CRC')
    args=parser.parse_args()
    if args.frontend: args.capture_frontend=True
    information=sch_information_bits(args.frame,args.bsic)
    coded=sch_coded_bits(information)
    uncoded=sch_uncoded_bits(information)
    parent=ROOT/'tmp/rom6-receive-kernel'
    parent.mkdir(exist_ok=True)
    run=Path(tempfile.mkdtemp(prefix='sch-',dir=parent))
    print(f'ROM6 isolated SCH evidence: {run}',flush=True)
    entry=args.entry_dir
    if entry is None:
        capture=run/'capture';capture.mkdir()
        prepare(capture,(ROOT/'roms/noki8210/8210_5.31ppm_c.fls').read_bytes(),
                (ROOT/'roms/noki8210/8210 virgin eeprom 003d0000.fls').read_bytes(),machine='nsm3afs')
        script='rom6_receive_frontend.lua' if args.capture_frontend else 'rom6_receive_entry.lua'
        entry,_=execute(capture,'primary',script,4,ROOT/'mame/mame',machine='nsm3afs')
    text=(entry/'error.log').read_text()
    if text.count('rom6_receive_entries_complete: stages=f expected=f observer_writes=0')!=1:
        parser.error('missing complete passive native entry capture')
    if args.capture_frontend and text.count('rom6_receive_frontend_complete: stages=ff expected=ff observer_writes=0')!=1:
        parser.error('missing complete passive SCH frontend capture')
    program=(entry/'rom6_sch_decode_program.le16').read_bytes()
    data=(entry/'rom6_sch_decode_data.le16').read_bytes()
    if len(program)!=131072 or len(data)!=131072:
        parser.error('entry snapshots must each contain65536 LE16 words')
    if data[2*0x2f80:2*(0x2f80+78)]!=b'\x01\x00'*78:
        parser.error('reference failure no longer has the captured uniform+1 soft-input contract')
    original=(ROOT/'roms/nsm3r6/nsm3_rom6_program.be16').read_bytes()
    drom=(ROOT/'roms/nsm3r6/nsm3_rom6_data.be16').read_bytes()
    if any(data[2*i:2*i+2]!=drom[2*i:2*i+2][::-1] for i in range(0xb000,0x10000)):
        parser.error('decoder DROM tables differ from the acquired ROM6 image')
    for first,last in ((0x9042,0x9071),(0x9217,0x9233),(0x9268,0x9281),
                       (0x9315,0x9331),(0x4d9d,0x4da7),(0x4f3a,0x4f5f)):
        if any(program[2*i:2*i+2]!=original[2*i:2*i+2][::-1] for i in range(first,last)):
            parser.error('native decoder region differs from the acquired ROM6 image')
    for name in ('program','data'):
        shutil.copyfile(entry/f'rom6_sch_decode_{name}.le16',run/f'rom6_sch_decode_{name}.le16')
        if args.frontend:
            shutil.copyfile(entry/f'rom6_sch_samples_{name}.le16',run/f'rom6_sch_samples_{name}.le16')
    if args.frontend:
        front_program=(run/'rom6_sch_samples_program.le16').read_bytes()
        front_data=(run/'rom6_sch_samples_data.le16').read_bytes()
        if (len(front_program)!=131072 or len(front_data)!=131072 or
                any(front_program[2*i:2*i+2]!=original[2*i:2*i+2][::-1] for i in range(0x2000,0x10000)) or
                any(front_data[2*i:2*i+2]!=drom[2*i:2*i+2][::-1] for i in range(0xb000,0x10000))):
            parser.error('frontend resident code or DROM differs from the acquired ROM6 image')
        configuration=tuple(int.from_bytes(front_data[2*i:2*i+2],'little') for i in (0x524,0x525,0x526,0x250f,0x2f02))
        if configuration!=(0xdf85,0x6d1b,0xdf85,0x30,0x40):
            parser.error('frontend entry is outside the qualified filter/class configuration')
    # The native decoder itself pads its terminated tail with positive127:
    # positive soft input therefore represents encoded zero, not one.
    words=[(32 if bit==0 else -32)&0xffff for bit in coded]
    if args.frontend:
        iq=gmsk_burst_samples(sch_burst_bits(information),2)
        raw=[0]*32+[word&0xffff for pair in iq for word in pair]
        raw.extend([0]*(32+4*184-len(raw)))
        history=[0]*16;words=[]
        for number,value in enumerate(raw,1):
            history[(number-1)%16]=value
            if number>32 and number%4==0:
                words.extend(fir_word(history,COEFFICIENTS,(number-1)%16,component) for component in (0,1))
    words.extend(sum(bit<<(index%16) for index,bit in enumerate(uncoded[:35]) if index//16==word)
                 for word in range(3))
    words.append(0)
    (run/'sch_vector.le16').write_bytes(b''.join(word.to_bytes(2,'little') for word in words))
    report=dict(frame=args.frame,bsic=args.bsic,entry_directory=str(entry.resolve()),
                program_sha256=hashlib.sha256(program).hexdigest(),data_sha256=hashlib.sha256(data).hexdigest(),
                input_kind='368 independently filtered I,Q words' if args.frontend else '78 signed soft words, not ADC samples',physical_capture_qualified=False,
                sch_sample_frontend_qualified=False,camping_qualified=False)
    (run/'inputs.json').write_text(json.dumps(report,indent=2)+'\n')
    env=os.environ.copy();env['C54X_ROM6_RECEIVE_DIR']=str(run)
    if args.frontend: env['C54X_ROM6_RECEIVE_FRONTEND']='1'
    else: env.pop('C54X_ROM6_RECEIVE_FRONTEND',None)
    result=subprocess.run([sys.executable,str(ROOT/'tools/run_c54x_section.py'),'rom6_sch',
                    '--fixture-header',str(ROOT/'tests/firmware/rom6_receive_kernel.h'),
                    '--fixture-cases','2'],env=env,capture_output=True,text=True)
    (run/'kernel.log').write_text(result.stdout+result.stderr)
    report['accepted']=result.returncode==0
    report['native_filtered_iq_kernel_qualified']=args.frontend and report['accepted']
    report['raw_adc_preprocessing_qualified']=False
    (run/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print(result.stdout+result.stderr,end='')
    result.check_returncode()


if __name__=='__main__':
    try:
        main()
    except (OSError,ValueError,subprocess.CalledProcessError) as error:
        raise SystemExit(f'ROM6 isolated receive fixture rejected: {error}')
