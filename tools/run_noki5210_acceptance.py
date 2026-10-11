"""Isolated fresh own-product NSM-5 SIM acceptance."""
import argparse
from dataclasses import dataclass
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
    host_sms: bool = False


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
                                     seconds=70, host_sms=True),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--port', type=int, default=18991)
    parser.add_argument('--scenario', choices=SCENARIOS,
                        default='toolkit-interactive')
    parser.add_argument('--observe-readiness', action='store_true',
                        help='passively observe own context roots during the early-PIN control')
    args = parser.parse_args()
    if args.observe_readiness and args.scenario != 'pin-registration':
        parser.error('--observe-readiness requires pin-registration')
    root = Path(__file__).resolve().parents[1]
    scenario = SCENARIOS[args.scenario]
    run = args.run_directory.resolve()
    try:
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
            if scenario.host_sms:
                ET.SubElement(ports, 'port', tag=':CALLHOST', type='CONFIG',
                              mask='1', defvalue='0', value='1')
            ET.ElementTree(config).write(run / 'cfg/noki5210.cfg',
                                        encoding='utf-8', xml_declaration=True)
        if scenario.pin:
            card = run / 'nvram/noki5210_3/sim_card'
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
        command = [str((args.mame or root / 'mame/mame').resolve()), 'noki5210',
                   '-bios', '540e', '-rompath', str(root / 'roms'), '-cfg_directory', 'cfg',
                   '-nvram_directory', 'nvram', '-snapshot_directory', 'snap', '-noreadconfig',
                   '-debug', '-debugger', 'none', '-verbose', '-log', '-video', 'none',
                   '-sound', 'none', '-nothrottle', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools' / script),
                   '-seconds_to_run', str(scenario.seconds)]
        if scenario.host_sms:
            command.extend(['-http', '-http_port', str(args.port)])
            launch = [sys.executable, str(root / 'tools/run_host_sms_gate.py'),
                      '--port', str(args.port), '--cwd', str(run),
                      '--user-data', '61', '--user-data-length', '1', '--decision', 'accept',
                      '--'] + command
        else:
            launch = command
        with (run / 'console.log').open('w') as console:
            subprocess.run(launch, cwd=run, stdout=console, stderr=subprocess.STDOUT, check=True)
        subprocess.run([sys.executable, str(root / 'tools' / checker),
                        str(run)] + ([scenario.checker_flag] if scenario.checker_flag else []),
                       check=True)
        if scenario.host_sms:
            subprocess.run([sys.executable, str(root / 'tools/radio_outgoing_host_sms_trace_check.py'),
                            '--octets', '1', '--outcome', 'rp_ack', str(run / 'error.log')],
                           check=True)
    except (OSError, subprocess.SubprocessError) as error:
        parser.exit(1, f'5210 isolated {args.scenario} FAIL: {error}\n')


if __name__ == '__main__':
    main()
