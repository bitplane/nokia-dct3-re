"""Isolated NSM-3 research-HLE acceptance with acquired base-record fixture."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki8210_pmm_check import base_record_fixture
from tools.noki8210_registration_check import verify as verify_registration
from tools.noki8210_staged_check import verify as verify_stage

SCENARIOS = {
    'power-cycle': ('power_cycle_input', 73, 'power_check'),
    'ussd': ('ussd_input', 40, 'ussd_check'),
    'divert': ('divert_input', 40, 'divert_check'),
    'registration': ('registration_input', 46, 'registration_check'),
    'outgoing-call': ('outgoing_call_input', 49, 'outgoing_call_check'),
    'incoming-call': ('incoming_call_input', 44, 'incoming_call_check'),
    'incoming-sms': ('incoming_sms_input', 32, 'incoming_sms_check'),
    'outgoing-sms': ('outgoing_sms_input', 46, 'outgoing_sms_check'),
    'calculator': ('calculator_input', 41, None),
    'phonebook': ('phonebook_input', 34, 'phonebook_check'),
    'idle-state': ('state_idle', 40, 'state_check'),
    'call-state': ('state_call', 50, 'state_check'),
    'sms-state': ('state_sms', 32, 'state_check'),
    'host-incoming-call': ('host_incoming_input', 60, 'incoming_call_check'),
    'host-outgoing-call': ('outgoing_call_input', 49, 'outgoing_call_check'),
    'host-incoming-sms': ('incoming_sms_input', 32, 'incoming_sms_check'),
    'host-outgoing-sms': ('outgoing_sms_input', 46, 'outgoing_sms_check'),
    'host-rejected-sms': ('sms_reject_input', 58, 'outgoing_sms_check'),
    'host-silent-sms': ('sms_silence_input', 125, 'outgoing_sms_check'),
}


MCU_SHA1 = 'c1a0fe95cedb89a92b19654208cc4855e1a4988e'
PMM_SHA256 = '31f51bcd69e183f23c39136574bd6a44864eb2ba1939417b9d848a3e0639ec59'


def check_host_registration(text, storage):
    verify_stage(text, runtime=True, selftest=True, base_record=True)
    verify_registration(text, storage, configured_carrier=True)
    if not re.search(r'gsm_call_adapter: network registered=1 arfcn=4\b', text):
        raise ValueError('host session lacks coherent ARFCN4 registration')


def prepare_run(run, mcu, pmm):
    """Pin provenance before creating storage; never reuse a previous run."""
    if hashlib.sha1(mcu).hexdigest() != MCU_SHA1:
        raise ValueError('unexpected acquired NSM-3 MCU/PPM image')
    if hashlib.sha256(pmm).hexdigest() != PMM_SHA256:
        raise ValueError('unexpected acquired NSM-3 PMM image')
    fixture = base_record_fixture(pmm)
    run.mkdir(parents=True, exist_ok=False)
    (run / 'nvram/nsm3hle').mkdir(parents=True)
    (run / 'cfg').mkdir()
    (run / 'nvram/nsm3hle/flash').write_bytes(mcu + fixture)


def configure_pin_topology(config_path):
    """Retain host inputs while selecting the independently configured cell."""
    if config_path.exists():
        config_tree = ET.parse(config_path).getroot()
        ports = config_tree.find("./system[@name='nsm3hle']/input")
        if ports is None:
            raise ValueError('product host configuration lacks its input section')
    else:
        config_tree = ET.Element('mameconfig', version='10')
        system = ET.SubElement(config_tree, 'system', name='nsm3hle')
        ports = ET.SubElement(system, 'input')
    carrier = ports.find("./port[@tag=':NEIGHBORCFG']")
    if carrier is None:
        ET.SubElement(ports, 'port', tag=':NEIGHBORCFG', type='CONFIG',
                      mask='256', defvalue='0', value='256')
    elif carrier.get('mask') != '256' or carrier.get('value') != '256':
        raise ValueError('PIN fixture requires coherent ARFCN4/5 topology')
    ET.ElementTree(config_tree).write(config_path, encoding='utf-8', xml_declaration=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--scenario', choices=SCENARIOS, default='registration')
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--port', type=int, default=18991)
    parser.add_argument('--pin-enabled', action='store_true',
                        help='test SIM PIN followed by phone security and registration')
    parser.add_argument('--dcs1800', action='store_true',
                        help='test registration on explicit DCS1800 carriers 823/824')
    args = parser.parse_args()
    if args.dcs1800 and args.scenario != 'registration':
        parser.error('--dcs1800 currently requires the registration scenario')
    if args.pin_enabled and args.scenario not in ('registration', 'host-incoming-call',
                                                 'host-incoming-sms', 'host-outgoing-call',
                                                 'host-outgoing-sms', 'phonebook',
                                                 'idle-state', 'call-state', 'sms-state'):
        parser.error('--pin-enabled requires registration or a supported host service')
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    try:
        mcu = (root / 'roms/noki8210/8210_5.31ppm_c.fls').read_bytes()
        pmm = (root / 'roms/noki8210/8210 virgin eeprom 003d0000.fls').read_bytes()
        if args.scenario in ('ussd', 'divert'):
            from tools.noki8210_ussd_check import verify_key_table
            verify_key_table(mcu)
        prepare_run(run, mcu, pmm)
        if args.pin_enabled:
            from tools.make_sim_card_profile import make_profile
            (run / 'nvram/nsm3hle/sim_card').write_bytes(make_profile(pin_enabled=True))
        host_sms = args.scenario.startswith('host-') and args.scenario.endswith('-sms')
        config = 'noki8210_host_gsm900' if args.scenario.startswith('host-') or args.scenario == 'power-cycle' else {
                  'incoming-call': 'radio_incoming_call_answered',
                  'incoming-sms': 'radio_incoming_sms',
                  'sms-state': 'radio_incoming_sms'}.get(args.scenario)
        if config:
            shutil.copyfile(root / f'fixtures/{config}/nsm3hle.cfg', run / 'cfg/nsm3hle.cfg')
        if args.dcs1800:
            shutil.copyfile(root / 'fixtures/noki8210_dcs1800/nsm3hle.cfg', run / 'cfg/nsm3hle.cfg')
        elif args.pin_enabled:
            configure_pin_topology(run / 'cfg/nsm3hle.cfg')
        environment = os.environ.copy()
        environment.pop('NOKIA_DCT3_8210_PIN_ENTRY', None)
        if args.pin_enabled:
            environment['NOKIA_DCT3_8210_PIN_ENTRY'] = '1'
        script, seconds, checker = SCENARIOS[args.scenario]
        command = [str((args.mame or root / 'mame/mame').resolve()), 'nsm3hle',
                   '-rompath', str(root / 'roms'), '-nvram_directory', 'nvram',
                   '-cfg_directory', 'cfg', '-noreadconfig', '-debug', '-debugger', 'none',
                   '-autoboot_script', str(root / f'tools/noki8210_{script}.lua'),
                   '-autoboot_delay', '0', '-seconds_to_run', str(seconds),
                   '-video', 'none', '-sound', 'none', '-nothrottle', '-log', '-verbose']
        def execute(cmd, output):
            with (run / output).open('w') as console:
                subprocess.run(cmd, cwd=run, stdout=console, stderr=subprocess.STDOUT,
                               check=True, env=environment)
        host_command = None
        if args.scenario.startswith('host-'):
            command.extend(['-http', '-http_port', str(args.port)])
            runner, options = {
                'host-incoming-call': ('run_host_incoming_signaling_gate', [
                    '--caller', '5551234', '--ready-file',
                    str(run / 'snap/8210_host_registered_idle.png')]),
                'host-outgoing-call': ('run_host_call_adapter_gate', [
                    '--number', '1234567', '--decision', 'connect']),
                'host-incoming-sms': ('run_host_incoming_sms_gate', []),
                'host-outgoing-sms': ('run_host_sms_gate', [
                    '--user-data', '41', '--user-data-length', '1']),
                'host-rejected-sms': ('run_host_sms_gate', [
                    '--user-data', '41', '--user-data-length', '1', '--decision', 'rp_error']),
                'host-silent-sms': ('run_host_sms_gate', [
                    '--user-data', '41', '--user-data-length', '1', '--decision', 'rp_silence']),
            }[args.scenario]
            host_command = [sys.executable, str(root / f'tools/{runner}.py'),
                            '--port', str(args.port), '--cwd', str(run)] + options + ['--'] + command
            execute(host_command, 'console.log')
        else:
            execute(command, 'console.log')
        if args.pin_enabled:
            from tools.sim_security_trace_check import validate as check_security
            pin_text = (run / 'error.log').read_text(errors='replace')
            check_security(pin_text,
                           (run / 'nvram/nsm3hle/sim_card').read_bytes(), 'verify', '1234')
            verify_stage(pin_text, runtime=True, selftest=True, base_record=True)
            if not re.search(
                    r'TX packet type=57 payload=4[^\n]*data=03050000.*?'
                    rf'RX enqueue type=8b payload=166[^\n]*data=0010{"0337" if args.dcs1800 else "0004"}00c4.*?'
                    r'8210_pin_measurement_route: enabled=01 message=([0-9a-f]{8}).*?'
                    r'8210_pin_measurement_completion: message=\1.*?'
                    r'8210_pin_physical: key=Menu.*?SIM status ins=20 sw=9000.*?'
                    r'LAPDm Location Updating Accept acknowledged nr=1', pin_text, re.S):
                raise ValueError('missing correlated measurement/PIN/registration sequence')
        check = [sys.executable, str(root / f'tools/noki8210_{checker}.py'), str(run / 'error.log')]
        storage = str(run / 'nvram/nsm3hle/sim_card')
        if args.scenario == 'power-cycle':
            check.extend([storage, str(run / 'snap')])
        elif args.scenario == 'host-incoming-call':
            check.extend(['--frames', str(run / 'snap'), '--configured-carrier'])
        elif args.scenario == 'host-outgoing-call':
            check.extend(['--configured-carrier', '--frames', str(run / 'snap')])
        elif args.scenario == 'host-outgoing-sms':
            check.extend(['--sent-frame', str(run / 'snap/8210_sms_sent.png')])
        elif args.scenario in ('host-rejected-sms', 'host-silent-sms'):
            check.extend(['--rejected' if args.scenario == 'host-rejected-sms' else '--rp-silence',
                          '--recovery-frames', str(run / 'snap')])
        elif args.scenario == 'phonebook':
            shutil.copyfile(run / 'error.log', run / 'write.log')
            saved_card = (run / 'nvram/nsm3hle/sim_card').read_bytes()
            cold = command.copy()
            cold[cold.index('-autoboot_script') + 1] = str(root / 'tools/noki8210_phonebook_read.lua')
            cold[cold.index('-seconds_to_run') + 1] = '28'
            execute(cold, 'cold_console.log')
            if args.pin_enabled:
                from tools.sim_security_trace_check import validate as check_security
                cold_card = (run / 'nvram/nsm3hle/sim_card').read_bytes()
                cold_text = (run / 'error.log').read_text(errors='replace')
                if saved_card != cold_card:
                    raise ValueError('PIN phonebook readback changed persisted SIM bytes')
                check_security(cold_text, cold_card, 'verify', '1234')
                verify_stage(cold_text, runtime=True, selftest=True, base_record=True)
                verify_registration(cold_text, cold_card, configured_carrier=True)
                retained = 'data=0080013f4905087200f110000133080910101032547698'
                read = cold_text.find('read-binary fid=6f7e offset=0 length=11')
                request = cold_text.find(retained)
                if read < 0 or request < read or 'update-binary fid=6f7e offset=10 length=1' in cold_text:
                    raise ValueError('PIN phonebook reboot did not preserve valid location state')
            check = [sys.executable, str(root / 'tools/noki8210_phonebook_check.py'),
                     str(run / 'write.log'), str(run / 'error.log'), storage,
                     '--frame', str(run / 'snap/8210_phonebook_read_contact.png')]
        elif args.scenario in ('idle-state', 'call-state', 'sms-state'):
            check.append(str(run / 'snap'))
            if args.pin_enabled:
                check.append('--configured-carrier')
                verify_registration(pin_text, (run / 'nvram/nsm3hle/sim_card').read_bytes(),
                                    configured_carrier=True)
            if args.scenario == 'call-state':
                check.append('--call')
            elif args.scenario == 'sms-state':
                check.extend(['--sms', '--storage', storage])
        elif args.scenario in ('registration', 'incoming-sms', 'host-incoming-sms'):
            check.append(storage)
            if args.scenario == 'registration':
                if args.dcs1800:
                    check.append('--dcs1800')
                elif args.pin_enabled:
                    check.append('--configured-carrier')
            if args.scenario == 'host-incoming-sms':
                check.extend(['--frame', str(run / 'snap/8210_sms_read_2.png')])
        if checker:
            subprocess.run(check, cwd=root, check=True)
            if host_sms:
                incoming = args.scenario == 'host-incoming-sms'
                host_checker = 'radio_incoming_host_sms_trace_check' if incoming else 'radio_outgoing_host_sms_trace_check'
                outcome = {'host-rejected-sms': 'rp_error', 'host-silent-sms': 'rp_silence'}.get(args.scenario, 'rp_ack')
                subprocess.run([sys.executable, str(root / f'tools/{host_checker}.py')] +
                               (['--arfcn', '4'] if incoming else ['--octets', '1', '--outcome', outcome]) + [str(run / 'error.log')],
                               cwd=root, check=True)
        else:
            from PIL import Image
            frame = Image.open(run / 'snap/8210_calculator_result.png').convert('L')
            digest = hashlib.sha256(frame.tobytes()).hexdigest()
            if frame.size != (84, 48) or digest != CALCULATOR_SHA256:
                raise ValueError('calculator result differs from reviewed 15 frame')
        if args.scenario.startswith('host-'):
            text = (run / 'error.log').read_text(errors='replace')
            check_host_registration(text, (run / 'nvram/nsm3hle/sim_card').read_bytes())
            if args.scenario == 'host-outgoing-call':
                from tools.radio_host_outgoing_connect_check import verify as check_host_call
                check_host_call(text, '1234567')
        (run / 'acceptance.json').write_text(json.dumps({
            'machine': 'nsm3hle', 'scenario': args.scenario, 'passed': True,
            'provisioning': 'unchanged acquired base record; later low journal omitted',
            'native_dsp_complete': False, 'speech_tested': False,
            'laboratory_carrier': 823 if args.dcs1800 else 4 if args.scenario.startswith('host-') or args.scenario == 'power-cycle' or args.pin_enabled else None,
            'sim_profile': 'PIN-enabled laboratory card' if args.pin_enabled else 'default laboratory card',
            'command': command,
            'host_command': host_command,
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'8210 acceptance FAIL: {error}\n')
    print(f'8210 {args.scenario} acceptance PASS: {run}')


CALCULATOR_SHA256 = '04ff410190e8c72f1fa6dbbb00c965051c520fee4b7ca6fe9c7d395ccdfc23e5'

if __name__ == '__main__':
    main()
