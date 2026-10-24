"""Prepare one external ADC2 hypothesis; no EEPROM, RAM or SIM state supplied."""
import argparse
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

IMAGE_SHA256 = '927f94aed4ece8e84ad2eda28614bcede1272e89a2d046b16192f88499da19c2'


def checksum_control(eeprom, include_security=False):
    if len(eeprom) != 0x2000:
        raise ValueError('requires own 8 KiB EEPROM')
    result = bytearray(eeprom)
    value = (sum(eeprom[0x120:0x1fe]) - eeprom[0x154] - eeprom[0x155]) & 0xffff
    result[0x1fe:0x200] = value.to_bytes(2, 'big')
    if include_security:
        result[0x11c:0x120] = (sum(eeprom[:0x11c]) & 0xffff).to_bytes(4, 'big')
    return bytes(result)


def prepare(image, output, eeprom=None, include_security=False):
    if hashlib.sha256(image).hexdigest() != IMAGE_SHA256:
        raise ValueError('requires own NSB-3 MCU/PPM-A image')
    if include_security and eeprom is None:
        raise ValueError('security checksum control requires own source EEPROM')
    if eeprom is not None and hashlib.sha256(eeprom).hexdigest() != '53584321c1f9e317e59cf67401fb5a28aa84fc4db32c67d264731825aa194d19':
        raise ValueError('requires qualified own native cold EEPROM')
    output.mkdir(parents=True, exist_ok=False)
    root = ET.Element('mameconfig', version='10')
    inputs = ET.SubElement(ET.SubElement(root, 'system', name='nsb3r4t'), 'input')
    ET.SubElement(inputs, 'port', tag=':RESEARCH_ADC2', type='CONFIG',
                  mask='1023', defvalue='1023', value='554')
    (output / 'cfg').mkdir()
    ET.ElementTree(root).write(output / 'cfg/nsb3r4t.cfg', encoding='utf-8', xml_declaration=True)
    metadata = {
        'adc2_raw': 554, 'basis': '6190 nominal 3.6 V / CCONT typical 6.5 mV per bit',
        'board_calibration_proved': False, 'copied_storage_or_guest_state': False,
        'image_sha256': hashlib.sha256(image).hexdigest()}
    if eeprom is not None:
        repaired = checksum_control(eeprom, include_security)
        target = output / 'nvram/nsb3r4t'
        target.mkdir(parents=True)
        (target / 'eeprom').write_bytes(repaired)
        (output / 'initial_eeprom.bin').write_bytes(repaired)
        metadata.update({'own_cold_eeprom_checksum_control': True,
                         'security_checksum_included': include_security,
                         'copied_storage_or_guest_state': True,
                         'copied_storage_scope': 'qualified own EEPROM only; no guest RAM/save state',
                         'source_eeprom_sha256': hashlib.sha256(eeprom).hexdigest(),
                         'initial_eeprom_sha256': hashlib.sha256(repaired).hexdigest(),
                         'changed_offsets': [hex(i) for i, (a, b) in enumerate(zip(eeprom, repaired)) if a != b]})
    (output / 'input.json').write_text(json.dumps(metadata, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--checksum-control-from', type=Path, help='qualified own native cold run; repairs only 1fe/1ff')
    parser.add_argument('--all-decoded-checksums', action='store_true', help='also repair independently decoded 11c..11f sum')
    args = parser.parse_args()
    try:
        image = args.image.read_bytes()
        eeprom = None
        if args.all_decoded_checksums and not args.checksum_control_from:
            raise ValueError('decoded-checksum control requires own qualified source run')
        if args.checksum_control_from:
            storage = args.checksum_control_from / 'nvram/nsb3r4t'
            if (storage / 'flash').read_bytes() != image:
                raise ValueError('own source-run executable differs')
            eeprom = (storage / 'eeprom').read_bytes()
        prepare(image, args.output, eeprom, args.all_decoded_checksums)
    except (OSError, ValueError) as exc:
        parser.exit(1, f'NSB-3 nominal input: {exc}\n')


if __name__ == '__main__':
    main()
