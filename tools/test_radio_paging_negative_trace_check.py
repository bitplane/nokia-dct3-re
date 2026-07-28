import unittest

from tools.radio_paging_negative_trace_check import verify


PREFIX = "dsp_hle: LAPDm Channel Release acknowledged nr=2\n"
FILL = (
    "\ndspif_transport: RX enqueue type=80 payload=34 "
    "data=601200000a4d033700001506210001f0\n")
LOGS = {
    "wrong-group": (
        PREFIX
        + "dsp_hle: PCH off-group IMSI page not monitored channel=60 "
        "air_fn=2643 monitor_fn=2637"
        + FILL),
    "unmatched": (
        PREFIX
        + "dspif_transport: RX enqueue type=80 payload=34 "
        "data=601200000a4d0337000031062110080910101032547608"
        + FILL),
    "malformed": (
        PREFIX
        + "dspif_transport: RX enqueue type=80 payload=34 "
        "data=601200000a4d0337000031062110090910101032547698"
        + FILL),
}


class PagingNegativeTraceCheckTest(unittest.TestCase):
    def test_accepts_all_negative_profiles(self):
        for profile, text in LOGS.items():
            with self.subTest(profile=profile):
                verify(text, profile)

    def test_rejects_paging_response(self):
        with self.assertRaisesRegex(ValueError, "Paging Response"):
            verify(
                LOGS["unmatched"]
                + "TX packet type=1b data=0080013f410627\n",
                "unmatched")

    def test_rejects_subscriber_mutation(self):
        with self.assertRaisesRegex(ValueError, "EF_LOCI"):
            verify(
                LOGS["malformed"]
                + "sim_device: update-binary fid=6f7e offset=4\n",
                "malformed")

    def test_rejects_same_wrong_group_frame(self):
        with self.assertRaisesRegex(ValueError, "monitored DRX"):
            verify(
                LOGS["wrong-group"].replace("air_fn=2643", "air_fn=2637"),
                "wrong-group")


if __name__ == "__main__":
    unittest.main()
