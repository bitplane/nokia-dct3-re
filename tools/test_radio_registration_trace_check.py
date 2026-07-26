import unittest

from tools.radio_registration_trace_check import verify


GOOD = """
dsp_hle: TX packet type=0c payload=8 words=5 radio_phase=random_access data=0000096b00000000
dsp_hle: TX packet type=1b payload=25 words=14 radio_phase=contention_resolution data=0080013f4905087000f000fffe
dspif_transport: RX enqueue type=80 payload=34 producer=09c data=8012000049b50001000001734905087000f000fffe330809101010325476982b2b2b
radio_mm_parse: phase=return object=001017d0 payload=00101b10 result=00000048 mm=00/00/00/00
display_frontier: operator-resource data=00 f1 10 01 00
dspif_transport: RX enqueue type=86 payload=8 producer=09d data=8000000000000000
dsp_hle: LAPDm Location Updating Accept acknowledged nr=1
dsp_hle: TX packet type=1b payload=25 words=14 radio_phase=rr_channel_release data=00800321012b2b
dspif_transport: RX enqueue type=86 payload=8 producer=0b4 data=8000000000000000
dsp_hle: LAPDm Channel Release acknowledged nr=2
dsp_hle: TX packet type=1b payload=25 words=14 radio_phase=release_deconfigure data=00800341012b2b
sim_device: update-binary fid=6f7e offset=4 length=5
sim_device: update-binary fid=6f7e offset=10 length=1
dsp_hle: TX packet type=02 payload=20 words=11 radio_phase=release_channel_change data=041202000000001a600000010000000f00000000
dsp_hle: radio peer RX type=89 sequence=53
dspif_transport: RX enqueue type=80 payload=34 producer=0aa data=601200000005000100001506210001f02b2b
dspif_transport: RX enqueue type=80 payload=34 producer=0ba data=5012000000010001000049061b00
dspif_transport: RX enqueue type=80 payload=34 producer=0cc data=5012000000020001000059061a00
dspif_transport: RX enqueue type=80 payload=34 producer=0de data=5012000000030001000031061c00
dspif_transport: RX enqueue type=80 payload=34 producer=0f0 data=5012000000040001000019061d00
"""

NHM5_GOOD = GOOD.replace(
    "radio_mm_parse: phase=return object=001017d0 payload=00101b10 result=00000048 mm=00/00/00/00\n"
    "display_frontier: operator-resource data=00 f1 10 01 00",
    "dspif_transport: RX enqueue type=80 payload=34 producer=0c1 "
    "data=80120000052a00580000030045050200f1100001170809101010325476982b2b2b2b",
).replace(
    "data=041202000000001a600000010000000f00000000",
    "data=041202000000001a600000580000000f00000000",
)


class RegistrationTraceCheckTest(unittest.TestCase):
    def test_complete_registration(self):
        verify(GOOD)

    def test_complete_nhm5_registration(self):
        verify(NHM5_GOOD, "nhm5")

    def test_nhm5_rejects_nse8_channel_deconfiguration(self):
        with self.assertRaisesRegex(ValueError, "RR channel deconfiguration"):
            verify(NHM5_GOOD.replace(
                "data=041202000000001a600000580000000f00000000",
                "data=041202000000001a600000010000000f00000000",
            ), "nhm5")

    def test_rejects_retry(self):
        with self.assertRaisesRegex(ValueError, "expected one Location Updating Request"):
            verify(GOOD + "\n" + GOOD.splitlines()[2])

    def test_rejects_missing_steady_camp(self):
        with self.assertRaisesRegex(ValueError, "serving BCCH"):
            verify("\n".join(GOOD.splitlines()[:-3]))

    def test_rejects_operator_resource_before_acceptance(self):
        lines = GOOD.splitlines()
        operator = next(line for line in lines if "operator-resource" in line)
        without = [line for line in lines if "operator-resource" not in line]
        without.insert(1, operator)
        with self.assertRaisesRegex(ValueError, "operator presentation"):
            verify("\n".join(without))

    def test_rejects_unrelated_type80_as_ua(self):
        bad = GOOD.replace(
            "8012000049b50001000001734905087000f000fffe330809101010325476982b2b2b",
            "5012000049b50001000049061b00f110",
        )
        with self.assertRaisesRegex(ValueError, "contention-resolution UA"):
            verify(bad)

    def test_rejects_missing_loci_update(self):
        without = "\n".join(
            line for line in GOOD.splitlines() if "update-binary fid=6f7e" not in line)
        with self.assertRaisesRegex(ValueError, "EF_LOCI"):
            verify(without)

    def test_rejects_missing_location_update_acknowledgement(self):
        without = "\n".join(
            line for line in GOOD.splitlines()
            if "LAPDm Location Updating Accept acknowledged" not in line)
        with self.assertRaisesRegex(ValueError, "LAPDm acknowledgement"):
            verify(without)

    def test_rejects_wrong_receive_sequence(self):
        bad = GOOD.replace("data=0080032101", "data=0080030101")
        with self.assertRaisesRegex(ValueError, "SAPI-0 RR"):
            verify(bad)

    def test_rejects_missing_channel_release_acknowledgement(self):
        without = "\n".join(
            line for line in GOOD.splitlines()
            if "LAPDm Channel Release acknowledged" not in line)
        with self.assertRaisesRegex(ValueError, "Channel Release LAPDm acknowledgement"):
            verify(without)

    def test_rejects_wrong_channel_release_receive_sequence(self):
        bad = GOOD.replace("data=0080034101", "data=0080032101")
        with self.assertRaisesRegex(ValueError, r"N\(R\)=2"):
            verify(bad)


if __name__ == "__main__":
    unittest.main()
