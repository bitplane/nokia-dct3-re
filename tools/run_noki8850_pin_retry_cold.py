#!/usr/bin/env python3
"""Own-input 8850 wrong PIN persistence and independent cold recovery."""
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
from tools.run_noki8850_pin_registration import check_pin_inputs
from tools.noki8850_staged_trace_check import check_trace,check_physical_inputs,check_navigation_frames
from tools.radio_registration_trace_check import verify as registration
from tools.run_sip_handset_gate import verify_handset_health
from tools.sim_security_trace_check import statuses
from tools.noki6210_puk_check import contacts


def storage_hashes(directory):
    return {str(path.relative_to(directory)):hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(directory.rglob('*')) if path.is_file()}


def check_wrong_frame(path):
    from PIL import Image
    with Image.open(path) as image:
        if (image.size!=(84,48) or hashlib.sha256(image.convert('L').tobytes()).hexdigest()!=
                'd11e338fd4431cb6a72f2e2c133267242f305dcc6ae0c2f15fbcddebfa310f61'):
            raise ValueError('8850 wrong-PIN Code error frame differs')


def check_card_phase(text,initial,final,recovery=False):
    failed=bytearray(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43)
    failed[3516]=2
    registered=bytearray(failed)
    registered[3516]=3
    registered[1604:1609]=bytes.fromhex('00f1100001')
    registered[1610]=0
    if initial!=(bytes(failed) if recovery else make_profile(True)):
        raise ValueError('8850 cold PIN phase did not inherit the complete expected card')
    if final!=(bytes(registered) if recovery else bytes(failed)):
        raise ValueError('8850 PIN phase changed unexpected persistent card bytes')
    wanted=[(0x9000,3,10)] if recovery else [(0x9804,2,10)]
    if statuses(text,0x20)!=wanted:
        raise ValueError('8850 PIN result or retry allowance differs')
    if re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=['01']:
        raise ValueError('8850 phase must VERIFY CHV1 exactly once')
    if any(statuses(text,ins) for ins in (4,0x24,0x26,0x28,0x2c,0x44,0xdc)):
        raise ValueError('8850 PIN phase changed credentials, configuration or records')
    if recovery:
        check_pin_inputs(text)
        registration(text,'nsm2')
    else:
        contacts(text,'8850_pin_wrong',[(8+i,'Keypad 9') for i in range(4)]+[(12,'Menu')])
        if ('8850_pin_wrong: complete=1 firmware_state_forcing=0' not in text or
                'LAPDm Location Updating Accept acknowledged' in text):
            raise ValueError('wrong-only PIN phase did not finish or incorrectly registered')
        submit=text.find('8850_pin_wrong: key=Menu state=1 t=12.')
        if submit<0 or text.find('SIM status ins=20 sw=9804',submit)<0:
            raise ValueError('wrong PIN status did not follow physical submission')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
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
        for name,script,seconds in (('wrong','pin_wrong',18),('recovery','security_input',46)):
            phase=run/name
            phase.mkdir()
            predecessor_storage=storage_hashes(previous/'nvram') if previous else None
            if previous:
                shutil.copytree(previous/'nvram',phase/'nvram')
                if storage_hashes(phase/'nvram')!=predecessor_storage:
                    raise ValueError('cold recovery did not inherit complete predecessor NVRAM')
            card=phase/'nvram/nsm2hle/sim_card'
            if not previous:
                card.parent.mkdir(parents=True)
                card.write_bytes(make_profile(True))
            initial=card.read_bytes()
            (phase/'initial_sim_card.bin').write_bytes(initial)
            command=[str(root/'mame/mame'),'nsm2hle','-rompath',str(root/'roms'),
                '-nvram_directory','nvram','-cfg_directory','cfg','-snapshot_directory','snap',
                '-noreadconfig','-debug','-debugger','none','-verbose','-log',
                '-video','none','-sound','none','-nothrottle','-autoboot_delay','0',
                '-autoboot_script',str(root/f'tools/noki8850_{script}.lua'),
                '-seconds_to_run',str(seconds)]
            launch={'invocation_id':str(uuid.uuid4()),'command':command,
                'initial_storage':storage_hashes(phase/'nvram'),
                'initial_card_sha256':hashlib.sha256(initial).hexdigest(),
                'mcu_sha1':profile[2],'pmm_sha1':profile[4],
                'mame_sha256':hashlib.sha256((root/'mame/mame').read_bytes()).hexdigest(),
                'observer_sha256':hashlib.sha256((root/'tools/noki8850_startup_observe.lua').read_bytes()).hexdigest(),
                'script_sha256':hashlib.sha256((root/f'tools/noki8850_{script}.lua').read_bytes()).hexdigest()}
            (phase/'launch.json').write_text(json.dumps(launch,indent=2)+'\n')
            environment=os.environ.copy()
            environment.pop('NOKIA_DCT3_8850_PIN_ENTRY',None)
            if previous:environment['NOKIA_DCT3_8850_PIN_ENTRY']='1'
            with (phase/'console.log').open('w') as output:
                subprocess.run(command,cwd=phase,env=environment,stdout=output,
                    stderr=subprocess.STDOUT,check=True,timeout=180)
            verify_handset_health((phase/'console.log').read_text(errors='replace'))
            text=(phase/'error.log').read_text(errors='replace')
            errors=check_trace(text,runtime_hle=True,sim_reads=bool(previous))
            if previous:errors+=check_physical_inputs(text)+check_navigation_frames(phase/'snap')
            if errors:raise ValueError('; '.join(errors))
            check_card_phase(text,initial,card.read_bytes(),bool(previous))
            if not previous:check_wrong_frame(phase/'snap/8850_pin_wrong.png')
            if previous and storage_hashes(previous/'nvram')!=predecessor_storage:
                raise ValueError('cold recovery mutated predecessor NVRAM')
            launches.append(launch)
            previous=phase
        (run/'acceptance.json').write_text(json.dumps({'phases':launches,
            'research_hle':True,'native_speech':False},indent=2)+'\n')
    except (ValueError,RuntimeError,OSError,subprocess.SubprocessError) as error:
        parser.exit(1,f'8850 cold PIN retry FAIL: {error}\n')
    print('8850 durable wrong-PIN retry and independent cold recovery PASS')


if __name__=='__main__':main()
