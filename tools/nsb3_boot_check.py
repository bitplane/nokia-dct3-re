"""Qualify the bounded erased-storage NSB-3 MCU run, not an interactive boot."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_static_check import inspect

FLASH = '927f94aed4ece8e84ad2eda28614bcede1272e89a2d046b16192f88499da19c2'
EEPROM = '4dc2b8ea96b930813282e930ebb9cddfddb916809c8f17d972540d59dd004917'
SAMPLE = re.compile(r'nsb3_boot: second=([1-5]) pc=002a8ac[8c] lr=002a8a89 '
                    r'selector=0004 catalogue3=002b7168 catalogue4=002bd0a8 '
                    r'dsp_control=02 shared0=0000 shared2=ffff observer_writes=0')


def check_samples(trace):
    if SAMPLE.findall(trace) != ['1', '2', '3', '4', '5']:
        raise ValueError('NSB-3 initial idle/catalogue samples differ')


def check_storage(image, flash, eeprom):
    inspect(image[:0xd6d00])
    if len(image) != 0x100000 or flash != image or hashlib.sha256(flash).hexdigest() != FLASH:
        raise ValueError('NSB-3 own flash changed')
    if len(eeprom) != 0x2000 or hashlib.sha256(eeprom).hexdigest() != EEPROM:
        raise ValueError('NSB-3 firmware-created erased EEPROM result differs')


def check(image, flash, eeprom, trace):
    check_storage(image, flash, eeprom)
    check_samples(trace)


def check_startup_trace(trace):
    returns = re.findall(r'nsb3_startup: site=0028e016 r0=([0-9a-f]{8})', trace)
    if not 1 <= len(returns) <= 16 or set(returns) != {'00000000'}:
        raise ValueError('NSB-3 sampled idle permission returns differ')
    if 'nsb3_idle_state: latch=01 companion=00 ctrl=05 fiq=00 irq=00 fmask=e3 imask=de selector=0004 program0=f7bb' not in trace:
        raise ValueError('NSB-3 late idle state differs')


def check_startup(image, run):
    from PIL import Image
    check_startup_trace((run / 'error.log').read_text(errors='replace'))
    payload = (run / 'nsb3_uploaded_program.bin').read_bytes()
    if payload != image[0xc210c:0xc210c + 372]:
        raise ValueError('NSB-3 installed program is not its own identity-4 bootstrap')
    with Image.open(run / 'snap/nsb3_startup_5s.png') as frame:
        if frame.size != (84, 48) or hashlib.sha256(frame.convert('RGB').tobytes()).hexdigest() != 'ed09afeeadae383b77840654453b183a05e10b2cad5ea5757e8c803cbd6b674b':
            raise ValueError('NSB-3 CONTACT SERVICE frame differs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run_dir', type=Path)
    parser.add_argument('--startup', action='store_true', help='qualify passive startup dump and CONTACT SERVICE frame')
    args = parser.parse_args()
    try:
        storage = args.run_dir / 'nvram/noki6190'
        image, flash, eeprom = args.image.read_bytes(), (storage / 'flash').read_bytes(), (storage / 'eeprom').read_bytes()
        if args.startup:
            check_storage(image, flash, eeprom)
            check_startup(image, args.run_dir)
        else:
            check(image, flash, eeprom, (args.run_dir / 'error.log').read_text(errors='replace'))
    except (OSError, ValueError) as exc:
        parser.exit(1, f'NSB-3 MCU boundary: {exc}\n')
    print('NSB-3 MCU boundary PASS: own initialization; CONTACT SERVICE' if args.startup else
          'NSB-3 MCU boundary PASS: selector 4, own erased EEPROM initialization; UI/DSP completion unqualified')


if __name__ == '__main__':
    main()
