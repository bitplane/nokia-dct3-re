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
from tools.nse2_fdn_check import check_phase
from tools.nse2_sim_check import verify_pin2_input, verify_security_attempt
from tools.run_nse2_native import prepare_native
from tools.run_nse2_puk2_exhaustion import run_stage
from tools.sim_fdn_profile import extend


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory',type=Path)
    parser.add_argument('--mame',type=Path)
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
        for phase,name in enumerate(('on','cold-on','off','cold-off')):
            target=run/name
            report=prepare_native(root,target,record_checksum=True,nominal_vbat=True,
                                  insert_sim=True,phone_retry_zero=True)
            before=card
            (target/'nvram/nse2r4v/sim_card').write_bytes(before)
            (target/'initial_sim_card.bin').write_bytes(before)
            script=target/'physical_input.lua'
            script.write_text(f'_G.nse2_fdn_phase={phase}\ndofile({json.dumps(str(root/"tools/nse2_fdn_fixture.lua"))})\n')
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
            result=check_phase(text,before,card,phase)
            with Image.open(target/'snap/nse2_security_result.png') as frame:
                verify_security_attempt(text,frame.convert('L').tobytes(),retry_zero=True)
            with Image.open(target/'snap/nse2_fdn_final.png') as frame:
                pixels=frame.convert('L').tobytes()
                result['frame_sha256']=hashlib.sha256(pixels).hexdigest()
                if frame.size!=(84,48):
                    raise ValueError('FDN frame geometry differs')
                cold_frames={1:'083eb48738463b6235e08ea44c2e4e24d57cc3c6fdffa37a2e413062c6af3f64',
                             3:'e8e8c3e26c5a79333b2b7870fb8f8e12d7f7e8bc167913097508a004f2891f20'}
                if phase in cold_frames and result['frame_sha256']!=cold_frames[phase]:
                    raise ValueError('cold FDN menu does not display persisted On/Off')
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
