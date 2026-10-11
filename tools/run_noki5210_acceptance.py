"""Isolated fresh own-product NSM-5 SIM acceptance."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_profile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--scenario', choices=('toolkit-interactive', 'pin-registration',
                                             'late-pin-boundary', 'late-pin-registration',
                                             'pin-incoming-call', 'pin-incoming-sms'),
                        default='toolkit-interactive')
    parser.add_argument('--observe-readiness', action='store_true',
                        help='passively observe own context roots during the early-PIN control')
    args = parser.parse_args()
    if args.observe_readiness and args.scenario != 'pin-registration':
        parser.error('--observe-readiness requires pin-registration')
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    try:
        run.mkdir(parents=True, exist_ok=False)
        for name in ('cfg', 'nvram', 'snap'):
            (run / name).mkdir()
        pin = args.scenario != 'toolkit-interactive'
        late = args.scenario in ('late-pin-boundary', 'late-pin-registration', 'pin-incoming-call', 'pin-incoming-sms')
        if args.scenario in ('late-pin-registration', 'pin-incoming-call', 'pin-incoming-sms'):
            config = ET.Element('mameconfig', version='10')
            system = ET.SubElement(config, 'system', name='noki5210')
            ports = ET.SubElement(system, 'input')
            ET.SubElement(ports, 'port', tag=':NEIGHBORCFG', type='CONFIG',
                          mask='32', defvalue='0', value='32')
            if args.scenario == 'pin-incoming-call':
                for mask in ('2', '32'):
                    ET.SubElement(ports, 'port', tag=':NETCFG', type='CONFIG',
                                  mask=mask, defvalue='0', value=mask)
            if args.scenario == 'pin-incoming-sms':
                ET.SubElement(ports, 'port', tag=':NETCFG', type='CONFIG',
                              mask='4', defvalue='0', value='4')
            ET.ElementTree(config).write(run / 'cfg/noki5210.cfg',
                                        encoding='utf-8', xml_declaration=True)
        if pin:
            card = run / 'nvram/noki5210_3/sim_card'
            card.parent.mkdir()
            card.write_bytes(make_profile(pin_enabled=True))
        else:
            shutil.copyfile(root / 'fixtures/sim_toolkit_5210_interactive/noki5210.cfg',
                            run / 'cfg/noki5210.cfg')
        script = ('noki5210_pin_late_input.lua' if late else
                  'noki5210_pin_input.lua' if pin else 'noki5210_toolkit_interactive_input.lua')
        if args.observe_readiness:
            script = 'noki5210_pin_early_observe.lua'
        if args.scenario == 'pin-incoming-call':
            script = 'noki5210_pin_incoming_call.lua'
        if args.scenario == 'pin-incoming-sms':
            script = 'noki5210_pin_incoming_sms.lua'
        checker = 'noki5210_pin_registration_check.py' if pin else 'noki5210_toolkit_interactive_check.py'
        command = [str((args.mame or root / 'mame/mame').resolve()), 'noki5210',
                   '-bios', '540e', '-rompath', str(root / 'roms'), '-cfg_directory', 'cfg',
                   '-nvram_directory', 'nvram', '-snapshot_directory', 'snap', '-noreadconfig',
                   '-debug', '-debugger', 'none', '-verbose', '-log', '-video', 'none',
                   '-sound', 'none', '-nothrottle', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools' / script),
                   '-seconds_to_run', '45' if pin else '65']
        with (run / 'console.log').open('w') as console:
            subprocess.run(command, cwd=run, stdout=console, stderr=subprocess.STDOUT, check=True)
        subprocess.run([sys.executable, str(root / 'tools' / checker),
                        str(run)] + (['--late-boundary'] if args.scenario == 'late-pin-boundary' else
                                     ['--coherent-late'] if args.scenario == 'late-pin-registration' else
                                     ['--incoming-call'] if args.scenario == 'pin-incoming-call' else
                                     ['--incoming-sms'] if args.scenario == 'pin-incoming-sms' else []), check=True)
    except (OSError, subprocess.SubprocessError) as error:
        parser.exit(1, f'5210 isolated {args.scenario} FAIL: {error}\n')


if __name__ == '__main__':
    main()
