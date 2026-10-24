"""Static NSB-3 type80/selector60 forwarding and040b consumer contract."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsb3_activation_census import callers


def forwards_selector60(flags, byte6, exclusive, gate, reference, previous, state, mode):
    """Own branch arithmetic only; does not validate or synthesize a packet."""
    if exclusive == 1 or flags & (1 << 22) or byte6 == 1:
        return False
    mismatch = not flags & (1 << 31) and reference != previous
    if mismatch and gate & 1 and state == 2:
        return False
    return not flags & (1 << 29) or mode == 0x1a


def selector60_block_result(first_byte):
    length = first_byte >> 2
    return 0x61a if first_byte & 3 == 1 and 1 <= length <= 22 else 0x61b


def inspect(image):
    if hashlib.sha256(image).hexdigest() != IMAGE_SHA256:
        raise ValueError('requires own qualified NSB-3 image')
    fragments = (
        (0x28a0c4, '20875d002801d104464079002840d000e189'),
        (0x28a260, '6fe82200464179892901d10022010d8107c90fc943112900d13049b4780b21000802d2054ae9681268a742bad00021014219d00349e678092902d01f0f80d3027a30281ad11a48ea4641'),
        (0x24cea0, '49c41a402800d100e0a4'),
        (0x24cff4, '4d701c38f7fffdb870a82800d1091c381c21f7fffddb1c06'),
        (0x24cb78, '2940d0142950d0122960d01029a0d00e'),
        (0x24ce36, '7ba8088006000e0120ff302c55017ba80882d2050840d3032916dc012900d10148d6e008210f1949221620ff30151900f068fe6f48cf'),
    )
    for address, encoded in fragments:
        expected = bytes.fromhex(encoded)
        if image[address - 0x200000:address - 0x200000 + len(expected)] != expected:
            raise ValueError('own type80/040b instruction contract differs')
    expected_literals = {0x28a454: 0x1088c0, 0x28a458: 0x10893a,
                         0x28a45c: 0x108944, 0x28a54c: 0x10a6a4,
                         0x28a62c: 0x108754, 0x28a630: 0x108721,
                         0x28a650: 0x40b, 0x24d1b4: 0x40b,
                         0x24d1b0: 0x61b, 0x24d1a8: 0x61a}
    if any(struct.unpack_from('>I', image, address - 0x200000)[0] != value
           for address, value in expected_literals.items()):
        raise ValueError('own type80/040b literal ownership differs')
    return {'type80_decoder': '0x28a09c', 'selector60': '0x28a1b0',
            'exclusive_byte': '0x108947', 'flags_word': '0x1089c0',
            'gate_byte': '0x10a6a4', 'reference_word': '0x108754',
            'previous_word': '0x1088c8', 'state_byte': '0x108721',
            'mode_byte': '0x108942', 'forward_status': '0x040b',
            'forward_task': 14, 'consumer': '0x24ce74',
            'consumer040b': '0x24cff4', 'classifier': '0x24cb6c',
            'payload_parser': '0x24cbc0',
            'selector60_block': {'first_byte': 'message+0x0e',
                                 'low_bits': 1, 'length_range': [1, 22],
                                 'fixed_copy_bytes': 22, 'payload': 'message+0x0f',
                                 'accepted_result': '0x061a', 'rejected_result': '0x061b'},
            'consumer_callers': callers(image[:0xd6d00], 0x24ce74),
            'scope': 'static own selector60 forwarding/consumer paths, not native RF payload validity or observed type80 delivery'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect(args.image.read_bytes()), indent=2))
    except (OSError, ValueError) as exc:
        parser.exit(1, f'NSB-3 RX80 contract: FAIL: {exc}\n')


if __name__ == '__main__':
    main()
