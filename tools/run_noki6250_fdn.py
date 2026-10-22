#!/usr/bin/env python3
"""Physical NHM-3 FDN On/cold-On/Off/cold-Off with inherited card storage."""
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
from tools.run_noki6250_pin2_cold import mame_command
from tools.run_noki6250_pin_retry_cold import hashes,read_evidence
from tools.noki6250_fdn_check import initial_card,check,check_frames
from tools.noki6250_coherent_registration_check import verify as registration
from tools.run_sip_handset_gate import verify_handset_health


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
        shared=root/'tools/noki6250_fdn.lua'
        fixture_hash=hashlib.sha256(shared.read_bytes()).hexdigest()
        for phase,(name,script) in enumerate((('on','noki6250_fdn_enable.lua'),
                ('cold-on','noki6250_fdn.lua'),('off','noki6250_fdn_disable.lua'),
                ('cold-off','noki6250_fdn.lua'))):
            directory=run/name
            if previous is None:
                prepare_run(directory,root)
                card=directory/'nvram/nhm3hle/sim_card'
                card.parent.mkdir(parents=True)
                card.write_bytes(initial_card(0))
                apply_coherent_config(directory/'cfg/nhm3hle.cfg',root/'fixtures/noki6250_host_gsm900/nhm3hle.cfg')
            else:
                directory.mkdir()
                for component in ('nvram','cfg','roms'):
                    shutil.copytree(previous/component,directory/component)
                if hashes(directory/'nvram')!=hashes(previous/'nvram'):
                    raise ValueError('FDN cold NVRAM differs from predecessor')
            card=directory/'nvram/nhm3hle/sim_card'
            initial=card.read_bytes()
            if initial!=initial_card(phase):
                raise ValueError('FDN cold input does not match its whole-card predecessor contract')
            (directory/'initial_sim_card.bin').write_bytes(initial)
            script=root/'tools'/script
            if hashlib.sha256(binary.read_bytes()).hexdigest()!=binary_hash or hashlib.sha256(shared.read_bytes()).hexdigest()!=fixture_hash:
                raise ValueError('FDN executable or shared physical fixture changed')
            command=mame_command(binary,root,directory,script,68 if phase%2==0 else 58)
            launch={'phase':name,'invocation_id':str(uuid.uuid4()),'command':command,
                    'initial_storage':hashes(directory/'nvram'),'mame_sha256':binary_hash,
                    'fixture_base_sha256':fixture_hash,'script_sha256':hashlib.sha256(script.read_bytes()).hexdigest()}
            (directory/'launch.json').write_text(json.dumps(launch,indent=2)+'\n')
            environment=os.environ.copy()
            environment.pop('NOKIA_DCT3_6250_PIN_ENTRY',None)
            with (directory/'console.log').open('w') as output:
                subprocess.run(command,cwd=directory,stdout=output,stderr=subprocess.STDOUT,
                               check=True,timeout=180,env=environment)
            verify_handset_health((directory/'console.log').read_text(errors='replace'))
            text=read_evidence(directory/'error.log')
            final=card.read_bytes()
            check(text,initial,final,phase)
            registration(text,final,preserved=previous is not None,require_host=False)
            check_frames(directory/'snap',phase)
            if any(hashes(path)!=digest for path,digest in protected.items()):
                raise ValueError('FDN lifecycle mutated predecessor artifacts')
            protected[directory]=hashes(directory)
            previous=directory
            launches.append(launch)
            print(f'6250 FDN {name}: verified',flush=True)
        (run/'acceptance.json').write_text(json.dumps({'phases':launches,'independent_processes':4,
            'research_hle':True,'native_speech':False},indent=2)+'\n')
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as error:
        parser.exit(1,f'6250 FDN lifecycle FAIL: {error}\n')
    print(f'6250 physical FDN On/cold-On/Off/cold-Off PASS: {run}')


if __name__=='__main__':main()
