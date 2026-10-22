#!/usr/bin/env python3
"""Independent NHM-3 cold processes qualify PIN1 retry and PUK recovery."""
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


def check_puk_phase(text,initial,final,name,*,puk_attempts=10):
    blocked=bytearray(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43)
    blocked[3516]=0
    rotated=bytearray(registered_card())
    rotated[3484:3492]=b'2468\xff\xff\xff\xff'
    if name=='blocked':
        if initial!=make_profile(True) or final!=bytes(blocked):
            raise ValueError('PIN exhaustion changed unrelated card bytes')
        if statuses(text,0x20)!=[(0x9804,2,10),(0x9804,1,10),(0x9840,0,10)]:
            raise ValueError('PIN exhaustion must consume three physical attempts')
        items=[]
        for start in (8,16,24):
            items += [(start+i,'Keypad 9') for i in range(4)]+[(start+4,'Left Softkey / Menu')]
        contacts(text,'6250_pin_retry',items)
        selectors=['01']*3
        ordered=[]
        for start,status in ((8,'9804'),(16,'9804'),(24,'9840')):
            ordered += [('submit',rf'6250_pin_retry: key=Left Softkey / Menu state=1 t={start+4}\.'),
                        ('result',rf'SIM status ins=20 sw={status}')]
        ordered.append(('complete',r'6250_pin_retry: wrong_only_complete=1 firmware_state_forcing=0'))
        if 'LAPDm Location Updating Accept acknowledged' in text:
            raise ValueError('exhausted PIN must not register')
    elif name in ('puk','wrong-puk'):
        wrong=name=='wrong-puk'
        if wrong:
            if not 1<=puk_attempts<=10:
                raise ValueError('wrong PUK requires one of the ten live allowances')
            blocked[3518]=puk_attempts
        else:
            if puk_attempts not in (9,10):
                raise ValueError('unsupported qualified pre-recovery PUK allowance')
            blocked[3518]=puk_attempts
        rejected=bytearray(blocked)
        rejected[3518]=puk_attempts-1
        if initial!=bytes(blocked) or final!=(bytes(rejected) if wrong else bytes(rotated)):
            raise ValueError('PUK recovery did not preserve unrelated fields or install the new PIN')
        expected=[(0x9840 if puk_attempts==1 else 0x9804,0,puk_attempts-1)] if wrong else [(0x9000,3,10)]
        if statuses(text,0x20) or statuses(text,0x2c)!=expected:
            raise ValueError('PUK phase requires its exact UNBLOCK result/allowance and no VERIFY')
        items=[(8+i,'Keypad '+str(9 if wrong else i+1)) for i in range(8)]+[(16,'Left Softkey / Menu')]
        for group in (0,1):
            items += [(20+group*8+i,'Keypad '+str(digit)) for i,digit in enumerate((2,4,6,8))]
            items.append((24+group*8,'Left Softkey / Menu'))
        if not wrong:items.append((52,'Left Softkey / Menu'))
        contacts(text,'6250_puk_input',items)
        selectors=[]
        if re.findall(r'header cla=a0 ins=2c p1=00 p2=(\w\w) p3=10',text)!=['00']:
            raise ValueError('PUK recovery must address UNBLOCK CHV1 exactly once')
        ordered=[('confirm',r'6250_puk_input: key=Left Softkey / Menu state=1 t=32\.'),
                 ('result',rf'SIM status ins=2c sw={("9840" if puk_attempts==1 else "9804") if wrong else "9000"}')]
        if wrong:
            if 'LAPDm Location Updating Accept acknowledged' in text:
                raise ValueError('wrong PUK must not register')
            ordered.append(('complete',r'6250_puk_input: wrong_complete=1 firmware_state_forcing=0'))
        else:
            ordered += [('registered',r'LAPDm Location Updating Accept acknowledged'),
                        ('Menu',r'6250_puk_input: key=Left Softkey / Menu state=1 t=52\.'),
                        ('complete',r'6250_puk_input: complete=1 firmware_state_forcing=0')]
    elif name=='permanent':
        blocked[3518]=0
        if initial!=bytes(blocked) or final!=bytes(blocked):
            raise ValueError('permanently rejected cold card must remain completely unchanged')
        if statuses(text,0x20) or statuses(text,0x2c):
            raise ValueError('permanent rejection must not attempt more credentials')
        if any(token in text for token in ('LAPDm Location Updating Accept acknowledged',
                                          'GSM outgoing request','gsm_mm: cm_service_request')):
            raise ValueError('permanent credential input must not start registration or a call')
        items=[(10+i,'Keypad 9') for i in range(8)]+[(18,'Left Softkey / Menu')]
        for group in (0,1):
            items += [(22+group*8+i,'Keypad '+str(digit)) for i,digit in enumerate((2,4,6,8))]
            items.append((26+group*8,'Left Softkey / Menu'))
        items.append((38,'End'))
        contacts(text,'6250_puk_input',items)
        selectors=[]
        ordered=[('first physical input',r'6250_puk_input: key=Keypad 9 state=1 t=10\.'),
                 ('End',r'6250_puk_input: key=End state=1 t=38\.'),
                 ('complete',r'6250_puk_input: permanent_complete=1 firmware_state_forcing=0')]
    elif name=='reauth':
        if initial!=bytes(rotated) or final!=bytes(rotated):
            raise ValueError('cold reauthentication did not retain the entire rotated card')
        if statuses(text,0x20)!=[(0x9000,3,10)] or statuses(text,0x2c):
            raise ValueError('cold new-PIN verification failed')
        items=[(8+i,'Keypad '+str(digit)) for i,digit in enumerate((2,4,6,8))]
        items += [(12,'Left Softkey / Menu'),(34,'Left Softkey / Menu')]
        contacts(text,'6250_pin_retry',items)
        selectors=['01']
        ordered=[('submit',r'6250_pin_retry: key=Left Softkey / Menu state=1 t=12\.'),
                 ('verified',r'SIM status ins=20 sw=9000'),
                 ('registered',r'LAPDm Location Updating Accept acknowledged'),
                 ('Menu',r'6250_pin_retry: key=Left Softkey / Menu state=1 t=34\.'),
                 ('complete',r'6250_pin_retry: complete=1 firmware_state_forcing=0')]
    else:
        raise ValueError('unknown PUK lifecycle phase')
    if re.findall(r'header cla=a0 ins=20 p1=00 p2=(\w\w) p3=08',text)!=selectors:
        raise ValueError('PUK lifecycle VERIFY selectors/count differ')
    forbidden=(4,0x24,0x26,0x28,0x44,0xdc)+(() if name in ('puk','wrong-puk') else (0x2c,))
    if any(statuses(text,ins) for ins in forbidden):
        raise ValueError('PUK lifecycle changed unrelated card configuration or records')
    require_ordered(text,ordered,'6250 PUK lifecycle')


PUK_FRAMES={
    'prompt':'87e8208502b6de9a3f3ff16396408599abb8c863f6fa0d92678ec117fc567f3c',
    'new_pin':'2175c020da9e667de3adf54a2fad22991e9cb713f10459a71e40d7dcfbdfce7d',
    'confirm':'56857396bfdf762074a04bc14a8d1d23654b87460003c51a6c0548a18367293e',
    'result':'b917509b5f70a8add9573f2b78a4afb775e3d248d02af2f642afb8f189d0bd4e',
    'idle':FRAMES['idle'],'menu':FRAMES['menu'],
}


def check_puk_frames(phase,name,*,after_wrong=False,remaining=9):
    if name=='blocked':
        for index in (1,2,3):
            check_rgb_frame(phase/f'snap/6250_pin_exhaust_{index}.png',
                            FRAMES['error'] if index<3 else PUK_FRAMES['prompt'])
    elif name=='puk':
        for frame,digest in PUK_FRAMES.items():
            if after_wrong and frame=='result':
                digest='b358d01988adefc3dfc51a5352b485d1802d16f9e5f600e047c723a43cf514a6'
            check_rgb_frame(phase/f'snap/6250_puk_{frame}.png',digest)
    elif name=='wrong-puk':
        for frame in ('prompt','new_pin','confirm','result'):
            digest=(('caf22794fcff62acbed78a4c3ca92c745b64565b225ba3c6ea1c3390919b0be1' if remaining==0 else
                     '2f5d94d74625776d287cd5d7d452943865fee0755ba56b93920a94b85408d029')
                    if frame=='result' else PUK_FRAMES[frame])
            check_rgb_frame(phase/f'snap/6250_puk_{frame}.png',digest)
    elif name=='permanent':
        # These fixture-stage filenames are not PUK-editor ownership claims.
        frames={'prompt':PUK_FRAMES['prompt'],
                'new_pin':'75f87a3ff51cf07847b36fd97efb2f2ed8bf128147d96dd384b0ed38a97cc432',
                'confirm':'a64e3f1a7c4f7206f56c8c668706edd6eccec4a46593ec2433b8fc8f58fb202e',
                'result':'02d7b33102fdfc682bb0b863290d377ca584d294409d5e3d003abf7d8025a9a5',
                'permanent_after':'2f5d94d74625776d287cd5d7d452943865fee0755ba56b93920a94b85408d029'}
        for frame,digest in frames.items():
            check_rgb_frame(phase/f'snap/6250_puk_{frame}.png',digest)
    else:
        for frame in ('idle','menu'):
            check_rgb_frame(phase/f'snap/6250_pin_retry_{frame}.png',FRAMES[frame])


def validate_retained_launch(recorded,expected,storage,seen,*,
                             fields=('phase','command','mame_sha256','script_sha256')):
    if not isinstance(recorded,dict):
        raise ValueError('retained launch provenance must be an object')
    identity=recorded.get('invocation_id')
    try:
        uuid.UUID(identity)
    except (ValueError,TypeError,AttributeError) as error:
        raise ValueError('retained phase has no valid independent launch identity') from error
    if identity in seen:
        raise ValueError('retained phases reuse a launch identity')
    for key in fields:
        if recorded.get(key)!=expected[key]:
            raise ValueError('retained launch provenance differs: '+key)
    if recorded.get('initial_storage')!=storage:
        raise ValueError('retained launch did not inherit complete predecessor storage')


def read_evidence(path):
    # Keep raw MMIO in the artifact, not in every protocol regex traversal.
    # No acceptance checker consumes mad2_ledger records; preserve errors even
    # if their line also happens to contain that diagnostic prefix.
    with path.open(errors='replace') as stream:
        return ''.join(line for line in stream if 'mad2_ledger:' not in line or
                       any(token in line for token in ('ERROR','fatal','unimplemented','exception')))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run',type=Path)
    parser.add_argument('--mame',type=Path)
    parser.add_argument('--puk-recovery',action='store_true',help='physically exhaust PIN1, recover with PUK and cold-authenticate the new PIN')
    parser.add_argument('--wrong-puk-first',action='store_true',help='reject one PUK in an independent cold process before recovery')
    parser.add_argument('--exhaust-puk',action='store_true',help='physically consume every PUK1 allowance and test cold permanent rejection')
    parser.add_argument('--resume',action='store_true',help='revalidate existing phase evidence and launch only missing phases; never replay completed credential input')
    args=parser.parse_args()
    if args.wrong_puk_first and not args.puk_recovery:
        parser.error('--wrong-puk-first requires --puk-recovery')
    if args.exhaust_puk and (not args.puk_recovery or args.wrong_puk_first):
        parser.error('--exhaust-puk requires --puk-recovery without --wrong-puk-first')
    root=Path(__file__).resolve().parents[1]
    binary=(args.mame or root/'mame/mame').resolve()
    try:
        run=args.run.resolve()
        run.mkdir(parents=True,exist_ok=args.resume)
        previous=None
        launches=[]
        phases=(
                (False,'wrong','noki6250_pin_wrong.lua',24),
                (True,'recovery','noki6250_pin_cold_recovery.lua',40))
        if args.puk_recovery:
            phases=((False,'blocked','noki6250_pin_exhaust.lua',36),
                    (True,'puk','noki6250_puk_recovery.lua',58),
                    (True,'reauth','noki6250_pin_rotated.lua',40))
            if args.wrong_puk_first:
                phases=(phases[0],(False,'wrong-puk','noki6250_puk_wrong.lua',40),*phases[1:])
            if args.exhaust_puk:
                phases=(phases[0],*((False,f'wrong-puk-{attempt:02d}','noki6250_puk_wrong.lua',40)
                                   for attempt in range(1,11)),
                        (False,'permanent','noki6250_puk_permanent.lua',46))
        for recovery,name,script,seconds in phases:
            phase=run/name
            retained=phase.exists()
            if retained and not args.resume:
                raise ValueError('existing cold phase requires explicit --resume')
            if not retained:
                prepare_run(phase,root)
                apply_coherent_config(phase/'cfg/nhm3hle.cfg',root/'fixtures/noki6250_host_gsm900/nhm3hle.cfg')
            card=phase/'nvram/nhm3hle/sim_card'
            if previous:
                before=hashes(previous)
                if not retained:
                    shutil.copytree(previous/'nvram',phase/'nvram',dirs_exist_ok=True)
                    if hashes(phase/'nvram')!=hashes(previous/'nvram'):
                        raise ValueError('cold input storage differs from predecessor')
            elif not retained:
                card.parent.mkdir(parents=True)
                card.write_bytes(make_profile(True))
            initial=(phase/'initial_sim_card.bin').read_bytes() if retained else card.read_bytes()
            if not retained:(phase/'initial_sim_card.bin').write_bytes(initial)
            command=[str(binary),'nhm3hle','-rompath',f'{phase / "roms"};{root / "roms"}',
                     '-nvram_directory','nvram','-cfg_directory','cfg','-noreadconfig',
                     '-autoboot_script',str(root/'tools'/script),'-autoboot_delay','0',
                     '-seconds_to_run',str(seconds),'-video','none','-sound','none',
                     '-nothrottle','-log','-verbose']
            launch={'phase':name,'invocation_id':str(uuid.uuid4()),'command':command,
                    'initial_storage':hashes(phase/'nvram'),
                    'mame_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
                    'script_sha256':hashlib.sha256((root/'tools'/script).read_bytes()).hexdigest()}
            if retained:
                recorded=json.loads((phase/'launch.json').read_text())
                expected_storage=(hashes(previous/'nvram') if previous else
                                  {'nhm3hle/sim_card':hashlib.sha256(make_profile(True)).hexdigest()})
                validate_retained_launch(recorded,launch,expected_storage,
                                         {item['invocation_id'] for item in launches})
                if recorded['initial_storage'].get('nhm3hle/sim_card')!=hashlib.sha256(initial).hexdigest():
                    raise ValueError('retained initial card differs from launch provenance')
                launch=recorded
            else:
                (phase/'launch.json').write_text(json.dumps(launch,indent=2)+'\n')
                environment=os.environ.copy()
                environment.pop('NOKIA_DCT3_6250_PIN_ENTRY',None)
                with (phase/'console.log').open('w') as output:
                    subprocess.run(command,cwd=phase,stdout=output,stderr=subprocess.STDOUT,
                                   check=True,timeout=180,env=environment)
            verify_handset_health((phase/'console.log').read_text(errors='replace'))
            text=read_evidence(phase/'error.log')
            uploads(text,runtime=True)
            check_initial_fixture(text)
            if args.puk_recovery:
                kind='wrong-puk' if name.startswith('wrong-puk-') else name
                allowance=(11-int(name.rsplit('-',1)[1]) if name.startswith('wrong-puk-') else
                           9 if name=='puk' and args.wrong_puk_first else 10)
                check_puk_phase(text,initial,card.read_bytes(),kind,puk_attempts=allowance)
                check_puk_frames(phase,kind,after_wrong=args.wrong_puk_first,remaining=allowance-1)
            else:
                check_phase(text,initial,card.read_bytes(),recovery=recovery)
                for frame in (('idle','menu') if recovery else ('error',)):
                    check_rgb_frame(phase/f'snap/6250_pin_retry_{frame}.png',FRAMES[frame])
            if recovery:
                registration(text,card.read_bytes(),preserved=name=='reauth',require_host=False)
            if previous and hashes(previous)!=before:
                raise ValueError('cold run mutated predecessor artifacts')
            launches.append(launch)
            previous=phase
            print(f'6250 cold phase {name}: '+('retained evidence verified' if retained else 'fresh process verified'),flush=True)
        (run/'acceptance.json').write_text(json.dumps({'phases':launches,
            'lifecycle':('permanent-puk-loss' if args.exhaust_puk else
                         'wrong-puk-recovery' if args.wrong_puk_first else
                         'puk-recovery' if args.puk_recovery else 'pin-retry'),
            'resumed':args.resume,'research_hle':True,'native_speech':False},indent=2)+'\n')
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as error:
        parser.exit(1,f'6250 cold PIN retry FAIL: {error}\n')
    print(f'6250 '+('PUK1 exhaustion and cold permanent rejection' if args.exhaust_puk else
                     'PIN exhaustion/PUK recovery/new-PIN cold authentication' if args.puk_recovery else
                     'durable wrong-PIN retry and cold physical recovery')+f' PASS: {run}')


if __name__=='__main__':main()
