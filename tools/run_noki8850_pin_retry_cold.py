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
from tools.noki8850_staged_trace_check import check_correlated_inputs
from tools.radio_registration_trace_check import verify as registration
from tools.run_sip_handset_gate import verify_handset_health
from tools.sim_security_trace_check import statuses
from tools.noki6210_puk_check import contacts
from tools.radio_call_lifecycle_common import require_ordered


def storage_hashes(directory):
    return {str(path.relative_to(directory)):hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(directory.rglob('*')) if path.is_file()}


def mame_command(root,script,seconds):
    return [str(root/'mame/mame'),'nsm2hle','-rompath',str(root/'roms'),
        '-nvram_directory','nvram','-cfg_directory','cfg','-snapshot_directory','snap',
        '-noreadconfig','-debug','-debugger','none','-verbose','-log','-video','none',
        '-sound','none','-nothrottle','-autoboot_delay','0',
        '-autoboot_script',str(root/f'tools/noki8850_{script}.lua'),'-seconds_to_run',str(seconds)]


def check_wrong_frame(path):
    from PIL import Image
    with Image.open(path) as image:
        if (image.size!=(84,48) or hashlib.sha256(image.convert('L').tobytes()).hexdigest()!=
                'd11e338fd4431cb6a72f2e2c133267242f305dcc6ae0c2f15fbcddebfa310f61'):
            raise ValueError('8850 wrong-PIN Code error frame differs')


PUK_FRAMES={
    'prompt':'6c3cab31a3ab5a0bf2d7a0fbd544f4893889df800b41d2126eb2a200906cca04',
    'new_pin':'7fa8272e82101045b96351f5beb7fbff5a75cd88c9314306b1c13fc7c2777369',
    'confirm':'4acba33e5eea3b83c961e19b0f7d70b82f8c948fcb4a61246664048fd4e8f7e8',
    'result':'4bb6d42eb358ea3ada7e343a1ffe18ff68c416ea3ac5a54ba99594c301d0533a',
}


def check_puk_frames(directory,name,remaining=9):
    from PIL import Image
    frames={}
    if name=='blocked':
        for attempt in (1,2):check_wrong_frame(directory/f'8850_pin_wrong_attempt_{attempt}.png')
        frames['8850_pin_wrong_attempt_3.png']=PUK_FRAMES['prompt']
    elif name in ('puk','wrong-puk'):
        frames={f'8850_puk_{label}.png':digest for label,digest in PUK_FRAMES.items()}
        if name=='wrong-puk':
            frames['8850_puk_result.png']=(
                '085e8c0db175dded35bd2fc7e9be74cfb7963fa563e525ae0c77cdb5208e0336' if remaining==0 else
                '0b450ca748a44647be303fb1ea7c9f4d7a43de11c8afb09d7a29cffd10976325')
        else:
            check_messages_menu(directory/'8850_puk_menu.png')
    elif name=='permanent':
        # Stage filenames persist, but these are ordinary dial-entry screens,
        # not active PUK/new-PIN editors. End returns to SIM card rejected.
        frames={f'8850_puk_{label}.png':digest for label,digest in {
            'prompt':'a8402b7db24e5909bc8aec8be13a357e9e4f1517c09a999f950dc62c9f43bfe9',
            'new_pin':'0ce85681cac414837bf0ee55c0ffb59b8d27317118b4c1d659c170cc74b81abe',
            'confirm':'a8f8aaeb95d1d304160155c9dc6c0d93324c612836a0e0569541d796d237341d',
            'result':'a8f8aaeb95d1d304160155c9dc6c0d93324c612836a0e0569541d796d237341d',
            'permanent_after':'a8402b7db24e5909bc8aec8be13a357e9e4f1517c09a999f950dc62c9f43bfe9'}.items()}
    for filename,digest in frames.items():
        with Image.open(directory/filename) as image:
            if image.size!=(84,48) or hashlib.sha256(image.convert('L').tobytes()).hexdigest()!=digest:
                raise ValueError('8850 PUK stage pixels differ: '+filename)


def check_messages_menu(path):
    from PIL import Image
    with Image.open(path) as frame:
        # The established Messages title oracle excludes the animated icon.
        if (frame.size!=(84,48) or hashlib.sha256(frame.convert('L').crop((0,0,72,16)).tobytes()).hexdigest()!=
                '1e5c11fcea9aac5331e18c0070697e8795003d6de9a0250284b3d9605209e654'):
            raise ValueError('8850 physical Menu did not render Messages')


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


def check_puk_phase(text,initial,final,name,*,puk_attempts=10):
    blocked=bytearray(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43)
    blocked[3516]=0
    if not 0<=puk_attempts<=10 or (puk_attempts==0 and name!='permanent'):
        raise ValueError('unqualified 8850 pre-recovery PUK allowance')
    if (name=='permanent' and puk_attempts!=0) or (name=='blocked' and puk_attempts!=10):
        raise ValueError('8850 exhaustion phase has inconsistent initial allowance')
    blocked[3518]=puk_attempts
    rotated=bytearray(blocked)
    rotated[3516]=3
    rotated[3518]=10
    rotated[3484:3492]=b'2468\xff\xff\xff\xff'
    rotated[1604:1609]=bytes.fromhex('00f1100001')
    rotated[1610]=0
    rejected=bytearray(blocked)
    rejected[3518]=max(0,puk_attempts-1)
    inputs={'blocked':make_profile(True),'puk':bytes(blocked),'reauth':bytes(rotated),
            'wrong-puk':bytes(blocked),'permanent':bytes(blocked)}
    outputs={'blocked':bytes(blocked),'puk':bytes(rotated),'reauth':bytes(rotated),
             'wrong-puk':bytes(rejected),'permanent':bytes(blocked)}
    if name not in inputs or initial!=inputs[name] or final!=outputs[name]:
        raise ValueError('8850 PUK phase did not retain complete expected card contents')
    verify={'blocked':[(0x9804,2,10),(0x9804,1,10),(0x9840,0,10)],
            'puk':[],'reauth':[(0x9000,3,10)],'wrong-puk':[],'permanent':[]}[name]
    unblock=([(0x9840 if puk_attempts==1 else 0x9804,0,puk_attempts-1)] if name=='wrong-puk' else
             [(0x9000,3,10)] if name=='puk' else [])
    if statuses(text,0x20)!=verify or statuses(text,0x2c)!=unblock:
        raise ValueError('8850 PUK phase has wrong credential results or retry allowances')
    selectors={'blocked':['01']*3,'puk':[],'reauth':['01'],'wrong-puk':[],'permanent':[]}[name]
    if re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=selectors:
        raise ValueError('8850 PUK phase has unexpected VERIFY selector/count')
    if re.findall(r'header cla=a0 ins=2c p1=00 p2=(\w\w) p3=10',text)!=(['00'] if name in ('puk','wrong-puk') else []):
        raise ValueError('8850 PUK phase has unexpected UNBLOCK selector/count')
    if any(statuses(text,ins) for ins in (4,0x24,0x26,0x28,0x44,0xdc)):
        raise ValueError('8850 PUK phase changed unrelated configuration or records')
    if name=='blocked':
        items=[]
        ordered=[]
        for start in (8,16,24):
            items += [(start+i,'Keypad 9') for i in range(4)]+[(start+4,'Menu')]
            status={8:'9804',16:'9804',24:'9840'}[start]
            ordered += [('submit',rf'8850_pin_wrong: key=Menu state=1 t={start+4}\.'),
                        ('result',rf'SIM status ins=20 sw={status}')]
        contacts(text,'8850_pin_wrong',items)
        require_ordered(text,ordered,'8850 PIN exhaustion')
        if ('8850_pin_wrong: complete=1 firmware_state_forcing=0' not in text or
                'LAPDm Location Updating Accept acknowledged' in text):
            raise ValueError('8850 exhausted PIN incorrectly registered or lacked completion')
    elif name in ('puk','wrong-puk','permanent'):
        wrong=name!='puk'
        permanent=name=='permanent'
        items=[(8+i,'Keypad '+str(9 if wrong else i+1)) for i in range(8)]+[(16,'Menu')]
        for group in (0,1):
            items += [(20+group*8+i,'Keypad '+str(digit)) for i,digit in enumerate((2,4,6,8))]
            items += [(24+group*8,'Menu')]
        if not wrong:items += [(40+i,'Keypad '+str(i+1)) for i in range(5)]+[(45,'Menu'),(50,'Menu')]
        if permanent:items.append((38,'End'))
        contacts(text,'8850_puk_input',items)
        marker='permanent_complete' if permanent else 'wrong_complete' if wrong else 'complete'
        if f'8850_puk_input: {marker}=1 firmware_state_forcing=0' not in text:
            raise ValueError('8850 PUK physical sequence did not finish')
        if wrong:
            if 'LAPDm Location Updating Accept acknowledged' in text:
                raise ValueError('wrong PUK incorrectly registered')
            if permanent:
                if any(token in text for token in ('GSM outgoing request','gsm_mm: cm_service_request')):
                    raise ValueError('permanently blocked credential input started a call')
                require_ordered(text,[('End',r'8850_puk_input: key=End state=1 t=38\.'),
                    ('complete',r'8850_puk_input: permanent_complete=1 firmware_state_forcing=0')],'8850 permanent card')
                return
            require_ordered(text,[('confirm',r'8850_puk_input: key=Menu state=1 t=32\.'),
                ('rejected',rf'SIM status ins=2c sw={"9840" if puk_attempts==1 else "9804"}'),
                ('complete',r'8850_puk_input: wrong_complete=1 firmware_state_forcing=0')],'8850 wrong PUK')
            return
        require_ordered(text,[('confirm',r'8850_puk_input: key=Menu state=1 t=32\.'),
            ('unblocked',r'SIM status ins=2c sw=9000'),
            ('registered',r'LAPDm Location Updating Accept acknowledged'),
            ('Menu',r'8850_puk_input: key=Menu state=1 t=50\.'),
            ('complete',r'8850_puk_input: complete=1 firmware_state_forcing=0')],'8850 PUK recovery')
        registration(text,'nsm2')
    else:
        errors=check_correlated_inputs(text,tuple(('pin_physical: key='+key,decoded)
            for key,decoded in (('Keypad 2','02'),('Keypad 4','04'),('Keypad 6','06'),('Keypad 8','08'),('Menu','19'))))
        if errors:raise ValueError('; '.join(errors))
        require_ordered(text,[('submit',r'8850_pin_physical: key=Menu'),
            ('verified',r'SIM status ins=20 sw=9000'),
            ('registered',r'LAPDm Location Updating Accept acknowledged')],'8850 rotated PIN')
        registration(text,'nsm2',preserved=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--puk',action='store_true',help='exhaust PIN1, recover via PUK and cold-authenticate the new PIN')
    parser.add_argument('--wrong-puk-first',action='store_true',help='reject one PUK in a cold process before correct recovery')
    parser.add_argument('--exhaust-puk',action='store_true',help='physically consume all ten PUK allowances and test cold permanent rejection')
    args=parser.parse_args()
    if args.wrong_puk_first and not args.puk:parser.error('--wrong-puk-first requires --puk')
    if args.exhaust_puk and (not args.puk or args.wrong_puk_first):
        parser.error('--exhaust-puk requires --puk without --wrong-puk-first')
    root=Path(__file__).resolve().parents[1]
    try:
        profile=PROFILES['8850']
        verify_inputs('8850',(root/'roms/noki8850'/profile[1]).read_bytes(),
                      (root/'roms/noki8850'/profile[3]).read_bytes())
        run=args.directory.resolve()
        run.mkdir(parents=True,exist_ok=False)
        previous=None
        launches=[]
        phases=(('blocked','pin_exhaust',36),('puk','puk_recovery',56),('reauth','pin_rotated',46)) if args.puk else (
            ('wrong','pin_wrong',18),('recovery','security_input',46))
        if args.wrong_puk_first:
            phases=(phases[0],('wrong-puk','puk_wrong',38),*phases[1:])
        if args.exhaust_puk:
            phases=(phases[0],*((f'wrong-puk-{attempt:02d}','puk_wrong',38) for attempt in range(1,11)),
                    ('permanent','puk_permanent',44))
        for name,script,seconds in phases:
            kind='wrong-puk' if name.startswith('wrong-puk-') else name
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
            command=mame_command(root,script,seconds)
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
            errors=check_trace(text,runtime_hle=True,sim_reads=bool(previous) and kind not in ('wrong-puk','permanent'))
            if previous and kind not in ('puk','wrong-puk','permanent'):errors+=check_physical_inputs(text)+check_navigation_frames(phase/'snap')
            if errors:raise ValueError('; '.join(errors))
            if args.puk:
                allowance=(11-int(name.rsplit('-',1)[1]) if name.startswith('wrong-puk-') else
                           0 if name=='permanent' else 9 if args.wrong_puk_first and name=='puk' else 10)
                check_puk_phase(text,initial,card.read_bytes(),kind,puk_attempts=allowance)
                check_puk_frames(phase/'snap',kind,remaining=allowance-1)
            else:
                check_card_phase(text,initial,card.read_bytes(),bool(previous))
                if not previous:check_wrong_frame(phase/'snap/8850_pin_wrong.png')
            if previous and storage_hashes(previous/'nvram')!=predecessor_storage:
                raise ValueError('cold recovery mutated predecessor NVRAM')
            launches.append(launch)
            previous=phase
            print('8850 cold phase '+name+': verified',flush=True)
        (run/'acceptance.json').write_text(json.dumps({'phases':launches,
            'puk_recovery':args.puk,'wrong_puk_first':args.wrong_puk_first,
            'permanent_puk_loss':args.exhaust_puk,
            'research_hle':True,'native_speech':False},indent=2)+'\n')
    except (ValueError,RuntimeError,OSError,subprocess.SubprocessError) as error:
        parser.exit(1,f'8850 cold PIN retry FAIL: {error}\n')
    print('8850 '+('PUK exhaustion and cold permanent rejection' if args.exhaust_puk else
                  'PIN exhaustion, PUK recovery and new-PIN cold authentication' if args.puk else
                  'durable wrong-PIN retry and independent cold recovery')+' PASS')


if __name__=='__main__':main()
