"""Fresh isolated NPE-3 own-upload and research-HLE graphical acceptance."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import shutil
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki6210_upload_contract import assess
from tools.noki6210_staged_check import verify
from tools.noki6210_radio_contract import verify as verify_radio_contract

SCENARIOS = {'stage': ('npe3stage', 'staged_observe', 12),
             'runtime': ('npe3hle', 'staged_observe', 18),
             'menu': ('npe3hle', 'menu_input', 25),
             'calculator': ('npe3hle', 'application_input', 38),
             'phonebook': ('npe3hle', 'phonebook_input', 33),
             'registration': ('npe3hle', 'menu_input', 25),
             'outgoing-call': ('npe3hle', 'outgoing_call_input', 48),
             'incoming-call': ('npe3hle', 'incoming_call_input', 42),
             'incoming-sms': ('npe3hle', 'incoming_sms_input', 30),
             'outgoing-sms': ('npe3hle', 'outgoing_sms_input', 43),
             'security': ('npe3hle', 'security_input', 37)}
MENU_SHA256 = '8c7650fdb0514ec34c85b89795e529de062e6f141268a507bafc7eb77370df65'
CALCULATOR_SHA256 = '2c5e99fd98ab56d41574c613021a7ed5270fe7d39e94ec57a1f52b9f732199fc'
CONTACT_SHA256 = '39ca7b13f4afdc8c6e3ca553d7fd0bafcdd7dd3de42c054edf0f445713dd09bc'
OPERATOR_SHA256 = '1138954cc94944c83019823ea500fa9ea9f8857c76e3d8ba929cdc40db4c0b74'
SMS_READ_SHA256 = 'ab21e640456a297698ff12e89d315fb469eca215975b8ba4cc5a1a9cb2a41be3'
SMS_SENT_SHA256 = '67f74edfd9817c67b2301a1118c32a5764da7ed54e5b1ec09caf9eb332abc7c8'
SECURITY_MENU_SHA256 = 'dca943c465ed8b7cc2c766e9ac0f6f69ce86228c04aa68cd52d1b20a75a8bf3f'


def check_registration(text, storage):
    import re
    patterns = (
        r'TX packet type=56 payload=160 .*data=0023',
        r'TX packet type=02 .*radio_phase=candidate_channel_change data=040000000000005050000023',
        r'TX packet type=0c .*radio_phase=random_access',
        r'RX enqueue type=89 payload=8 .*data=0100000000000000',
        r'TX packet type=1b .*data=0080013f4905087000f000fffe33080910101032547698',
        r'LAPDm Location Updating Accept acknowledged nr=1',
        r'LAPDm Channel Release acknowledged nr=2',
        r'update-binary fid=6f7e offset=4 length=5',
        r'update-binary fid=6f7e offset=10 length=1',
        r'TX packet type=02 .*radio_phase=release_channel_change data=040000000000001a600000230000000f',
    )
    cursor = 0
    for pattern in patterns:
        match = re.search(pattern, text[cursor:])
        if not match:
            raise ValueError(f'missing ordered NPE-3 registration evidence: {pattern}')
        cursor += match.end()
    if len(storage) < 1611 or storage[1604:1609] != bytes.fromhex('00f1100001') or storage[1610] != 0:
        raise ValueError('persisted EF_LOCI is not laboratory location-updated')


def events(path):
    with path.open(errors='replace') as stream:
        return ''.join(line for line in stream if any(token in line for token in
                      ('staged_dsp:', '6210_', 'dspif_transport:', 'sim_device:',
                       'SIM status', 'radio peer', 'dsp_hle:', 'gsm_sms_submit:', '[LUA ERROR]')))


def check_frame(frame, digest, description):
    if frame.size != (96, 60) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != digest:
        raise ValueError(f'frame differs from reviewed {description}')


def check_output(text):
    for failure in ('Disk quota exceeded', 'No space left on device',
                    'Error writing NVRAM file', 'Error generating PNG'):
        if failure in text:
            raise ValueError(f'MAME could not persist acceptance artifacts: {failure}')


def check_calculator(text, frame):
    cursor = 0
    for action in ('application', 'input_1', 'input_12', 'operation_options',
                   'subtract', 'minus', 'input_3', 'options', 'result'):
        event = f'6210_application_physical: action={action}'
        cursor = text.find(event, cursor)
        if cursor < 0:
            raise ValueError(f'missing ordered Calculator input: {action}')
        cursor += len(event)
    check_frame(frame, CALCULATOR_SHA256, 'Calculator 12 - 3 = 9')


def check_phonebook(write_trace, read_trace, storage, frame):
    from tools.sim_phonebook_check import validate_phonebook_storage
    cursor = 0
    for event in ('6210_phonebook_physical: action=save',
                  'header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3a',
                  'body ins=dc length=32 selected=6f3a',
                  'SIM status ins=dc sw=9000'):
        cursor = write_trace.find(event, cursor)
        if cursor < 0:
            raise ValueError(f'missing ordered SIM save: {event}')
        cursor += len(event)
    if 'header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3a' not in read_trace:
        raise ValueError('cold process did not read EF_ADN record 1')
    if '6210_phonebook_read_physical: action=contact' not in read_trace or 'ins=dc' in read_trace:
        raise ValueError('cold read must physically select contact without writing SIM')
    validate_phonebook_storage(storage, b'A')
    check_frame(frame, CONTACT_SHA256, 'cold A / 123 contact')


def check_menu(text, frame):
    import re
    from tools.radio_call_lifecycle_common import require_ordered
    records = [int(value, 16) for value in re.findall(
        r'sim_device: header cla=a0 ins=b2 p1=([0-9a-f]{2}) p2=04 p3=20 selected=6f3a', text)]
    if records != list(range(1, 51)):
        raise ValueError('expected complete own SIM initialization/50 ADN reads')
    require_ordered(text, (
        ('physical Menu', re.compile('6210_menu_physical: press=1')),
        ('own decoder', re.compile('6210_keypad_decoded: key=19')),
    ), '6210 physical input')
    if frame.size != (96, 60) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != MENU_SHA256:
        raise ValueError('Menu frame differs from reviewed Messages screen')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--scenario', choices=SCENARIOS, default='menu')
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    try:
        contract = assess((root / 'roms/noki6210/6210_556c.fls').read_bytes(),
                          (root / 'roms/noki6210/6210 virgin eeprom 005fa000.fls').read_bytes())
        contract['radio_receive'] = verify_radio_contract((root / 'roms/noki6210/6210_556c.fls').read_bytes())
        run = args.run_directory.resolve()
        run.mkdir(parents=True, exist_ok=False)
        machine, script, seconds = SCENARIOS[args.scenario]
        if args.scenario == 'security':
            from tools.make_sim_card_profile import make_profile
            card = run / f'nvram/{machine}/sim_card'
            card.parent.mkdir(parents=True)
            card.write_bytes(make_profile(pin_enabled=True))
        if args.scenario in ('incoming-call', 'incoming-sms'):
            (run / 'cfg').mkdir()
            config = ET.Element('mameconfig', version='10')
            system = ET.SubElement(config, 'system', name=machine)
            ports = ET.SubElement(system, 'input')
            mask = '2' if args.scenario == 'incoming-call' else '4'
            ET.SubElement(ports, 'port', tag=':NETCFG', type='CONFIG',
                          mask=mask, defvalue='0', value=mask)
            ET.ElementTree(config).write(run / f'cfg/{machine}.cfg', encoding='utf-8', xml_declaration=True)
        command = [str((args.mame or root / 'mame/mame').resolve()), machine,
                   '-rompath', str(root / 'roms'), '-nvram_directory', 'nvram',
                   '-cfg_directory', 'cfg', '-noreadconfig', '-debug', '-debugger', 'none',
                   '-autoboot_script', str(root / f'tools/noki6210_{script}.lua'),
                   '-autoboot_delay', '0', '-seconds_to_run', str(seconds),
                   '-video', 'none', '-sound', 'none', '-nothrottle', '-log', '-verbose']
        with (run / 'console.log').open('w') as output:
            subprocess.run(command, cwd=run, stdout=output, stderr=subprocess.STDOUT,
                           check=True, timeout=180)
        check_output((run / 'console.log').read_text(errors='replace'))
        text = events(run / 'error.log')
        runtime = args.scenario != 'stage'
        verify(text, runtime=runtime, selftest=runtime)
        if args.scenario == 'menu':
            from PIL import Image
            with Image.open(run / 'snap/6210_after_menu.png') as frame:
                check_menu(text, frame)
        elif args.scenario == 'calculator':
            from PIL import Image
            with Image.open(run / 'snap/6210_calculator_result.png') as frame:
                check_calculator(text, frame)
        elif args.scenario == 'phonebook':
            from PIL import Image
            cold = run / 'cold'
            cold.mkdir()
            shutil.copytree(run / 'nvram', cold / 'nvram')
            cold_command = command.copy()
            cold_command[cold_command.index('-autoboot_script') + 1] = str(root / 'tools/noki6210_phonebook_read.lua')
            cold_command[cold_command.index('-seconds_to_run') + 1] = '27'
            with (cold / 'console.log').open('w') as output:
                subprocess.run(cold_command, cwd=cold, stdout=output, stderr=subprocess.STDOUT,
                               check=True, timeout=180)
            check_output((cold / 'console.log').read_text(errors='replace'))
            read_trace = events(cold / 'error.log')
            verify(read_trace, runtime=True, selftest=True)
            with Image.open(cold / 'snap/6210_phonebook_read_contact.png') as frame:
                check_phonebook(text, read_trace,
                               (cold / 'nvram/npe3hle/sim_card').read_bytes(), frame)
        elif args.scenario == 'registration':
            check_registration(text, (run / 'nvram/npe3hle/sim_card').read_bytes())
            from PIL import Image
            with Image.open(run / 'snap/6210_before_menu.png') as frame:
                check_frame(frame, OPERATOR_SHA256, 'DCT3 LAB registered idle')
        elif args.scenario == 'outgoing-call':
            from tools.noki6210_outgoing_call_check import verify as check_call
            check_call(text)
        elif args.scenario == 'incoming-call':
            from tools.noki6210_incoming_call_check import verify as check_call
            check_call(text)
        elif args.scenario == 'outgoing-sms':
            from tools.noki6210_outgoing_sms_check import verify as check_submission
            check_submission(text)
            from PIL import Image
            with Image.open(run / 'snap/6210_sms_sent.png') as frame:
                check_frame(frame, SMS_SENT_SHA256, 'Message sent')
        elif args.scenario == 'incoming-sms':
            from tools.noki6210_incoming_sms_check import verify as check_delivery
            check_delivery(text, (run / 'nvram/npe3hle/sim_card').read_bytes())
            from PIL import Image
            with Image.open(run / 'snap/6210_sms_read_1.png') as frame:
                check_frame(frame, SMS_READ_SHA256, 'received hello SMS')
        elif args.scenario == 'security':
            import re
            from tools.radio_call_lifecycle_common import require_ordered
            require_ordered(text, (
                ('physical PIN', re.compile(r'6210_security_physical: action=confirm')),
                ('VERIFY CHV1', re.compile(r'sim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08')),
                ('accepted PIN', re.compile(r'SIM status ins=20 sw=9000')),
                ('physical Menu', re.compile(r'6210_security_physical: action=menu')),
            ), '6210 security')
            from PIL import Image
            with Image.open(run / 'snap/6210_security_then_menu.png') as frame:
                check_frame(frame, SECURITY_MENU_SHA256, 'Messages after PIN verification')
        (run / 'acceptance.json').write_text(json.dumps({
            'machine': machine, 'scenario': args.scenario, 'passed': True,
            'provisioning': 'unchanged acquired product PMM', 'contract': contract,
            'sim_profile': 'PIN-enabled laboratory card' if args.scenario == 'security' else 'default laboratory card',
            'native_dsp_complete': False, 'speech_tested': False, 'command': command,
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'6210 acceptance FAIL: {error}\n')
    print(f'6210 {args.scenario} acceptance PASS: {run}')


if __name__ == '__main__':
    main()
