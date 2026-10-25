"""Compare fresh NSE-2 Phase-2 and Phase-2+ cards through physical contact save."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.run_nse2_native import prepare_native
from tools.process_runner import run_emulator
from tools.nse2_toolkit_check import check_trace, check_storage
from tools.nse2_sim_check import PHONEBOOK_SAVED_FRAME
from tools.sim_phonebook_check import validate_phonebook_storage


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--observe-nv', action='store_true', help='qualify own empty-field write buffers in both runs')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    binary = (args.mame or root / 'mame/mame').resolve()
    endpoint = []
    try:
        args.output.mkdir(parents=True, exist_ok=False)
        for profile in (0, 1):
            run = (args.output / ('proactive' if profile else 'control')).resolve()
            metadata = prepare_native(root, run, record_checksum=True,
                nominal_vbat=True, insert_sim=True, phone_retry_zero=True)
            config = run / 'cfg/nse2r4v.cfg'
            tree = ET.parse(config)
            ET.SubElement(tree.find('./system/input'), 'port', tag=':SATCFG',
                type='CONFIG', mask='15', defvalue='0', value=str(profile))
            tree.write(config, encoding='utf-8', xml_declaration=True)
            command = [str(binary), 'nse2r4v', '-rompath', str(run / 'roms'),
                '-nvram_directory', str(run / 'nvram'), '-cfg_directory', str(run / 'cfg'),
                '-video', 'none', '-sound', 'none', '-joystickprovider', 'none',
                '-midiprovider', 'none', '-nothrottle', '-noreadconfig', '-log', '-verbose',
                '-seconds_to_run', '31', '-autoboot_delay', '0', '-autoboot_script',
                str(root / 'tools' / ('nse2_nv_source.lua' if args.observe_nv else 'nse2_toolkit_fixture.lua'))]
            metadata.update(command=command, external_toolkit_profile=profile,
                mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest())
            (run / 'inputs.json').write_text(json.dumps(metadata, indent=2) + '\n')
            with (run / 'console.log').open('w') as output:
                rc = run_emulator(command, run, output)
            if rc:
                raise ValueError(f'MAME exit {rc}; evidence: {run}')
            console = (run / 'console.log').read_text()
            trace = (run / 'error.log').read_text()
            check_trace(console + trace, bool(profile))
            storage = run / 'nvram/nse2r4v'
            card = (storage / 'sim_card').read_bytes()
            validate_phonebook_storage(card, b'A')
            with Image.open(run / 'snap/nse2_phonebook_5.png') as image:
                if hashlib.sha256(image.convert('L').tobytes()).hexdigest() != PHONEBOOK_SAVED_FRAME:
                    raise ValueError('reviewed Saved frame differs')
            endpoint.append({name: (storage / name).read_bytes()
                             for name in ('eeprom', 'flash', 'sim_card')})
            if args.observe_nv:
                from tools.nse2_nv_source_check import check
                check(trace, (run / 'roms/nse2r4v/5110i_5.53a.fls').read_bytes(),
                      endpoint[-1]['eeprom'])
        check_storage(*endpoint)
    except (OSError, ValueError, ET.ParseError) as error:
        parser.exit(1, f'NSE-2 Toolkit refusal: FAIL: {error}\n')
    print('NSE-2 Toolkit refusal: PASS own_profile=03018000 result=30 physical_save=1 card_flash_equal=1 eeprom_delta=18 display_support=0')


if __name__ == '__main__':
    main()
