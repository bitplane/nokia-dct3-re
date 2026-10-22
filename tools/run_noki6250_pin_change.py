#!/usr/bin/env python3
"""Fresh ordinary NHM-3 PIN1 change followed by cold replacement authentication."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.run_noki6250_acceptance import prepare_run,apply_coherent_config
from tools.make_sim_card_profile import make_profile
from tools.run_noki6250_pin2_cold import mame_command
from tools.run_noki6250_pin_retry_cold import hashes,read_evidence,check_puk_phase,check_puk_frames
from tools.noki6250_coherent_registration_check import verify as registration
from tools.run_sip_handset_gate import verify_handset_health
from tools.noki6250_pin_change_check import check,check_frames


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run',type=Path)
    parser.add_argument('--mame',type=Path)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    binary=(args.mame or root/'mame/mame').resolve()
    try:
        run=args.run.resolve()
        run.mkdir(parents=True,exist_ok=False)
        previous=None
        protected={}
        launches=[]
        binary_hash=hashlib.sha256(binary.read_bytes()).hexdigest()
        for name,script,seconds in (('change','noki6250_pin_change.lua',84),
                                    ('reauth','noki6250_pin_rotated.lua',40)):
            phase=run/name
            if previous is None:
                prepare_run(phase,root)
                card=phase/'nvram/nhm3hle/sim_card'
                card.parent.mkdir(parents=True)
                card.write_bytes(make_profile(True))
                apply_coherent_config(phase/'cfg/nhm3hle.cfg',root/'fixtures/noki6250_host_gsm900/nhm3hle.cfg')
            else:
                phase.mkdir()
                for component in ('nvram','cfg','roms'):
                    shutil.copytree(previous/component,phase/component)
                if hashes(phase/'nvram')!=hashes(previous/'nvram'):
                    raise ValueError('cold PIN1 replacement storage differs from predecessor')
            card=phase/'nvram/nhm3hle/sim_card'
            initial=card.read_bytes()
            (phase/'initial_sim_card.bin').write_bytes(initial)
            script=root/'tools'/script
            if hashlib.sha256(binary.read_bytes()).hexdigest()!=binary_hash:
                raise ValueError('PIN1 executable changed between phases')
            command=mame_command(binary,root,phase,script,seconds)
            launch={'phase':name,'invocation_id':str(uuid.uuid4()),'command':command,
                    'initial_storage':hashes(phase/'nvram'),'mame_sha256':binary_hash,
                    'script_sha256':hashlib.sha256(script.read_bytes()).hexdigest()}
            (phase/'launch.json').write_text(json.dumps(launch,indent=2)+'\n')
            environment=os.environ.copy()
            environment.pop('NOKIA_DCT3_6250_PIN_ENTRY',None)
            with (phase/'console.log').open('w') as output:
                subprocess.run(command,cwd=phase,stdout=output,stderr=subprocess.STDOUT,
                               check=True,timeout=180,env=environment)
            verify_handset_health((phase/'console.log').read_text(errors='replace'))
            text=read_evidence(phase/'error.log')
            final=card.read_bytes()
            if name=='change':
                check(text,initial,final)
                check_frames(phase/'snap')
            else:
                check_puk_phase(text,initial,final,'reauth')
                check_puk_frames(phase,'reauth')
            registration(text,final,preserved=previous is not None,require_host=False)
            if any(hashes(path)!=digest for path,digest in protected.items()):
                raise ValueError('cold PIN1 phase mutated predecessor evidence')
            protected[phase]=hashes(phase)
            launches.append(launch)
            previous=phase
            print(f'6250 PIN1 {name}: verified',flush=True)
        (run/'acceptance.json').write_text(json.dumps({'phases':launches,'independent_processes':2,
            'research_hle':True,'native_speech':False},indent=2)+'\n')
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as error:
        parser.exit(1,f'6250 ordinary PIN1 change FAIL: {error}\n')
    print(f'6250 ordinary PIN1 change and cold replacement authentication PASS: {run}')


if __name__=='__main__':main()
