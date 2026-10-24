"""Check the erased-storage NSM-4 MCU boundary; no graphical boot claim."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsm4_static_check import inspect

FLASH_SHA256 = '7bc92e6b3bdedbc26a28e134b74909f478f2c5953020ff4f8681a7dd99c0a2fb'
SAMPLE = re.compile(r'nsm4_boot: second=([1-5]) pc=(0033e2e[024]) lr=0033e2d9 '
                    r'catalogue0=003fae04 catalogue27=004019d8 read_callback=003e0499 '
                    r'program_callback=0015a6b1 dsp_control=01 shared0=0000 shared2=ffff observer_writes=0')


def check(source, trace, flash):
    inspect(source[:0x290000])
    samples = SAMPLE.findall(trace)
    if [int(second) for second, _ in samples] != [1, 2, 3, 4, 5]:
        raise ValueError('missing or changed NSM-4 read-only ownership-wait samples')
    if len(source) != 0x380000 or len(flash) != 0x400000 or flash[:len(source)] != source:
        raise ValueError('NSM-4 flash extent or acquired prefix changed')
    if hashlib.sha256(flash).hexdigest() != FLASH_SHA256:
        raise ValueError('NSM-4 firmware-created erased-tail result differs')
    if flash[0x390006:0x390009] != b'PMM' or flash[0x3e0006:0x3e000c] != b'EEPROM':
        raise ValueError('NSM-4 own storage headers missing')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefix', type=Path)
    parser.add_argument('run_dir', type=Path)
    args = parser.parse_args()
    try:
        check(args.prefix.read_bytes(), (args.run_dir / 'error.log').read_text(errors='replace'),
              (args.run_dir / 'nvram/noki8855/flash').read_bytes())
    except (OSError, ValueError) as exc:
        parser.exit(1, f'NSM-4 boot boundary: {exc}\n')
    print('NSM-4 boundary PASS: own storage headers and catalogue; DSP wait, not boot completion')


if __name__ == '__main__':
    main()
