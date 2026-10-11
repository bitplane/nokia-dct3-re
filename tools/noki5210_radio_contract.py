"""Own NSM-5 background-measurement receive contract, not DSP silicon proof."""
import argparse
import hashlib
import json
from pathlib import Path
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_BIG_ENDIAN


def verify(image):
    if hashlib.sha1(image).hexdigest() != 'f6c11cb013468c1d5e8550a2903f58ffa467a001':
        raise ValueError('requires acquired NSM-5 v5.40 PPM E')
    def read(address, size):
        return image[address - 0x200000:address - 0x200000 + size]
    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_BIG_ENDIAN)
    def instructions(address, size):
        return [(ins.mnemonic, ins.op_str) for ins in decoder.disasm(read(address, size), address)]
    if instructions(0x3de77a, 14) != [
            ('movs', 'r0, #0x80'), ('subs', 'r0, r1, r0'),
            ('cmp', 'r0, #0'), ('beq', '#0x3de834'),
            ('subs', 'r0, #3'), ('cmp', 'r0, #0xc'), ('bls', '#0x3de796')]:
        raise ValueError('own RX 83..8f table-index namespace differs')
    table = [int.from_bytes(read(0x3de7a0 + index * 4, 4), 'big') for index in range(13)]
    if table[8] != 0x3de7f4 or instructions(0x3de7f4, 8) != [
            ('adds', 'r0, r4, #0'), ('bl', '#0x37019c'), ('b', '#0x3de89c')]:
        raise ValueError('own RX 8b dispatch differs')
    if instructions(0x3701b0, 8) != [
            ('movs', 'r0, #0xc'), ('bl', '#0x2ed2e8'), ('pop', '{pc}')]:
        raise ValueError('own task-12 forwarding differs')
    if instructions(0x2e337a, 6) != [
            ('ldrb', 'r0, [r0]'), ('bl', '#0x2bd782')]:
        raise ValueError('own measurement completion consumer differs')
    if instructions(0x2bd7b8, 6) != [
            ('movs', 'r1, #0x27'), ('mvns', 'r6, r1'), ('str', 'r0, [sp]')]:
        raise ValueError('own forty-record bound differs')
    if instructions(0x2bd7c4, 4) != [('adds', 'r6, #1'), ('adds', 'r5, #4')]:
        raise ValueError('own four-byte measurement stride differs')
    if instructions(0x2bd7de, 18) != [
            ('ldrb', 'r0, [r5, #7]'), ('ldrb', 'r1, [r5, #6]'),
            ('lsls', 'r1, r1, #8'), ('adds', 'r0, r0, r1'),
            ('lsls', 'r0, r0, #0x10'), ('lsrs', 'r7, r0, #0x10'),
            ('ldrb', 'r1, [r5, #9]'), ('lsls', 'r0, r1, #0x18'),
            ('asrs', 'r0, r0, #0x18')]:
        raise ValueError('own big-endian carrier/signed-RSSI lanes differ')
    return {'product': '5210', 'type_8b_destination_task': 12,
            'records': 40, 'record_stride': 4,
            'carrier_message_offsets': [6, 7], 'signed_rssi_message_offset': 9,
            'scope': 'ROM-derived receive grammar; runtime request ownership checked separately'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.image.read_bytes()), indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f'5210 radio contract FAIL: {error}\n')


if __name__ == '__main__':
    main()
