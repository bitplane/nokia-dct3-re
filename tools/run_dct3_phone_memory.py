#!/usr/bin/env python3
"""Product-qualified physical handset-bank save/cold read and lifecycle gates."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from PIL import Image

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.run_noki6210_acceptance import build_command, assess, CONTACT_SHA256, check_frame
from tools.run_sip_handset_gate import verify_handset_health

EMPTY_SHA256='d83d9b52d4f7e98e48ec50060bd97cc17392015bc61b7fe11d54ea401352d3f6'
SELECTED_SHA256='fbae7ab0adf3755f093e4667f00ef00abe06f9ec3209bdc38716b9ee18581bed'
SAVED_SHA256='15855ac7ae0e186aa9522e69c56cf5995ff558541faa17815d2b9b07aff34715'
EDITED_CONTACT_SHA256='850b1d4dfad2663b696d3786cba7b2b6cea45310b1ea96efc6536c13a492c59d'
EDIT_SAVED_SHA256='38494461645e0c2ca1fdb260b741cb0690eb66a8bad42aae39b85bb3c6bb4893'
ERASED_SHA256='b2fab27b6d6da26aff2cf4c8cbf06e66f1187f1a705a4cdfd438161b2a34935a'
ERASE_CONFIRM_SHA256='bbfa86d829e7eef9f98c735bbe7c5c9bb71ec3a2a72b5b6c045aba5f0187e1f3'
LIFECYCLE_FRAMES=(
    ('edited_cold','6210_phonebook_read_contact',EDITED_CONTACT_SHA256),
    ('cancelled_cold','6210_phonebook_read_contact',EDITED_CONTACT_SHA256),
    ('erased_cold','6210_phonebook_read_search_results',EMPTY_SHA256),
    ('edit','6210_phone_memory_lifecycle_edited',EDIT_SAVED_SHA256),
    ('erase','6210_phone_memory_lifecycle_erase',ERASED_SHA256),
    ('cancel','6210_phone_memory_lifecycle_erase_confirm',ERASE_CONFIRM_SHA256),
    ('erase','6210_phone_memory_lifecycle_erase_confirm',ERASE_CONFIRM_SHA256))


def verify_storage(cards,empty_flash,written_flash):
    if len(cards)!=3 or not cards[0] or not cards[0]==cards[1]==cards[2]:
        raise ValueError('handset contact changed SIM contents')
    if len(empty_flash)!=0x400000 or len(written_flash)!=len(empty_flash):
        raise ValueError('incomplete handset flash artifact')
    changed=[i for i,(a,b) in enumerate(zip(empty_flash,written_flash)) if a!=b]
    if not changed:raise ValueError('contact save did not change persistent handset flash')
    return {'count':len(changed),'first':changed[0],'last':changed[-1]}


def verify_lifecycle_digests(actual):
    expected={(phase,filename):digest for phase,filename,digest in LIFECYCLE_FRAMES}
    if actual!=expected:
        raise ValueError('handset contact edit/cancel/erase frame contract differs')


def verify_lifecycle_frames(directory):
    actual={}
    for phase,filename,_ in LIFECYCLE_FRAMES:
        with Image.open(directory/phase/f'snap/{filename}.png') as frame:
            actual[phase,filename]=hashlib.sha256(frame.convert('L').tobytes()).hexdigest()
    verify_lifecycle_digests(actual)


def run(directory,lifecycle=False,product='6210'):
    root=Path(__file__).resolve().parents[1]
    if product not in ('6210','6250') or (lifecycle and product!='6210'):
        raise ValueError('unqualified handset-memory product/lifecycle combination')
    if product=='6210':
        assess((root/'roms/noki6210/6210_556c.fls').read_bytes(),
               (root/'roms/noki6210/6210 virgin eeprom 005fa000.fls').read_bytes())
    machine='npe3hle' if product=='6210' else 'nhm3hle'
    directory.mkdir(parents=True,exist_ok=False)
    environment=os.environ.copy()
    environment.pop('NOKIA_DCT3_6210_PIN_ENTRY',None)
    environment.pop('NOKIA_DCT3_6250_PIN_ENTRY',None)
    results={}
    phases=[('empty','phone_memory_empty',42),('write','phone_memory',46),
            ('cold','phonebook_read' if product=='6210' else 'phonebook_readback',
             27 if product=='6210' else 30)]
    if lifecycle:
        phases += [('edit','phone_memory_edit',35),('edited_cold','phonebook_read',27),
                   ('cancel','phone_memory_cancel',33),('cancelled_cold','phonebook_read',27),
                   ('erase','phone_memory_erase',33),('erased_cold','phonebook_read',27)]
    previous=None
    for phase,script,seconds in phases:
        target=directory/phase
        audit_members=[]
        if product=='6250':
            from tools.run_noki6250_acceptance import prepare_run
            _,audit_members=prepare_run(target,root)
        else:
            target.mkdir()
        if phase not in ('empty','write'):
            shutil.copytree(directory/previous/'nvram',target/'nvram',dirs_exist_ok=True)
        initial={str(p.relative_to(target/'nvram')):hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in sorted((target/'nvram').rglob('*')) if p.is_file()}
        command=build_command(root,None,machine,script,seconds)
        command[command.index('-autoboot_script')+1]=str(root/f'tools/noki{product}_{script}.lua')
        if product=='6250':
            command[command.index('-rompath')+1]=f'{target/"roms"};{root/"roms"}'
        launch={'command':command,'initial_storage':initial,'product':product,
                'provisioning': 'own PMM' if product=='6210' else 'own initial-record PMM comparison',
                'audit_members_not_native_product_evidence':audit_members,
                'script_sha256':hashlib.sha256(Path(command[command.index('-autoboot_script')+1]).read_bytes()).hexdigest(),
                'physical_helper_sha256':hashlib.sha256((root/'tools/dct3_phone_memory_input.lua').read_bytes()).hexdigest(),
                'mame_sha256':hashlib.sha256(Path(command[0]).read_bytes()).hexdigest()}
        (target/'launch.json').write_text(json.dumps(launch,indent=2)+'\n')
        with (target/'console.log').open('w') as output:
            subprocess.run(command,cwd=target,stdout=output,stderr=subprocess.STDOUT,
                           check=True,timeout=180,env=environment)
        verify_handset_health((target/'console.log').read_text(errors='replace'))
        text=(target/'error.log').read_text(errors='replace')
        if product=='6250':
            from tools.noki6250_staged_check import check,check_initial_fixture
            check(text,runtime=True)
            check_initial_fixture(text)
        else:
            from tools.noki6210_staged_check import verify
            verify(text,runtime=True,selftest=True)
        if 'ins=dc' in text:raise ValueError('handset-memory lifecycle wrote a SIM record')
        results[phase]=text
        previous=phase
    cards=[(directory/p/f'nvram/{machine}/sim_card').read_bytes()
           for p in ('empty','write','cold')]
    delta=verify_storage(cards,(directory/f'empty/nvram/{machine}/flash').read_bytes(),
                         (directory/f'write/nvram/{machine}/flash').read_bytes())
    for phase in ('empty','write'):
        if product+'_phone_memory: complete=1 firmware_state_forcing=0' not in results[phase]:
            raise ValueError('physical contact fixture did not finish')
    cold_marker=('6210_phonebook_read_physical: action=contact' if product=='6210' else
                 '6250_phonebook_readback_input: step=8 pressed=0')
    if cold_marker not in results['cold']:
        raise ValueError('cold process did not physically select the contact')
    cold_frame='6210_phonebook_read_contact.png' if product=='6210' else '6250_phonebook_readback_5.png'
    with Image.open(directory/'cold/snap'/cold_frame) as frame:
        check_frame(frame,CONTACT_SHA256,'cold handset A / 123 contact')
    for phase,name,digest in (('empty','empty',EMPTY_SHA256),
                               ('empty','selected',SELECTED_SHA256),
                               ('write','selected',SELECTED_SHA256),
                               ('write','saved',SAVED_SHA256 if product=='6210' else
                                '364aac2f791e37fdfd37df6f6252c489d0ff837375079339f6fad70a1eefc8d7')):
        with Image.open(directory/phase/f'snap/{product}_phone_memory_{name}.png') as frame:
            check_frame(frame,digest,phase+' '+name)
    if lifecycle:
        for phase,_,_ in phases[3:]:
            if (directory/phase/'nvram/npe3hle/sim_card').read_bytes()!=cards[0]:
                raise ValueError('handset lifecycle changed SIM contents: '+phase)
        for phase in ('edit','cancel','erase'):
            if '6210_phone_memory_lifecycle: complete=1 firmware_state_forcing=0' not in results[phase]:
                raise ValueError('physical lifecycle fixture did not finish: '+phase)
        verify_lifecycle_frames(directory)
    (directory/'result.json').write_text(json.dumps({'flash_delta':delta,
        'product':product,'research_hle':True,'sim_unchanged':True,
        'cold_contact':True,'edit_cancel_erase':lifecycle},indent=2)+'\n')
    print('OK - handset-memory '+('save/edit/cancel/erase cold lifecycle' if lifecycle else
          'A/123 cold read')+', unchanged SIM, separate empty control')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--lifecycle',action='store_true')
    parser.add_argument('--product',choices=('6210','6250'),default='6210')
    args=parser.parse_args()
    try:run(args.directory.resolve(),args.lifecycle,args.product)
    except (ValueError,RuntimeError,OSError,subprocess.SubprocessError) as error:
        parser.exit(1,f'FAIL - {error}\n')


if __name__=='__main__':main()
