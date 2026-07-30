import unittest

from tools.radio_periodic_location_update_trace_check import verify


GOOD = """
dspif_transport: RX enqueue type=80 payload=34 data=5012000000010001000049061b000100f11000014000010000000000002b2b2b2b2b t=12.0
radio_peer: GSM service establish sapi=0 pd=05 message=08 length=18 data=05087000f000fffe33080910101032547698 t=16.5
radio_peer: LAPDm Location Updating Accept acknowledged nr=1 t=16.6
radio_peer: LAPDm Channel Release acknowledged nr=2 t=16.7
dsp_hle: TX packet type=0c payload=8 radio_phase=random_access data=0000096b00000000 t=402.9
radio_peer: GSM service establish sapi=0 pd=05 message=08 length=18 data=05087100f110000133080910101032547698 t=403.1
radio_peer: LAPDm Location Updating Accept acknowledged nr=1 t=403.2
radio_peer: LAPDm Channel Release acknowledged nr=2 t=403.3
dspif_transport: RX enqueue type=80 payload=34 data=601200000005000100001506210001f02b2b t=404.0
"""


class PeriodicLocationUpdateTraceCheckTest(unittest.TestCase):
    def test_complete_periodic_update(self):
        verify(GOOD)

    def test_rejects_disabled_timer(self):
        with self.assertRaisesRegex(ValueError, "T3212=1"):
            verify(GOOD.replace("0001400001", "0001400000"))

    def test_rejects_second_normal_update(self):
        with self.assertRaisesRegex(ValueError, "periodic update"):
            verify(GOOD.replace("05087100", "05087000"))

    def test_rejects_immediate_retry(self):
        with self.assertRaisesRegex(ValueError, "interval"):
            verify(GOOD.replace("t=403.1", "t=20.1"))

    def test_rejects_missing_second_release(self):
        marker = "radio_peer: LAPDm Channel Release acknowledged nr=2 t=403.3\n"
        with self.assertRaisesRegex(ValueError, "periodic RR channel"):
            verify(GOOD.replace(marker, ""))

    def test_rejects_third_update(self):
        with self.assertRaisesRegex(ValueError, "observed 3"):
            verify(GOOD + GOOD.splitlines()[6] + "\n")


if __name__ == "__main__":
    unittest.main()
