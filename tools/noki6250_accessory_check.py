"""Check NHM-3's nominal unattached input and reviewed idle indication area."""
import argparse
from hashlib import sha256
from pathlib import Path
import re

from PIL import Image


def verify(text, pixels, size):
    decisions = re.findall(r'6250_accessory_decision: state=(\w+) sample=(\w+)', text)
    if not decisions or any(state != '0f' or sample != '03ff' for state, sample in decisions):
        raise ValueError('missing unattached high-input accessory decision')
    endpoints = re.findall(r'6250_accessory_endpoint: state=(\w+) sample=(\w+) t=([0-9.]+)', text)
    if not any(state == '0f' and sample == '03ff' and float(time) >= 20
               for state, sample, time in endpoints):
        raise ValueError('missing settled unattached accessory endpoint')
    if size != (96, 60):
        raise ValueError('unexpected NHM-3 frame geometry')
    # The idle accessory-label area; excludes operator, bars and softkeys.
    frame = Image.frombytes('L', size, pixels)
    if sha256(frame.crop((15, 24, 81, 40)).tobytes()).hexdigest() != (
            'cfb8375808408fa803db5b09ab90086eb9fab195ae5b14a2084b1c66f9e567d2'):
        raise ValueError('accessory label remained in reviewed idle area')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('frame', type=Path)
    args = parser.parse_args()
    try:
        with Image.open(args.frame) as source:
            frame = source.convert('L')
            verify(args.log.read_text(errors='replace'), frame.tobytes(), frame.size)
    except (OSError, ValueError) as error:
        parser.exit(1, f'6250 accessory FAIL: {error}\n')
    print('6250 nominal unattached accessory input PASS; ADC electrical scale unmeasured')
