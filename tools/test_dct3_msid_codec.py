import random
import unittest

from tools.dct3_msid_codec import decode_msid, encode_msid
from tools.make_5110_eeprom_profile import decode_msid as decode_native_82


class MsidCodecTests(unittest.TestCase):
    def test_native_rom4_vector_and_independent_byte_decoder(self):
        msid = bytes.fromhex("8264b000eb8f457e168bd2d32a")
        plain = bytes.fromhex("054b7d8900160010a8a9aa60")
        self.assertEqual(decode_msid(msid), plain)
        self.assertEqual(decode_msid(msid), b"".join(decode_native_82(msid)))
        self.assertEqual(encode_msid(plain, 0x82), msid)

    def test_published_service_capture_family_83(self):
        # Capture by mdmosheur, 2010: RAE-3 v4.13, not an 8250 fixture.
        # https://forum.gsmhosting.com/vbb/f550/9110-contact-service-951428/
        msid = bytes.fromhex("83473137e5156e62c245946a70")
        plain = bytes.fromhex("e7b24d8900000000acadab4c")
        self.assertEqual(decode_msid(msid), plain)
        self.assertEqual(encode_msid(plain, 0x83), msid)

    def test_explicit_families_round_trip_arbitrary_inputs(self):
        rng = random.Random(8250)
        for family in (0x82, 0x83):
            for _ in range(50):
                plain = bytes(rng.randrange(256) for _ in range(12))
                self.assertEqual(decode_msid(encode_msid(plain, family)), plain)

    def test_no_implicit_family_or_extent(self):
        for msid in (b"", bytes(12), bytes(14), b"\x81" + bytes(12)):
            with self.assertRaises(ValueError):
                decode_msid(msid)
        for plain, family in ((bytes(11), 0x83), (bytes(13), 0x83),
                              (bytes(12), 0x81), (bytes(12), 0x100)):
            with self.assertRaises(ValueError):
                encode_msid(plain, family)


if __name__ == "__main__":
    unittest.main()
