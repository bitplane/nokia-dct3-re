#!/usr/bin/env python3
"""Independent cold PIN2/PUK2 Settings lifecycles with exact inherited storage."""
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
from tools.run_noki6250_pin_retry_cold import hashes,read_evidence,validate_retained_launch
from tools.noki6250_coherent_registration_check import verify as registration
from tools.run_sip_handset_gate import verify_handset_health


def check_cold_input(initial,predecessor,*,retries=3,pin2='8765',puk2=10):
    expected=bytearray(registered_card())
    if len(pin2)!=4 or not pin2.isascii() or not pin2.isdigit():
        raise ValueError('cold PIN2 input requires its qualified four-digit credential')
    expected[3492:3500]=pin2.encode('ascii')+b'\xff'*4
    if retries not in (0,1,2,3):
        raise ValueError('cold PIN2 input requires its qualified live allowance')
    expected[3517]=retries
    if type(puk2) is not int or not 0<=puk2<=10:
        raise ValueError('cold PIN2 input requires its qualified PUK2 allowance')
    expected[3519]=puk2
    if initial!=predecessor or initial!=bytes(expected):
        raise ValueError('cold PIN2 input must retain the complete physically installed predecessor card')


def mame_command(binary,root,phase,script,seconds=84):
    return [str(binary),'nhm3hle','-rompath',f'{phase / "roms"};{root / "roms"}',
            '-nvram_directory','nvram','-cfg_directory','cfg','-noreadconfig',
            '-autoboot_script',str(script),'-autoboot_delay','0','-seconds_to_run',str(seconds),
            '-video','none','-sound','none','-nothrottle','-log','-verbose']


def verify_install_capture(first,command):
    manifest=json.loads((first/'acceptance.json').read_text())
    if (not isinstance(manifest,dict) or manifest.get('machine')!='nhm3hle' or manifest.get('scenario')!='pin2-change' or
            manifest.get('command')!=command):
        raise ValueError('PIN2 installation manifest differs from the own first-phase command')
    verify_handset_health((first/'console.log').read_text(errors='replace'))
    text=read_evidence(first/'error.log')
    card=(first/'nvram/nhm3hle/sim_card').read_bytes()
    check(text,card)
    registration(text,card,require_host=False)
    check_frames(first/'snap')


def completed_legacy_launches(run):
    path=run/'acceptance.json'
    if not path.exists():return set()
    manifest=json.loads(path.read_text())
    if not isinstance(manifest,dict):
        raise ValueError('retained acceptance manifest must be an object')
    phases=manifest.get('phases')
    if (not isinstance(phases,list) or type(manifest.get('independent_processes')) is not int or
            manifest['independent_processes']!=len(phases)):
        raise ValueError('retained acceptance manifest is not a completed phase chain')
    identities=[]
    for phase in phases:
        try:
            identity=phase['invocation_id']
            uuid.UUID(identity)
        except (KeyError,TypeError,ValueError,AttributeError) as error:
            raise ValueError('retained acceptance has an invalid launch identity') from error
        identities.append(identity)
    if len(set(identities))!=len(identities):
        raise ValueError('retained acceptance reuses independent launch identities')
    return set(identities)


def verify_process_completion(phase,identity,legacy):
    path=phase/'process_exit.json'
    if path.exists():
        result=json.loads(path.read_text())
        if (not isinstance(result,dict) or result.get('invocation_id')!=identity or
                type(result.get('returncode')) is not int or result['returncode']!=0):
            raise ValueError('retained phase has no matching successful process exit')
    elif identity not in legacy:
        raise ValueError('retained phase has no completed-process evidence')


def record_process_completion(phase,identity):
    (phase/'process_exit.json').write_text(json.dumps({'invocation_id':identity,'returncode':0})+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run',type=Path)
    parser.add_argument('--mame',type=Path)
    parser.add_argument('--reject-first',action='store_true',help='reject the current PIN2 in a separate cold process before recovery')
    parser.add_argument('--exhaust-pin2',action='store_true',help='consume all three PIN2 allowances in independent cold Settings transactions')
    parser.add_argument('--recover-puk2',action='store_true',help='recover the physically exhausted PIN2 and cold-use the installed replacement')
    parser.add_argument('--wrong-puk2-first',action='store_true',help='reject one PUK2 in a separate cold process before recovery')
    parser.add_argument('--puk2-rejections',type=int,default=0,help='consume 1-10 PUK2 attempts in separate cold processes after PIN2 exhaustion')
    parser.add_argument('--permanent-recheck',action='store_true',help='cold-authenticate PIN1 and reopen the PIN2 setting after ten rejected PUK2 attempts')
    parser.add_argument('--resume',action='store_true',help='revalidate completed phase captures and launch only absent phases; never repeat retained credential input')
    args=parser.parse_args()
    if args.reject_first and args.exhaust_pin2:
        parser.error('--exhaust-pin2 and --reject-first select different phase plans')
    if args.recover_puk2 and not args.exhaust_pin2:
        parser.error('--recover-puk2 requires --exhaust-pin2')
    if args.wrong_puk2_first and not args.recover_puk2:
        parser.error('--wrong-puk2-first requires --recover-puk2')
    if args.puk2_rejections:
        if not 1<=args.puk2_rejections<=10 or not args.exhaust_pin2:
            parser.error('--puk2-rejections requires --exhaust-pin2 and a count from 1 to 10')
        if args.wrong_puk2_first or (args.recover_puk2 and args.puk2_rejections==10):
            parser.error('--puk2-rejections conflicts with --wrong-puk2-first or recovery after permanent loss')
    if args.permanent_recheck and args.puk2_rejections!=10:
        parser.error('--permanent-recheck requires --puk2-rejections 10')
    root=Path(__file__).resolve().parents[1]
    binary=(args.mame or root/'mame/mame').resolve()
    try:
        run=args.run.resolve()
        run.mkdir(parents=True,exist_ok=args.resume)
        legacy=completed_legacy_launches(run) if args.resume else set()
        first=run/'install'
        first_retained=first.exists()
        if first_retained and not args.resume:
            raise ValueError('existing PIN2 installation requires explicit --resume')
        first_command=[sys.executable,str(root/'tools/run_noki6250_acceptance.py'),
                       str(first),'--scenario','pin2-change','--mame',str(binary)]
        first_launch={'phase':'install','invocation_id':str(uuid.uuid4()),'command':first_command,
                      'mame_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
                      'fixture_base_sha256':hashlib.sha256((root/'tools/noki6250_pin2_change.lua').read_bytes()).hexdigest(),
                      'mame_command':mame_command(binary,root,first,root/'tools/noki6250_pin2_change.lua')}
        environment=os.environ.copy()
        environment.pop('NOKIA_DCT3_6250_PIN_ENTRY',None)
        if first_retained:
            recorded=json.loads((run/'install_launch.json').read_text())
            validate_retained_launch(recorded,first_launch,None,set(),
                fields=('phase','command','mame_sha256','fixture_base_sha256','mame_command'))
            first_launch=recorded
            verify_process_completion(first,first_launch['invocation_id'],legacy)
        else:
            (run/'install_launch.json').write_text(json.dumps(first_launch,indent=2)+'\n')
            with (run/'install_launcher.log').open('w') as output:
                subprocess.run(first_command,stdout=output,stderr=subprocess.STDOUT,
                               check=True,timeout=180,env=environment)
            record_process_completion(first,first_launch['invocation_id'])
        verify_install_capture(first,first_launch['mame_command'])
        print('6250 PIN2 phase install: '+('retained evidence verified' if first_retained else 'fresh process verified'),flush=True)
        protected={first:hashes(first)}
        launches=[first_launch]
        source=first
        phases=[('reject','reject',3,10),('cold','change',2,10)] if args.reject_first else [('cold','change',3,10)]
        if args.exhaust_pin2:
            phases=[(f'reject-{attempt}','reject',4-attempt,10) for attempt in (1,2,3)]
        phases += [(f'wrong-puk2-{attempt}','puk2-reject',0,11-attempt)
                   for attempt in range(1,args.puk2_rejections+1)]
        if args.recover_puk2:
            if args.wrong_puk2_first:phases.append(('wrong-puk2','puk2-reject',0,10))
            phases += [('puk2','puk2',0,9 if args.wrong_puk2_first else 10-args.puk2_rejections),('recheck','recheck',3,10)]
        if args.permanent_recheck:phases.append(('permanent','permanent',0,0))
        scripts={'reject':'noki6250_pin2_change_wrong.lua','change':'noki6250_pin2_change_cold.lua',
                 'puk2':'noki6250_puk2_recovery.lua','puk2-reject':'noki6250_puk2_wrong.lua',
                 'recheck':'noki6250_puk2_reauth.lua','permanent':'noki6250_puk2_permanent.lua'}
        for name,kind,attempts,puk_attempts in phases:
            rejected=kind=='reject'
            cold=run/name
            retained=cold.exists()
            if retained and not args.resume:
                raise ValueError('existing PIN2 phase requires explicit --resume')
            if not retained:
                cold.mkdir()
                for component in ('nvram','cfg','roms'):
                    shutil.copytree(source/component,cold/component)
                if hashes(cold/'nvram')!=hashes(source/'nvram'):
                    raise ValueError('cold PIN2 storage differs from predecessor')
            initial=((cold/'initial_sim_card.bin').read_bytes() if retained else
                     (cold/'nvram/nhm3hle/sim_card').read_bytes())
            check_cold_input(initial,(source/'nvram/nhm3hle/sim_card').read_bytes(),retries=attempts,
                             pin2='2468' if kind=='recheck' else '8765',puk2=puk_attempts)
            if not retained:(cold/'initial_sim_card.bin').write_bytes(initial)
            script=root/'tools'/scripts[kind]
            command=mame_command(binary,root,cold,script,90 if kind in ('puk2','puk2-reject') else 84)
            if (hashlib.sha256(binary.read_bytes()).hexdigest()!=first_launch['mame_sha256'] or
                    hashlib.sha256((root/'tools/noki6250_pin2_change.lua').read_bytes()).hexdigest()!=first_launch['fixture_base_sha256']):
                raise ValueError('PIN2 executable or shared fixture changed between phases')
            launch={'phase':name,'invocation_id':str(uuid.uuid4()),'command':command,
                    'initial_storage':hashes(cold/'nvram'),
                    'mame_sha256':first_launch['mame_sha256'],
                    'fixture_base_sha256':first_launch['fixture_base_sha256'],
                    'script_sha256':hashlib.sha256(script.read_bytes()).hexdigest()}
            seen={item['invocation_id'] for item in launches}
            if retained:
                recorded=json.loads((cold/'launch.json').read_text())
                validate_retained_launch(recorded,launch,hashes(source/'nvram'),seen,
                    fields=('phase','command','mame_sha256','fixture_base_sha256','script_sha256'))
                if recorded['initial_storage'].get('nhm3hle/sim_card')!=hashlib.sha256(initial).hexdigest():
                    raise ValueError('retained PIN2 initial card differs from launch provenance')
                launch=recorded
                verify_process_completion(cold,launch['invocation_id'],legacy)
            else:
                if launch['invocation_id'] in seen:
                    raise ValueError('cold PIN2 launch must be independent')
                (cold/'launch.json').write_text(json.dumps(launch,indent=2)+'\n')
                with (cold/'console.log').open('w') as output:
                    subprocess.run(command,cwd=cold,stdout=output,stderr=subprocess.STDOUT,
                                   check=True,timeout=180,env=environment)
                record_process_completion(cold,launch['invocation_id'])
            verify_handset_health((cold/'console.log').read_text(errors='replace'))
            text=read_evidence(cold/'error.log')
            card=(cold/'nvram/nhm3hle/sim_card').read_bytes()
            if kind=='permanent':
                from tools.noki6250_puk2_check import check_permanent,check_permanent_frames
                check_permanent(text,initial,card)
                check_permanent_frames(cold/'snap')
            elif kind in ('puk2','puk2-reject'):
                from tools.noki6250_puk2_check import check as check_puk2,check_frames as check_puk2_frames
                check_puk2(text,initial,card,rejected=kind=='puk2-reject',puk_attempts=puk_attempts)
                check_puk2_frames(cold/'snap',rejected=kind=='puk2-reject',puk_attempts=puk_attempts)
            else:
                check(text,card,old='2468' if kind=='recheck' else '8764' if rejected else '8765',
                      new='1357' if kind=='recheck' else '2468',rejected=rejected,
                      stored='8765',initial_retries=attempts)
            registration(text,card,preserved=True,require_host=False)
            if kind not in ('puk2','puk2-reject','permanent'):check_frames(cold/'snap',rejected=rejected,initial_retries=attempts)
            if any(hashes(path)!=digest for path,digest in protected.items()):
                raise ValueError('cold PIN2 run mutated predecessor artifacts')
            protected[cold]=hashes(cold)
            launches.append(launch)
            source=cold
            print(f'6250 PIN2 phase {name}: '+('retained evidence verified' if retained else 'fresh process verified'),flush=True)
        (run/'acceptance.json').write_text(json.dumps({'phases':launches,
            'independent_processes':len(launches),'rejected_change':args.reject_first,
            'pin2_exhausted':args.exhaust_pin2,
            'puk2_recovered':args.recover_puk2,
            'wrong_puk2_first':args.wrong_puk2_first,
            'puk2_rejections':args.puk2_rejections,
            'permanent_rechecked':args.permanent_recheck,
            'resumed':args.resume,
            'research_hle':True,'native_speech':False},indent=2)+'\n')
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as error:
        parser.exit(1,f'6250 cold PIN2 FAIL: {error}\n')
    label=(f'{args.puk2_rejections} cold PUK2 rejections'+
           (' and permanent-card cold PIN1/menu recheck' if args.permanent_recheck else
            ' and recovery' if args.recover_puk2 else '') if args.puk2_rejections else
           'wrong-PUK2 rejection, cold recovery and new-PIN2 reuse' if args.wrong_puk2_first else
           'physical PUK2 recovery and cold new-PIN2 reuse' if args.recover_puk2 else
           'physical PIN2 exhaustion across cold Settings transactions' if args.exhaust_pin2 else
           'wrong-PIN2 rejection, durable retries and cold recovery' if args.reject_first else
           'installed PIN2 cold physical authentication/change')
    print(f'6250 {label} PASS: {run}')


if __name__=='__main__':main()
