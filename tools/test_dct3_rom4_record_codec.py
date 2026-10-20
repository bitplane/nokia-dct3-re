import random
import unittest

from tools.dct3_rom4_record_codec import decode_record,encode_record,retained_record_transform


class Rom4RecordCodecTest(unittest.TestCase):
    def test_own_nsk1_native_vectors_include_private_markers(self):
        chip = bytes.fromhex('00160010')
        for encoded,plain in (
                ('7f80'*6,'b2ade0d999085170cbb4b35b'),
                ('ff'*12,'5d3e6f9a3cf547fbb1f0f7db')):
            self.assertEqual(bytes.fromhex(plain),decode_record(bytes.fromhex(encoded),chip))
            self.assertEqual(bytes.fromhex(encoded),encode_record(bytes.fromhex(plain),chip))

    def test_roundtrips_and_retained_involution(self):
        rng = random.Random(5130)
        for _ in range(100):
            plain,chip,identity,pair = (rng.randbytes(size) for size in (12,4,12,24))
            self.assertEqual(plain,decode_record(encode_record(plain,chip),chip))
            self.assertEqual(pair,retained_record_transform(retained_record_transform(pair,identity),identity))

    def test_bounds(self):
        for function in (encode_record,decode_record):
            for size,chip_size in ((11,4),(13,4),(12,3),(12,5)):
                with self.assertRaises(ValueError):
                    function(bytes(size),bytes(chip_size))
