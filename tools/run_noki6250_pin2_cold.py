#!/usr/bin/env python3
"""Install PIN2 physically, then use it in an independent cold Settings change."""
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
from tools.noki6250_pin2_change_check import check,FRAMES
from tools.noki6250_pin_retry_check import registered_card
from tools.noki6210_puk_check import check_rgb_frame
from tools.run_noki6250_pin_retry_cold import hashes,read_evidence
from tools.noki6250_coherent_registration_check import verify as registration
from tools.run_sip_handset_gate import verify_handset_health


def check_cold_input(initial,predecessor):
    expected=bytearray(registered_card())
    expected[3492:3500]=b'8765\xff\xff\xff\xff'
    if initial!=predecessor or initial!=bytes(expected):
        raise ValueError('cold PIN2 input must retain the complete physically installed predecessor card')


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
        first,cold=run/'install',run/'cold'
        first_command=[sys.executable,str(root/'tools/run_noki6250_acceptance.py'),
                       str(first),'--scenario','pin2-change','--mame',str(binary)]
        first_launch={'invocation_id':str(uuid.uuid4()),'command':first_command,
                      'mame_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
                      'fixture_base_sha256':hashlib.sha256((root/'tools/noki6250_pin2_change.lua').read_bytes()).hexdigest()}
        (run/'install_launch.json').write_text(json.dumps(first_launch,indent=2)+'\n')
        environment=os.environ.copy()
        environment.pop('NOKIA_DCT3_6250_PIN_ENTRY',None)
        with (run/'install_launcher.log').open('w') as output:
            subprocess.run(first_command,stdout=output,stderr=subprocess.STDOUT,
                           check=True,timeout=180,env=environment)
        first_launch['mame_command']=json.loads((first/'acceptance.json').read_text())['command']
        (run/'install_launch.json').write_text(json.dumps(first_launch,indent=2)+'\n')
        frozen=hashes(first)
        cold.mkdir()
        for name in ('nvram','cfg','roms'):
            shutil.copytree(first/name,cold/name)
        if hashes(cold/'nvram')!=hashes(first/'nvram'):
            raise ValueError('cold PIN2 storage differs from predecessor')
        initial=(cold/'nvram/nhm3hle/sim_card').read_bytes()
        check_cold_input(initial,(first/'nvram/nhm3hle/sim_card').read_bytes())
        (cold/'initial_sim_card.bin').write_bytes(initial)
        command=json.loads((first/'acceptance.json').read_text())['command']
        command[command.index('-rompath')+1]=f'{cold / "roms"};{root / "roms"}'
        script=root/'tools/noki6250_pin2_change_cold.lua'
        command[command.index('-autoboot_script')+1]=str(script)
        launch={'invocation_id':str(uuid.uuid4()),'command':command,
                'initial_storage':hashes(cold/'nvram'),
                'mame_sha256':first_launch['mame_sha256'],
                'fixture_base_sha256':first_launch['fixture_base_sha256'],
                'script_sha256':hashlib.sha256(script.read_bytes()).hexdigest()}
        if launch['invocation_id']==first_launch['invocation_id']:
            raise ValueError('cold PIN2 launch must be independent')
        (cold/'launch.json').write_text(json.dumps(launch,indent=2)+'\n')
        with (cold/'console.log').open('w') as output:
            subprocess.run(command,cwd=cold,stdout=output,stderr=subprocess.STDOUT,
                           check=True,timeout=180,env=environment)
        verify_handset_health((cold/'console.log').read_text(errors='replace'))
        text=read_evidence(cold/'error.log')
        card=(cold/'nvram/nhm3hle/sim_card').read_bytes()
        check(text,card,old='8765',new='2468')
        registration(text,card,preserved=True,require_host=False)
        for second,digest in FRAMES.items():
            check_rgb_frame(cold/f'snap/6250_pin2_key_{second}.png',digest)
        if hashes(first)!=frozen:
            raise ValueError('cold PIN2 run mutated predecessor artifacts')
        (run/'acceptance.json').write_text(json.dumps({'phases':[first_launch,launch],
            'independent_processes':2,'research_hle':True,'native_speech':False},indent=2)+'\n')
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as error:
        parser.exit(1,f'6250 cold PIN2 FAIL: {error}\n')
    print(f'6250 installed PIN2 cold physical authentication/change PASS: {run}')


if __name__=='__main__':main()
