import unittest

from tools.radio_incoming_ringing_trace_check import verify


GOOD = """
dsp_hle: PCH IMSI page transmitted channel=60 fn=3759
buzzer: enabled=1 divider=4930 frequency=2636 volume=21
dsp_hle: GSM service uplink sapi=0 pd=03 message=08 length=5
dsp_hle: GSM service uplink sapi=0 pd=03 message=01 length=2
dsp_hle: GSM service uplink sapi=0 pd=06 message=29 length=3
buzzer: enabled=0 divider=0 frequency=0 volume=21
dsp_hle: GSM service uplink sapi=0 pd=03 message=07 length=2
"""


class IncomingRingingTraceCheckTest(unittest.TestCase):
    def test_complete_ringing_lifecycle(self):
        verify(GOOD)

    def test_rejects_zero_volume(self):
        with self.assertRaisesRegex(ValueError, "enable"):
            verify(GOOD.replace("volume=21", "volume=0", 1))

    def test_rejects_stop_without_connect(self):
        with self.assertRaisesRegex(ValueError, "Connect"):
            verify(GOOD.replace(
                "dsp_hle: GSM service uplink sapi=0 pd=03 message=07 length=2",
                ""))


if __name__ == "__main__":
    unittest.main()
