"""Isolated fresh own-product NSM-5 SIM acceptance."""
import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_profile


@dataclass(frozen=True)
class Scenario:
    script: str
    checker_flag: str | None = None
    seconds: int = 45
    coherent_cell: bool = True
    net_masks: tuple[str, ...] = ()
    pin: bool = True
    host_sms: str | None = None
    host_outcome: str = 'rp_ack'
    host_call: str | None = None
    sms_mask: str | None = None
    preserved: bool = False
    cold_scenario: str | None = None


# The early and negative controls deliberately retain the default cell topology.
SCENARIOS = {
    'toolkit-interactive': Scenario('noki5210_toolkit_interactive_input.lua',
                                    seconds=65, coherent_cell=False, pin=False),
    'pin-registration': Scenario('noki5210_pin_input.lua', coherent_cell=False),
    'late-pin-boundary': Scenario('noki5210_pin_late_input.lua', '--late-boundary',
                                  coherent_cell=False),
    'late-pin-registration': Scenario('noki5210_pin_late_input.lua', '--coherent-late'),
    'pin-incoming-call': Scenario('noki5210_pin_incoming_call.lua', '--incoming-call',
                                  net_masks=('2', '32')),
    'pin-incoming-sms': Scenario('noki5210_pin_incoming_sms.lua', '--incoming-sms',
                                 net_masks=('4',)),
    'pin-idle-state': Scenario('noki5210_pin_idle_state.lua', '--idle-state'),
    'pin-call-state': Scenario('noki5210_pin_call_state.lua', '--call-state',
                               seconds=55, net_masks=('2', '32')),
    'pin-sms-state': Scenario('noki5210_pin_sms_state.lua', '--sms-state', net_masks=('4',)),
    'pin-outgoing-call': Scenario('noki5210_pin_outgoing_call.lua', '--outgoing-call',
                                  seconds=60, net_masks=('32',)),
    'pin-outgoing-sms': Scenario('noki5210_pin_outgoing_sms.lua', '--outgoing-sms', seconds=70),
    'pin-outgoing-call-state': Scenario('noki5210_pin_outgoing_call_state.lua',
                                        '--outgoing-call-state', seconds=60, net_masks=('32',)),
    'pin-host-outgoing-sms': Scenario('noki5210_pin_outgoing_sms.lua', '--outgoing-sms',
                                     seconds=70, host_sms='outgoing'),
    'pin-host-incoming-sms': Scenario('noki5210_pin_incoming_sms.lua', '--incoming-sms',
                                     host_sms='incoming'),
    'pin-host-rejected-sms': Scenario('noki5210_pin_sms_reject.lua', '--rejected-sms',
                                     seconds=80, host_sms='outgoing', host_outcome='rp_error'),
    'pin-host-silent-sms': Scenario('noki5210_pin_sms_silence.lua', '--silent-sms',
                                   seconds=155, host_sms='outgoing', host_outcome='rp_silence'),
    'pin-host-outgoing-call': Scenario('noki5210_pin_outgoing_call.lua', '--outgoing-call',
                                      seconds=60, net_masks=('32',), host_call='outgoing'),
    'pin-host-outgoing-call-state': Scenario('noki5210_pin_outgoing_call_state.lua',
                                            '--outgoing-call-state', seconds=60,
                                            net_masks=('32',), host_call='outgoing'),
    'pin-host-outgoing-media-state': Scenario('noki5210_pin_host_media_state.lua',
                                             '--outgoing-call-state', seconds=65,
                                             net_masks=('32',), host_call='outgoing'),
    'pin-host-incoming-call': Scenario('noki5210_pin_host_incoming_call.lua', '--host-incoming-call',
                                      seconds=60, net_masks=('32',), host_call='incoming'),
    'pin-host-incoming-call-state': Scenario('noki5210_pin_host_incoming_state.lua', '--call-state',
                                            seconds=60, net_masks=('32',), host_call='incoming'),
    'pin-host-incoming-media-state': Scenario('noki5210_pin_host_incoming_media_state.lua', '--call-state',
                                             seconds=65, net_masks=('32',), host_call='incoming'),
    'pin-sip-outgoing-call-state': Scenario('noki5210_pin_sip_outgoing_state.lua',
                                          seconds=60, net_masks=('32',), host_call='outgoing'),
    'pin-sip-incoming-call-state': Scenario('noki5210_pin_sip_incoming_state.lua', '--incoming',
                                          seconds=60, net_masks=('32',), host_call='incoming'),
    'pin-sms-wait-state': Scenario('noki5210_pin_sms_wait_state.lua', '--silent-sms-state',
                                  seconds=155, sms_mask='24'),
    'pin-phonebook': Scenario('noki5210_pin_phonebook.lua', '--phonebook-save', seconds=55),
    'pin-phone-memory': Scenario('noki5210_pin_phone_memory.lua', '--save', seconds=70,
                                 cold_scenario='pin-phone-memory-read'),
    'pin-phone-memory-read': Scenario('noki5210_pin_phone_memory_read.lua', '--read', seconds=55, preserved=True),
    'pin-phone-memory-edit': Scenario('noki5210_pin_phone_memory_edit.lua', '--edit', seconds=65, preserved=True,
                                      cold_scenario='pin-phone-memory-edit-read'),
    'pin-phone-memory-edit-read': Scenario('noki5210_pin_phone_memory_edit_read.lua', '--edit-read', seconds=55, preserved=True),
    'pin-phone-memory-rename': Scenario('noki5210_pin_phone_memory_rename.lua', '--rename', seconds=65,
        preserved=True, cold_scenario='pin-phone-memory-rename-read'),
    'pin-phone-memory-rename-read': Scenario('noki5210_pin_phone_memory_rename_read.lua', '--rename-read',
        seconds=55, preserved=True),
    'pin-phone-memory-add': Scenario('noki5210_pin_phone_memory_add.lua', '--add',
        seconds=70, preserved=True, cold_scenario='pin-phone-memory-multi-read'),
    'pin-phone-memory-multi-read': Scenario('noki5210_pin_phone_memory_multi_read.lua', '--multi-read',
        seconds=65, preserved=True),
    'pin-phone-memory-erase-first': Scenario('noki5210_pin_phone_memory_erase_first.lua', '--erase-first',
        seconds=65, preserved=True, cold_scenario='pin-phone-memory-read'),
    'pin-phone-memory-erase-cancel': Scenario('noki5210_pin_phone_memory_erase_cancel.lua', '--erase-cancel',
        seconds=65, preserved=True, cold_scenario='pin-phone-memory-read'),
    'pin-phone-memory-erase': Scenario('noki5210_pin_phone_memory_erase.lua', '--erase',
        seconds=65, preserved=True, cold_scenario='pin-phone-memory-empty-read'),
    'pin-phone-memory-empty-read': Scenario('noki5210_pin_phone_memory_empty_read.lua', '--empty-read',
        seconds=55, preserved=True),
    'pin-ussd': Scenario('noki5210_pin_ussd.lua', seconds=60),
    'pin-phonebook-read': Scenario('noki5210_pin_phonebook_read.lua', '--phonebook-read', preserved=True),
}


def file_identity(path):
    path = Path(path).resolve()
    result = {'path': str(path), 'present': path.is_file()}
    if result['present']:
        with path.open('rb') as source:
            result['sha256'] = hashlib.file_digest(source, 'sha256').hexdigest()
        result['bytes'] = path.stat().st_size
    return result


def storage_inventory(directory):
    directory = Path(directory)
    if not directory.is_dir():
        raise ValueError('preserved storage directory is absent')
    return {str(path.relative_to(directory)): file_identity(path)['sha256']
            for path in sorted(directory.rglob('*')) if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--port', type=int, default=18991)
    parser.add_argument('--pjsua', type=Path,
                        help='use the real loopback SIP bridge for a PIN host-call scenario')
    parser.add_argument('--sip-port', type=int, default=25100)
    parser.add_argument('--preserved-nvram', type=Path)
    parser.add_argument('--scenario', choices=SCENARIOS,
                        default='toolkit-interactive')
    parser.add_argument('--observe-readiness', action='store_true',
                        help='passively observe own context roots during the early-PIN control')
    args = parser.parse_args()
    scenario = SCENARIOS[args.scenario]
    if args.observe_readiness and args.scenario != 'pin-registration':
        parser.error('--observe-readiness requires pin-registration')
    if (args.preserved_nvram is not None) != scenario.preserved:
        parser.error('--preserved-nvram must match the selected storage-dependent scenario')
    root = Path(__file__).resolve().parents[1]
    if args.pjsua and not scenario.host_call:
        parser.error('--pjsua requires a PIN host-call scenario')
    sip_restore = args.scenario in ('pin-sip-outgoing-call-state', 'pin-sip-incoming-call-state')
    if sip_restore and not args.pjsua:
        parser.error('PIN SIP restoration requires --pjsua')
    if args.pjsua and args.scenario in ('pin-host-outgoing-call-state',
                                      'pin-host-outgoing-media-state', 'pin-host-incoming-call-state',
                                      'pin-host-incoming-media-state'):
        parser.error('PIN host-call restoration has no promoted SIP contract')
    run = args.run_directory.resolve()
    try:
        source_before = storage_inventory(args.preserved_nvram) if scenario.preserved else None
        run.mkdir(parents=True, exist_ok=False)
        for name in ('cfg', 'nvram', 'snap'):
            (run / name).mkdir()
        if scenario.coherent_cell:
            config = ET.Element('mameconfig', version='10')
            system = ET.SubElement(config, 'system', name='noki5210')
            ports = ET.SubElement(system, 'input')
            ET.SubElement(ports, 'port', tag=':NEIGHBORCFG', type='CONFIG',
                          mask='32', defvalue='0', value='32')
            for mask in scenario.net_masks:
                ET.SubElement(ports, 'port', tag=':NETCFG', type='CONFIG',
                              mask=mask, defvalue='0', value=mask)
            if scenario.host_sms or scenario.host_call:
                ET.SubElement(ports, 'port', tag=':CALLHOST', type='CONFIG',
                              mask='1', defvalue='0', value='1')
            if scenario.sms_mask:
                ET.SubElement(ports, 'port', tag=':SMSCFG', type='CONFIG',
                              mask='24', defvalue='0', value=scenario.sms_mask)
            ET.ElementTree(config).write(run / 'cfg/noki5210.cfg',
                                        encoding='utf-8', xml_declaration=True)
        if scenario.pin:
            card = run / 'nvram/noki5210_3/sim_card'
            if args.preserved_nvram:
                shutil.copytree(args.preserved_nvram.resolve(), run / 'nvram', dirs_exist_ok=True)
                if not card.is_file():
                    raise ValueError('own BIOS-local preserved SIM card absent')
            else:
                card.parent.mkdir()
                card.write_bytes(make_profile(pin_enabled=True))
        else:
            shutil.copyfile(root / 'fixtures/sim_toolkit_5210_interactive/noki5210.cfg',
                            run / 'cfg/noki5210.cfg')
        script = scenario.script
        if args.observe_readiness:
            script = 'noki5210_pin_early_observe.lua'
        checker = ('noki5210_pin_registration_check.py' if scenario.pin
                   else 'noki5210_toolkit_interactive_check.py')
        if sip_restore:
            checker = 'noki5210_pin_sip_restore_check.py'
        elif args.scenario == 'pin-ussd':
            checker = 'noki5210_pin_ussd_check.py'
        elif args.scenario.startswith('pin-phone-memory'):
            checker = 'noki5210_phone_memory_check.py'
        command = [str((args.mame or root / 'mame/mame').resolve()), 'noki5210',
                   '-bios', '540e', '-rompath', str(root / 'roms'), '-cfg_directory', 'cfg',
                   '-nvram_directory', 'nvram', '-snapshot_directory', 'snap', '-noreadconfig',
                   '-debug', '-debugger', 'none', '-verbose', '-log', '-video', 'none',
                   '-sound', 'none', '-nothrottle', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools' / script),
                   '-seconds_to_run', str(scenario.seconds)]
        if scenario.host_sms or scenario.host_call:
            command.extend(['-http', '-http_port', str(args.port)])
            if args.pjsua:
                # Real SIP timers run in wall time; retain normal handset pacing.
                command.remove('-nothrottle')
                launch = [sys.executable, str(root / 'tools/run_sip_handset_gate.py'),
                          '--pjsua', str(args.pjsua.resolve()), '--run-dir', str(run),
                          '--product', '5210', '--http-port', str(args.port),
                          '--sip-port', str(args.sip_port)]
                if scenario.host_call == 'incoming':
                    launch.extend(['--incoming', '--ready-file',
                                   str(run / 'snap/5210_pin_host_idle.png')])
                    if args.scenario == 'pin-sip-incoming-call-state':
                        launch.append('--restore-call')
                elif args.scenario == 'pin-sip-outgoing-call-state':
                    launch.append('--restore-outgoing')
                launch.extend(['--'] + command)
            else:
                launch = None
            host_script = ('run_host_incoming_call_gate.py' if args.scenario == 'pin-host-incoming-media-state' else
                           'run_host_incoming_signaling_gate.py' if scenario.host_call == 'incoming' else
                           'run_host_media_restore_gate.py' if args.scenario == 'pin-host-outgoing-media-state' else
                           'run_host_call_adapter_gate.py' if scenario.host_call == 'outgoing' else
                           'run_host_incoming_sms_gate.py' if scenario.host_sms == 'incoming'
                           else 'run_host_sms_gate.py')
            if not args.pjsua:
                launch = [sys.executable, str(root / 'tools' / host_script),
                          '--port', str(args.port), '--cwd', str(run)]
            if scenario.host_call == 'incoming' and not args.pjsua:
                launch.extend(['--caller', '5551234', '--ready-file',
                               str(run / 'snap/5210_pin_host_idle.png')])
                if args.scenario == 'pin-host-incoming-call-state':
                    launch.append('--observe-restore')
                elif args.scenario == 'pin-host-incoming-media-state':
                    launch.extend(['--require-restore', '--physical-end', '--frames', '80'])
                    launch.extend(['--restored-state-file', str(run / 'host_restored_state.json')])
            elif scenario.host_call == 'outgoing' and not args.pjsua:
                if args.scenario == 'pin-host-outgoing-media-state':
                    launch.extend(['--number', '5551234', '--physical-end'])
                else:
                    launch.extend(['--number', '5551234', '--decision', 'connect'])
                if args.scenario == 'pin-host-outgoing-call-state':
                    launch.append('--observe-restore')
            elif scenario.host_sms == 'outgoing':
                launch.extend(['--user-data', '61', '--user-data-length', '1',
                               '--decision', {'rp_ack': 'accept', 'rp_error': 'rp_error',
                                              'rp_silence': 'rp_silence'}[scenario.host_outcome]])
            if not args.pjsua:
                launch.extend(['--'] + command)
        else:
            launch = command
        # Capture initial storage before firmware can mutate it. Source HEAD is
        # not executable provenance; hash the binary actually being launched.
        inputs = [Path(command[0]), root / 'tools' / script,
                  Path(__file__).resolve(), root / 'tools' / checker]
        inputs.extend(root / 'roms/noki5210' / name for name in (
            '5210_5.40_ppm_e.fls', '5210 virgin eeprom 007f0000.fls',
            'mad2_mask_rom.bin', 'dsp_prom', 'dsp_drom', 'dsp_pdrom'))
        inputs.extend(sorted(path for directory in ('cfg', 'nvram')
                             for path in (run / directory).rglob('*') if path.is_file()))
        if args.pjsua:
            inputs.extend((args.pjsua, root / 'tools/run_sip_handset_gate.py'))
        (run / 'invocation.json').write_text(json.dumps({
            'schema': 1, 'scenario': args.scenario, 'machine': 'noki5210',
            'bios': '540e', 'cwd': str(run), 'command': launch,
            'inputs': [file_identity(path) for path in inputs],
            'scope': 'research HLE; not native DSP speech',
        }, indent=2) + '\n')
        with (run / 'console.log').open('w') as console:
            subprocess.run(launch, cwd=run, stdout=console, stderr=subprocess.STDOUT, check=True)
        subprocess.run([sys.executable, str(root / 'tools' / checker),
                        str(run)] + ([scenario.checker_flag] if scenario.checker_flag else []),
                       check=True)
        if scenario.host_sms:
            host_checker = ('radio_incoming_host_sms_trace_check.py'
                            if scenario.host_sms == 'incoming'
                            else 'radio_outgoing_host_sms_trace_check.py')
            host_args = (['--arfcn', '86'] if scenario.host_sms == 'incoming'
                         else ['--octets', '1', '--outcome', scenario.host_outcome])
            subprocess.run([sys.executable, str(root / 'tools' / host_checker)] +
                           host_args + [str(run / 'error.log')], check=True)
        if scenario.host_call == 'outgoing' and not args.pjsua:
            from tools.radio_host_outgoing_connect_check import verify, verify_restored
            text = (run / 'error.log').read_text(errors='replace')
            if args.scenario == 'pin-host-outgoing-media-state':
                from tools.radio_outgoing_host_media_restore_trace_check import check
                check(text, physical_end=True)
            else:
                verify_host_call = (verify_restored if args.scenario == 'pin-host-outgoing-call-state'
                                    else verify)
                verify_host_call(text, '5551234')
        if args.scenario == 'pin-host-incoming-call-state':
            from tools.radio_host_incoming_restore_check import verify as verify_incoming_restore
            verify_incoming_restore((run / 'error.log').read_text(errors='replace'))
        elif args.scenario == 'pin-host-incoming-media-state':
            from tools.radio_host_incoming_restore_check import verify_media
            verify_media((run / 'error.log').read_text(errors='replace'),
                         (run / 'host_restored_state.json').read_text())
        if args.scenario == 'pin-phonebook':
            subprocess.run([sys.executable, str(Path(__file__).resolve()), str(run / 'cold'),
                            '--scenario', 'pin-phonebook-read', '--preserved-nvram',
                            str(run / 'nvram'), '--mame', command[0]], check=True)
        elif scenario.cold_scenario:
            source = run / 'nvram'
            before = storage_inventory(source)
            subprocess.run([sys.executable, str(Path(__file__).resolve()), str(run / 'cold'),
                            '--scenario', scenario.cold_scenario, '--preserved-nvram',
                            str(source), '--mame', command[0]], check=True)
            if storage_inventory(source) != before:
                raise ValueError('cold phone-memory read changed its source NVRAM')
            (run / 'phone_memory_storage.json').write_text(json.dumps(before, indent=2) + '\n')
        if source_before is not None:
            if storage_inventory(args.preserved_nvram) != source_before:
                raise ValueError('storage-dependent run changed its source NVRAM')
            (run / 'source_storage.json').write_text(json.dumps(source_before, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'5210 isolated {args.scenario} FAIL: {error}\n')


if __name__ == '__main__':
    main()
