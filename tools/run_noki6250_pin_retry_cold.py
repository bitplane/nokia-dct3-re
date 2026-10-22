#!/usr/bin/env python3
"""Two independent NHM-3 processes qualify durable PIN1 retry state."""
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
from tools.run_noki6250_acceptance import prepare_run,apply_coherent_config
from tools.make_sim_card_profile import make_profile
from tools.run_sip_handset_gate import verify_handset_health
from tools.noki6250_pin_retry_check import registered_card,FRAMES
from tools.noki6210_puk_check import contacts,check_rgb_frame
from tools.sim_security_trace_check import statuses
from tools.noki6250_coherent_registration_check import verify as registration
from tools.noki6250_staged_check import check as uploads,check_initial_fixture
from tools.radio_call_lifecycle_common import require_ordered


def check_phase(text,initial,final,recovery=False):
    expected=bytearray(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43)
    expected[3516]=2
    if recovery:
        if initial!=bytes(expected) or final!=registered_card():
            raise ValueError('cold recovery must inherit the complete failed card and restore only retries/location')
        wanted=[(0x9000,3,10)]
        items=[(8+i,'Keypad '+d) for i,d in enumerate('1234')]+[(12,'Left Softkey / Menu'),(34,'Left Softkey / Menu')]
    else:
        if initial!=make_profile(True) or final!=bytes(expected):
            raise ValueError('wrong PIN must change only the first retry allowance in the serialized card')
        wanted=[(0x9804,2,10)]
        items=[(second,'Keypad 9') for second in range(8,12)]+[(12,'Left Softkey / Menu')]
        if 'LAPDm Location Updating Accept acknowledged' in text:
            raise ValueError('rejected PIN must not reach registration')
    if statuses(text,0x20)!=wanted:
        raise ValueError('cold PIN phase has unexpected authentication result/retries')
    if re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=['01']:
        raise ValueError('cold phase must authenticate CHV1 exactly once')
    if any(statuses(text,ins) for ins in (4,0x24,0x26,0x28,0x2c,0x44,0xdc)):
        raise ValueError('cold PIN phase changed credentials, card configuration or records')
    contacts(text,'6250_pin_retry',items)
    ordered=[('submit',r'6250_pin_retry: key=Left Softkey / Menu state=1 t=12\.'),
             ('result',rf'SIM status ins=20 sw={"9000" if recovery else "9804"}')]
    if recovery:
        ordered += [('registered',r'LAPDm Location Updating Accept acknowledged'),
                    ('Menu',r'6250_pin_retry: key=Left Softkey / Menu state=1 t=34\.'),
                    ('complete',r'6250_pin_retry: complete=1 firmware_state_forcing=0')]
    else:
        ordered += [('complete',r'6250_pin_retry: wrong_only_complete=1 firmware_state_forcing=0')]
    require_ordered(text,ordered,'6250 cold PIN phase')


def hashes(directory):
    result={}
    for path in sorted(directory.rglob('*')):
        if path.is_file():
            with path.open('rb') as source:
                result[str(path.relative_to(directory))]=hashlib.file_digest(source,'sha256').hexdigest()
    return result


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
        launches=[]
        for recovery,name,script,seconds in (
                (False,'wrong','noki6250_pin_wrong.lua',24),
                (True,'recovery','noki6250_pin_cold_recovery.lua',40)):
            phase=run/name
            prepare_run(phase,root)
            apply_coherent_config(phase/'cfg/nhm3hle.cfg',root/'fixtures/noki6250_host_gsm900/nhm3hle.cfg')
            card=phase/'nvram/nhm3hle/sim_card'
            if previous:
                before=hashes(previous)
                shutil.copytree(previous/'nvram',phase/'nvram',dirs_exist_ok=True)
                if hashes(phase/'nvram')!=hashes(previous/'nvram'):
                    raise ValueError('cold input storage differs from predecessor')
            else:
                card.parent.mkdir(parents=True)
                card.write_bytes(make_profile(True))
            initial=card.read_bytes()
            (phase/'initial_sim_card.bin').write_bytes(initial)
            command=[str(binary),'nhm3hle','-rompath',f'{phase / "roms"};{root / "roms"}',
                     '-nvram_directory','nvram','-cfg_directory','cfg','-noreadconfig',
                     '-autoboot_script',str(root/'tools'/script),'-autoboot_delay','0',
                     '-seconds_to_run',str(seconds),'-video','none','-sound','none',
                     '-nothrottle','-log','-verbose']
            launch={'invocation_id':str(uuid.uuid4()),'command':command,
                    'initial_storage':hashes(phase/'nvram'),
                    'mame_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
                    'script_sha256':hashlib.sha256((root/'tools'/script).read_bytes()).hexdigest()}
            (phase/'launch.json').write_text(json.dumps(launch,indent=2)+'\n')
            environment=os.environ.copy()
            environment.pop('NOKIA_DCT3_6250_PIN_ENTRY',None)
            with (phase/'console.log').open('w') as output:
                subprocess.run(command,cwd=phase,stdout=output,stderr=subprocess.STDOUT,
                               check=True,timeout=180,env=environment)
            verify_handset_health((phase/'console.log').read_text(errors='replace'))
            text=(phase/'error.log').read_text(errors='replace')
            uploads(text,runtime=True)
            check_initial_fixture(text)
            check_phase(text,initial,card.read_bytes(),recovery=recovery)
            for frame in (('idle','menu') if recovery else ('error',)):
                check_rgb_frame(phase/f'snap/6250_pin_retry_{frame}.png',FRAMES[frame])
            if recovery:
                registration(text,card.read_bytes(),require_host=False)
                if hashes(previous)!=before:
                    raise ValueError('cold run mutated predecessor artifacts')
            launches.append(launch)
            previous=phase
        (run/'acceptance.json').write_text(json.dumps({'phases':launches,
            'research_hle':True,'native_speech':False},indent=2)+'\n')
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as error:
        parser.exit(1,f'6250 cold PIN retry FAIL: {error}\n')
    print(f'6250 durable wrong-PIN retry and cold physical recovery PASS: {run}')


if __name__=='__main__':main()
