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


def check_cold_input(initial,predecessor,*,retries=3,pin2='8765'):
    expected=bytearray(registered_card())
    if len(pin2)!=4 or not pin2.isascii() or not pin2.isdigit():
        raise ValueError('cold PIN2 input requires its qualified four-digit credential')
    expected[3492:3500]=pin2.encode('ascii')+b'\xff'*4
    if retries not in (0,1,2,3):
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
    parser.add_argument('--recover-puk2',action='store_true',help='recover the physically exhausted PIN2 and cold-use the installed replacement')
    args=parser.parse_args()
    if args.reject_first and args.exhaust_pin2:
        parser.error('--exhaust-pin2 and --reject-first select different phase plans')
    if args.recover_puk2 and not args.exhaust_pin2:
        parser.error('--recover-puk2 requires --exhaust-pin2')
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
        phases=[('reject','reject',3),('cold','change',2)] if args.reject_first else [('cold','change',3)]
        if args.exhaust_pin2:
            phases=[(f'reject-{attempt}','reject',4-attempt) for attempt in (1,2,3)]
        if args.recover_puk2:
            phases += [('puk2','puk2',0),('recheck','recheck',3)]
        scripts={'reject':'noki6250_pin2_change_wrong.lua','change':'noki6250_pin2_change_cold.lua',
                 'puk2':'noki6250_puk2_recovery.lua','recheck':'noki6250_puk2_reauth.lua'}
        for name,kind,attempts in phases:
            rejected=kind=='reject'
            cold=run/name
            cold.mkdir()
            for component in ('nvram','cfg','roms'):
                shutil.copytree(source/component,cold/component)
            if hashes(cold/'nvram')!=hashes(source/'nvram'):
                raise ValueError('cold PIN2 storage differs from predecessor')
            initial=(cold/'nvram/nhm3hle/sim_card').read_bytes()
            check_cold_input(initial,(source/'nvram/nhm3hle/sim_card').read_bytes(),retries=attempts,
                             pin2='2468' if kind=='recheck' else '8765')
            (cold/'initial_sim_card.bin').write_bytes(initial)
            command=first_launch['mame_command'].copy()
            command[command.index('-rompath')+1]=f'{cold / "roms"};{root / "roms"}'
            script=root/'tools'/scripts[kind]
            command[command.index('-autoboot_script')+1]=str(script)
            if kind=='puk2':command[command.index('-seconds_to_run')+1]='90'
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
            if kind=='puk2':
                from tools.noki6250_puk2_check import check as check_puk2,check_frames as check_puk2_frames
                check_puk2(text,initial,card)
                check_puk2_frames(cold/'snap')
            else:
                check(text,card,old='2468' if kind=='recheck' else '8764' if rejected else '8765',
                      new='1357' if kind=='recheck' else '2468',rejected=rejected,
                      stored='8765',initial_retries=attempts)
            registration(text,card,preserved=True,require_host=False)
            if kind!='puk2':check_frames(cold/'snap',rejected=rejected,initial_retries=attempts)
            if any(hashes(path)!=digest for path,digest in protected.items()):
                raise ValueError('cold PIN2 run mutated predecessor artifacts')
            protected[cold]=hashes(cold)
            launches.append(launch)
            source=cold
            print(f'6250 PIN2 phase {name}: verified',flush=True)
        (run/'acceptance.json').write_text(json.dumps({'phases':launches,
            'independent_processes':len(launches),'rejected_change':args.reject_first,
            'pin2_exhausted':args.exhaust_pin2,
            'puk2_recovered':args.recover_puk2,
            'research_hle':True,'native_speech':False},indent=2)+'\n')
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as error:
        parser.exit(1,f'6250 cold PIN2 FAIL: {error}\n')
    label=('physical PUK2 recovery and cold new-PIN2 reuse' if args.recover_puk2 else
           'physical PIN2 exhaustion across cold Settings transactions' if args.exhaust_pin2 else
           'wrong-PIN2 rejection, durable retries and cold recovery' if args.reject_first else
           'installed PIN2 cold physical authentication/change')
    print(f'6250 {label} PASS: {run}')


if __name__=='__main__':main()
