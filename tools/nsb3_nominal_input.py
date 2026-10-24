"""Prepare one external ADC2 hypothesis; no EEPROM, RAM or SIM state supplied."""
import argparse
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

IMAGE_SHA256 = '927f94aed4ece8e84ad2eda28614bcede1272e89a2d046b16192f88499da19c2'


def prepare(image, output):
    if hashlib.sha256(image).hexdigest() != IMAGE_SHA256:
        raise ValueError('requires own NSB-3 MCU/PPM-A image')
    output.mkdir(parents=True, exist_ok=False)
    root = ET.Element('mameconfig', version='10')
    inputs = ET.SubElement(ET.SubElement(root, 'system', name='nsb3r4t'), 'input')
    ET.SubElement(inputs, 'port', tag=':RESEARCH_ADC2', type='CONFIG',
                  mask='1023', defvalue='1023', value='554')
    (output / 'cfg').mkdir()
    ET.ElementTree(root).write(output / 'cfg/nsb3r4t.cfg', encoding='utf-8', xml_declaration=True)
    (output / 'input.json').write_text(json.dumps({
        'adc2_raw': 554, 'basis': '6190 nominal 3.6 V / CCONT typical 6.5 mV per bit',
        'board_calibration_proved': False, 'copied_storage_or_guest_state': False,
        'image_sha256': hashlib.sha256(image).hexdigest()}, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    try:
        prepare(args.image.read_bytes(), args.output)
    except (OSError, ValueError) as exc:
        parser.exit(1, f'NSB-3 nominal input: {exc}\n')


if __name__ == '__main__':
    main()
