"""Fresh isolated NPE-3 own-upload and research-HLE graphical acceptance."""
import argparse
import os
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import shutil
import uuid
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki6210_upload_contract import assess
from tools.noki6210_staged_check import verify
from tools.noki6210_radio_contract import verify as verify_radio_contract

SCENARIOS = {'stage': ('npe3stage', 'staged_observe', 12),
             'power-cycle': ('npe3hle', 'power_cycle_input', 115),
             'runtime': ('npe3hle', 'staged_observe', 18),
             'menu': ('npe3hle', 'menu_input', 25),
             'calculator': ('npe3hle', 'application_input', 38),
             'phonebook': ('npe3hle', 'phonebook_input', 33),
             'registration': ('npe3hle', 'menu_input', 25),
             'foreign-registration': ('npe3hle', 'menu_input', 25),
             'plmn-reject': ('npe3hle', 'staged_observe', 90),
             'home-plmn-reject': ('npe3hle', 'staged_observe', 30),
             'outgoing-call': ('npe3hle', 'outgoing_call_input', 48),
             'incoming-call': ('npe3hle', 'incoming_call_input', 42),
             'incoming-pcm-missing': ('npe3hle', 'incoming_call_input', 42),
             'incoming-sms': ('npe3hle', 'incoming_sms_input', 30),
             'outgoing-sms': ('npe3hle', 'outgoing_sms_input', 43),
             'security': ('npe3hle', 'security_input', 37),
             'security-retry': ('npe3hle', 'security_retry_input', 45),
             'security-retry-cold': ('npe3hle', 'security_retry_cold_input', 18),
             'security-block-cold': ('npe3hle', 'security_block_input', 33),
             'security-block': ('npe3hle', 'security_block_input', 33),
             'pin2-change': ('npe3hle', 'pin2_change_input', 84),
             'pin2-change-cold': ('npe3hle', 'pin2_change_input', 84),
             'pin2-retry-cold': ('npe3hle', 'pin2_wrong_input', 84),
             'pin2-reject': ('npe3hle', 'pin2_wrong_input', 84),
             'fdn-observe': ('npe3hle', 'fdn_input', 58),
             'fdn-enable': ('npe3hle', 'fdn_enable', 66),
             'fdn-cycle': ('npe3hle', 'fdn_enable', 66),
             'fdn-retry-cold': ('npe3hle', 'fdn_enable_wrong', 66),
             'host-incoming-ussd': ('npe3hle', 'host_ussd_notify', 38),
             'host-incoming-ussd-request': ('npe3hle', 'host_ussd_request', 44),
             'host-incoming-ussd-restore': ('npe3hle', 'host_ussd_restore', 44),
             'toolkit': ('npe3hle', 'toolkit_input', 40),
             'toolkit-interactive': ('npe3hle', 'toolkit_interactive', 65),
             'toolkit-menu': ('npe3hle', 'toolkit_menu', 75),
             'toolkit-sms': ('npe3hle', 'toolkit_sms', 100),
             'toolkit-call': ('npe3hle', 'toolkit_call', 105),
             'toolkit-call-decline': ('npe3hle', 'toolkit_call_decline', 105),
             'ussd': ('npe3hle', 'ussd_input', 40),
             'ussd-continued': ('npe3hle', 'ussd_continued', 54),
             'divert': ('npe3hle', 'divert_input', 35),
             'divert-lifecycle': ('npe3hle', 'divert_lifecycle_input', 80),
             'state-divert': ('npe3hle', 'state_divert', 80),
             'state-idle': ('npe3hle', 'state_idle', 24),
             'state-call': ('npe3hle', 'state_call', 48),
             'state-sms': ('npe3hle', 'state_sms', 30),
             'accessory': ('npe3hle', 'accessory_input', 25),
             'host-incoming-call': ('npe3hle', 'host_incoming_input', 60),
             'host-incoming-sms': ('npe3hle', 'incoming_sms_input', 30),
             'host-outgoing-sms': ('npe3hle', 'outgoing_sms_input', 43),
             'host-sms-status': ('npe3hle', 'outgoing_sms_input', 43),
             'host-rejected-sms': ('npe3hle', 'sms_reject_input', 50),
             'host-silent-sms': ('npe3hle', 'sms_silence_input', 135),
             'host-outgoing-call': ('npe3hle', 'outgoing_call_input', 48)}
TOOLKIT_PROFILES = {'toolkit': 1, 'toolkit-interactive': 3, 'toolkit-menu': 4,
                    'toolkit-sms': 5, 'toolkit-call': 6, 'toolkit-call-decline': 6}
FDN_SCENARIOS=frozenset(('fdn-observe','fdn-enable','fdn-cycle','fdn-retry-cold'))
HOST_USSD_SCENARIOS=frozenset(('host-incoming-ussd','host-incoming-ussd-request','host-incoming-ussd-restore'))
PIN_SCENARIOS=frozenset(('ussd-continued','security','security-retry','security-retry-cold',
                         'security-block-cold','security-block','pin2-change','pin2-change-cold','pin2-retry-cold','pin2-reject')) | FDN_SCENARIOS | HOST_USSD_SCENARIOS
MENU_SHA256 = '8c7650fdb0514ec34c85b89795e529de062e6f141268a507bafc7eb77370df65'
CALCULATOR_SHA256 = '2c5e99fd98ab56d41574c613021a7ed5270fe7d39e94ec57a1f52b9f732199fc'
CONTACT_SHA256 = '39ca7b13f4afdc8c6e3ca553d7fd0bafcdd7dd3de42c054edf0f445713dd09bc'
OPERATOR_SHA256 = '9b3fe27be727ece0d99040211b125ac319ee93f6c7ce6a0599b9ebe27ab84d37'
FOREIGN_OPERATOR_SHA256 = '3a130f147f3fb7c3083c9508bf6a3a4f11115a6fa42b0806bfbb7b1d39c48ac3'
SMS_READ_SHA256 = 'ab21e640456a297698ff12e89d315fb469eca215975b8ba4cc5a1a9cb2a41be3'
SMS_SENT_SHA256 = '67f74edfd9817c67b2301a1118c32a5764da7ed54e5b1ec09caf9eb332abc7c8'
SECURITY_MENU_SHA256 = 'dca943c465ed8b7cc2c766e9ac0f6f69ce86228c04aa68cd52d1b20a75a8bf3f'
USSD_RESULT_SHA256 = '7282a48b0f972545caac4db1a02189d25775dd13e02b11857cba98cfd58b3112'
DIVERT_RESULT_SHA256 = '0218f879565b696f4768f55215c45166b725b1897d0d78715da54278c74244b4'


def build_command(root,mame,machine,script,seconds,throttle=False):
    return [str((mame or root/'mame/mame').resolve()),machine,
            '-rompath',str(root/'roms'),'-nvram_directory','nvram',
            '-cfg_directory','cfg','-noreadconfig','-debug','-debugger','none',
            '-autoboot_script',str(root/f'tools/noki6210_{script}.lua'),
            '-autoboot_delay','0','-seconds_to_run',str(seconds),
            '-video','none','-sound','none','-throttle' if throttle else '-nothrottle',
            '-log','-verbose']


def run_retained_security(source,target,command,script,seconds,environment):
    target.mkdir()
    source_hashes={str(p.relative_to(source/'nvram')):hashlib.sha256(p.read_bytes()).hexdigest()
                   for p in sorted((source/'nvram').rglob('*')) if p.is_file()}
    shutil.copytree(source/'nvram',target/'nvram')
    shutil.copytree(source/'cfg',target/'cfg')
    copied_hashes={str(p.relative_to(target/'nvram')):hashlib.sha256(p.read_bytes()).hexdigest()
                   for p in sorted((target/'nvram').rglob('*')) if p.is_file()}
    if copied_hashes!=source_hashes:
        raise ValueError('retained security storage copy differs from predecessor')
    initial=(target/'nvram/npe3hle/sim_card').read_bytes()
    if hashlib.sha256(initial).hexdigest()!=source_hashes['npe3hle/sim_card']:
        raise ValueError('retained security input differs from predecessor')
    cold_command=list(command)
    cold_command[cold_command.index('-autoboot_script')+1]=str(script)
    cold_command[cold_command.index('-seconds_to_run')+1]=str(seconds)
    launch={'invocation_id':str(uuid.uuid4()),'run_directory':str(target),'command':cold_command,
            'source_card_sha256':hashlib.sha256(initial).hexdigest(),
            'script_sha256':hashlib.sha256(script.read_bytes()).hexdigest(),'initial_storage':source_hashes}
    (target/'launch.json').write_text(json.dumps(launch,indent=2)+'\n')
    with (target/'console.log').open('w') as output:
        subprocess.run(cold_command,cwd=target,stdout=output,stderr=subprocess.STDOUT,
                       check=True,timeout=180,env=environment)
    check_output((target/'console.log').read_text(errors='replace'))
    text=events(target/'error.log')
    verify(text,runtime=True,selftest=True)
    if source_hashes!={str(p.relative_to(source/'nvram')):hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in sorted((source/'nvram').rglob('*')) if p.is_file()}:
        raise ValueError('retained security continuation changed predecessor storage')
    return text,initial,launch


def check_ussd(text, frames):
    from tools import radio_ussd_trace_check
    from tools.noki8210_supplementary_check import verify_transaction
    verify_transaction(text, frames, 'ussd',
                       ('Keypad *', 'Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad #', 'Send'),
                       radio_ussd_trace_check, USSD_RESULT_SHA256, OPERATOR_SHA256,
                       product='6210', geometry=(96, 60))


def check_divert(text, frames):
    from tools import radio_call_divert_trace_check
    from tools.noki8210_supplementary_check import verify_transaction
    verify_transaction(text, frames, 'divert',
                       ('Keypad *', 'Keypad #', 'Keypad 2', 'Keypad 1', 'Keypad #', 'Send'),
                       radio_call_divert_trace_check, DIVERT_RESULT_SHA256, OPERATOR_SHA256,
                       product='6210', geometry=(96, 60))


def check_registration(text, storage, *, preserved_location=False,
                       channel_header='0000', expected_lai=bytes.fromhex('00f1100001')):
    import re
    if len(expected_lai) != 5:
        raise ValueError('registration LAI must contain five octets')
    patterns = (
        r'TX packet type=56 payload=160 .*data=0023',
        rf'TX packet type=02 .*radio_phase=candidate_channel_change data=04{channel_header}000000005050000023',
        r'TX packet type=0c .*radio_phase=random_access',
        r'RX enqueue type=89 payload=8 .*data=0100000000000000',
        (rf'TX packet type=1b .*data=0080013f49050872{expected_lai.hex()}33080910101032547698'
         if preserved_location else
         r'TX packet type=1b .*data=0080013f4905087000f000fffe33080910101032547698'),
        r'LAPDm Location Updating Accept acknowledged nr=1',
        r'LAPDm Channel Release acknowledged nr=2',
        rf'TX packet type=02 .*radio_phase=release_channel_change data=04{channel_header}000000001a600000230000000f',
    )
    cursor = 0
    for pattern in patterns:
        match = re.search(pattern, text[cursor:])
        if not match:
            raise ValueError(f'missing ordered NPE-3 registration evidence: {pattern}')
        cursor += match.end()
    # SIM persistence is downstream of LU acceptance, but may complete
    # before or after radio release. Do not serialize independent consumers.
    cursor = text.index('LAPDm Location Updating Accept acknowledged nr=1')
    updates = ['update-binary fid=6f7e offset=4 length=5']
    if not preserved_location:
        updates.append('update-binary fid=6f7e offset=10 length=1')
    else:
        read = text.find('read-binary fid=6f7e offset=0 length=11')
        request = text.find(f'data=0080013f49050872{expected_lai.hex()}33080910101032547698')
        if read < 0 or request < 0 or read > request:
            raise ValueError('warm NPE-3 registration did not read retained EF_LOCI before requesting')
        if 'update-binary fid=6f7e offset=10 length=1' in text:
            raise ValueError('warm NPE-3 registration redundantly rewrote valid location status')
    for record in updates:
        position = text.find(record, cursor)
        if position < 0:
            raise ValueError('missing ordered NPE-3 SIM location persistence: ' + record)
        cursor = position + len(record)
    if len(storage) < 1611 or storage[1604:1609] != expected_lai or storage[1610] != 0:
        raise ValueError('persisted EF_LOCI is not laboratory location-updated')


def check_plmn_reject(text, storage, *, home=False):
    from tools.radio_call_lifecycle_common import require_ordered
    import re
    require_ordered(text, (
        ('LU reject', re.compile(r'RX enqueue type=80 .*data=.*05040b')),
        ('LU reject acknowledgement', re.compile(r'LAPDm Location Updating Reject acknowledged nr=1')),
        ('channel release', re.compile(r'LAPDm Channel Release acknowledged nr=2')),
    ), '6210 PLMN rejection')
    if 'LAPDm Location Updating Accept acknowledged' in text:
        raise ValueError('rejected PLMN was also accepted')
    if len(storage) not in (3562, 3570):
        raise ValueError('incomplete SIM storage after rejection')
    if storage[1610] != 3:
        raise ValueError('rejected location update did not persist roaming-not-allowed status')
    if home:
        if 'update-binary fid=6f7b' in text or storage[3548:3560] != b'\xff' * 12:
            raise ValueError('home PLMN incorrectly added to EF_FPLMN')
        return
    if text.find('update-binary fid=6f7b') < text.find('05040b'):
        raise ValueError('handset did not update EF_FPLMN after cause 11')
    if bytes.fromhex('00f120') not in (
            storage[offset:offset + 3] for offset in range(3548, 3560, 3)):
        raise ValueError('laboratory PLMN was not retained in EF_FPLMN')


def check_forbidden_selection(text, storage, *, forbidden, previous):
    import re
    if len(storage) not in (3562, 3570):
        raise ValueError('incomplete cold SIM storage')
    if 'read-binary fid=6f7b offset=0 length=12' not in text:
        raise ValueError('cold selection did not read EF_FPLMN')
    if not re.search(r'RX enqueue type=80 .*data=.*061b.*00f120', text):
        raise ValueError('cold selection did not receive visited test PLMN SI3')
    if forbidden:
        if re.search(r'TX packet type=0c |Location Updating Accept acknowledged', text):
            raise ValueError('automatic selection accessed the forbidden PLMN')
        if storage != previous:
            raise ValueError('forbidden-network cold boot changed retained SIM storage')
    else:
        from tools.radio_call_lifecycle_common import require_ordered
        require_ordered(text, (
            ('random access', re.compile(r'TX packet type=0c ')),
            ('accepted update', re.compile(r'LAPDm Location Updating Accept acknowledged nr=1')),
            ('release', re.compile(r'LAPDm Channel Release acknowledged nr=2')),
        ), '6210 cleared forbidden-list control')
        if storage[1604:1609] != bytes.fromhex('00f1200001') or storage[1610] != 0:
            raise ValueError('cleared forbidden-list control did not persist visited location')
        if storage[3548:3560] != b'\xff' * 12:
            raise ValueError('accepted cleared-list control unexpectedly forbade a PLMN')


def events(path):
    with path.open(errors='replace') as stream:
        return ''.join(line for line in stream if any(token in line for token in
                      ('staged_dsp:', '6210_', 'dspif_transport:', 'sim_device:',
                       'SIM status', 'SIM completion', 'radio peer', 'dsp_hle:', 'gsm_sms_submit:', 'gsm_ss:', 'gsm_call_adapter:',
                       'state_replay:', 'state_roundtrip:', '[LUA ERROR]')))


def check_accessory(text, frame):
    import re
    decisions = re.findall(r'6210_accessory_decision: state=(\w+) sample=(\w+)', text)
    if not decisions or any(state != '0f' or sample != '03ff' for state, sample in decisions):
        raise ValueError('unattached accessory input/state did not remain high/0f')
    check_frame(frame, OPERATOR_SHA256, 'registered unattached idle without Headset')


def check_frame(frame, digest, description):
    if frame.size != (96, 60) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != digest:
        raise ValueError(f'frame differs from reviewed {description}')


def check_host_sms_sent(frame):
    # Host response timing may change the animated envelope at the right.
    if frame.size != (96, 60) or hashlib.sha256(
            frame.convert('L').crop((0, 0, 72, 60)).tobytes()).hexdigest() != (
            '83ec6b273d52bc1b98af2a0ca31602acf43066fd21337536b7b72b4d65c53ae7'):
        raise ValueError('missing reviewed host Message sent text')


def check_output(text):
    for failure in ('Disk quota exceeded', 'No space left on device',
                    'Error writing NVRAM file', 'Error generating PNG', '[LUA ERROR]'):
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
    parser.add_argument('--port', type=int, default=16210)
    parser.add_argument('--throttle', action='store_true',
                        help='run at real-time speed to verify host wait budgets')
    parser.add_argument('--coherent-cell', action='store_true',
                        help='configure the laboratory network on ARFCNs 35/36')
    parser.add_argument('--foreign-cell', action='store_true',
                        help='use the laboratory 234-15 cell for outgoing-call or incoming-sms')
    parser.add_argument('--pin-enabled', action='store_true',
                        help='authenticate a PIN-enabled SIM before the host service')
    parser.add_argument('--wrong-puk-first',action='store_true',
                        help='reject a physical PUK in a separate process before cold recovery')
    parser.add_argument('--trace-dsp-control', action='store_true',
                        help='read-only compiler/mode observations during coherent outgoing call')
    parser.add_argument('--trace-sleep', action='store_true',
                        help='bounded read-only clock-stop/End-input observations')
    args = parser.parse_args()
    if args.trace_sleep and (args.scenario != 'outgoing-call' or args.trace_dsp_control):
        parser.error('--trace-sleep requires outgoing-call without --trace-dsp-control')
    if args.wrong_puk_first and args.scenario!='security-block-cold':
        parser.error('--wrong-puk-first requires security-block-cold')
    if args.scenario in PIN_SCENARIOS-{'security'} and not args.coherent_cell:
        parser.error('security-retry requires --coherent-cell')
    if args.foreign_cell:
        if args.scenario not in ('outgoing-call', 'incoming-sms'):
            parser.error('--foreign-cell requires outgoing-call or incoming-sms')
        args.coherent_cell = True
    if args.pin_enabled and (not args.coherent_cell or args.scenario not in
                             ('host-incoming-call', 'host-incoming-sms', 'host-outgoing-call',
                              'host-outgoing-sms', 'host-sms-status', 'host-rejected-sms', 'host-silent-sms', 'phonebook', 'state-call',
                              'state-idle', 'state-sms')):
        parser.error('--pin-enabled requires --coherent-cell and a supported service fixture')
    root = Path(__file__).resolve().parents[1]
    if args.trace_dsp_control and (args.scenario != 'outgoing-call' or
                                  not args.coherent_cell or args.foreign_cell):
        parser.error('--trace-dsp-control requires the home coherent outgoing-call fixture')
    try:
        contract = assess((root / 'roms/noki6210/6210_556c.fls').read_bytes(),
                          (root / 'roms/noki6210/6210 virgin eeprom 005fa000.fls').read_bytes())
        contract['radio_receive'] = verify_radio_contract((root / 'roms/noki6210/6210_556c.fls').read_bytes())
        if args.scenario == 'accessory':
            from tools.noki6210_accessory_contract import verify as verify_accessory_contract
            contract['accessory_decision'] = verify_accessory_contract(
                (root / 'roms/noki6210/6210_556c.fls').read_bytes())
        run = args.run_directory.resolve()
        run.mkdir(parents=True, exist_ok=False)
        machine, script, seconds = SCENARIOS[args.scenario]
        if args.trace_dsp_control:
            script = 'dsp_mode_observe'
        if args.trace_sleep:
            script = 'sleep_observe'
            machine = 'npe3mbus'
        if args.scenario in PIN_SCENARIOS or args.pin_enabled:
            from tools.make_sim_card_profile import make_profile
            card = run / f'nvram/{machine}/sim_card'
            card.parent.mkdir(parents=True)
            card.write_bytes(make_profile(pin_enabled=True))
            if args.scenario in FDN_SCENARIOS:
                from tools.sim_fdn_profile import extend
                card.write_bytes(extend(make_profile(True)+bytes.fromhex('00f110')+b'\xff'*43))
        if args.scenario in ('plmn-reject', 'foreign-registration') or args.foreign_cell:
            from tools.make_sim_card_profile import make_profile
            card = run / f'nvram/{machine}/sim_card'
            card.parent.mkdir(parents=True)
            # A visited preferred PLMN is external subscriber data, not an
            # injected selection event. Keep IMSI/home identity unchanged.
            plmn = '32f451' if args.scenario == 'foreign-registration' or args.foreign_cell else '00f120'
            card.write_bytes(make_profile(pin_enabled=False) + bytes.fromhex(plmn) +
                             b'\xff' * 21 + b'\xff' * 12)
        cold_retry = None
        host = args.scenario.startswith('host-')
        configured = args.scenario in ('incoming-call', 'incoming-pcm-missing', 'incoming-sms', 'state-sms') or args.scenario in TOOLKIT_PROFILES or host
        if configured or args.coherent_cell or args.scenario in ('plmn-reject', 'home-plmn-reject', 'foreign-registration'):
            (run / 'cfg').mkdir()
            config = ET.Element('mameconfig', version='10')
            system = ET.SubElement(config, 'system', name=machine)
            ports = ET.SubElement(system, 'input')
            if args.scenario in TOOLKIT_PROFILES:
                tag, mask, value = ':SATCFG', '15', str(TOOLKIT_PROFILES[args.scenario])
            else:
                tag = ':CALLHOST' if host else ':NETCFG'
                mask = '1' if host else '2' if args.scenario in ('incoming-call', 'incoming-pcm-missing') else '4'
                value = mask
            if configured:
                ET.SubElement(ports, 'port', tag=tag, type='CONFIG',
                              mask=mask, defvalue='0', value=value)
            if args.scenario == 'incoming-pcm-missing':
                ET.SubElement(ports, 'port', tag=':NETCFG', type='CONFIG',
                              mask='32', defvalue='0', value='32')
            if args.scenario == 'ussd-continued':
                ET.SubElement(ports,'port',tag=':USSDCFG',type='CONFIG',
                              mask='7',defvalue='0',value='4')
            if args.coherent_cell or args.scenario == 'foreign-registration':
                ET.SubElement(ports, 'port', tag=':NEIGHBORCFG', type='CONFIG',
                              mask='1024', defvalue='0', value='1024')
            if args.scenario in ('plmn-reject', 'home-plmn-reject'):
                ET.SubElement(ports, 'port', tag=':AUTHCFG', type='CONFIG',
                              mask='2', defvalue='0', value='2')
                if args.scenario == 'plmn-reject':
                    ET.SubElement(ports, 'port', tag=':AUTHCFG', type='CONFIG',
                                  mask='12', defvalue='0', value='4')
            if args.scenario == 'foreign-registration' or args.foreign_cell:
                ET.SubElement(ports, 'port', tag=':AUTHCFG', type='CONFIG',
                              mask='12', defvalue='0', value='8')
            ET.ElementTree(config).write(run / f'cfg/{machine}.cfg', encoding='utf-8', xml_declaration=True)
        command = build_command(root,args.mame,machine,script,seconds,args.throttle)
        host_command = None
        if host:
            command.extend(['-http', '-http_port', str(args.port)])
            runner, options = {
                'host-incoming-call': ('run_host_incoming_signaling_gate', [
                    '--caller', '5551234', '--ready-file', str(run / 'snap/6210_host_registered_idle.png')]),
                'host-incoming-sms': ('run_host_incoming_sms_gate', []),
                'host-outgoing-sms': ('run_host_sms_gate', ['--user-data', '41', '--user-data-length', '1']),
                'host-sms-status': ('run_host_sms_gate', ['--user-data', '41', '--user-data-length', '1']),
                'host-rejected-sms': ('run_host_sms_gate', ['--user-data', '41', '--user-data-length', '1', '--decision', 'rp_error']),
                'host-silent-sms': ('run_host_sms_gate', ['--user-data', '41', '--user-data-length', '1', '--decision', 'rp_silence']),
                'host-outgoing-call': ('run_host_call_adapter_gate', ['--number', '1234567', '--decision', 'connect']),
                'host-incoming-ussd': ('run_host_incoming_ussd_gate', [
                    '--text','Host notice','--ready-marker','6210_host_ussd: ready']),
                'host-incoming-ussd-request': ('run_host_incoming_ussd_gate', [
                    '--text','Choose 2','--request','--expected-reply','2',
                    '--ready-marker','6210_host_ussd: ready']),
                'host-incoming-ussd-restore': ('run_host_incoming_ussd_gate', [
                    '--text','Choose 2','--request','--expected-reply','2','--require-restore',
                    '--ready-marker','6210_host_ussd: ready']),
            }[args.scenario]
            host_command = [sys.executable, str(root / f'tools/{runner}.py'),
                            '--port', str(args.port), '--cwd', str(run)] + options + ['--'] + command
        # Bind accepted artifacts to this launch, not merely to equal final bytes.
        launch = {
            'run_directory': str(run), 'invocation_id': str(uuid.uuid4()),
            'input_sha256': {name: hashlib.sha256(path.read_bytes()).hexdigest()
                for name, path in {
                    'mame': Path(command[0]), 'mcu': root/'roms/noki6210/6210_556c.fls',
                    'pmm': root/'roms/noki6210/6210 virgin eeprom 005fa000.fls',
                    'script': root/f'tools/noki6210_{script}.lua'}.items()},
            'initial_storage': {str(path.relative_to(run/'nvram')):
                hashlib.sha256(path.read_bytes()).hexdigest()
                for path in sorted((run/'nvram').rglob('*')) if path.is_file()},
        }
        with (run / 'console.log').open('w') as output:
            environment = os.environ.copy()
            environment.pop('NOKIA_DCT3_6210_PIN_ENTRY', None)
            if args.pin_enabled:
                environment['NOKIA_DCT3_6210_PIN_ENTRY'] = '1'
            subprocess.run(host_command or command, cwd=run, stdout=output, stderr=subprocess.STDOUT,
                           check=True, timeout=180, env=environment)
        check_output((run / 'console.log').read_text(errors='replace'))
        text = ((run / 'error.log').read_text(errors='replace')
                if args.scenario == 'power-cycle' else events(run / 'error.log'))
        runtime = args.scenario != 'stage'
        if args.scenario == 'power-cycle':
            from tools.noki6210_power_check import verify as check_power, check_frames
            check_power(text, (run / 'nvram/npe3hle/sim_card').read_bytes(), minimum_off=50)
            check_frames(run / 'snap')
        else:
            verify(text, runtime=runtime, selftest=runtime)
        if args.scenario == 'foreign-registration':
            check_registration(text, (run / 'nvram/npe3hle/sim_card').read_bytes(),
                               channel_header='1202', expected_lai=bytes.fromhex('32f4510001'))
            cold = run / 'cold-registration'
            cold.mkdir()
            shutil.copytree(run / 'nvram', cold / 'nvram')
            shutil.copytree(run / 'cfg', cold / 'cfg')
            with (cold / 'console.log').open('w') as output:
                subprocess.run(command, cwd=cold, stdout=output, stderr=subprocess.STDOUT,
                               check=True, timeout=180, env=environment)
            check_output((cold / 'console.log').read_text(errors='replace'))
            cold_text = events(cold / 'error.log')
            verify(cold_text, runtime=True, selftest=True)
            check_registration(cold_text, (cold / 'nvram/npe3hle/sim_card').read_bytes(),
                               preserved_location=True, channel_header='1202',
                               expected_lai=bytes.fromhex('32f4510001'))
            from PIL import Image
            for directory, trace in ((run, text), (cold, cold_text)):
                with Image.open(directory / 'snap/6210_before_menu.png') as frame:
                    check_frame(frame, FOREIGN_OPERATOR_SHA256, 'VODAFONE / DCT3 LAB foreign idle')
                with Image.open(directory / 'snap/6210_after_menu.png') as frame:
                    check_menu(trace, frame)
        if args.foreign_cell:
            check_registration(text, (run / 'nvram/npe3hle/sim_card').read_bytes(),
                               channel_header='1202', expected_lai=bytes.fromhex('32f4510001'))
        if args.pin_enabled:
            from tools.radio_call_lifecycle_common import require_ordered
            from tools.sim_security_trace_check import validate as check_security
            import re
            require_ordered(text, (
                ('physical PIN', re.compile(r'6210_security_physical: action=confirm')),
                ('accepted PIN', re.compile(r'SIM status ins=20 sw=9000')),
                ('location acceptance', re.compile(r'LAPDm Location Updating Accept acknowledged nr=1')),
            ), '6210 PIN before host service')
            card_bytes = (run / 'nvram/npe3hle/sim_card').read_bytes()
            check_security(text, card_bytes, 'verify', '1234')
            check_registration(text, card_bytes,
                               channel_header='1202')
        if args.scenario in ('menu', 'accessory'):
            from PIL import Image
            with Image.open(run / 'snap/6210_after_menu.png') as frame:
                check_menu(text, frame)
            if args.scenario == 'accessory':
                with Image.open(run / 'snap/6210_before_menu.png') as frame:
                    check_accessory(text, frame)
        elif args.scenario in ('toolkit-interactive', 'toolkit-menu', 'toolkit-sms', 'toolkit-call', 'toolkit-call-decline'):
            import re
            from tools.dct3_toolkit_check import verify_interactive
            from PIL import Image
            actions = ['dismiss', 'inkey_5', 'input_4', 'input_2', 'confirm']
            if args.scenario != 'toolkit-interactive':
                from tools.dct3_toolkit_check import verify_menu
                status = {'toolkit-menu': '9000', 'toolkit-sms': '9124', 'toolkit-call': '911c', 'toolkit-call-decline': '911c'}[args.scenario]
                verify_menu(text, '6210', selection_status=status)
                actions.extend(['menu', 'menu_last', 'menu_open', 'menu_select'])
                if args.scenario == 'toolkit-call':
                    from tools.sim_toolkit_call_trace_check import verify as check_toolkit_call
                    from tools.sim_toolkit_trace_check import require_in_order
                    check_toolkit_call(text)
                    require_in_order(text, ['6210_toolkit_interactive: action=call_accept',
                                           'GSM outgoing request id=1 digits=5551234',
                                           '6210_toolkit_interactive: action=menu_exit',
                                           'GSM service uplink sapi=0 pd=03 message=25'])
                    actions.append('call_accept')
                elif args.scenario == 'toolkit-call-decline':
                    from tools.sim_toolkit_call_trace_check import verify_decline
                    verify_decline(text, '6210')
                    actions.append('call_decline')
                actions.append('menu_exit')
                if args.scenario == 'toolkit-sms':
                    from tools.sim_toolkit_sms_trace_check import verify as check_toolkit_sms
                    check_toolkit_sms(text, cp=0x39, message_reference=1)
            else:
                verify_interactive(text, '6210')
            if re.findall(r'6210_toolkit_interactive: action=(\w+)\b', text) != actions:
                raise ValueError('NPE-3 interactive Toolkit physical sequence differs')
            check_registration(text, (run / 'nvram/npe3hle/sim_card').read_bytes())
            expected = {
                'display': '1c27b5e561a2183e01fffc11e71a78f5df35e342fde2c01d6df3fffc26c4199b',
                'inkey': '60ece9b44b016ae749bc6b3498ded166628a81f5b983a2c7dd06768e93782fbf',
                'input': '91bdd069d33a7cd535d1f472ed04f006a8615b8de3158b1c0d574ca5844063c6',
                'entered': 'c0fe70b64f1b25657e22bc7efb786b17e4831a48230bf3eced61dc0a21616b2a',
                'idle': OPERATOR_SHA256,
            }
            for phase, digest in expected.items():
                with Image.open(run / 'snap' / f'6210_toolkit_interactive_{phase}.png') as frame:
                    check_frame(frame, digest, 'interactive Toolkit ' + phase)
            if args.scenario != 'toolkit-interactive':
                for phase, digest in {
                    'entry': '904ec53842abe3b1dcf50fc5e6bc93d61ad352295e07dcbb7d13cf0e3594c5d1',
                    'items': 'a9d5d7e7caa5f074af70d12c5b25ec7dfcad057e7bebc49a91f181cf0a1cfcea',
                    'result': ('66063a4df7efdacdfce3e8d90995423c48d856dfc50e1a649e008d42572cc35a'
                               if args.scenario in ('toolkit-call', 'toolkit-call-decline') else
                               'a9d5d7e7caa5f074af70d12c5b25ec7dfcad057e7bebc49a91f181cf0a1cfcea'),
                    'idle': OPERATOR_SHA256,
                }.items():
                    with Image.open(run / 'snap' / f'6210_toolkit_menu_{phase}.png') as frame:
                        check_frame(frame, digest, 'Toolkit menu ' + phase)
                if args.scenario in ('toolkit-sms', 'toolkit-call', 'toolkit-call-decline'):
                    with Image.open(run / 'snap/6210_toolkit_network_result.png') as frame:
                        check_frame(frame, ('0b46b0a01032750f26d6856034443cb7b5d7d2f6a37f282051867fd112e05f06'
                                            if args.scenario == 'toolkit-call' else
                                            'a9d5d7e7caa5f074af70d12c5b25ec7dfcad057e7bebc49a91f181cf0a1cfcea'),
                                    'Toolkit network result')
        elif args.scenario == 'toolkit':
            from tools.noki6210_toolkit_check import verify as check_toolkit
            check_toolkit(text)
            check_registration(text, (run / 'nvram/npe3hle/sim_card').read_bytes())
            from PIL import Image
            with Image.open(run / 'snap/6210_toolkit_display.png') as frame:
                check_frame(frame, '1c27b5e561a2183e01fffc11e71a78f5df35e342fde2c01d6df3fffc26c4199b',
                            'proactive DCT3 SAT')
            with Image.open(run / 'snap/6210_toolkit_after_dismiss.png') as frame:
                check_frame(frame, OPERATOR_SHA256, 'registered idle after Toolkit clearance')
        elif args.scenario in ('divert-lifecycle', 'state-divert'):
            if args.scenario == 'state-divert':
                from tools.noki6210_state_check import verify as check_state
                check_state(text, 'divert')
            from tools.radio_call_divert_lifecycle_trace_check import EVENTS
            from tools.radio_call_lifecycle_common import require_ordered
            if args.scenario == 'state-divert':
                import re
                require_ordered(text, (
                    ('forwarding activated', EVENTS[0]),
                    ('active state saved', re.compile(r'6210_state: scenario=divert event=saved')),
                    ('active state restored', re.compile(r'6210_state: scenario=divert event=restored')),
                    ('physical query after restore', re.compile(r'6210_divert_lifecycle_physical: transaction=2')),
                    ('restored forwarding active', EVENTS[2]),
                ), '6210 forwarding restore')
            require_ordered(text, tuple((f'divert lifecycle {index}', pattern)
                                       for index, pattern in enumerate(EVENTS, 1)), '6210 divert lifecycle')
            check_registration(text, (run / 'nvram/npe3hle/sim_card').read_bytes())
            from PIL import Image
            # Own reviewed presentation: activation, query acknowledgement,
            # inactive service summary, then inactive interrogation result.
            expected_frames = (
                'e9e4c057d769f66a48893d561b8edc1de9213a44c3756f047db4bf3d14653295',
                '466a5a0241eb09e162c00227e8737eaddc5717251ec2663bf9a97b3c8c8c58d9',
                '45ca2e2d94aa2af50a1f008baef60fffecbb49ba304c3ff8e68818a91b88bf3e',
                DIVERT_RESULT_SHA256,
            )
            for index, expected in enumerate(expected_frames, 1):
                with Image.open(run / f'snap/6210_divert_lifecycle_{index}.png') as frame:
                    check_frame(frame, expected, f'divert lifecycle result {index}')
            with Image.open(run / 'snap/6210_divert_lifecycle_idle.png') as frame:
                check_frame(frame, OPERATOR_SHA256, 'registered idle after divert lifecycle')
        elif args.scenario=='ussd-continued':
            from tools.noki6210_ussd_continued_check import check,check_frames
            check(text,(run/'nvram/npe3hle/sim_card').read_bytes())
            check_frames(run/'snap')
            check_registration(text,(run/'nvram/npe3hle/sim_card').read_bytes(),channel_header='1202')
        elif args.scenario in ('ussd', 'divert'):
            if args.scenario == 'ussd':
                check_ussd(text, run / 'snap')
            else:
                check_divert(text, run / 'snap')
            check_registration(text, (run / 'nvram/npe3hle/sim_card').read_bytes())
        elif args.scenario == 'calculator':
            from PIL import Image
            with Image.open(run / 'snap/6210_calculator_result.png') as frame:
                check_calculator(text, frame)
        elif args.scenario == 'phonebook':
            from PIL import Image
            cold = run / 'cold'
            cold.mkdir()
            shutil.copytree(run / 'nvram', cold / 'nvram')
            if args.coherent_cell:
                shutil.copytree(run / 'cfg', cold / 'cfg')
            cold_command = command.copy()
            cold_command[cold_command.index('-autoboot_script') + 1] = str(root / 'tools/noki6210_phonebook_read.lua')
            cold_command[cold_command.index('-seconds_to_run') + 1] = '27'
            with (cold / 'console.log').open('w') as output:
                subprocess.run(cold_command, cwd=cold, stdout=output, stderr=subprocess.STDOUT,
                               check=True, timeout=180, env=environment)
            check_output((cold / 'console.log').read_text(errors='replace'))
            read_trace = events(cold / 'error.log')
            verify(read_trace, runtime=True, selftest=True)
            if args.pin_enabled:
                from tools.sim_security_trace_check import validate as check_security
                saved_card = (run / 'nvram/npe3hle/sim_card').read_bytes()
                cold_card = (cold / 'nvram/npe3hle/sim_card').read_bytes()
                if saved_card != cold_card:
                    raise ValueError('PIN phonebook readback changed persisted card bytes')
                check_security(read_trace, cold_card, 'verify', '1234')
                check_registration(read_trace, cold_card, preserved_location=True,
                                   channel_header='1202')
            with Image.open(cold / 'snap/6210_phonebook_read_contact.png') as frame:
                check_phonebook(text, read_trace,
                               (cold / 'nvram/npe3hle/sim_card').read_bytes(), frame)
        elif args.scenario == 'registration':
            check_registration(text, (run / 'nvram/npe3hle/sim_card').read_bytes())
            from PIL import Image
            with Image.open(run / 'snap/6210_before_menu.png') as frame:
                check_frame(frame, OPERATOR_SHA256, 'DCT3 LAB registered idle')
        elif args.scenario in ('outgoing-call', 'host-outgoing-call'):
            from tools.noki6210_outgoing_call_check import verify as check_call
            check_call(text, coherent_pin=args.pin_enabled or args.coherent_cell)
            if args.trace_dsp_control:
                from tools.noki6210_dsp_control_contract import verify_mode_observation
                # The compact events view omits the mailbox CPU snapshots.
                contract['dsp_control_observation'] = verify_mode_observation(
                    (run / 'error.log').read_text(errors='replace'))
                from tools.noki6210_installed_program import verify_catalogue_reads
                contract['upload_catalogue_observation'] = verify_catalogue_reads(
                    (root / 'roms/noki6210/6210_556c.fls').read_bytes(),
                    (run / 'error.log').read_text(errors='replace'))
            if host:
                from tools.radio_host_outgoing_connect_check import verify as check_host
                check_host((run / 'error.log').read_text(errors='replace'), '1234567')
        elif args.scenario in ('incoming-call', 'incoming-pcm-missing', 'host-incoming-call'):
            from tools.noki6210_incoming_call_check import verify as check_call
            check_call(text, coherent_pin=args.pin_enabled)
            if args.scenario == 'incoming-pcm-missing':
                from tools.radio_pcm_missing_trace_check import check_unclocked
                check_unclocked(run / 'error.log', expected_control=0x070b)
            if args.scenario == 'host-incoming-call':
                from PIL import Image
                with Image.open(run / 'snap/6210_host_registered_idle.png') as frame:
                    check_frame(frame, OPERATOR_SHA256, 'registered idle before host call')
                with Image.open(run / 'snap/6210_after_incoming_call.png') as frame:
                    check_frame(frame, OPERATOR_SHA256, 'registered idle after host call')
                with Image.open(run / 'snap/6210_incoming_ringing.png') as frame:
                    if frame.size != (96, 60) or hashlib.sha256(
                            frame.convert('L').crop((0, 8, 96, 40)).tobytes()).hexdigest() != (
                            'edde96356123c9684d846f94418f1d2e6eedc7fd331a3dab41d92cd38f5006ca'):
                        raise ValueError('missing reviewed host caller 5551234')
        elif args.scenario in ('outgoing-sms', 'host-outgoing-sms', 'host-sms-status', 'host-rejected-sms', 'host-silent-sms'):
            from tools.noki6210_outgoing_sms_check import verify as check_submission
            if args.scenario != 'host-silent-sms':
                check_submission(text, rejected=args.scenario == 'host-rejected-sms')
            from PIL import Image
            if args.scenario in ('host-rejected-sms', 'host-silent-sms'):
                from tools.noki6210_sms_failure_check import verify as check_failure
                check_failure((run / 'error.log').read_text(errors='replace'), run / 'snap',
                              rp_silence=args.scenario == 'host-silent-sms')
            elif host:
                with Image.open(run / 'snap/6210_sms_sent.png') as frame:
                    check_host_sms_sent(frame)
            else:
                with Image.open(run / 'snap/6210_sms_sent.png') as frame:
                    check_frame(frame, SMS_SENT_SHA256, 'Message sent')
        elif args.scenario in ('incoming-sms', 'host-incoming-sms'):
            from tools.noki6210_incoming_sms_check import verify as check_delivery
            check_delivery(text, (run / 'nvram/npe3hle/sim_card').read_bytes())
            from PIL import Image
            with Image.open(run / 'snap/6210_sms_read_1.png') as frame:
                check_frame(frame, SMS_READ_SHA256, 'received hello SMS')
        elif args.scenario in ('plmn-reject', 'home-plmn-reject'):
            retained = (run / 'nvram/npe3hle/sim_card').read_bytes()
            check_plmn_reject(text, retained,
                              home=args.scenario == 'home-plmn-reject')
            if args.scenario == 'plmn-reject':
                for forbidden in (True, False):
                    cold = run / ('cold-forbidden' if forbidden else 'cold-cleared')
                    cold.mkdir()
                    shutil.copytree(run / 'nvram', cold / 'nvram')
                    shutil.copytree(run / 'cfg', cold / 'cfg')
                    config_path = cold / f'cfg/{machine}.cfg'
                    config = ET.parse(config_path)
                    for port in config.findall('.//port'):
                        if port.get('tag') == ':AUTHCFG' and port.get('mask') == '2':
                            port.set('value', '0')
                    config.write(config_path, encoding='utf-8', xml_declaration=True)
                    card = cold / f'nvram/{machine}/sim_card'
                    if not forbidden:
                        cleared = bytearray(retained)
                        cleared[3548:3560] = b'\xff' * 12
                        card.write_bytes(cleared)
                    cold_command = command.copy()
                    cold_command[cold_command.index('-seconds_to_run') + 1] = '30'
                    with (cold / 'console.log').open('w') as output:
                        subprocess.run(cold_command, cwd=cold, stdout=output,
                                       stderr=subprocess.STDOUT, check=True, timeout=180,
                                       env=environment)
                    check_output((cold / 'console.log').read_text(errors='replace'))
                    cold_text = events(cold / 'error.log')
                    verify(cold_text, runtime=True, selftest=True)
                    check_forbidden_selection(cold_text, card.read_bytes(),
                                              forbidden=forbidden, previous=retained)
        elif args.scenario in HOST_USSD_SCENARIOS:
            from tools.noki6210_host_ussd_check import check_notification,check_request,check_frames as check_ussd_frames
            if args.scenario in ('host-incoming-ussd-request','host-incoming-ussd-restore'):
                restored=args.scenario=='host-incoming-ussd-restore'
                check_request(text,(run/'nvram/npe3hle/sim_card').read_bytes(),(run/'console.log').read_text(errors='replace'),restored=restored)
                if restored and not (run/'sta/npe3hle/npe3_ussd_pending.sta').is_file():
                    raise ValueError('pending USSD save artifact is absent')
                check_ussd_frames(run/'snap',request=True)
            else:
                check_notification(text,(run/'nvram/npe3hle/sim_card').read_bytes())
                check_ussd_frames(run/'snap')
            check_registration(text,(run/'nvram/npe3hle/sim_card').read_bytes(),channel_header='1202')
        elif args.scenario in FDN_SCENARIOS:
            from tools.noki6210_fdn_check import check as check_fdn,check_frames as check_fdn_frames
            phase=3 if args.scenario=='fdn-observe' else 0
            rejected=args.scenario=='fdn-retry-cold'
            card=(run/'nvram/npe3hle/sim_card').read_bytes()
            check_fdn(text,card,phase,rejected=rejected)
            check_fdn_frames(run/'snap',phase,rejected=rejected)
            check_registration(text,card,channel_header='1202')
            if args.scenario in ('fdn-cycle','fdn-retry-cold'):
                source=run
                cold_retry={'phases':[]}
                ids={launch['invocation_id']}
                phases=([(0,'accepted','fdn_enable',66)] if rejected else [])
                phases += [(1,'cold-on','fdn_input',58),(2,'off','fdn_disable',66),(3,'cold-off','fdn_input',58)]
                for phase,name,script,seconds in phases:
                    cold=run/name
                    before=(source/'nvram/npe3hle/sim_card').read_bytes()
                    cold_text,initial,continuation=run_retained_security(source,cold,command,
                        root/f'tools/noki6210_{script}.lua',seconds,environment)
                    if initial!=before or continuation['invocation_id'] in ids:
                        raise ValueError('FDN cold input or launch identity differs')
                    ids.add(continuation['invocation_id'])
                    card=(cold/'nvram/npe3hle/sim_card').read_bytes()
                    check_fdn(cold_text,card,phase,initial_pin2_attempts=2 if name=='accepted' else 3)
                    check_fdn_frames(cold/'snap',phase)
                    check_registration(cold_text,card,preserved_location=True,channel_header='1202')
                    cold_retry['phases'].append(continuation)
                    source=cold
        elif args.scenario in ('pin2-change','pin2-change-cold','pin2-retry-cold','pin2-reject'):
            from tools.noki6210_pin2_change_check import check as check_pin2,check_frames as check_pin2_frames
            rejected=args.scenario in ('pin2-retry-cold','pin2-reject')
            check_pin2(text,(run/'nvram/npe3hle/sim_card').read_bytes(),
                       old_pin='9999' if rejected else '5678',rejected=rejected)
            check_registration(text,(run/'nvram/npe3hle/sim_card').read_bytes(),channel_header='1202')
            check_pin2_frames(run/'snap',rejected=rejected)
            if args.scenario in ('pin2-change-cold','pin2-retry-cold'):
                cold=run/'cold-pin2'
                first=(run/'nvram/npe3hle/sim_card').read_bytes()
                script='noki6210_pin2_change_input.lua' if rejected else 'noki6210_pin2_change_cold_input.lua'
                cold_text,initial,cold_retry=run_retained_security(run,cold,command,
                    root/'tools'/script,84,environment)
                if initial!=first:
                    raise ValueError('cold PIN2 change must use the exact installed-PIN2 card')
                check_pin2(cold_text,(cold/'nvram/npe3hle/sim_card').read_bytes(),
                           '5678' if rejected else '8765','8765' if rejected else '5678')
                check_registration(cold_text,(cold/'nvram/npe3hle/sim_card').read_bytes(),
                                   preserved_location=True,channel_header='1202')
                check_pin2_frames(cold/'snap')
        elif args.scenario=='security-block':
            from tools.noki6210_security_retry_check import check_frames as retry_frames
            from tools.noki6210_puk_check import check_blocked,check_frames as puk_frames
            check_blocked(text,(run/'nvram/npe3hle/sim_card').read_bytes())
            retry_frames(run/'snap',wrong_only=True)
            puk_frames(run/'snap',('6210_pin_blocked.png',))
        elif args.scenario in ('security-retry-cold','security-block-cold'):
            from tools.noki6210_security_retry_check import check_wrong,check_cold,check_frames as retry_frames
            blocked=args.scenario=='security-block-cold'
            if blocked:
                from tools.noki6210_puk_check import check_blocked,check_unblocked,check_reauthenticated,check_frames as puk_frames
            from PIL import Image
            card=run/'nvram/npe3hle/sim_card'
            checkpoint=card.read_bytes()
            (check_blocked if blocked else check_wrong)(text,checkpoint)
            retry_frames(run/'snap',wrong_only=True)
            if blocked:
                puk_frames(run/'snap',('6210_pin_blocked.png',))
            recovery_source=run
            recovery_checkpoint=checkpoint
            wrong_launch=None
            if args.wrong_puk_first:
                from tools.noki6210_puk_check import check_wrong_puk
                wrong=run/'wrong-puk'
                wrong.mkdir()
                shutil.copytree(run/'nvram',wrong/'nvram')
                shutil.copytree(run/'cfg',wrong/'cfg')
                if (wrong/'nvram/npe3hle/sim_card').read_bytes()!=checkpoint:
                    raise ValueError('wrong-PUK cold input differs from blocked predecessor')
                wrong_script=root/'tools/noki6210_puk_wrong_input.lua'
                wrong_command=list(command)
                wrong_command[wrong_command.index('-autoboot_script')+1]=str(wrong_script)
                wrong_command[wrong_command.index('-seconds_to_run')+1]='37'
                wrong_launch={'invocation_id':str(uuid.uuid4()),'command':wrong_command,
                    'run_directory':str(wrong),'source_card_sha256':hashlib.sha256(checkpoint).hexdigest(),
                    'script_sha256':hashlib.sha256(wrong_script.read_bytes()).hexdigest(),
                    'initial_storage':{str(path.relative_to(wrong/'nvram')):hashlib.sha256(path.read_bytes()).hexdigest()
                        for path in sorted((wrong/'nvram').rglob('*')) if path.is_file()}}
                (wrong/'launch.json').write_text(json.dumps(wrong_launch,indent=2)+'\n')
                with (wrong/'console.log').open('w') as output:
                    subprocess.run(wrong_command,cwd=wrong,stdout=output,stderr=subprocess.STDOUT,
                                   check=True,timeout=180,env=environment)
                check_output((wrong/'console.log').read_text(errors='replace'))
                wrong_text=events(wrong/'error.log')
                verify(wrong_text,runtime=True,selftest=True)
                recovery_checkpoint=(wrong/'nvram/npe3hle/sim_card').read_bytes()
                check_wrong_puk(wrong_text,checkpoint,recovery_checkpoint)
                puk_frames(wrong/'snap',('6210_puk_prompt.png','6210_puk_new_pin.png',
                                        '6210_puk_confirm.png','6210_puk_result.png'))
                recovery_source=wrong
            cold=run/'cold-recovery'
            recovery_script=root/('tools/noki6210_puk_input.lua' if blocked else 'tools/noki6210_security_input.lua')
            cold_text,initial,cold_retry=run_retained_security(recovery_source,cold,command,
                recovery_script,57 if blocked else 37,environment)
            if initial!=recovery_checkpoint:
                raise ValueError('cold PIN recovery input differs from predecessor')
            if blocked:
                check_unblocked(cold_text,initial,(cold/'nvram/npe3hle/sim_card').read_bytes(),
                                puk_attempts=9 if args.wrong_puk_first else 10)
            else:
                check_cold(cold_text,initial,(cold/'nvram/npe3hle/sim_card').read_bytes())
            check_registration(cold_text,(cold/'nvram/npe3hle/sim_card').read_bytes(),channel_header='1202')
            with Image.open(cold/'snap/6210_security_then_menu.png') as frame:
                check_frame(frame,SECURITY_MENU_SHA256,'cold Messages after PIN recovery')
            if card.read_bytes()!=checkpoint:
                raise ValueError('cold process changed its predecessor card')
            if (recovery_source/'nvram/npe3hle/sim_card').read_bytes()!=recovery_checkpoint:
                raise ValueError('cold recovery changed its wrong-PUK predecessor')
            if wrong_launch:
                cold_retry['wrong_puk_predecessor']=wrong_launch
            if blocked:
                puk_frames(cold/'snap',('6210_puk_prompt.png','6210_puk_new_pin.png','6210_puk_confirm.png','6210_puk_idle.png'))
                reauth=run/'cold-new-pin'
                reauth.mkdir()
                shutil.copytree(cold/'nvram',reauth/'nvram')
                shutil.copytree(cold/'cfg',reauth/'cfg')
                recovered=(cold/'nvram/npe3hle/sim_card').read_bytes()
                if (reauth/'nvram/npe3hle/sim_card').read_bytes()!=recovered:
                    raise ValueError('new-PIN cold input differs from recovered card')
                reauth_command=list(cold_retry['command'])
                reauth_script=root/'tools/noki6210_puk_reauth_input.lua'
                reauth_command[reauth_command.index('-autoboot_script')+1]=str(reauth_script)
                reauth_command[reauth_command.index('-seconds_to_run')+1]='39'
                reauth_launch={'invocation_id':str(uuid.uuid4()),'command':reauth_command,
                    'run_directory':str(reauth),'source_card_sha256':hashlib.sha256(recovered).hexdigest(),
                    'script_sha256':hashlib.sha256(reauth_script.read_bytes()).hexdigest(),
                    'initial_storage':{str(path.relative_to(reauth/'nvram')):hashlib.sha256(path.read_bytes()).hexdigest()
                        for path in sorted((reauth/'nvram').rglob('*')) if path.is_file()}}
                (reauth/'launch.json').write_text(json.dumps(reauth_launch,indent=2)+'\n')
                with (reauth/'console.log').open('w') as output:
                    subprocess.run(reauth_command,cwd=reauth,stdout=output,stderr=subprocess.STDOUT,
                                   check=True,timeout=180,env=environment)
                check_output((reauth/'console.log').read_text(errors='replace'))
                reauth_text=events(reauth/'error.log')
                verify(reauth_text,runtime=True,selftest=True)
                check_reauthenticated(reauth_text,recovered,(reauth/'nvram/npe3hle/sim_card').read_bytes())
                check_registration(reauth_text,(reauth/'nvram/npe3hle/sim_card').read_bytes(),
                                   preserved_location=True,channel_header='1202')
                puk_frames(reauth/'snap',('6210_puk_idle.png',))
                with Image.open(reauth/'snap/6210_security_then_menu.png') as frame:
                    check_frame(frame,SECURITY_MENU_SHA256,'Messages after cold new-PIN authentication')
                if (cold/'nvram/npe3hle/sim_card').read_bytes()!=recovered:
                    raise ValueError('new-PIN process changed its recovered predecessor')
                cold_retry['new_pin_authentication']=reauth_launch
        elif args.scenario in ('security','security-retry'):
            import re
            from tools.radio_call_lifecycle_common import require_ordered
            if args.scenario=='security-retry':
                from tools.noki6210_security_retry_check import check as check_retry, check_frames as check_retry_frames
                check_retry(text,(run/'nvram/npe3hle/sim_card').read_bytes())
                check_retry_frames(run/'snap')
            else:
                require_ordered(text, (
                    ('physical PIN', re.compile(r'6210_security_physical: action=confirm')),
                    ('VERIFY CHV1', re.compile(r'sim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08')),
                    ('accepted PIN', re.compile(r'SIM status ins=20 sw=9000')),
                    ('physical Menu', re.compile(r'6210_security_physical: action=menu')),
                ), '6210 security')
            from PIL import Image
            with Image.open(run / 'snap/6210_security_then_menu.png') as frame:
                check_frame(frame, SECURITY_MENU_SHA256, 'Messages after PIN verification')
            if args.coherent_cell and args.scenario=='security':
                require_ordered(text, (
                    ('background request', re.compile(r'TX packet type=57 payload=4 .*data=03050000')),
                    ('serving-cell measurement', re.compile(r'RX enqueue type=8b payload=166 .*data=0010002300c4')),
                    ('enabled firmware route', re.compile(r'6210_pin_measurement_route: enabled=01')),
                    ('task-14 delivery', re.compile(r'6210_pin_measurement_post: target=0e')),
                    ('measurement completion', re.compile(r'6210_pin_measurement_completion:')),
                    ('accepted PIN', re.compile(r'SIM status ins=20 sw=9000')),
                    ('location acceptance', re.compile(r'LAPDm Location Updating Accept acknowledged nr=1')),
                ), '6210 delayed-PIN registration')
                check_registration(text, (run / 'nvram/npe3hle/sim_card').read_bytes(),
                                   channel_header='1202')
            if args.coherent_cell and args.scenario=='security-retry':
                check_registration(text,(run/'nvram/npe3hle/sim_card').read_bytes(),channel_header='1202')
        elif args.scenario.startswith('state-'):
            from tools.noki6210_state_check import verify as check_state
            check_state(text, args.scenario.removeprefix('state-'),
                        pin_enabled=args.pin_enabled)
            if args.scenario == 'state-call':
                from tools.noki6210_outgoing_call_check import verify as check_call
                check_call(text, coherent_pin=args.pin_enabled)
            elif args.scenario == 'state-sms':
                from tools.radio_incoming_sms_trace_check import SMS_NVRAM_OFFSET, STORED_RECORD_PREFIX
                storage = (run / 'nvram/npe3hle/sim_card').read_bytes()
                expected = b'\x01' + STORED_RECORD_PREFIX[1:]
                if storage[SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET + len(expected)] != expected:
                    raise ValueError('restored SMS did not persist read hello')
                from PIL import Image
                with Image.open(run / 'snap/6210_state_sms_read_1.png') as frame:
                    check_frame(frame, SMS_READ_SHA256, 'restored received hello SMS')
        if args.scenario in ('host-incoming-sms', 'host-outgoing-sms', 'host-sms-status'):
            name = 'radio_incoming_host_sms_trace_check' if args.scenario == 'host-incoming-sms' else 'radio_outgoing_host_sms_trace_check'
            options = [] if args.scenario == 'host-incoming-sms' else ['--octets', '1']
            if args.scenario == 'host-incoming-sms' and args.coherent_cell:
                options.extend(['--arfcn', '35'])
            subprocess.run([sys.executable, str(root / f'tools/{name}.py'),
                            str(run / 'error.log')] + options, check=True)
        if args.scenario == 'host-sms-status':
            from tools.noki6210_outgoing_sms_check import verify as check_submit, verify_status
            from tools.radio_outgoing_host_sms_trace_check import verify as check_host_sms
            card_path = Path(f'nvram/{machine}/sim_card')
            first_reference = verify_status(text, (run / card_path).read_bytes())
            cold = run / 'cold-sms-status'
            cold.mkdir()
            shutil.copytree(run / 'nvram', cold / 'nvram')
            shutil.copytree(run / 'cfg', cold / 'cfg')
            cold_host_command = host_command.copy()
            cold_host_command[cold_host_command.index('--cwd') + 1] = str(cold)
            with (cold / 'console.log').open('w') as output:
                subprocess.run(cold_host_command, cwd=cold, stdout=output,
                               stderr=subprocess.STDOUT, check=True, timeout=180,
                               env=environment)
            check_output((cold / 'console.log').read_text(errors='replace'))
            cold_text = events(cold / 'error.log')
            verify(cold_text, runtime=True, selftest=True)
            check_submit(cold_text)
            check_host_sms((cold / 'error.log').read_text(errors='replace'), octets=1)
            cold_storage = (cold / card_path).read_bytes()
            second_reference = verify_status(cold_text, cold_storage)
            if second_reference != (first_reference + 1) % 256:
                raise ValueError('cold SMS did not advance the retained TP-MR')
            if args.pin_enabled:
                from tools.sim_security_trace_check import validate as check_security
                check_security(cold_text, cold_storage, 'verify', '1234')
                check_registration(cold_text, cold_storage, preserved_location=True,
                                   channel_header='1202')
            from PIL import Image
            with Image.open(cold / 'snap/6210_sms_sent.png') as frame:
                check_host_sms_sent(frame)
        (run / 'acceptance.json').write_text(json.dumps({
            'machine': machine, 'scenario': args.scenario, 'passed': True,
            'launch': launch,
            **({('fdn_cold_lifecycle' if args.scenario in ('fdn-cycle','fdn-retry-cold') else 'cold_pin2_change' if args.scenario in ('pin2-change-cold','pin2-retry-cold') else 'cold_pin_retry_recovery'): cold_retry} if cold_retry else {}),
            'provisioning': 'unchanged acquired product PMM', 'contract': contract,
            'sim_profile': ('foreign two-digit-MNC preferred laboratory card' if args.scenario == 'foreign-registration' or args.foreign_cell
                            else 'visited-test-PLMN preferred laboratory card' if args.scenario == 'plmn-reject'
                            else 'PIN-enabled extended FDN laboratory card' if args.scenario in FDN_SCENARIOS
                            else 'PIN-enabled laboratory card' if args.scenario in PIN_SCENARIOS or args.pin_enabled
                            else 'default laboratory card'),
            'plmn_rejection': ({'cause': 11, 'visited_plmn': '001-02',
                                'firmware_sim_update': True, 'cold_window_seconds': 30,
                                'controls': ['retained-forbidden', 'cleared-list-only']}
                               if args.scenario == 'plmn-reject' else
                               {'cause': 11, 'home_plmn_forbidden': False}
                               if args.scenario == 'home-plmn-reject' else None),
            'sms_status_persistence': ({'first_reference': first_reference,
                                        'cold_reference': second_reference,
                                        'firmware_read_and_write': True,
                                        'independent_processes': 2}
                                       if args.scenario == 'host-sms-status' else None),
            'native_dsp_complete': False, 'speech_tested': False, 'command': command,
            'foreign_registration': ({'plmn': '234-15', 'lai': '32f4510001',
                                      'firmware_sim_update': True, 'independent_processes': 2}
                                     if args.scenario == 'foreign-registration' else
                                     {'plmn': '234-15', 'lai': '32f4510001',
                                      'firmware_sim_update': True, 'independent_processes': 1}
                                     if args.foreign_cell else None),
            'host_command': host_command,
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'6210 acceptance FAIL: {error}\n')
    print(f'6210 {args.scenario} acceptance PASS: {run}')


if __name__ == '__main__':
    main()
