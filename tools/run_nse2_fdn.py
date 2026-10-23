"""Run physical NSE-2 FDN activation and cold persistence in isolated processes."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nse2_fdn_check import check_phase, check_record
from tools.nse2_sim_check import verify_pin2_input, verify_security_attempt
from tools.run_nse2_native import prepare_native
from tools.run_nse2_puk2_exhaustion import run_stage
from tools.sim_fdn_profile import extend


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory',type=Path)
    parser.add_argument('--mame',type=Path)
    parser.add_argument('--records',action='store_true',help='also save/read FDN and reject wrong PIN2')
    parser.add_argument('--edit-lifecycle',action='store_true',help='also edit/cold-read/cancel-erase/erase/cold-empty')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    run=args.run_directory.resolve()
    mame=(args.mame or root/'mame/mame').resolve()
    try:
        run.mkdir(parents=True,exist_ok=False)
        seed=run/'seed'
        command=[sys.executable,str(root/'tools/run_nse2_native.py'),str(seed),
                 '--mame',str(mame),'--record-checksum-only','--nominal-vbat',
                 '--insert-sim','--security-code','--phone-retry-zero','--seconds','18']
        with (run/'seed.log').open('w') as stream:
            run_stage(command,root,stream)
        source=seed/'nvram/nse2r4v/sim_card'
        base=source.read_bytes()
        verify_pin2_input(base)
        card=extend(base,True)
        stages=[]
        names=['on','cold-on','off','cold-off']
        if args.records or args.edit_lifecycle:
            names+=['write','read','wrong']
        if args.edit_lifecycle:
            names+=['edit','read-edited','cancel','erase','empty']
        for phase,name in enumerate(names):
            if name=='wrong':
                source=run/'cold-off/nvram/nse2r4v/sim_card'
                card=source.read_bytes()
            elif name=='edit':
                source=run/'write/nvram/nse2r4v/sim_card'
                card=source.read_bytes()
            target=run/name
            report=prepare_native(root,target,record_checksum=True,nominal_vbat=True,
                                  insert_sim=True,phone_retry_zero=True)
            before=card
            (target/'nvram/nse2r4v/sim_card').write_bytes(before)
            (target/'initial_sim_card.bin').write_bytes(before)
            script=target/'physical_input.lua'
            if phase<4:
                setting=f'_G.nse2_fdn_phase={phase}'
                fixture=root/'tools/nse2_fdn_fixture.lua'
            else:
                setting=f'_G.nse2_fdn_record_mode={json.dumps(name)}'
                fixture=root/'tools/nse2_fdn_record_fixture.lua'
            script.write_text(f'{setting}\ndofile({json.dumps(str(fixture))})\n')
            command=[str(arg).replace(str(seed),str(target)) for arg in
                     json.loads((seed/'inputs.json').read_text())['command']]
            command[command.index('-seconds_to_run')+1]='30'
            command[command.index('-autoboot_script')+1]=str(script)
            report.update(command=command,fdn_phase=phase,card_source=str(source),
                          source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                          card_sha256=hashlib.sha256(before).hexdigest(),
                          explicit_fdn_profile=True,adn_access_when_invalid=True,
                          mame_sha256=json.loads((seed/'inputs.json').read_text())['mame_sha256'])
            (target/'inputs.json').write_text(json.dumps(report,indent=2)+'\n')
            with (target/'console.log').open('w') as stream:
                run_stage(command,target,stream)
            console=(target/'console.log').read_text(errors='replace')
            if 'LUA ERROR' in console or 'Error in' in console:
                raise ValueError('FDN observer failed')
            text=(target/'error.log').read_text(errors='replace')
            source=target/'nvram/nse2r4v/sim_card'
            card=source.read_bytes()
            result=check_phase(text,before,card,phase) if phase<4 else check_record(text,before,card,name)
            with Image.open(target/'snap/nse2_security_result.png') as frame:
                verify_security_attempt(text,frame.convert('L').tobytes(),retry_zero=True)
            with Image.open(target/'snap/nse2_fdn_final.png') as frame:
                pixels=frame.convert('L').tobytes()
                result['frame_sha256']=hashlib.sha256(pixels).hexdigest()
                if frame.size!=(84,48):
                    raise ValueError('FDN frame geometry differs')
                cold_frames={1:'083eb48738463b6235e08ea44c2e4e24d57cc3c6fdffa37a2e413062c6af3f64',
                             3:'e8e8c3e26c5a79333b2b7870fb8f8e12d7f7e8bc167913097508a004f2891f20',
                             4:'38d49c2e16c4d71080cb49d07f4756964393347b3393d6644f7f10cb7b07c580',
                             5:'1ddc4a9f01b9d098073e2f3cfb4cd3aa190abb0584fb0b3b21bd88c4a3e213b7',
                             6:'1ec8ac307ef10d0426a2f4769cdec97b6bbbdb26c057b41a3838a9a8ef8ded6c',
                             7:'38d49c2e16c4d71080cb49d07f4756964393347b3393d6644f7f10cb7b07c580',
                             8:'20f9bb82e048cf8d6385e0c56c8e37d0245ed3cafb0c55b1714f3ee352dca1f9',
                             9:'38d49c2e16c4d71080cb49d07f4756964393347b3393d6644f7f10cb7b07c580',
                             10:'8d58f1afb3d10c470efd80667818c969ecead7cda16935b0d6ce3815d0457052',
                             11:'34478708c13e25cc3cb0778a9fa40f9cb812c6eb56636909c1ce133a1d616900'}
                if phase in cold_frames and result['frame_sha256']!=cold_frames[phase]:
                    raise ValueError('FDN settings/record result frame differs')
            (target/'acceptance.json').write_text(json.dumps(result,indent=2)+'\n')
            stages.append(dict(name=name,**result))
            (run/'stages.json').write_text(json.dumps(stages,indent=2)+'\n')
            print(f'NSE-2 FDN {name} PASS',flush=True)
        (run/'acceptance.json').write_text(json.dumps(dict(stages=stages,
            native_long_run_stability=False,call_restrictions_verified=False),indent=2)+'\n')
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        parser.exit(1,f'NSE-2 FDN FAIL: {error}\n')


if __name__=='__main__':
    main()
