import unittest

from tools import nsm1_record_exchange_check as check


class Nsm1RecordExchangeTests(unittest.TestCase):
    REQUEST = bytes.fromhex("ac9db72cdc5386529fa4ad4946dbdaf7c70f006dd16407d9")
    DECODED = bytes.fromhex("20673cc1760df2c72eec0000fbe1ef0608e402bdb4b80000")
    IDENTITY = "18021074340e0082d10917b88e61c1122e4e42f6"

    def test_own_retained_transform_matches_cold_request(self):
        raw = bytes.fromhex("73654ae2a7cad1f10e8752b699232739bc9657ce4047f826")
        identity = bytes.fromhex("3000abb18f2632abcc51301a")
        self.assertEqual(self.REQUEST, check.retained_record_transform(raw, identity))
        self.assertEqual(raw, check.retained_record_transform(self.REQUEST, identity))

    def test_retained_transform_requires_complete_records(self):
        with self.assertRaisesRegex(ValueError, "twenty-four/twelve"):
            check.retained_record_transform(bytes(23), bytes(12))
        with self.assertRaisesRegex(ValueError, "twenty-four/twelve"):
            check.retained_record_transform(bytes(24), bytes(11))

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

    def validation_trace(self):
        return "\n".join(
            f"nsm1_record_validation: pc={pc:08x} r0={value:08x} "
            "r1=00112f50 r5=00000001 object=00102030"
            for pc, value in ((0x27E688, 0x32), (0x27E6B0, 0xEC),
                              (0x27E6EA, 0xEC), (0x27E796, 0xB8)))

    def test_observed_validation_rejection_is_classified(self):
        result = check.check(self.transcript() + "\n" + self.validation_trace(), minimum=1)
        self.assertEqual(1, result["validation_trace"]["rejections"])
        self.assertIn("not boot acceptance", result["validation_trace"]["scope"])

    def test_missing_validation_trace_is_explicit(self):
        result = check.check(self.transcript(), minimum=1)
        self.assertEqual({"observed": False}, result["validation_trace"])

    def test_partial_validation_trace_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "incomplete or changed"):
            check.check(self.transcript() + "\n" + self.validation_trace().splitlines()[0], minimum=1)

    def test_validation_byte_must_match_native_reply(self):
        trace = self.validation_trace().replace("r0=000000b8", "r0=000000b9")
        with self.assertRaisesRegex(ValueError, "inputs disagree"):
            check.check(self.transcript() + "\n" + trace, minimum=1)

    def test_validation_format_drift_is_not_missing_observation(self):
        with self.assertRaisesRegex(ValueError, "unrecognized"):
            check.check(self.transcript() + "\nnsm1_record_validation: changed format", minimum=1)

    def test_validation_path_change_is_not_silently_accepted(self):
        trace = self.validation_trace().replace("pc=0027e796", "pc=0027e75a")
        with self.assertRaisesRegex(ValueError, "path/object"):
            check.check(self.transcript() + "\n" + trace, minimum=1)

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
