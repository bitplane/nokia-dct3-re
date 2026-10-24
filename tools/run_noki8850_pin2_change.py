#!/usr/bin/env python3
"""Physical 8850 PIN2 change and cold use of the replacement credential."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import uuid

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_profile
from tools.run_noki8xxx_supplementary import PROFILES,verify_inputs
from tools.run_noki8850_pin_retry_cold import storage_hashes,check_messages_menu,check_wrong_frame,mame_command
from tools.run_noki8850_pin_registration import check_pin_inputs
from tools.noki8850_staged_trace_check import check_trace
from tools.radio_registration_trace_check import verify as registration
from tools.run_sip_handset_gate import verify_handset_health
from tools.noki6210_puk_check import contacts
from tools.sim_security_trace_check import statuses
from tools.radio_call_lifecycle_common import require_ordered


PIN2_FRAMES={
    'current_pin2':'63b832d3fb39ac6518526d26cf937757d78b9cc73d62a5ef4c28e07d85798dcc',
    'new_pin2':'99cd520f4f3e9496bdbecec5550c679ea48388116cecbb9ee32bb79bf296098f',
    'confirm':'333a1ef512910c1110ed1d685095d1b68470a583789f87a0543f18d10495500c',
    'result':'4d0774f24041e4fde5334a16ab458556e225d1c18aa10bf26ec86c7f7b461082',
}


def check_frames(directory,rejected=False,remaining=2,puk2=False,permanent=False):
    from PIL import Image
    frames=({'current_pin2':'64b9b1653dea6132cded485fb1d4c81332c56dd7eef8301f5c3e3df312be4542',
             'permanent_digit':'cb8cf263c8bea9bca5312f02d302c4c063793e1f06eb2c492457375279110848'}
            if permanent else PIN2_FRAMES)
    for label,digest in frames.items():
        if puk2 and label=='current_pin2':digest='d2b301fa8d10bb8a12f0541444b866ed6ab51a770b13aada88b32da8632ea545'
        if puk2 and label=='result':digest='27b8f689bd860c433e6e7c2f32282b3a745a5e82258b887c80477e89276a1ec5'
        if rejected and label=='result':
            if remaining==0:
                digest=('64b9b1653dea6132cded485fb1d4c81332c56dd7eef8301f5c3e3df312be4542' if puk2 else
                        'd2b301fa8d10bb8a12f0541444b866ed6ab51a770b13aada88b32da8632ea545')
            else:
                check_wrong_frame(directory/f'8850_pin2_{label}.png')
                continue
        with Image.open(directory/f'8850_pin2_{label}.png') as image:
            if image.size!=(84,48) or hashlib.sha256(image.convert('L').tobytes()).hexdigest()!=digest:
                raise ValueError('8850 PIN2 pixels differ: '+label)
    check_messages_menu(directory/'8850_pin2_responsive_menu.png')


def registered_card(pin2):
    card=bytearray(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43)
    card[3492:3500]=pin2.encode().ljust(8,b'\xff')
    card[1604:1609]=bytes.fromhex('00f1100001')
    card[1610]=0
    return bytes(card)


def editor_keys(*groups):
    keys=['Menu']+['Scroll Down']*3+['Menu']+['Scroll Down']*5+['Menu']+['Scroll Down']*5+['Menu']+['Scroll Down']*2+['Menu']
    for digits in groups:keys += ['Keypad '+digit for digit in digits]+['Menu']
    return keys


def check_phase(text,initial,final,reverse=False,*,rejected=False,prior_rejection=False,pin2_attempts=3):
    old,new=('2468','5678') if reverse else ('5678','2468')
    if rejected and (reverse or prior_rejection):
        raise ValueError('rejected PIN2 fixture must start independently')
    if not 1<=pin2_attempts<=3:raise ValueError('invalid live PIN2 allowance')
    expected_initial=bytearray(registered_card(old) if reverse or prior_rejection or pin2_attempts<3 else make_profile(True))
    expected_final=bytearray(registered_card(old if rejected else new))
    if prior_rejection:expected_initial[3517]=2
    if rejected:
        expected_initial[3517]=pin2_attempts
        expected_final[3517]=pin2_attempts-1
    if initial!=bytes(expected_initial) or final!=bytes(expected_final):
        raise ValueError('8850 PIN2 phase changed unexpected card bytes or lost the replacement PIN')
    status=0x9840 if pin2_attempts==1 else 0x9804
    if statuses(text,0x20)!=[(0x9000,3,10)] or statuses(text,0x24,chv=2)!=[(status,pin2_attempts-1,10) if rejected else (0x9000,3,10)]:
        raise ValueError('8850 requires one startup PIN1 VERIFY and its exact PIN2 CHANGE result')
    if re.findall(r'header cla=a0 ins=24 p1=00 p2=(\w\w) p3=10',text)!=['02']:
        raise ValueError('8850 CHANGE must address CHV2 exactly once')
    if any(statuses(text,ins) for ins in (4,0x26,0x28,0x2c,0x44,0xdc)):
        raise ValueError('8850 PIN2 change unexpectedly altered other card configuration or records')
    keys=editor_keys('9999' if rejected else old,new,new)
    contacts(text,'8850_pin2',[(24+i,key) for i,key in enumerate(keys)]+[(62,'End'),(63,'Menu')])
    check_pin_inputs(text)
    registration(text,'nsm2',preserved=reverse or prior_rejection or pin2_attempts<3)
    require_ordered(text,[('confirm',r'8850_pin2: key=Menu state=1 t=58\.'),
        ('result',rf'SIM status ins=24 sw={format(status,"04x") if rejected else "9000"}'),
        ('Menu',r'8850_pin2: key=Menu state=1 t=63\.'),
        ('complete',r'8850_pin2: complete=1 firmware_state_forcing=0')],'8850 PIN2 change')


def check_puk2_phase(text,initial,final,*,rejected=False,puk2_attempts=10,permanent=False):
    if not 0<=puk2_attempts<=10 or (puk2_attempts==0)!=permanent:
        raise ValueError('inconsistent 8850 PUK2 allowance/lifecycle')
    if permanent and rejected:raise ValueError('permanent PUK2 has no further rejected transaction')
    blocked=bytearray(registered_card('5678'))
    blocked[3517]=0
    blocked[3519]=puk2_attempts
    expected=bytearray(blocked if rejected or permanent else registered_card('2468'))
    if rejected:expected[3519]-=1
    if initial!=bytes(blocked) or final!=bytes(expected):
        raise ValueError('8850 PUK2 did not preserve the blocked card or install only the new PIN2')
    pin1=[(0x9000,3,10)]*(1 if rejected or permanent else 2)
    pin2=[(0x9000,0,puk2_attempts)]+([] if rejected or permanent else [(0x9000,3,10)])
    failure='9840' if puk2_attempts==1 else '9804'
    unblock=[] if permanent else [(int(failure,16),0,puk2_attempts-1)] if rejected else [(0x9000,3,10)]
    if (statuses(text,0x20)!=pin1 or statuses(text,0x20,chv=2)!=pin2 or statuses(text,0x2c,chv=2)!=unblock):
        raise ValueError('8850 PUK2 credential results or retry state differ')
    if re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=(['01'] if rejected or permanent else ['01','02']):
        raise ValueError('8850 PUK2 must VERIFY PIN1, then PIN2 only after successful UNBLOCK')
    if re.findall(r'header cla=a0 ins=2c p1=00 p2=(\w\w) p3=10',text)!=([] if permanent else ['02']):
        raise ValueError('8850 PUK2 must use selector02 once')
    if any(statuses(text,ins) for ins in (4,0x24,0x26,0x28,0x44,0xdc)):
        raise ValueError('8850 PUK2 changed unrelated configuration or records')
    if permanent and re.search(r'GSM service (?:establish|uplink) sapi=\d+ pd=(?:03|09) ',text):
        raise ValueError('permanent PUK2 input unexpectedly started a call or SMS')
    keys=editor_keys() if permanent else editor_keys('99999999' if rejected else '87654321','2468','2468')
    ending=[(46,'Keypad 9'),(48,'End'),(49,'Menu')] if permanent else [(67,'End'),(68,'Menu')]
    contacts(text,'8850_pin2',[(24+i,key) for i,key in enumerate(keys)]+ending)
    check_pin_inputs(text)
    registration(text,'nsm2',preserved=True)
    ordered=([('rejected entry',r'8850_pin2: key=Menu state=1 t=43\.'),
              ('digit',r'8850_pin2: key=Keypad 9 state=1 t=46\.'),
              ('End',r'8850_pin2: key=End state=1 t=48\.')] if permanent else
             [('confirm',r'8850_pin2: key=Menu state=1 t=62\.')])
    if not permanent:ordered += [('result',rf'SIM status ins=2c sw={failure if rejected else "9000"}')]
    if not rejected and not permanent:
        ordered += [('verify new PIN2',r'header cla=a0 ins=20 p1=00 p2=02 p3=08'),
                    ('verified',r'SIM status ins=20 sw=9000')]
    ordered += [
        ('Menu',rf'8850_pin2: key=Menu state=1 t={49 if permanent else 68}\.'),
        ('complete',r'8850_pin2: complete=1 firmware_state_forcing=0')]
    require_ordered(text,ordered,'8850 PUK2 transaction')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    mode=parser.add_mutually_exclusive_group()
    mode.add_argument('--reject-first',action='store_true',help='consume one PIN2 retry with a wrong old PIN before cold recovery')
    mode.add_argument('--puk2',action='store_true',help='physically exhaust PIN2, recover with PUK2 and cold-use the replacement PIN2')
    parser.add_argument('--wrong-puk2-first',action='store_true',help='reject one PUK2 in a cold process before recovery')
    parser.add_argument('--exhaust-puk2',action='store_true',help='consume all PUK2 allowances physically and test cold permanent refusal')
    args=parser.parse_args()
    if args.wrong_puk2_first and not args.puk2:parser.error('--wrong-puk2-first requires --puk2')
    if args.exhaust_puk2 and (not args.puk2 or args.wrong_puk2_first):
        parser.error('--exhaust-puk2 requires --puk2 without --wrong-puk2-first')
    root=Path(__file__).resolve().parents[1]
    try:
        profile=PROFILES['8850']
        verify_inputs('8850',(root/'roms/noki8850'/profile[1]).read_bytes(),
                      (root/'roms/noki8850'/profile[3]).read_bytes())
        run=args.directory.resolve()
        run.mkdir(parents=True,exist_ok=False)
        previous=None
        launches=[]
        phases=(('change','pin2_change'),('cold_restore','pin2_restore'))
        if args.reject_first:phases=(('reject','pin2_wrong'),*phases)
        if args.puk2:
            phases=(*((f'reject-{attempt}','pin2_wrong') for attempt in range(1,4)),
                    ('puk2','puk2_recovery'),('cold_restore','pin2_restore'))
            if args.wrong_puk2_first:phases=(*phases[:3],('wrong-puk2','puk2_wrong'),*phases[3:])
            if args.exhaust_puk2:
                phases=(*phases[:3],*((f'wrong-puk2-{attempt:02d}','puk2_wrong') for attempt in range(1,11)),
                        ('permanent','puk2_permanent'))
        for name,script in phases:
            kind='wrong-puk2' if name.startswith('wrong-puk2-') else name
            phase=run/name
            phase.mkdir()
            predecessor=storage_hashes(previous/'nvram') if previous else None
            if previous:shutil.copytree(previous/'nvram',phase/'nvram')
            card=phase/'nvram/nsm2hle/sim_card'
            if not previous:
                card.parent.mkdir(parents=True)
                card.write_bytes(make_profile(True))
            if previous and storage_hashes(phase/'nvram')!=predecessor:
                raise ValueError('8850 cold PIN2 phase did not inherit complete predecessor NVRAM')
            initial=card.read_bytes()
            (phase/'initial_sim_card.bin').write_bytes(initial)
            command=mame_command(root,script,74 if kind in ('puk2','wrong-puk2','permanent') else 69)
            launch={'invocation_id':str(uuid.uuid4()),'command':command,
                'initial_storage':storage_hashes(phase/'nvram'),'mcu_sha1':profile[2],'pmm_sha1':profile[4],
                'mame_sha256':hashlib.sha256((root/'mame/mame').read_bytes()).hexdigest(),
                'input_sha256':{name:hashlib.sha256((root/'tools'/name).read_bytes()).hexdigest() for name in (
                    f'noki8850_{script}.lua','noki8850_pin2_change.lua','noki8850_security_input.lua','noki8850_startup_observe.lua')}}
            (phase/'launch.json').write_text(json.dumps(launch,indent=2)+'\n')
            environment=os.environ.copy()
            environment['NOKIA_DCT3_8850_PIN_ENTRY']='1'
            with (phase/'console.log').open('w') as output:
                subprocess.run(command,cwd=phase,env=environment,stdout=output,
                    stderr=subprocess.STDOUT,check=True,timeout=180)
            verify_handset_health((phase/'console.log').read_text(errors='replace'))
            text=(phase/'error.log').read_text(errors='replace')
            errors=check_trace(text,runtime_hle=True,sim_reads=True)
            if errors:raise ValueError('; '.join(errors))
            if kind in ('puk2','wrong-puk2','permanent'):
                allowance=(11-int(name.rsplit('-',1)[1]) if name.startswith('wrong-puk2-') else
                           0 if kind=='permanent' else 9 if args.wrong_puk2_first and name=='puk2' else 10)
                check_puk2_phase(text,initial,card.read_bytes(),rejected=kind=='wrong-puk2',
                    puk2_attempts=allowance,permanent=kind=='permanent')
                check_frames(phase/'snap',puk2=kind!='permanent',rejected=kind=='wrong-puk2',
                             remaining=allowance-1,permanent=kind=='permanent')
            else:
                rejected=name=='reject' or name.startswith('reject-')
                allowance=4-int(name.rsplit('-',1)[1]) if name.startswith('reject-') else 3
                check_phase(text,initial,card.read_bytes(),name=='cold_restore',rejected=rejected,
                            prior_rejection=args.reject_first and name=='change',pin2_attempts=allowance)
                check_frames(phase/'snap',rejected=rejected,remaining=allowance-1)
            if previous and storage_hashes(previous/'nvram')!=predecessor:
                raise ValueError('8850 cold PIN2 phase mutated predecessor NVRAM')
            launches.append(launch)
            previous=phase
            print('8850 PIN2 phase '+name+': verified',flush=True)
        (run/'acceptance.json').write_text(json.dumps({'phases':launches,
            'rejected_change_recovery':args.reject_first,
            'puk2_recovery':args.puk2,
            'wrong_puk2_first':args.wrong_puk2_first,
            'permanent_puk2_loss':args.exhaust_puk2,
            'research_hle':True,'native_speech':False},indent=2)+'\n')
    except (ValueError,RuntimeError,OSError,subprocess.SubprocessError) as error:
        parser.exit(1,f'8850 PIN2 change FAIL: {error}\n')
    print('8850 '+('permanent PUK2 loss with retained PIN1/registration' if args.exhaust_puk2 else
                  'PIN2 change and cold replacement-credential use')+' PASS')


if __name__=='__main__':main()
