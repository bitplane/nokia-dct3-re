"""Check the erased-EEPROM/silent-DSP NSE-2 failure frame, not usable boot."""
import argparse
import hashlib
import json
from pathlib import Path
import re

from PIL import Image

FRAME = '7e3ade861af1e0e47c76100c7a7c7f8c7719c1c497e02d1024ab91c1e55c1f8e'
STATE = re.compile(
    r'nse2_state: t=([0-9.]+) pc=([0-9a-f]{8}) cpsr=([0-9a-f]{8}) '
    r'sp=([0-9a-f]{8}) shared_writes=(\d+) gensio_tx=(\d+) gensio_rx=(\d+) '
    r'lcd_commands=(\d+) lcd_bytes=(\d+) eeprom_signal_writes=(\d+) observer_writes=0')
SHARED = re.compile(r'nse2_shared: address=([0-9a-f]{8}) data=([0-9a-f]{4}) mask=ffff '
                    r'pc=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0')


def verify(text, pixels, size):
    if text.count('Soft reset\n') != 1:
        raise ValueError('NSE-2 missing initial reset or reset repeated')
    if re.search(r'RX enqueue|peer RAM W off=00[0246]\b|bootstrap.*complete', text):
        raise ValueError('NSE-2 silent-DSP boundary received a peer completion')
    expected = [(0x10000 + 2 * i, value, 0x27dca8)
                for i, value in enumerate((0xea, 0xff, 0xff, 0xea))]
    expected += [(0x10000, 0, 0x27817a), (0x10002, 0xffff, 0x278182),
                 (0x10004, 0xffff, 0x278184), (0x10006, 0xffff, 0x278188)]
    writes = SHARED.findall(text)
    if [(int(a, 16), int(v, 16), int(pc, 16)) for a, v, pc, _ in writes] != expected:
        raise ValueError('NSE-2 own shared-memory initialization differs')
    samples = STATE.findall(text)
    if len(samples) < 7 or float(samples[-1][0]) < 7:
        raise ValueError('NSE-2 passive state horizon missing')
    times = [float(sample[0]) for sample in samples]
    if times != sorted(set(times)):
        raise ValueError('NSE-2 state samples duplicate or regress')
    for sample in samples:
        if not (0x200040 <= int(sample[1], 16) < 0x390000
                and 0x100000 <= int(sample[3], 16) < 0x110000):
            raise ValueError('NSE-2 execution/stack outside bounded own-image window')
        if int(sample[4]) != 8:
            raise ValueError('NSE-2 shared-memory lifecycle changed after initialization')
    last = [int(value) for value in samples[-1][4:]]
    if last[1] < 100 or last[2] < 40 or last[3] < 8 or last[4] < 504 or last[5] < 100:
        raise ValueError('NSE-2 CCONT/LCD/EEPROM bus activity missing')
    if size != (84, 48) or hashlib.sha256(pixels).hexdigest() != FRAME:
        raise ValueError('NSE-2 reviewed CONTACT SERVICE frame differs')
    return dict(graphical_failure_frame=True, usable_boot=False, native_dsp=False,
                physical_key_delivery=False, samples=len(samples),
                gensio_tx=last[1], gensio_rx=last[2], lcd_bytes=last[4])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('frame', type=Path)
    args = parser.parse_args()
    try:
        with Image.open(args.frame) as frame:
            result = verify(args.log.read_text(errors='replace'), frame.convert('L').tobytes(), frame.size)
        print(json.dumps(result, indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSE-2 boundary FAIL: {error}\n')


if __name__ == '__main__':
    main()
