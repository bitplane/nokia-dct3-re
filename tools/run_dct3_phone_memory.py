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
RENAMED_CONTACT_SHA256='8f1c905e4eef5dec78b1016d3a1b6ae2513757c6b7bb7187063f6f6b6efeb632'
NAME_D_SHA256='1ba8b029ab4a03f6af634b7ad8a822aee6d639fc880bb9d8e543babf93673156'
SECOND_NUMBER_SHA256='7efcee0e836a4612968e1bc784f10b17e1bedff7297c92f1be5bbc8be28e3632'
NUMBER_ERASE_CONFIRM_SHA256='30c481db732cafc9d66eaac83e60a5e52c7ffc2b63385cebc9f3e943e2a59b49'
CONTACT_NAME_SHA256='0c07ce69970044e3a81a22b900615000dc70bd32188daac594765b4acaa6540f'
LIFECYCLE_FRAMES=(
    ('edited_cold','6210_phonebook_read_contact',EDITED_CONTACT_SHA256),
    ('cancelled_cold','6210_phonebook_read_contact',EDITED_CONTACT_SHA256),
    ('erased_cold','6210_phonebook_read_search_results',EMPTY_SHA256),
    ('edit','6210_phone_memory_lifecycle_edited',EDIT_SAVED_SHA256),
    ('erase','6210_phone_memory_lifecycle_erase',ERASED_SHA256),
    ('cancel','6210_phone_memory_lifecycle_erase_confirm',ERASE_CONFIRM_SHA256),
    ('erase','6210_phone_memory_lifecycle_erase_confirm',ERASE_CONFIRM_SHA256))
LIFECYCLE_FRAMES_6250=(
    ('edited_cold','6250_phonebook_readback_5',EDITED_CONTACT_SHA256),
    ('cancelled_cold','6250_phonebook_readback_5',EDITED_CONTACT_SHA256),
    ('erased_cold','6250_phonebook_readback_4',EMPTY_SHA256),
    ('edit','6250_phone_memory_lifecycle_edited','364aac2f791e37fdfd37df6f6252c489d0ff837375079339f6fad70a1eefc8d7'),
    ('erase','6250_phone_memory_lifecycle_erase',ERASED_SHA256),
    ('cancel','6250_phone_memory_lifecycle_erase_confirm',ERASE_CONFIRM_SHA256),
    ('erase','6250_phone_memory_lifecycle_erase_confirm',ERASE_CONFIRM_SHA256))


def lifecycle_frames(product):
    return {'6210':LIFECYCLE_FRAMES,'6250':LIFECYCLE_FRAMES_6250}[product]


def verify_storage(cards,empty_flash,written_flash):
    if len(cards)!=3 or not cards[0] or not cards[0]==cards[1]==cards[2]:
        raise ValueError('handset contact changed SIM contents')
    if len(empty_flash)!=0x400000 or len(written_flash)!=len(empty_flash):
        raise ValueError('incomplete handset flash artifact')
    changed=[i for i,(a,b) in enumerate(zip(empty_flash,written_flash)) if a!=b]
    if not changed:raise ValueError('contact save did not change persistent handset flash')
    return {'count':len(changed),'first':changed[0],'last':changed[-1]}


def verify_lifecycle_digests(actual,product='6210'):
    expected={(phase,filename):digest for phase,filename,digest in lifecycle_frames(product)}
    if actual!=expected:
        raise ValueError('handset contact edit/cancel/erase frame contract differs')


def verify_rename_digests(actual,product='6210'):
    saved=EDIT_SAVED_SHA256 if product=='6210' else '364aac2f791e37fdfd37df6f6252c489d0ff837375079339f6fad70a1eefc8d7'
    if actual!={'editor':NAME_D_SHA256,'saved':saved,'cold':RENAMED_CONTACT_SHA256}:
        raise ValueError('handset rename did not preserve D / 123 across cold boot')


def verify_number_digests(actual):
    expected={'before_first':CONTACT_SHA256,'before_second':SECOND_NUMBER_SHA256,
              'before_third':CONTACT_NAME_SHA256,
              'erase_confirm':NUMBER_ERASE_CONFIRM_SHA256,
              'after_first':CONTACT_SHA256,'after_second':CONTACT_NAME_SHA256,
              'after_third':CONTACT_SHA256}
    if actual!=expected:
        raise ValueError('second-number add/erase did not preserve A / 123 across cold boots')


def verify_lifecycle_frames(directory,product='6210'):
    actual={}
    for phase,filename,_ in lifecycle_frames(product):
        with Image.open(directory/phase/f'snap/{filename}.png') as frame:
            actual[phase,filename]=hashlib.sha256(frame.convert('L').tobytes()).hexdigest()
    verify_lifecycle_digests(actual,product)


def run(directory,lifecycle=False,product='6210',rename=False,numbers=False):
    root=Path(__file__).resolve().parents[1]
    if product not in ('6210','6250'):
        raise ValueError('unqualified handset-memory product/lifecycle combination')
    if sum((lifecycle,rename,numbers))>1:
        raise ValueError('mutation lifecycles require independent fresh fixtures')
    if product=='6210':
        assess((root/'roms/noki6210/6210_556c.fls').read_bytes(),
               (root/'roms/noki6210/6210 virgin eeprom 005fa000.fls').read_bytes())
    machine='npe3hle' if product=='6210' else 'nhm3hle'
    directory.mkdir(parents=True,exist_ok=False)
    environment=os.environ.copy()
    environment.pop('NOKIA_DCT3_6210_PIN_ENTRY',None)
    environment.pop('NOKIA_DCT3_6250_PIN_ENTRY',None)
    results={}
    read_script='phonebook_read' if product=='6210' else 'phonebook_readback'
    read_seconds=27 if product=='6210' else 30
    phases=[('empty','phone_memory_empty',42),('write','phone_memory',46),
            ('cold',read_script,read_seconds)]
    if lifecycle:
        phases += [('edit','phone_memory_edit',35),('edited_cold',read_script,read_seconds),
                   ('cancel','phone_memory_cancel',33),('cancelled_cold',read_script,read_seconds),
                   ('erase','phone_memory_erase',33),('erased_cold',read_script,read_seconds)]
    if rename:
        phases += [('rename','phone_memory_rename',45),('renamed_cold',read_script,read_seconds)]
    if numbers:
        phases += [('number_add','phone_memory_number_add',40),
                   ('numbers_cold','phone_memory_number_read',32),
                   ('number_erase','phone_memory_number_erase',35),
                   ('number_erased_cold','phone_memory_number_read',32)]
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
                'lifecycle_helper_sha256':hashlib.sha256((root/'tools/dct3_phone_memory_lifecycle.lua').read_bytes()).hexdigest(),
                'numbers_helper_sha256':hashlib.sha256((root/'tools/dct3_phone_memory_numbers.lua').read_bytes()).hexdigest(),
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
            if (directory/phase/f'nvram/{machine}/sim_card').read_bytes()!=cards[0]:
                raise ValueError('handset lifecycle changed SIM contents: '+phase)
        for phase in ('edit','cancel','erase'):
            if product+'_phone_memory_lifecycle: complete=1 firmware_state_forcing=0' not in results[phase]:
                raise ValueError('physical lifecycle fixture did not finish: '+phase)
        verify_lifecycle_frames(directory,product)
    if rename:
        for phase in ('rename','renamed_cold'):
            if (directory/phase/f'nvram/{machine}/sim_card').read_bytes()!=cards[0]:
                raise ValueError('handset rename changed SIM contents: '+phase)
        if product+'_phone_memory_lifecycle: complete=1 firmware_state_forcing=0' not in results['rename']:
            raise ValueError('physical rename fixture did not finish')
        if cold_marker not in results['renamed_cold']:
            raise ValueError('renamed cold process did not physically select the contact')
        verify_storage(cards,(directory/f'cold/nvram/{machine}/flash').read_bytes(),
                       (directory/f'rename/nvram/{machine}/flash').read_bytes())
        rename_digests={}
        for key,phase,filename in (
                ('editor','rename',f'{product}_phone_memory_lifecycle_name_D.png'),
                ('saved','rename',f'{product}_phone_memory_lifecycle_renamed.png'),
                ('cold','renamed_cold',cold_frame)):
            with Image.open(directory/phase/'snap'/filename) as frame:
                rename_digests[key]=hashlib.sha256(frame.convert('L').tobytes()).hexdigest()
        verify_rename_digests(rename_digests,product)
    if numbers:
        for phase,_,_ in phases[3:]:
            if (directory/phase/f'nvram/{machine}/sim_card').read_bytes()!=cards[0]:
                raise ValueError('handset number lifecycle changed SIM contents: '+phase)
            if product+'_phone_memory_numbers: complete=1 firmware_state_forcing=0' not in results[phase]:
                raise ValueError('physical number fixture did not finish: '+phase)
        number_digests={}
        for key,phase,label in (
                ('before_first','numbers_cold','first'),('before_second','numbers_cold','second'),
                ('before_third','numbers_cold','third'),
                ('erase_confirm','number_erase','erase_confirm'),
                ('after_first','number_erased_cold','first'),('after_second','number_erased_cold','second'),
                ('after_third','number_erased_cold','third')):
            with Image.open(directory/phase/f'snap/{product}_phone_memory_numbers_{label}.png') as frame:
                number_digests[key]=hashlib.sha256(frame.convert('L').tobytes()).hexdigest()
        verify_number_digests(number_digests)
        for before,after in (('cold','number_add'),('numbers_cold','number_erase')):
            verify_storage(cards,(directory/f'{before}/nvram/{machine}/flash').read_bytes(),
                           (directory/f'{after}/nvram/{machine}/flash').read_bytes())
    (directory/'result.json').write_text(json.dumps({'flash_delta':delta,
        'product':product,'research_hle':True,'sim_unchanged':True,
        'cold_contact':True,'edit_cancel_erase':lifecycle,'rename':rename,
        'second_number_add_erase':numbers},indent=2)+'\n')
    label=('save/edit/cancel/erase cold lifecycle' if lifecycle else
           'A to D rename cold lifecycle' if rename else
           'second-number add/erase cold lifecycle' if numbers else 'A/123 cold read')
    print('OK - handset-memory '+label+', unchanged SIM, separate empty control')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    mode=parser.add_mutually_exclusive_group()
    mode.add_argument('--lifecycle',action='store_true')
    mode.add_argument('--rename',action='store_true')
    mode.add_argument('--numbers',action='store_true')
    parser.add_argument('--product',choices=('6210','6250'),default='6210')
    args=parser.parse_args()
    try:run(args.directory.resolve(),args.lifecycle,args.product,args.rename,args.numbers)
    except (ValueError,RuntimeError,OSError,subprocess.SubprocessError) as error:
        parser.exit(1,f'FAIL - {error}\n')


if __name__=='__main__':main()
