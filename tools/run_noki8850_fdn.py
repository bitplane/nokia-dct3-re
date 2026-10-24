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
import xml.etree.ElementTree as ET

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
    saved_record:bool=False
    dial:str|None=None


def phase_plan(records=False,dial=False,lifecycle=False):
    phases=[Phase('on','fdn_on',None,55,0),Phase('cold_on','fdn','on',55,1),
            Phase('off','fdn_off','cold_on',55,2),Phase('cold_off','fdn','off',55,3)]
    if records or dial or lifecycle:
        phases += [Phase('write','fdn_write','on',67,record='write'),
                   Phase('read','fdn_read','write',55,record='read')]
    if lifecycle:
        phases += [Phase('edit','fdn_edit','read',67,record='edit'),
                   Phase('edited_read','fdn_read','edit',55,record='edited_read'),
                   Phase('cancel','fdn_cancel','edited_read',65,record='cancel'),
                   Phase('cancelled_read','fdn_read','cancel',55,record='cancelled_read'),
                   Phase('erase','fdn_erase','cancelled_read',65,record='erase'),
                   Phase('empty','fdn_empty','erase',55,record='empty')]
    if dial:
        phases += [Phase('listed','fdn_dial','read',52,dial='listed'),
                   Phase('denied','fdn_dial_unlisted','read',52,dial='denied'),
                   Phase('off_saved','fdn_off','read',55,2,saved_record=True),
                   Phase('off_call','fdn_dial_unlisted','off_saved',52,dial='off')]
    return phases


def expected_card(valid,registered=True):
    base=registered_card('5678') if registered else make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43
    card=bytearray(extend(base))
    card[3575]=int(valid)
    return bytes(card)


def check_phase(text,initial,final,phase,*,saved_record=False):
    if phase not in range(4):raise ValueError('unknown 8850 FDN lifecycle phase')
    before=bytearray(expected_card(phase in (0,3),phase!=0))
    after=bytearray(expected_card(phase>=2))
    if saved_record:
        before[3578:3610]=after[3578:3610]=expected_record()
    if initial!=bytes(before) or final!=bytes(after):
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


def record_card(saved,edited=False):
    if edited and not saved:raise ValueError('an empty FDN card cannot have an edited record')
    card=bytearray(expected_card(False))
    if saved:card[3578:3610]=expected_record()
    if edited:card[3600]=0xf6
    return bytes(card)


def check_record(text,initial,final,mode):
    if mode not in ('write','read','edit','edited_read','cancel','cancelled_read','erase','empty'):
        raise ValueError('unknown 8850 FDN record phase')
    writing=mode in ('write','edit','erase')
    protected=mode in ('write','edit','cancel','erase')
    before=record_card(mode not in ('write','empty'),mode in ('edited_read','cancel','cancelled_read','erase'))
    after=record_card(mode not in ('erase','empty'),mode in ('edit','edited_read','cancel','cancelled_read'))
    if initial!=before or final!=after:
        raise ValueError('8850 FDN record lifecycle changed unrelated card data or lost its expected record')
    if statuses(text,0x20)!=[(0x9000,3,10)]*(2 if protected else 1):
        raise ValueError('8850 FDN record operation has unexpected PIN authentication')
    if re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=(['01','02'] if protected else ['01']):
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
    extra={'write':2,'read':3,'edit':7,'edited_read':3,'cancel':6,'cancelled_read':3,'erase':3,'empty':0}[mode]
    reads=[f'{i:02x}' for i in range(1,11)]+['01']*extra
    if re.findall(r'header cla=a0 ins=b2 p1=(\w\w) p2=04 p3=20 selected=6f3b',text)!=reads:
        raise ValueError('8850 FDN startup/selection/readback extent differs')
    keys=['Menu']+['Scroll Down']*3+['Menu']+['Scroll Down']*5+['Menu']+['Scroll Down']*2+['Menu']
    keys += ['Scroll Down','Scroll Down','Menu']
    if mode=='write':keys += ['Scroll Down','Menu']+['Keypad '+d for d in '5678']+['Menu','Keypad 2','Menu']+['Keypad '+d for d in '12345']+['Menu']
    elif mode=='empty':keys += ['Menu']*2
    else:
        keys += ['Menu']*3
        if protected:
            keys += ['Menu']+['Keypad '+d for d in '5678']+['Menu']
            keys += (['Menu','Menu','Names / C','Keypad 6','Menu'] if mode=='edit' else
                     ['Scroll Down','Menu','Names / C' if mode=='cancel' else 'Menu'])
    finish=60 if mode in ('write','edit') else 58 if mode in ('cancel','erase') else 48
    contacts(text,'8850_fdn',[(24+i,key) for i,key in enumerate(keys)]+[(finish,'End'),(finish+1,'Menu')])
    check_pin_inputs(text)
    registration(text,'nsm2',preserved=True)
    ordered=[('Numbers',r'8850_fdn: key=Menu state=1 t=40\.')]
    if protected:
        submit=47 if mode=='write' else 49
        ordered += [('PIN2 submit',rf'8850_fdn: key=Menu state=1 t={submit}\.'),
            ('PIN2',r'header cla=a0 ins=20 p1=00 p2=02 p3=08'),
            ('verified',r'SIM status ins=20 sw=9000')]
    if mode in ('cancel','erase'):
        ordered += [('erase dialog',r'8850_fdn: key=Menu state=1 t=51\.'),
                    ('answer',rf'8850_fdn: key={"Names / C" if mode=="cancel" else "Menu"} state=1 t=52\.')]
    if writing:
        save={'write':55,'edit':54,'erase':52}[mode]
        if mode!='erase':ordered += [('Save',rf'8850_fdn: key=Menu state=1 t={save}\.')]
        ordered += [
            ('UPDATE',r'header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b'),
            ('saved',r'SIM status ins=dc sw=9000')]
        if mode!='erase':ordered += [('readback',r'header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3b')]
    ordered += [('Menu',rf'8850_fdn: key=Menu state=1 t={finish+1}\.'),
                ('complete',r'8850_fdn: complete=1 firmware_state_forcing=0')]
    require_ordered(text,ordered,'8850 FDN protected record')


def check_record_frames(directory,mode):
    from PIL import Image
    pin2='a7a38ac6f4fe16945c758d9f223e2aeb7e28d46ea55a1371a1a9b9e21565cd9f'
    edited='a1fcf3e79a5ed757c57369d72c75d3d6aa2fd46c1c1d998eff4f45554ee597cb'
    saved='c0701126a0b251a6ae542c05fe44f58c921ddea4db30ddf50181d0dceb6c0377'
    frames={'write':{'record_pin2':pin2,
             'name_editor':'5fe5287ff4f8391953ac6355677ce688f5db043df752ce22537e17115a55b4e9',
             'number_editor':'a4497c68de0c8428e70ae6690f9589203526140d0abb1d581834108802c001d6',
             'saved':saved},
            'read':{'contact':'f5f0050462c46e6d602544f7c153adbfb77d4d0cf2232e4262b7f4039dff1445'},
            'edit':{'record_pin2':pin2,
                    'name_editor':'37a6517fd8bd89097c5355fb9321c71787dec8edd1300632cce60fc707d5cac6',
                    'number_editor':'319e63c9c7e7a85a5fb73ed0c498081cd32d02923d89d815e0e6b0e0185409a3',
                    'edited':saved},
            'edited_read':{'contact':edited},'cancelled_read':{'contact':edited},
            'cancel':{'contact':edited,'record_pin2':pin2,
                      'erase_confirm':'3d2ffab2e4af9a377c9962ebc75e29c74439700bdef4a0e87cc9a74a366c2210'},
            'erase':{'contact':edited,'record_pin2':pin2,
                     'erase_confirm':'3d2ffab2e4af9a377c9962ebc75e29c74439700bdef4a0e87cc9a74a366c2210',
                     'erased':'9df01df08c0765229fae69d03425cfccbe65d5deaf76efb44c8c87c728524ffc'},
            'empty':{'empty':'d9cadf8458021b378a81857585c97b0c591b28f89803d6977839695354f86cf1'}}[mode]
    for label,digest in frames.items():
        with Image.open(directory/f'8850_fdn_{label}.png') as image:
            if image.size!=(84,48) or hashlib.sha256(image.convert('L').tobytes()).hexdigest()!=digest:
                raise ValueError('8850 FDN record pixels differ: '+label)
    check_messages_menu(directory/'8850_fdn_responsive_menu.png')


def dial_card(mode):
    if mode not in ('listed','denied','off'):raise ValueError('unknown 8850 FDN dial control')
    card=bytearray(record_card(True))
    if mode=='off':card[3575]=1
    return bytes(card)


def configure_lab_call_peer(directory):
    directory.mkdir()
    root=ET.Element('mameconfig',version='10')
    inputs=ET.SubElement(ET.SubElement(root,'system',name='nsm2hle'),'input')
    ET.SubElement(inputs,'port',tag=':CALLHOST',type='CONFIG',mask='1',defvalue='0',value='0')
    ET.ElementTree(root).write(directory/'nsm2hle.cfg')


def check_dial(text,initial,final,mode):
    from tools import radio_outgoing_call_trace_check as call
    from tools.noki8850_outgoing_call_check import verify
    if initial!=dial_card(mode) or final!=initial:
        raise ValueError('8850 FDN dial control changed complete saved-card contents')
    if statuses(text,0x20)!=[(0x9000,3,10)] or re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=['01']:
        raise ValueError('8850 FDN dialing must authenticate only startup PIN1')
    if re.search(r'header cla=a0 ins=(?:04|24|26|28|2c|dc) ',text):
        raise ValueError('8850 FDN dial control changed credentials, validity or records')
    targets=[] if mode=='off' else ['6f7e','6f07']
    if (re.findall(r'header cla=a0 ins=44 p1=00 p2=00 p3=00 selected=(\w{4})',text)!=targets or
            statuses(text,0x44)!=[(0x9000,3,10)]*len(targets)):
        raise ValueError('8850 FDN dial startup validity differs')
    reads=[f'{i:02x}' for i in range(1,11)]+(['01'] if mode!='off' else [])
    if re.findall(r'header cla=a0 ins=b2 p1=(\w\w) p2=04 p3=20 selected=6f3b',text)!=reads:
        raise ValueError('8850 FDN dial startup/Send lookup extent differs')
    digits='12345' if mode=='listed' else '12346'
    items=[(24,'End')]+[(26+i,'Keypad '+digit) for i,digit in enumerate(digits)]
    items += [(32,'Call / Send'),(44,'End'),(46,'Menu')]
    contacts(text,'8850_fdn_dial',items)
    check_pin_inputs(text)
    registration(text,'nsm2',preserved=True)
    send=r'8850_fdn_dial: key=Call / Send state=1 t=32\.'
    end=r'8850_fdn_dial: key=End state=1 t=44\.'
    ordered=[('Send',send),('Send decode',r'8850_keypad_decoded key=0e\b')]
    if mode!='off':ordered += [('FDN lookup',r'header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3b')]
    ordered += [('End',end),('Menu',r'8850_fdn_dial: key=Menu state=1 t=46\.'),
        ('complete',r'8850_fdn_dial: complete=1 firmware_state_forcing=0')]
    require_ordered(text,ordered,'8850 FDN physical dial')
    if mode=='denied':
        if any(pattern.search(text) for pattern in (call.CM_SERVICE_REQUEST,call.SETUP,call.TRAFFIC_ASSIGNMENT,call.CONNECT)) or \
                re.search(r'GSM service (?:establish|uplink) sapi=\d+ pd=03 ',text):
            raise ValueError('8850 FDN-denied number reached call establishment')
    else:verify(text,digits,physical_send=send,physical_end=end)


def check_dial_frames(directory,mode):
    from PIL import Image
    frames={
        'listed':('72d54d1aa058a23c026d530f15025c856c9908a7a2c81ef64e7b6dd7b102f592',
                  '3f5d31143e9e9127593892ddce43c96b628ed4f5d2182881f0b4ee53409f95d5'),
        'denied':('0a2afc3a01bea44974930c99c2ed578031cad0bd57f6ca21bcfbb1d2757bcb9c',
                  'cd33e26eaa54b22c20076043c8e03a6a786792c4f1a4c626aa9a3ca2d3bec029'),
        'off':('b34822be18c3b0ffd6bdeaff59753d638f802741a85d68eb73e9480c3dae1828',
               '3f5d31143e9e9127593892ddce43c96b628ed4f5d2182881f0b4ee53409f95d5'),
    }
    for label,digest in zip(('early','result'),frames[mode]):
        with Image.open(directory/f'8850_fdn_dial_{label}.png') as image:
            if image.size!=(84,48) or hashlib.sha256(image.convert('L').tobytes()).hexdigest()!=digest:
                raise ValueError('8850 FDN dial pixels differ: '+mode+'/'+label)
    check_messages_menu(directory/'8850_fdn_dial_menu.png')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--records',action='store_true',help='add protected A/12345 Save and cold FDN Search/Details')
    parser.add_argument('--dial-controls',action='store_true',help='test listed/unlisted FDN-On and physically disabled Off calls with the isolated automatic lab peer')
    parser.add_argument('--record-lifecycle',action='store_true',help='add protected Edit/cold Details, erase cancellation/cold Details and confirmed erase/cold empty Search')
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
        for item in phase_plan(args.records,args.dial_controls,args.record_lifecycle):
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
            if item.dial:configure_lab_call_peer(phase/'cfg')
            launch={'invocation_id':str(uuid.uuid4()),'command':command,
                'source_phase':item.source,
                'initial_storage':storage_hashes(phase/'nvram'),'mcu_sha1':profile[2],'pmm_sha1':profile[4],
                'mame_sha256':hashlib.sha256((root/'mame/mame').read_bytes()).hexdigest(),
                'input_sha256':{path.name:hashlib.sha256(path.read_bytes()).hexdigest()
                    for path in sorted((root/'tools').glob('noki8850_fdn*.lua'))+
                        [root/'tools/noki8850_security_input.lua',root/'tools/noki8850_startup_observe.lua']}}
            if item.dial:
                launch['laboratory_callhost']=0
                launch['config_sha256']=hashlib.sha256((phase/'cfg/nsm2hle.cfg').read_bytes()).hexdigest()
                launch['dial_fixture_sha256']=hashlib.sha256((root/'tools/noki8850_fdn_dial.lua').read_bytes()).hexdigest()
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
            if item.dial:
                check_dial(text,initial,card.read_bytes(),item.dial)
                check_dial_frames(phase/'snap',item.dial)
            elif item.record:
                check_record(text,initial,card.read_bytes(),item.record)
                check_record_frames(phase/'snap',item.record)
            else:
                check_phase(text,initial,card.read_bytes(),item.validity,saved_record=item.saved_record)
                check_frames(phase/'snap',item.validity)
            if previous and storage_hashes(previous/'nvram')!=predecessor:
                raise ValueError('8850 FDN cold process mutated predecessor NVRAM')
            launches.append(launch)
            previous=phase
            print('8850 FDN phase '+name+': verified',flush=True)
        (run/'acceptance.json').write_text(json.dumps({'phases':launches,'research_hle':True,
            'protected_record_save_cold_read':args.records or args.dial_controls or args.record_lifecycle,
            'protected_record_lifecycle':args.record_lifecycle,
            'dial_permission_tested':args.dial_controls,'native_speech':False},indent=2)+'\n')
    except (ValueError,RuntimeError,OSError,subprocess.SubprocessError) as error:
        parser.exit(1,f'8850 FDN lifecycle FAIL: {error}\n')
    print('8850 '+('listed/unlisted FDN-On and physical-Off call controls' if args.dial_controls else
                  'physical FDN validity and cold On/Off persistence')+' PASS')


if __name__=='__main__':main()
