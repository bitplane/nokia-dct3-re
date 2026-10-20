"""Qualify own NSM-1 policy suppression of empty packet-window resets."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsm1_v523_static_check import SHA1, instruction, read32
from tools.nsm1_native_radio_check import samples, packets, PROGRAM_SHA256
from tools.nsm1_published_record_check import records
from tools.nsm1_pin_check import check_authenticated
from tools.nsm1_security_check import MENU
from tools.nsk1_phonebook_roundtrip import frame_check


def contract(image):
    if hashlib.sha1(image).hexdigest() != SHA1:
        raise ValueError('own NSM-1 image required')
    anchors = {
        0x2c1b96: ('ldrh', 'r0, [r4, #2]'),
        0x2c1b9a: ('strh', 'r0, [r4, #2]'),
        0x2c1cc2: ('ldrh', 'r0, [r4, #2]'),
        0x2c1cc6: ('bne', '#0x2c1cca'),
        0x2c1b8e: ('movs', 'r0, #1'),
        0x2c1b90: ('bl', '#0x2b4b72'),
        0x2b4b9e: ('cmp', 'r5, #1'),
        0x2b4ba2: ('movs', 'r0, #1'),
        0x2b4ba6: ('ldrb', 'r1, [r1]'),
        0x2b4ba8: ('tst', 'r1, r0'),
        0x2b4baa: ('beq', '#0x2b4bb0'),
        0x2b4bac: ('movs', 'r4, #0x68'),
        0x2b4bb0: ('movs', 'r4, #0'),
        0x2b4bc8: ('b', '#0x2b4c1e'),
        0x2b4c28: ('movs', 'r0, #0'),
        0x2b4c2a: ('strh', 'r0, [r1]'),
    }
    for address, expected in anchors.items():
        decoded = instruction(image, address)
        if (decoded.mnemonic, decoded.op_str) != expected:
            raise ValueError(f'own supervision instruction differs at {address:06x}')
    if (read32(image, 0x2c1cd8), read32(image, 0x2c1cdc), read32(image, 0x2b4f34)) != (0x112860, 0x9cd, 0x607):
        raise ValueError('own counter/timer/policy literals differ')


def check_trace(trace):
    windows = re.findall(r'nsm1_liveness_window: counter=([0-9a-f]+) context=([0-9a-f]+) cycles=([0-9a-f]+) observer_writes=0', trace)
    if [(a, b) for a, b, _ in windows] != [('0000', '00112860'), ('0001', '00112860'), ('0000', '00112860')]:
        raise ValueError('own packet window counts differ')
    entries = re.findall(r'nsm1_liveness_entry: argument=([0-9a-f]+) caller=([0-9a-f]+) cycles=([0-9a-f]+) observer_writes=0', trace)
    config = re.findall(r'nsm1_liveness_config: argument=([0-9a-f]+) bytes=([0-9a-f]+) cycles=([0-9a-f]+) observer_writes=0', trace)
    exits = re.findall(r'nsm1_liveness_exit: reason=([0-9a-f]+) cycles=([0-9a-f]+) observer_writes=0', trace)
    if [(a, b) for a, b, _ in entries] != [('00000001', '002c1b95')] * 2 or [(a, b) for a, b, _ in config] != [('00000001', '0002')] * 2 or [a for a, _ in exits] != ['00000000'] * 2:
        raise ValueError('empty-window policy result differs')
    for index, window in enumerate((windows[0], windows[2])):
        if not int(window[2], 16) < int(entries[index][2], 16) < int(config[index][2], 16) < int(exits[index][1], 16):
            raise ValueError('window/policy ordering differs')
    for label, expected in (('window', 3), ('entry', 2), ('config', 2), ('exit', 2), ('selected', 0)):
        if trace.count(f'nsm1_liveness_{label}:') != expected:
            raise ValueError('supervision trace census incomplete')
    if 'nsm1_native_long_restart:' in trace:
        raise ValueError('restart request observed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        run = args.run
        storage = run / 'nvram/nsm1r4t'
        contract((storage / 'flash').read_bytes())
        trace = (run / 'error.log').read_text()
        check_trace(trace)
        samples(trace); packets(trace); records(trace, True); check_authenticated(trace)
        horizon = re.findall(r'nsm1_native_snapshot: .* t=([0-9.]+)', trace)
        if not horizon or float(horizon[-1]) < 100:
            raise ValueError('100s observation horizon missing')
        if hashlib.sha256((run / 'nsm1_native_program_45s.bin').read_bytes()).hexdigest() != PROGRAM_SHA256:
            raise ValueError('own live program differs')
        if (run / 'initial_sim_card.bin').read_bytes() != (storage / 'sim_card').read_bytes():
            raise ValueError('card changed')
        frame_check(run / 'snap/nsm1_pin_menu.png', MENU)
    except (ValueError, OSError) as error:
        print(f'NSM-1 supervision failed: {error}', file=sys.stderr)
        return 1
    print('NSM-1 empty-window policy bit0-clear suppression PASS; no policy edited, RF unqualified')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
