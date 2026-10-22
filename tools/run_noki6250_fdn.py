#!/usr/bin/env python3
"""Physical NHM-3 FDN validity and protected-record cold lifecycles."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid
import xml.etree.ElementTree as ET
from typing import NamedTuple

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.run_noki6250_acceptance import prepare_run,apply_coherent_config
from tools.run_noki6250_pin2_cold import mame_command
from tools.run_noki6250_pin_retry_cold import hashes,read_evidence
from tools.noki6250_fdn_check import initial_card,check,check_frames
from tools.noki6250_coherent_registration_check import verify as registration
from tools.run_sip_handset_gate import verify_handset_health


class Phase(NamedTuple):
    name:str
    script:str
    source:str|None
    seconds:int
    validity:int|None=None
    record_mode:str|None=None
    pin2_attempts:int=3
    rejected:bool=False
    saved_record:bool=False
    dial_mode:str|None=None


def phase_plan(*,records=False,lifecycle=False,rejections=False,dial=False):
    phases=([Phase('reject-on','noki6250_fdn_enable_wrong.lua',None,68,0,rejected=True)] if rejections else [])
    phases += [Phase('on','noki6250_fdn_enable.lua','reject-on' if rejections else None,68,0,pin2_attempts=2 if rejections else 3),
            Phase('cold-on','noki6250_fdn.lua','on',58,1),
            Phase('off','noki6250_fdn_disable.lua','cold-on',68,2),
            Phase('cold-off','noki6250_fdn.lua','off',58,3)]
    if records or lifecycle or rejections or dial:
        if rejections:phases += [Phase('wrong','noki6250_fdn_write_wrong.lua','on',76,record_mode='wrong')]
        phases += [Phase('write','noki6250_fdn_write.lua','wrong' if rejections else 'on',88,record_mode='write',pin2_attempts=2 if rejections else 3),
                   Phase('read','noki6250_fdn_read.lua','write',70,record_mode='read')]
    if lifecycle:
        phases += [Phase('edit','noki6250_fdn_edit.lua','read',90,record_mode='edit'),
                   Phase('edited-read','noki6250_fdn_read.lua','edit',70,record_mode='edited-read'),
                   Phase('cancel','noki6250_fdn_cancel.lua','edited-read',82,record_mode='cancel'),
                   Phase('erase','noki6250_fdn_erase.lua','cancel',82,record_mode='erase'),
                   Phase('empty','noki6250_fdn_empty.lua','erase',64,record_mode='empty')]
    if dial:
        phases += [Phase('listed','noki6250_fdn_dial.lua','read',48,dial_mode='listed'),
                   Phase('denied','noki6250_fdn_dial_unlisted.lua','read',48,dial_mode='denied'),
                   Phase('off-record','noki6250_fdn_disable.lua','read',68,2,saved_record=True),
                   Phase('off-call','noki6250_fdn_dial_unlisted.lua','off-record',48,dial_mode='off')]
    return phases


def configure_dial_peer(path):
    tree=ET.parse(path)
    system=tree.find("./system")
    if system is None or system.get('name')!='nhm3hle':
        raise ValueError('FDN dial configuration must belong to nhm3hle')
    inputs=system.find('input')
    if inputs is None:inputs=ET.SubElement(system,'input')
    host=inputs.find("port[@tag=':CALLHOST']")
    if host is None:
        host=ET.SubElement(inputs,'port',tag=':CALLHOST',type='CONFIG',mask='1',defvalue='0')
    host.set('value','0')
    tree.write(path,encoding='utf-8',xml_declaration=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run',type=Path)
    parser.add_argument('--mame',type=Path)
    parser.add_argument('--records',action='store_true',help='add PIN2-protected FDN Save and cold Details after the validity cycle')
    parser.add_argument('--record-lifecycle',action='store_true',help='also qualify protected Edit, cold edited Details, erase cancellation, confirmed erase and cold empty list')
    parser.add_argument('--rejections',action='store_true',help='reject activation and protected Add with wrong PIN2, then cold-recover each retained retry loss')
    parser.add_argument('--dial-controls',action='store_true',help='test listed/unlisted FDN-On calls and the unlisted number after physical Off against the isolated automatic lab peer')
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
        phases=phase_plan(records=args.records,lifecycle=args.record_lifecycle,rejections=args.rejections,dial=args.dial_controls)
        for phase in phases:
            name,script=phase.name,phase.script
            previous=run/phase.source if phase.source is not None else None
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
            if phase.dial_mode is not None:
                from tools.noki6250_fdn_dial_check import expected_card as dial_card
                expected_input=dial_card(phase.dial_mode)
                configure_dial_peer(directory/'cfg/nhm3hle.cfg')
            elif phase.record_mode is not None:
                from tools.noki6250_fdn_record_check import expected_card
                mode=phase.record_mode
                expected_input=expected_card(mode not in ('write','empty','wrong'),mode in ('edited-read','cancel','erase'),pin2_attempts=phase.pin2_attempts)
            else:
                from tools.nsk3_phonebook_check import expected_record
                expected_input=initial_card(phase.validity,pin2_attempts=phase.pin2_attempts,preserved=previous is not None,
                                            record=expected_record() if phase.saved_record else None)
            if initial!=expected_input:
                raise ValueError('FDN cold input does not match its whole-card predecessor contract')
            (directory/'initial_sim_card.bin').write_bytes(initial)
            script=root/'tools'/script
            if hashlib.sha256(binary.read_bytes()).hexdigest()!=binary_hash or hashlib.sha256(shared.read_bytes()).hexdigest()!=fixture_hash:
                raise ValueError('FDN executable or shared physical fixture changed')
            command=mame_command(binary,root,directory,script,phase.seconds)
            launch={'phase':name,'invocation_id':str(uuid.uuid4()),'command':command,
                    'initial_storage':hashes(directory/'nvram'),'source_phase':previous.name if previous is not None else None,'mame_sha256':binary_hash,
                    'fixture_base_sha256':fixture_hash,'script_sha256':hashlib.sha256(script.read_bytes()).hexdigest()}
            if phase.dial_mode is not None:launch['laboratory_callhost']=0
            (directory/'launch.json').write_text(json.dumps(launch,indent=2)+'\n')
            environment=os.environ.copy()
            environment.pop('NOKIA_DCT3_6250_PIN_ENTRY',None)
            with (directory/'console.log').open('w') as output:
                subprocess.run(command,cwd=directory,stdout=output,stderr=subprocess.STDOUT,
                               check=True,timeout=180,env=environment)
            verify_handset_health((directory/'console.log').read_text(errors='replace'))
            text=read_evidence(directory/'error.log')
            final=card.read_bytes()
            if phase.dial_mode is not None:
                from tools.noki6250_fdn_dial_check import check as check_dial,check_frames as check_dial_frames
                check_dial(text,initial,final,phase.dial_mode)
                check_dial_frames(directory/'snap',phase.dial_mode)
            elif phase.record_mode is not None:
                from tools.noki6250_fdn_record_check import check as check_record,check_frames as check_record_frames
                check_record(text,initial,final,phase.record_mode,pin2_attempts=phase.pin2_attempts)
                check_record_frames(directory/'snap',phase.record_mode)
            else:
                check(text,initial,final,phase.validity,rejected=phase.rejected,pin2_attempts=phase.pin2_attempts,preserved=previous is not None,
                      record=expected_record() if phase.saved_record else None)
                check_frames(directory/'snap',phase.validity,rejected=phase.rejected)
            registration(text,final,preserved=previous is not None,require_host=False)
            if any(hashes(path)!=digest for path,digest in protected.items()):
                raise ValueError('FDN lifecycle mutated predecessor artifacts')
            protected[directory]=hashes(directory)
            previous=directory
            launches.append(launch)
            print(f'6250 FDN {name}: verified',flush=True)
        (run/'acceptance.json').write_text(json.dumps({'phases':launches,'independent_processes':len(phases),'fdn_records':args.records or args.record_lifecycle or args.rejections or args.dial_controls,
            'fdn_record_lifecycle':args.record_lifecycle,'fdn_rejections':args.rejections,
            'fdn_dial_controls':args.dial_controls,
            'research_hle':True,'native_speech':False},indent=2)+'\n')
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as error:
        parser.exit(1,f'6250 FDN lifecycle FAIL: {error}\n')
    label=('FDN listed/unlisted/Off laboratory call controls' if args.dial_controls else
           'FDN rejection and cold retry recovery'+(' with complete record lifecycle' if args.record_lifecycle else '') if args.rejections else
           'FDN protected record lifecycle' if args.record_lifecycle else
           'FDN validity plus protected Save/cold Details' if args.records else 'FDN On/cold-On/Off/cold-Off')
    print(f'6250 physical {label} PASS: {run}')


if __name__=='__main__':main()
