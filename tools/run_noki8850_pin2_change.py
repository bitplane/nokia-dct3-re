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
from tools.run_noki8850_pin_retry_cold import storage_hashes,check_messages_menu,check_wrong_frame
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


def check_frames(directory,rejected=False):
    from PIL import Image
    for label,digest in PIN2_FRAMES.items():
        if rejected and label=='result':
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


def check_phase(text,initial,final,reverse=False,*,rejected=False,prior_rejection=False):
    old,new=('2468','5678') if reverse else ('5678','2468')
    if rejected and (reverse or prior_rejection):
        raise ValueError('rejected PIN2 fixture must start independently')
    expected_initial=bytearray(registered_card(old) if reverse or prior_rejection else make_profile(True))
    expected_final=bytearray(registered_card(old if rejected else new))
    if prior_rejection:expected_initial[3517]=2
    if rejected:expected_final[3517]=2
    if initial!=bytes(expected_initial) or final!=bytes(expected_final):
        raise ValueError('8850 PIN2 phase changed unexpected card bytes or lost the replacement PIN')
    if statuses(text,0x20)!=[(0x9000,3,10)] or statuses(text,0x24,chv=2)!=[(0x9804,2,10) if rejected else (0x9000,3,10)]:
        raise ValueError('8850 requires one startup PIN1 VERIFY and its exact PIN2 CHANGE result')
    if re.findall(r'header cla=a0 ins=24 p1=00 p2=(\w\w) p3=10',text)!=['02']:
        raise ValueError('8850 CHANGE must address CHV2 exactly once')
    if any(statuses(text,ins) for ins in (4,0x26,0x28,0x2c,0x44,0xdc)):
        raise ValueError('8850 PIN2 change unexpectedly altered other card configuration or records')
    keys=['Menu']+['Scroll Down']*3+['Menu']+['Scroll Down']*5+['Menu']+['Scroll Down']*5+['Menu']+['Scroll Down']*2+['Menu']
    for digits in ('9999' if rejected else old,new,new):keys += ['Keypad '+digit for digit in digits]+['Menu']
    contacts(text,'8850_pin2',[(24+i,key) for i,key in enumerate(keys)]+[(62,'End'),(63,'Menu')])
    check_pin_inputs(text)
    registration(text,'nsm2',preserved=reverse or prior_rejection)
    require_ordered(text,[('confirm',r'8850_pin2: key=Menu state=1 t=58\.'),
        ('result',rf'SIM status ins=24 sw={"9804" if rejected else "9000"}'),
        ('Menu',r'8850_pin2: key=Menu state=1 t=63\.'),
        ('complete',r'8850_pin2: complete=1 firmware_state_forcing=0')],'8850 PIN2 change')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--reject-first',action='store_true',help='consume one PIN2 retry with a wrong old PIN before cold recovery')
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
        phases=(('change','pin2_change'),('cold_restore','pin2_restore'))
        if args.reject_first:phases=(('reject','pin2_wrong'),*phases)
        for name,script in phases:
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
            command=[str(root/'mame/mame'),'nsm2hle','-rompath',str(root/'roms'),
                '-nvram_directory','nvram','-cfg_directory','cfg','-snapshot_directory','snap',
                '-noreadconfig','-debug','-debugger','none','-verbose','-log','-video','none',
                '-sound','none','-nothrottle','-autoboot_delay','0',
                '-autoboot_script',str(root/f'tools/noki8850_{script}.lua'),'-seconds_to_run','69']
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
            check_phase(text,initial,card.read_bytes(),name=='cold_restore',
                        rejected=name=='reject',prior_rejection=args.reject_first and name=='change')
            check_frames(phase/'snap',rejected=name=='reject')
            if previous and storage_hashes(previous/'nvram')!=predecessor:
                raise ValueError('8850 cold PIN2 phase mutated predecessor NVRAM')
            launches.append(launch)
            previous=phase
        (run/'acceptance.json').write_text(json.dumps({'phases':launches,
            'rejected_change_recovery':args.reject_first,
            'research_hle':True,'native_speech':False},indent=2)+'\n')
    except (ValueError,RuntimeError,OSError,subprocess.SubprocessError) as error:
        parser.exit(1,f'8850 PIN2 change FAIL: {error}\n')
    print('8850 PIN2 change and cold replacement-credential use PASS')


if __name__=='__main__':main()
