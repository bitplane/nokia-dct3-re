#!/usr/bin/env python3
"""Own-input 8850 physical FDN validity toggles and cold persistence."""
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
from typing import NamedTuple

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_profile
from tools.sim_fdn_profile import extend
from tools.run_noki8xxx_supplementary import PROFILES,verify_inputs
from tools.run_noki8850_pin_retry_cold import storage_hashes,mame_command,check_messages_menu
from tools.run_noki8850_pin2_change import registered_card
from tools.run_noki8850_pin_registration import check_pin_inputs
from tools.noki8850_staged_trace_check import check_trace
from tools.radio_registration_trace_check import verify as registration
from tools.run_sip_handset_gate import verify_handset_health
from tools.noki6210_puk_check import contacts
from tools.sim_security_trace_check import statuses
from tools.radio_call_lifecycle_common import require_ordered
from tools.nsk3_phonebook_check import expected_record


class Phase(NamedTuple):
    name:str
    script:str
    source:str|None
    seconds:int
    validity:int|None=None
    record:str|None=None


def phase_plan(records=False):
    phases=[Phase('on','fdn_on',None,55,0),Phase('cold_on','fdn','on',55,1),
            Phase('off','fdn_off','cold_on',55,2),Phase('cold_off','fdn','off',55,3)]
    if records:
        phases += [Phase('write','fdn_write','on',67,record='write'),
                   Phase('read','fdn_read','write',55,record='read')]
    return phases


def expected_card(valid,registered=True):
    base=registered_card('5678') if registered else make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43
    card=bytearray(extend(base))
    card[3575]=int(valid)
    return bytes(card)


def check_phase(text,initial,final,phase):
    if phase not in range(4):raise ValueError('unknown 8850 FDN lifecycle phase')
    if initial!=expected_card(phase in (0,3),phase!=0) or final!=expected_card(phase>=2):
        raise ValueError('8850 FDN validity changed unrelated card bytes or lost its cold state')
    toggle=phase%2==0
    if statuses(text,0x20)!=[(0x9000,3,10)]*(2 if toggle else 1):
        raise ValueError('8850 FDN requires PIN1 and PIN2 only on physical toggles')
    if re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=(['01','02'] if toggle else ['01']):
        raise ValueError('8850 FDN VERIFY selectors/count differ')
    if re.search(r'header cla=a0 ins=(?:24|26|28|2c|dc) ',text):
        raise ValueError('8850 FDN toggle changed credentials or records')
    for ins,targets in ((4,['6f3a'] if phase==0 else []),
                        (0x44,[['6f7e','6f07'],['6f7e','6f07'],['6f7e','6f07','6f3a'],[]][phase])):
        found=re.findall(rf'header cla=a0 ins={ins:02x} p1=00 p2=00 p3=00 selected=(\w{{4}})',text)
        if found!=targets or statuses(text,ins)!=[(0x9000,3,10)]*len(targets):
            raise ValueError('8850 FDN identity/ADN validity transaction differs')
    if re.findall(r'header cla=a0 ins=b2 p1=(\w\w) p2=04 p3=20 selected=6f3b',text)!=[f'{i:02x}' for i in range(1,11)]:
        raise ValueError('8850 FDN did not read all ten records')
    keys=['Menu']+['Scroll Down']*3+['Menu']+['Scroll Down']*5+['Menu']+['Scroll Down']*2+['Menu']
    if toggle:keys += ['Scroll Up' if phase==0 else 'Scroll Down','Menu']+['Keypad '+d for d in '5678']+['Menu']
    contacts(text,'8850_fdn',[(24+i,key) for i,key in enumerate(keys)]+[(48,'End'),(49,'Menu')])
    check_pin_inputs(text)
    registration(text,'nsm2',preserved=phase!=0)
    ordered=[('FDN menu',r'8850_fdn: key=Menu state=1 t=37\.')]
    if toggle:ordered += [('submit',r'8850_fdn: key=Menu state=1 t=44\.'),
        ('PIN2',r'header cla=a0 ins=20 p1=00 p2=02 p3=08'),
        ('validity',rf'header cla=a0 ins={4 if phase==0 else 0x44:02x} p1=00 p2=00 p3=00 selected=6f3a')]
    ordered += [('Menu',r'8850_fdn: key=Menu state=1 t=49\.'),
                ('complete',r'8850_fdn: complete=1 firmware_state_forcing=0')]
    require_ordered(text,ordered,'8850 FDN validity')


def check_frames(directory,phase):
    from PIL import Image
    on='3c8d472bb55b57c58a0118110bd524e5b431b14856507359705821868672e84c'
    off='9caa1f296909cf16bdb90d47f492c14c8b108a3b3562b76224a2ccb5c510b86f'
    frames={'current':off if phase in (0,3) else on}
    if phase%2==0:
        frames.update(selected=on if phase==0 else off,
            pin2='a7a38ac6f4fe16945c758d9f223e2aeb7e28d46ea55a1371a1a9b9e21565cd9f',
            result='8c2dd1cb7eb4cafadaa8f6afa7c7b569f09d2f14af7c41cba6ee8c92b23130c5' if phase==0 else
                   '05f10013bd55709f5730fec00021a767400ba24b924930b09f258feeec8138b9')
    for label,digest in frames.items():
        with Image.open(directory/f'8850_fdn_{label}.png') as image:
            if image.size!=(84,48) or hashlib.sha256(image.convert('L').tobytes()).hexdigest()!=digest:
                raise ValueError('8850 FDN pixels differ: '+label)
    check_messages_menu(directory/'8850_fdn_responsive_menu.png')


def record_card(saved):
    card=bytearray(expected_card(False))
    if saved:card[3578:3610]=expected_record()
    return bytes(card)


def check_record(text,initial,final,mode):
    if mode not in ('write','read'):raise ValueError('unknown 8850 FDN record phase')
    writing=mode=='write'
    if initial!=record_card(not writing) or final!=record_card(True):
        raise ValueError('8850 FDN save/read changed unrelated card data or lost A/12345')
    if statuses(text,0x20)!=[(0x9000,3,10)]*(2 if writing else 1):
        raise ValueError('8850 FDN Save requires PIN2; cold read must not reauthenticate PIN2')
    if re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=(['01','02'] if writing else ['01']):
        raise ValueError('8850 FDN record authentication selectors/count differ')
    if re.search(r'header cla=a0 ins=(?:04|24|26|28|2c) ',text):
        raise ValueError('8850 FDN record operation changed credentials or ADN validity')
    if (re.findall(r'header cla=a0 ins=44 p1=00 p2=00 p3=00 selected=(\w{4})',text)!=['6f7e','6f07'] or
            statuses(text,0x44)!=[(0x9000,3,10)]*2):
        raise ValueError('8850 FDN record startup identity rehabilitation differs')
    updates=re.findall(r'header cla=a0 ins=dc p1=(\w\w) p2=(\w\w) p3=(\w\w) selected=(\w{4})',text)
    if (updates!=([('01','04','20','6f3b')] if writing else []) or
            statuses(text,0xdc)!=([(0x9000,3,10)] if writing else [])):
        raise ValueError('8850 FDN UPDATE must target only record1 after physical Save')
    reads=[f'{i:02x}' for i in range(1,11)]+['01']*(2 if writing else 3)
    if re.findall(r'header cla=a0 ins=b2 p1=(\w\w) p2=04 p3=20 selected=6f3b',text)!=reads:
        raise ValueError('8850 FDN startup/selection/readback extent differs')
    keys=['Menu']+['Scroll Down']*3+['Menu']+['Scroll Down']*5+['Menu']+['Scroll Down']*2+['Menu']
    keys += ['Scroll Down','Scroll Down','Menu']
    if writing:keys += ['Scroll Down','Menu']+['Keypad '+d for d in '5678']+['Menu','Keypad 2','Menu']+['Keypad '+d for d in '12345']+['Menu']
    else:keys += ['Menu']*3
    finish=60 if writing else 48
    contacts(text,'8850_fdn',[(24+i,key) for i,key in enumerate(keys)]+[(finish,'End'),(finish+1,'Menu')])
    check_pin_inputs(text)
    registration(text,'nsm2',preserved=True)
    ordered=[('Numbers',r'8850_fdn: key=Menu state=1 t=40\.')]
    if writing:
        ordered += [('PIN2 submit',r'8850_fdn: key=Menu state=1 t=47\.'),
            ('PIN2',r'header cla=a0 ins=20 p1=00 p2=02 p3=08'),
            ('verified',r'SIM status ins=20 sw=9000'),
            ('Save',r'8850_fdn: key=Menu state=1 t=55\.'),
            ('UPDATE',r'header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b'),
            ('saved',r'SIM status ins=dc sw=9000'),
            ('readback',r'header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3b')]
    ordered += [('Menu',rf'8850_fdn: key=Menu state=1 t={finish+1}\.'),
                ('complete',r'8850_fdn: complete=1 firmware_state_forcing=0')]
    require_ordered(text,ordered,'8850 FDN protected record')


def check_record_frames(directory,mode):
    from PIL import Image
    frames=({'record_pin2':'a7a38ac6f4fe16945c758d9f223e2aeb7e28d46ea55a1371a1a9b9e21565cd9f',
             'name_editor':'5fe5287ff4f8391953ac6355677ce688f5db043df752ce22537e17115a55b4e9',
             'number_editor':'a4497c68de0c8428e70ae6690f9589203526140d0abb1d581834108802c001d6',
             'saved':'c0701126a0b251a6ae542c05fe44f58c921ddea4db30ddf50181d0dceb6c0377'} if mode=='write' else
            {'contact':'f5f0050462c46e6d602544f7c153adbfb77d4d0cf2232e4262b7f4039dff1445'})
    for label,digest in frames.items():
        with Image.open(directory/f'8850_fdn_{label}.png') as image:
            if image.size!=(84,48) or hashlib.sha256(image.convert('L').tobytes()).hexdigest()!=digest:
                raise ValueError('8850 FDN record pixels differ: '+label)
    check_messages_menu(directory/'8850_fdn_responsive_menu.png')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--records',action='store_true',help='add protected A/12345 Save and cold FDN Search/Details')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    try:
        profile=PROFILES['8850']
        verify_inputs('8850',(root/'roms/noki8850'/profile[1]).read_bytes(),
                      (root/'roms/noki8850'/profile[3]).read_bytes())
        run=args.directory.resolve()
        run.mkdir(parents=True,exist_ok=False)
        previous=None
        launches=[]
        for item in phase_plan(args.records):
            name,script=item.name,item.script
            previous=run/item.source if item.source is not None else None
            phase=run/name
            phase.mkdir()
            predecessor=storage_hashes(previous/'nvram') if previous else None
            if previous:shutil.copytree(previous/'nvram',phase/'nvram')
            card=phase/'nvram/nsm2hle/sim_card'
            if not previous:
                card.parent.mkdir(parents=True)
                card.write_bytes(expected_card(True,False))
            if previous and storage_hashes(phase/'nvram')!=predecessor:
                raise ValueError('8850 FDN cold process did not inherit complete predecessor NVRAM')
            initial=card.read_bytes()
            (phase/'initial_sim_card.bin').write_bytes(initial)
            command=mame_command(root,script,item.seconds)
            launch={'invocation_id':str(uuid.uuid4()),'command':command,
                'source_phase':item.source,
                'initial_storage':storage_hashes(phase/'nvram'),'mcu_sha1':profile[2],'pmm_sha1':profile[4],
                'mame_sha256':hashlib.sha256((root/'mame/mame').read_bytes()).hexdigest(),
                'input_sha256':{filename:hashlib.sha256((root/'tools'/filename).read_bytes()).hexdigest()
                    for filename in (f'noki8850_{script}.lua','noki8850_fdn.lua','noki8850_security_input.lua','noki8850_startup_observe.lua')}}
            (phase/'launch.json').write_text(json.dumps(launch,indent=2)+'\n')
            environment=os.environ.copy()
            environment['NOKIA_DCT3_8850_PIN_ENTRY']='1'
            with (phase/'console.log').open('w') as output:
                subprocess.run(command,cwd=phase,env=environment,stdout=output,
                    stderr=subprocess.STDOUT,check=True,timeout=180)
            verify_handset_health((phase/'console.log').read_text(errors='replace'))
            text=(phase/'error.log').read_text(errors='replace')
            errors=check_trace(text,runtime_hle=True)
            if errors:raise ValueError('; '.join(errors))
            if item.record:
                check_record(text,initial,card.read_bytes(),item.record)
                check_record_frames(phase/'snap',item.record)
            else:
                check_phase(text,initial,card.read_bytes(),item.validity)
                check_frames(phase/'snap',item.validity)
            if previous and storage_hashes(previous/'nvram')!=predecessor:
                raise ValueError('8850 FDN cold process mutated predecessor NVRAM')
            launches.append(launch)
            previous=phase
            print('8850 FDN phase '+name+': verified',flush=True)
        (run/'acceptance.json').write_text(json.dumps({'phases':launches,'research_hle':True,
            'protected_record_save_cold_read':args.records,
            'dial_permission_tested':False,'native_speech':False},indent=2)+'\n')
    except (ValueError,RuntimeError,OSError,subprocess.SubprocessError) as error:
        parser.exit(1,f'8850 FDN lifecycle FAIL: {error}\n')
    print('8850 physical FDN validity and cold On/Off persistence PASS')


if __name__=='__main__':main()
