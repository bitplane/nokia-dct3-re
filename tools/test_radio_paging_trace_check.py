import unittest

from tools.radio_paging_trace_check import verify


GOOD = """
dsp_hle: TX packet type=1b payload=25 words=14 radio_phase=contention_resolution data=0080013f49050800000000000030080910101032547698
dsp_hle: LAPDm Channel Release acknowledged nr=2
dsp_hle: PCH no-identity fill channel=60 fn=3657
dspif_transport: RX enqueue type=80 payload=34 producer=0a4 data=601200000eaf00010000310621100809101010325476982b2b
dsp_hle: PCH IMSI page transmitted channel=60 fn=3759
dsp_hle: TX packet type=0c payload=8 words=5 radio_phase=random_access data=0000186c00ff0000
dspif_transport: RX enqueue type=80 payload=34 producer=08c data=6012000012bf000100002d063f002000011818af
dsp_hle: TX packet type=1b payload=25 words=14 radio_phase=contention_resolution data=0080013f4106270703331881080910101032547698
dspif_transport: RX enqueue type=80 payload=34 producer=0a8 data=8012000012c40001000001734106270703331881080910101032547698
dsp_hle: LAPDm Channel Release acknowledged nr=1
dspif_transport: RX enqueue type=80 payload=34 producer=085 data=6012000013dd000100001506210001f02b2b
"""


class PagingTraceCheckTest(unittest.TestCase):
    def test_complete_paging_lifecycle(self):
        verify(GOOD)

    def test_rejects_page_before_fill(self):
        lines = GOOD.splitlines()
        fill = next(line for line in lines if "PCH no-identity" in line)
        without = [line for line in lines if "PCH no-identity" not in line]
        without.insert(4, fill)
        with self.assertRaisesRegex(ValueError, "IMSI Paging Request"):
            verify("\n".join(without))

    def test_rejects_wrong_paged_identity(self):
        bad = GOOD.replace("0910101032547698", "0910101000000000", 1)
        with self.assertRaisesRegex(ValueError, "registered subscriber"):
            verify(bad)

    def test_rejects_missing_paging_response(self):
        without = "\n".join(
            line for line in GOOD.splitlines()
            if "data=0080013f410627" not in line)
        with self.assertRaisesRegex(ValueError, "Paging Response"):
            verify(without)

    def test_rejects_duplicate_page(self):
        duplicate = (
            GOOD + "\n"
            "dsp_hle: PCH IMSI page transmitted channel=60 fn=3861\n"
        )
        with self.assertRaisesRegex(ValueError, "exactly one IMSI page"):
            verify(duplicate)


if __name__ == "__main__":
    unittest.main()
