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
from tools.noki6250_pin2_change_check import check,check_frames
from tools.noki6250_pin_retry_check import registered_card
from tools.run_noki6250_pin_retry_cold import hashes,read_evidence
from tools.noki6250_coherent_registration_check import verify as registration
from tools.run_sip_handset_gate import verify_handset_health


def check_cold_input(initial,predecessor,*,retries=3):
    expected=bytearray(registered_card())
    expected[3492:3500]=b'8765\xff\xff\xff\xff'
    if retries not in (1,2,3):
        raise ValueError('cold PIN2 input requires its qualified live allowance')
    expected[3517]=retries
    if initial!=predecessor or initial!=bytes(expected):
        raise ValueError('cold PIN2 input must retain the complete physically installed predecessor card')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run',type=Path)
    parser.add_argument('--mame',type=Path)
    parser.add_argument('--reject-first',action='store_true',help='reject the current PIN2 in a separate cold process before recovery')
    parser.add_argument('--exhaust-pin2',action='store_true',help='consume all three PIN2 allowances in independent cold Settings transactions')
    args=parser.parse_args()
    if args.reject_first and args.exhaust_pin2:
        parser.error('--exhaust-pin2 and --reject-first select different phase plans')
    root=Path(__file__).resolve().parents[1]
    binary=(args.mame or root/'mame/mame').resolve()
    try:
        run=args.run.resolve()
        run.mkdir(parents=True,exist_ok=False)
        first=run/'install'
        first_command=[sys.executable,str(root/'tools/run_noki6250_acceptance.py'),
                       str(first),'--scenario','pin2-change','--mame',str(binary)]
        first_launch={'phase':'install','invocation_id':str(uuid.uuid4()),'command':first_command,
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
        protected={first:hashes(first)}
        launches=[first_launch]
        source=first
        phases=[('reject',True,3),('cold',False,2)] if args.reject_first else [('cold',False,3)]
        if args.exhaust_pin2:
            phases=[(f'reject-{attempt}',True,4-attempt) for attempt in (1,2,3)]
        for name,rejected,attempts in phases:
            cold=run/name
            cold.mkdir()
            for component in ('nvram','cfg','roms'):
                shutil.copytree(source/component,cold/component)
            if hashes(cold/'nvram')!=hashes(source/'nvram'):
                raise ValueError('cold PIN2 storage differs from predecessor')
            initial=(cold/'nvram/nhm3hle/sim_card').read_bytes()
            check_cold_input(initial,(source/'nvram/nhm3hle/sim_card').read_bytes(),retries=attempts)
            (cold/'initial_sim_card.bin').write_bytes(initial)
            command=first_launch['mame_command'].copy()
            command[command.index('-rompath')+1]=f'{cold / "roms"};{root / "roms"}'
            script=root/'tools'/('noki6250_pin2_change_wrong.lua' if rejected else 'noki6250_pin2_change_cold.lua')
            command[command.index('-autoboot_script')+1]=str(script)
            if (hashlib.sha256(binary.read_bytes()).hexdigest()!=first_launch['mame_sha256'] or
                    hashlib.sha256((root/'tools/noki6250_pin2_change.lua').read_bytes()).hexdigest()!=first_launch['fixture_base_sha256']):
                raise ValueError('PIN2 executable or shared fixture changed between phases')
            launch={'phase':name,'invocation_id':str(uuid.uuid4()),'command':command,
                    'initial_storage':hashes(cold/'nvram'),
                    'mame_sha256':first_launch['mame_sha256'],
                    'fixture_base_sha256':first_launch['fixture_base_sha256'],
                    'script_sha256':hashlib.sha256(script.read_bytes()).hexdigest()}
            if launch['invocation_id'] in {item['invocation_id'] for item in launches}:
                raise ValueError('cold PIN2 launch must be independent')
            (cold/'launch.json').write_text(json.dumps(launch,indent=2)+'\n')
            with (cold/'console.log').open('w') as output:
                subprocess.run(command,cwd=cold,stdout=output,stderr=subprocess.STDOUT,
                               check=True,timeout=180,env=environment)
            verify_handset_health((cold/'console.log').read_text(errors='replace'))
            text=read_evidence(cold/'error.log')
            card=(cold/'nvram/nhm3hle/sim_card').read_bytes()
            check(text,card,old='8764' if rejected else '8765',new='2468',
                  rejected=rejected,stored='8765',initial_retries=attempts)
            registration(text,card,preserved=True,require_host=False)
            check_frames(cold/'snap',rejected=rejected,initial_retries=attempts)
            if any(hashes(path)!=digest for path,digest in protected.items()):
                raise ValueError('cold PIN2 run mutated predecessor artifacts')
            protected[cold]=hashes(cold)
            launches.append(launch)
            source=cold
            print(f'6250 PIN2 phase {name}: verified',flush=True)
        (run/'acceptance.json').write_text(json.dumps({'phases':launches,
            'independent_processes':len(launches),'rejected_change':args.reject_first,
            'pin2_exhausted':args.exhaust_pin2,
            'research_hle':True,'native_speech':False},indent=2)+'\n')
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as error:
        parser.exit(1,f'6250 cold PIN2 FAIL: {error}\n')
    label=('physical PIN2 exhaustion across cold Settings transactions' if args.exhaust_pin2 else
           'wrong-PIN2 rejection, durable retries and cold recovery' if args.reject_first else
           'installed PIN2 cold physical authentication/change')
    print(f'6250 {label} PASS: {run}')


if __name__=='__main__':main()
