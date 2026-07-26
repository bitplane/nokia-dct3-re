import unittest

from tools.radio_3310_incoming_call_boundary_check import verify


class Radio3310IncomingCallBoundaryCheckTest(unittest.TestCase):
    def test_accepts_ordered_frontier(self):
        verify("\n".join((
            "LAPDm Channel Release acknowledged nr=2",
            "PCH IMSI page transmitted channel=60 fn=1719",
            "TX packet type=1b data=0080013f410627",
            "RX enqueue type=80 payload=34 data=8012000006ca005800000173410627",
            "RX enqueue type=80 payload=34 data=8012000006cc0058000003000d063500",
            "TX packet type=14 payload=12 data=001affffffffffffffff0000",
            "RX enqueue type=80 payload=34 data=8012000006ce0058000003022905324762704221000000",
            "GSM service uplink sapi=0 pd=06 message=32 length=2",
            "RX enqueue type=80 payload=34 data=8012000006d100580000032445030504046002008134015c0581551532f4",
        )))

    def test_rejects_call_confirmed_past_frontier(self):
        text = "\n".join((
            "LAPDm Channel Release acknowledged nr=2",
            "PCH IMSI page transmitted channel=60 fn=1719",
            "TX packet type=1b data=0080013f410627",
            "RX enqueue type=80 payload=34 data=8012000006ca005800000173410627",
            "RX enqueue type=80 payload=34 data=8012000006cc0058000003000d063500",
            "TX packet type=14 payload=12 data=001affffffffffffffff0000",
            "RX enqueue type=80 payload=34 data=8012000006ce0058000003022905324762704221000000",
            "GSM service uplink sapi=0 pd=06 message=32 length=2",
            "RX enqueue type=80 payload=34 data=8012000006d100580000032445030504046002008134015c0581551532f4",
            "GSM service uplink sapi=0 pd=03 message=08",
        ))
        with self.assertRaisesRegex(ValueError, "promote"):
            verify(text)


if __name__ == "__main__":
    unittest.main()
