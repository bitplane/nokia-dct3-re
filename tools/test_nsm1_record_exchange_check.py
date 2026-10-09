import unittest

from tools import nsm1_record_exchange_check as check


class Nsm1RecordExchangeTests(unittest.TestCase):
    REQUEST = bytes.fromhex("ac9db72cdc5386529fa4ad4946dbdaf7c70f006dd16407d9")
    DECODED = bytes.fromhex("20673cc1760df2c72eec0000fbe1ef0608e402bdb4b80000")
    IDENTITY = "18021074340e0082d10917b88e61c1122e4e42f6"

    def transcript(self):
        request = "00021a701618" + self.REQUEST.hex()
        reply = "1802347435320000" + self.DECODED.hex() + self.REQUEST.hex()
        return "\n".join((
            "nsm1_record_request: bytes=0002067013049a1870dd caller=00000000",
            f"nsm1_restart_packet: bytes={self.IDENTITY} caller=00000000",
            f"nsm1_record_request: bytes={request} caller=00000000",
            f"nsm1_restart_packet: bytes={reply} caller=00000000"))

    def test_native_observed_record_and_private_marker(self):
        plain = check.decode_record(self.REQUEST[:12], bytes.fromhex("00160010"))
        self.assertEqual("20673cc1760df2c72eec24e8", plain.hex())

    def test_complete_exchange_is_arithmetic_not_acceptance(self):
        result = check.check(self.transcript(), minimum=1)
        self.assertEqual(["9a1870dd00160010a8a9aa27"], result["identity_completions"])
        self.assertEqual(["24e8", "d061"], result["record_completions"][0]["private_markers"])
        self.assertIn("no fitted-mask or EEPROM acceptance", result["scope"])

    def test_default_requires_both_cold_and_restart_exchanges(self):
        with self.assertRaisesRegex(ValueError, "incomplete"):
            check.check(self.transcript())
        self.assertEqual(2, len(check.check(self.transcript() + "\n" + self.transcript())
                                ["record_completions"]))

    def test_bad_native_transform_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "transform/echo"):
            check.check(self.transcript().replace("20673cc1", "21673cc1"), minimum=1)

    def test_wrong_flash_value_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "flash value"):
            check.check(self.transcript().replace("13049a1870dd", "13049a1870de"), minimum=1)

    def test_truncated_response_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "size/format/ordering"):
            check.check(self.transcript()[:-24], minimum=1)

    def test_missing_identity_is_rejected(self):
        text = "\n".join(self.transcript().splitlines()[2:])
        with self.assertRaisesRegex(ValueError, "lacks identity"):
            check.check(text, minimum=1)

    def test_incomplete_pending_exchange_is_rejected(self):
        text = "\n".join(self.transcript().splitlines()[:-1])
        with self.assertRaisesRegex(ValueError, "incomplete"):
            check.check(text, minimum=1)

    def test_unsupported_identity_family_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "family/ordering"):
            check.check(self.transcript().replace("340e0082", "340e0083"), minimum=1)

    def test_record_sizes_are_checked(self):
        with self.assertRaisesRegex(ValueError, "twelve/four"):
            check.decode_record(bytes(11), bytes(4))
        with self.assertRaisesRegex(ValueError, "twelve/four"):
            check.decode_record(bytes(12), bytes(3))


if __name__ == "__main__":
    unittest.main()
