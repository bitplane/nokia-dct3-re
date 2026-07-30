import unittest

from tools.radio_smart_message_envelope_trace_check import verify


class SmartMessageEnvelopeTraceCheckTest(unittest.TestCase):
    def test_rejects_unknown_profile(self):
        with self.assertRaisesRegex(ValueError, "unsupported"):
            verify("", b"", "unknown")

    def test_missing_part_stays_silent(self):
        log = (
            "GSM service downlink kind=16 sapi=3 pd=09 message=01 t=10.0\n")
        card = bytearray([0xff] * 3524)
        offset = 50 * 32 + 11 + 9 + 16
        card[offset] = 0
        verify(log, bytes(card), "missing")


if __name__ == "__main__":
    unittest.main()
