"""Isolated own-ROM 8850/8890 laboratory supplementary-service acceptance."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

PROFILES = {
    '8850': ('nsm2hle', '8850v531.fls', '9f966787403b68a09530680ad911302403eb1521',
             '8850 virgin eeprom 003d0000.fls', 'b09455302d98fbedf35072c9ecfd7721a04924b0',
             0x33f504, 43, 36),
    '8890': ('nsb6hle', '8890_12.20_ppmc.fls', 'a214a0d69760ecd8eeca0b9d82f95c94bdfe70ed',
             '8890 virgin eeprom 003d0000.fls', 'cc0924cfd4c0ce796fca157c640fc3183c2b5f2c',
             0x339f4c, 64, 56),
}
KEY_TABLE = bytes.fromhex('3e3e3e3e3e11190102030e170405060f18070809101a0c0a0b')


def verify_inputs(product, mcu, pmm):
    profile = PROFILES[product]
    if hashlib.sha1(mcu).hexdigest() != profile[2]:
        raise ValueError('unexpected own-product MCU/PPM image')
    if hashlib.sha1(pmm).hexdigest() != profile[4]:
        raise ValueError('unexpected own-product acquired PMM image')
    offset = profile[5] - 0x200000
    if mcu[offset:offset + len(KEY_TABLE)] != KEY_TABLE:
        raise ValueError('own-product keypad table differs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('product', choices=PROFILES)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--service', choices=('ussd', 'divert', 'toolkit', 'toolkit-busy'), required=True)
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    profile = PROFILES[args.product]
    machine = profile[0]
    try:
        if args.service == 'toolkit' and args.product != '8850':
            raise ValueError('Toolkit acceptance is not yet established for this product')
        if args.service == 'toolkit-busy' and args.product != '8890':
            raise ValueError('Toolkit screen-busy acceptance is only established for 8890')
        roms = root / f'roms/noki{args.product}'
        verify_inputs(args.product, (roms / profile[1]).read_bytes(),
                      (roms / profile[3]).read_bytes())
        run.mkdir(parents=True, exist_ok=False)
        for directory in ('cfg', 'nvram', 'snap'):
            (run / directory).mkdir()
        if args.service in ('toolkit', 'toolkit-busy'):
            config = ET.Element('mameconfig', version='10')
            inputs = ET.SubElement(ET.SubElement(config, 'system', name=machine), 'input')
            ET.SubElement(inputs, 'port', tag=':SATCFG', type='CONFIG',
                          mask='15', defvalue='0', value='1')
            ET.ElementTree(config).write(run / f'cfg/{machine}.cfg')
        fixture = args.service.replace('-', '_')
        duration = {'toolkit': 43, 'toolkit-busy': 45}.get(
            args.service, profile[6 if args.service == 'ussd' else 7])
        command = [str((args.mame or root / 'mame/mame').resolve()), machine,
                   '-rompath', str(root / 'roms'), '-nvram_directory', 'nvram',
                   '-cfg_directory', 'cfg', '-snapshot_directory', 'snap', '-noreadconfig',
                   '-debug', '-debugger', 'none', '-verbose', '-log', '-video', 'none',
                   '-sound', 'none', '-nothrottle', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / f'tools/noki{args.product}_{fixture}_input.lua'),
                   '-seconds_to_run', str(duration)]
        with (run / 'console.log').open('w') as console:
            subprocess.run(command, cwd=run, stdout=console, stderr=subprocess.STDOUT, check=True)
        if args.service == 'divert':
            checker = [sys.executable, str(root / 'tools/noki8xxx_divert_check.py'), args.product, str(run)]
        else:
            checker = [sys.executable, str(root / f'tools/noki{args.product}_{fixture}_check.py'), str(run)]
        subprocess.run(checker, check=True)
        (run / 'acceptance.json').write_text(json.dumps({
            'product': args.product, 'machine': machine, 'service': args.service,
            'mcu_sha1': profile[2], 'pmm_sha1': profile[4],
            'provisioning': 'own acquired PMM unchanged',
            'boundary': 'research HLE; native resident DSP and speech unproved',
            'command': command, 'checker': checker, 'result': 'pass',
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'{args.product} {args.service} acceptance FAIL: {error}\n')
    print(f'{args.product} {args.service} isolated acceptance PASS: {run}')


if __name__ == '__main__':
    main()
