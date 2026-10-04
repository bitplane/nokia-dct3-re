import struct
import unittest

from tools.nsm3d_service_contract import inspect_record_reply


class ServiceContractTests(unittest.TestCase):
    def test_short_record_has_no_checksum_claim(self):
        result = inspect_record_reply(bytes.fromhex("3532") + bytes(50))
        self.assertFalse(result["checksum_present"])
        self.assertNotIn("checksum_matches", result)
        self.assertEqual(result["identity_verdict"], "not evaluated")

    def test_extended_checksum_is_big_endian_and_wraps(self):
        words = [0xffff, 0xffff, 0x1234] + [0] * 22
        expected = (sum(words) & 0xffff) ^ 0xffff
        payload = bytes.fromhex("3534") + struct.pack(">25H", *words)
        result = inspect_record_reply(payload + struct.pack(">H", expected))
        self.assertTrue(result["checksum_matches"])
        self.assertEqual(result["checksum_expected"], expected)
        self.assertFalse(inspect_record_reply(
            payload + struct.pack("<H", expected))["checksum_matches"])

    def test_corruption_does_not_become_a_success_verdict(self):
        payload = bytes.fromhex("3534") + bytes(50) + bytes.fromhex("fffe")
        result = inspect_record_reply(payload)
        self.assertFalse(result["checksum_matches"])
        self.assertEqual(result["identity_verdict"], "not evaluated")

    def test_rejects_wrong_primitive_size_and_truncated_records(self):
        for payload in (b"", b"\x35", bytes.fromhex("3632") + bytes(50),
                        bytes.fromhex("3533") + bytes(51),
                        bytes.fromhex("3534") + bytes(50),
                        bytes.fromhex("3532") + bytes(52)):
            with self.subTest(payload=payload.hex()), self.assertRaises(ValueError):
                inspect_record_reply(payload)


if __name__ == "__main__":
    unittest.main()
