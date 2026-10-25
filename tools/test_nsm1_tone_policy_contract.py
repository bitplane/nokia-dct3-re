import unittest
from unittest.mock import patch

from tools import nsm1_tone_policy_contract as checker


def image():
    data = bytearray(0x100000)
    fragments = {
        0x2a8736: '790b', 0x2a873c: '7b80', 0x2a8740: 'd103',
        0x2a8744: '0840', 0x2a8746: 'd200', 0x2a8748: 'e0ce',
        0x291fd2: '7942', 0x291fd4: '71ca', 0x2923c2: '2c07',
        0x292c10: '2801', 0x292c30: '2006', 0x2bd3b2: '55a8',
        0x296578: 'f029fdce', 0x291fb8: '49b7', 0x292298: '00111ee0',
        0x291fbc: '49e5', 0x292354: '0011fc00', 0x2923aa: '49dd',
        0x292720: '00111ee0', 0x2923b2: '49dc', 0x292724: '00000762',
        0x2923b4: '4ad9', 0x29271c: '00007ff0', 0x2bd268: '486d',
        0x2bd420: '002ca91c', 0x2bd2da: '494b', 0x2bd408: '002cb954',
        0x2bd39c: '4c1a', 0x2bd3a8: '4d1a', 0x2bd414: '0011fc00',
        0x2cbe7c: 'c0ff3f0062b90000',
        0x2cb400: '0000419400000001000000dc',
        0x2db038: '002dad6801000000',
        0x2a1e6e: '2039', 0x2a1e70: '0140', 0x2a1e72: 'f01bfcdd',
        0x280c20: 'f021f920', 0x2cc864: '8e3f4100ffff0000',
        0x2cc894: '8e3f4000ffff0000',
        0x2bb824: '2000', 0x2bb826: 'f007fd65', 0x280b60: 'f03afe5f',
        0x280b68: '49e7', 0x280f08: '00000312', 0x280b6c: 'db00',
        0x280b6e: 'e4d6', 0x28051e: '2139', 0x280520: '0109',
    }
    for address, raw in fragments.items():
        raw = bytes.fromhex(raw)
        data[address-0x200000:address-0x200000+len(raw)] = raw
    return data


class TonePolicyContractTest(unittest.TestCase):
    def test_sequence_is_data_driven_and_physical_meaning_is_unproved(self):
        with patch.object(checker, 'decode_group7', return_value=[
                {'id': 0x762, 'offset': 0x3f9c, 'size': 12}]):
            result = checker.contract(image())
        self.assertEqual(result['event'], 0x194)
        self.assertEqual(result['settings_variants'], 7)
        self.assertEqual(result['selector_event_arguments'], 0)
        self.assertFalse(result['physical_channel_identity_proved'])
        self.assertFalse(result['settings_field_semantics_proved'])

    def test_mutated_branch_sequence_channel_or_record_is_rejected(self):
        for address in (0x2a8740, 0x291fd4, 0x292c30, 0x2cb400,
                        0x2cb407, 0x2cbe80, 0x2db03c, 0x292724,
                        0x2a1e70, 0x2cc866, 0x280f0b, 0x280b6c):
            data = image()
            data[address-0x200000] ^= 1
            with self.subTest(address=hex(address)), patch.object(
                    checker, 'decode_group7', return_value=[
                        {'id': 0x762, 'offset': 0x3f9c, 'size': 12}]), self.assertRaises(ValueError):
                checker.contract(data)


if __name__ == '__main__':
    unittest.main()
