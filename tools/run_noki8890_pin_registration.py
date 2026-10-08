"""Isolated own-PMM 8890 physical PIN and configured GSM900 registration."""
import argparse
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
from tools.make_sim_card_profile import make_profile
from tools.run_noki8xxx_supplementary import PROFILES, verify_inputs
from tools.sim_security_trace_check import validate
from tools.noki8890_staged_check import verify as verify_stage
from tools.noki8890_registration_check import verify as verify_registration


def check_pin_inputs(text, *, gsm900_measurements=True):
    position = 0
    for key, code in (('Keypad 1', '01'), ('Keypad 2', '02'),
                      ('Keypad 3', '03'), ('Keypad 4', '04'), ('Menu', '19')):
        match = re.search(r'8890_pin_physical: key=' + re.escape(key) +
                          r'.*?8890_keypad_decoded: key=' + code + r'\b', text[position:], re.S)
        if not match:
            raise ValueError('missing physical PIN decode: ' + key)
        position += match.end()
    if not re.search(r'SIM status ins=20 sw=9000.*?'
                     r'LAPDm Location Updating Accept acknowledged nr=1', text[position:], re.S):
        raise ValueError('registration did not follow physical PIN acceptance')
    if gsm900_measurements and not re.search(r'TX packet type=57 payload=4 .*data=01140000.*?'
                     r'RX enqueue type=8b payload=166 .*data=0010003c00c4.*?'
                     r'8890_band_rx: object=([0-9a-f]+).*?'
                     r'8890_band_parse: object=\1 arfcn=003c rssi=c4', text, re.S):
        raise ValueError('missing own late measurement response/consumer correlation')
    if not gsm900_measurements and not re.search(
            r'TX packet type=55 payload=4 .*data=04080000.*?'
            r'RX enqueue type=8b payload=166 .*data=0010025800c3025900b9.*?'
            r'8890_band_rx: object=([0-9a-f]+).*?'
            r'8890_band_parse: object=\1 arfcn=0258 rssi=c3', text, re.S):
        raise ValueError('missing own PCS measurement response/consumer correlation')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--without-pin', action='store_true')
    parser.add_argument('--pcs1900', action='store_true', help='independent PCS registration and host services')
    parser.add_argument('--scenario', choices=('registration', 'host-incoming-call', 'host-incoming-sms',
                                              'host-outgoing-call', 'host-outgoing-sms', 'phonebook',
                                              'idle-state', 'call-state', 'sms-state'), default='registration')
    parser.add_argument('--port', type=int, default=18890)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    profile = PROFILES['8890']
    try:
        if args.pcs1900 and args.scenario not in ('registration', 'host-incoming-call',
                'host-outgoing-call', 'host-incoming-sms', 'host-outgoing-sms',
                'idle-state', 'call-state', 'sms-state'):
            raise ValueError('PCS downstream scenarios require separate acceptance coverage')
        roms = root / 'roms/noki8890'
        verify_inputs('8890', (roms / profile[1]).read_bytes(), (roms / profile[3]).read_bytes())
        run.mkdir(parents=True, exist_ok=False)
        for directory in ('cfg', 'nvram/nsb6hle', 'snap', 'sta'):
            (run / directory).mkdir(parents=True)
        shutil.copyfile(root / ('fixtures/noki8890_pcs1900/nsb6hle.cfg' if args.pcs1900 else
                               'fixtures/noki8890_host/nsb6hle.cfg'), run / 'cfg/nsb6hle.cfg')
        if args.pcs1900 and args.scenario.startswith('host-'):
            configuration = ET.parse(run / 'cfg/nsb6hle.cfg')
            ET.SubElement(configuration.find('./system/input'), 'port', tag=':CALLHOST',
                          type='CONFIG', mask='1', defvalue='0', value='1')
            configuration.write(run / 'cfg/nsb6hle.cfg', encoding='utf-8', xml_declaration=True)
        state = args.scenario.endswith('-state')
        if state:
            configuration = ET.parse(run / 'cfg/nsb6hle.cfg')
            inputs = configuration.find('./system/input')
            host_port = inputs.find("port[@tag=':CALLHOST']")
            if host_port is not None:
                host_port.set('value', '0')
            if args.scenario == 'sms-state':
                ET.SubElement(inputs, 'port', tag=':NETCFG', type='CONFIG',
                              mask='4', defvalue='0', value='4')
            configuration.write(run / 'cfg/nsb6hle.cfg', encoding='utf-8', xml_declaration=True)
        card = run / 'nvram/nsb6hle/sim_card'
        card.write_bytes(make_profile(pin_enabled=not args.without_pin))
        environment = os.environ.copy()
        environment.pop('NOKIA_DCT3_8890_PIN_ENTRY', None)
        if not args.without_pin:
            environment['NOKIA_DCT3_8890_PIN_ENTRY'] = '1'
        script = {'registration': 'security_input', 'host-incoming-call': 'clock_incoming_input',
                  'host-incoming-sms': 'incoming_sms_input', 'host-outgoing-call': 'clock_call_input',
                  'host-outgoing-sms': 'outgoing_sms_input', 'phonebook': 'phonebook_input',
                  'idle-state': 'state_idle', 'call-state': 'state_call', 'sms-state': 'state_sms'}[args.scenario]
        if args.pcs1900 and args.scenario == 'sms-state':
            script = 'pcs_state_sms'
        command = [str((args.mame or root / 'mame/mame').resolve()), 'nsb6hle',
                   '-rompath', str(root / 'roms'), '-nvram_directory', 'nvram',
                   '-cfg_directory', 'cfg', '-snapshot_directory', 'snap', '-noreadconfig',
                   '-state_directory', 'sta',
                   '-debug', '-debugger', 'none', '-verbose', '-log', '-video', 'none',
                   '-sound', 'none', '-nothrottle', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / f'tools/noki8890_{script}.lua'),
                   '-seconds_to_run', '80' if state else '74' if args.scenario.endswith('-call') else
                       '60' if args.scenario.endswith('-sms') else '46']
        host_command = None
        if args.scenario == 'host-incoming-call':
            command += ['-http', '-http_port', str(args.port)]
            host_command = [sys.executable, str(root / 'tools/run_host_incoming_signaling_gate.py'),
                            '--port', str(args.port), '--cwd', str(run), '--caller', '447700900123',
                            '--ready-file', str(run / 'snap/8890_date_after.png'), '--'] + command
        elif args.scenario == 'host-incoming-sms':
            command += ['-http', '-http_port', str(args.port)]
            host_command = [sys.executable, str(root / 'tools/run_host_incoming_sms_gate.py'),
                            '--port', str(args.port), '--cwd', str(run), '--'] + command
        elif args.scenario.startswith('host-outgoing-'):
            command += ['-http', '-http_port', str(args.port)]
            call = args.scenario == 'host-outgoing-call'
            runner = 'run_host_call_adapter_gate' if call else 'run_host_sms_gate'
            options = ['--number', '1234567', '--decision', 'connect'] if call else [
                '--user-data', '41', '--user-data-length', '1']
            host_command = [sys.executable, str(root / f'tools/{runner}.py'),
                            '--port', str(args.port), '--cwd', str(run)] + options + ['--'] + command
        with (run / 'console.log').open('w') as console:
            subprocess.run(host_command or command, cwd=run, env=environment, stdout=console,
                           stderr=subprocess.STDOUT, check=True, timeout=180)
        text = (run / 'error.log').read_text(errors='replace')
        if not args.without_pin:
            validate(text, card.read_bytes(), 'verify', '1234')
            check_pin_inputs(text, gsm900_measurements=not args.pcs1900)
        verify_stage(text, runtime=True, selftest=True)
        verify_registration(text, configured_gsm900=not args.pcs1900, pcs1900=args.pcs1900)
        if state:
            from tools.noki8890_state_check import verify, check_frames
            call = args.scenario == 'call-state'
            sms = args.scenario == 'sms-state'
            before_save, boundary, _ = text.partition('8890_state: event=saved')
            if not boundary:
                raise ValueError('restoration prerequisites lack a save boundary')
            verify_registration(before_save, configured_gsm900=not args.pcs1900, pcs1900=args.pcs1900)
            if not args.without_pin:
                validate(before_save, card.read_bytes(), 'verify', '1234')
                check_pin_inputs(before_save, gsm900_measurements=not args.pcs1900)
            if call:
                from tools.radio_outgoing_call_trace_check import CONNECT_ACKNOWLEDGE
                if not CONNECT_ACKNOWLEDGE.search(before_save):
                    raise ValueError('call was not connected before saving')
            if sms and ('sim_device: update fid=6f3c record=1 length=176' not in before_save or
                        'LAPDm service Channel Release acknowledged' not in before_save):
                raise ValueError('SMS was not stored and released before saving')
            verify(text, call=call, sms=sms, storage=card.read_bytes(),
                   configured_gsm900=not args.pcs1900, pcs1900=args.pcs1900)
            check_frames(run / 'snap', call=call, sms=sms)
        elif args.scenario == 'host-incoming-call':
            from tools.noki8890_incoming_call_check import verify, check_host_frames
            verify(text, caller='447700900123', configured_gsm900=not args.pcs1900, pcs1900=args.pcs1900)
            check_host_frames(run / 'snap', '447700900123')
        elif args.scenario == 'host-incoming-sms':
            subprocess.run([sys.executable, str(root / 'tools/noki8890_incoming_sms_check.py'),
                            str(run / 'error.log'), str(card),
                            str(run / 'snap/8890_sms_read_2.png')] +
                           (['--pcs1900'] if args.pcs1900 else []), check=True)
            subprocess.run([sys.executable, str(root / 'tools/radio_incoming_host_sms_trace_check.py'),
                            '--arfcn', '600' if args.pcs1900 else '60', str(run / 'error.log')], check=True)
        elif args.scenario == 'host-outgoing-call':
            from tools.noki8890_outgoing_call_check import verify
            from tools.noki8890_clock_check import verify as verify_clock, check_frames
            verify(text, configured_gsm900=not args.pcs1900, pcs1900=args.pcs1900)
            verify_clock(text)
            check_frames(run / 'snap', call=True)
        elif args.scenario == 'host-outgoing-sms':
            subprocess.run([sys.executable, str(root / 'tools/noki8890_outgoing_sms_check.py'),
                            str(run / 'error.log')] + (['--pcs1900'] if args.pcs1900 else []), check=True)
            subprocess.run([sys.executable, str(root / 'tools/radio_outgoing_host_sms_trace_check.py'),
                            '--octets', '1', str(run / 'error.log')], check=True)
        elif args.scenario == 'phonebook':
            original_card = card.read_bytes()
            subprocess.run([sys.executable, str(root / 'tools/noki8890_phonebook_check.py'),
                            'save', str(run / 'error.log'), str(card),
                            str(run / 'snap/8890_phonebook_save.png')], check=True)
            shutil.copyfile(run / 'error.log', run / 'write.log')
            cold_command = command.copy()
            cold_command[cold_command.index('-autoboot_script') + 1] = str(
                root / 'tools/noki8890_phonebook_read.lua')
            with (run / 'cold_console.log').open('w') as console:
                subprocess.run(cold_command, cwd=run, env=environment, stdout=console,
                               stderr=subprocess.STDOUT, check=True, timeout=180)
            cold_text = (run / 'error.log').read_text(errors='replace')
            if card.read_bytes() != original_card:
                raise ValueError('cold readback changed persistent SIM bytes')
            if not args.without_pin:
                validate(cold_text, card.read_bytes(), 'verify', '1234')
                check_pin_inputs(cold_text)
            verify_stage(cold_text, runtime=True, selftest=True)
            verify_registration(cold_text, configured_gsm900=True, preserved_location=True)
            subprocess.run([sys.executable, str(root / 'tools/noki8890_phonebook_check.py'),
                            'readback', str(run / 'error.log'), str(card),
                            str(run / 'snap/8890_phonebook_contact.png')], check=True)
        (run / 'acceptance.json').write_text(json.dumps({
            'machine': 'nsb6hle', 'sim_pin_enabled': not args.without_pin,
            'provisioning': 'own acquired PMM unchanged', 'command': command,
            'native_dsp_complete': False, 'speech_tested': False, 'result': 'pass',
            'scenario': args.scenario, 'host_command': host_command,
            'band': 'PCS1900' if args.pcs1900 else 'GSM900',
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'8890 PIN registration FAIL: {error}\n')
    print(f'8890 PIN registration PASS: {run}')


if __name__ == '__main__':
    main()
