import unittest

from tools.radio_outgoing_call_trace_check import check, decode_called_digits


TRACE = """
GSM service establish sapi=0 pd=05 message=24 length=16 data=05247103331881080910101032547698
GSM service downlink kind=5 sapi=0 pd=05 message=21 length=2
GSM service uplink sapi=0 pd=03 message=05 length=15 data=03450401a05e0581551532f4150101
GSM service downlink kind=12 sapi=0 pd=03 message=02 length=2
GSM service downlink kind=15 sapi=0 pd=06 message=2e length=8
GSM service uplink sapi=0 pd=06 message=29 length=3 data=062900
GSM service downlink kind=13 sapi=0 pd=03 message=01 length=2
GSM service downlink kind=14 sapi=0 pd=03 message=07 length=2
GSM service uplink sapi=0 pd=03 message=0f length=2 data=030f
speech tick uplink=50 downlink=43 pcm=50 nonzero=0/43
GSM service uplink sapi=0 pd=03 message=25 length=5 data=032502e090
GSM service downlink kind=20 sapi=0 pd=03 message=2d length=2
GSM service uplink sapi=0 pd=03 message=2a length=2 data=032a
LAPDm service Channel Release acknowledged nr=4
speech stop control=040a uplink=237 downlink=230
PCH no-identity fill channel=60
"""


class OutgoingCallTraceTest(unittest.TestCase):
    def test_complete_trace(self):
        check(TRACE)

    def test_requires_one_assignment(self):
        with self.assertRaisesRegex(ValueError, "exactly one"):
            check(TRACE.replace(
                "GSM service downlink kind=15 sapi=0 pd=06 message=2e length=8\n",
                "GSM service downlink kind=15 sapi=0 pd=06 message=2e length=8\n" * 2,
            ))

    def test_rejects_wrong_number(self):
        with self.assertRaisesRegex(ValueError, "expected"):
            check(TRACE, "123")

    def test_decodes_odd_bcd_number(self):
        self.assertEqual(
            decode_called_digits(bytes.fromhex("03455e0581551532f4")),
            "5551234",
        )


if __name__ == "__main__":
    unittest.main()
