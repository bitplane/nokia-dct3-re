import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki6250_call_check import verify, verify_outgoing


# Compact protocol transcript from the reviewed NHM-3 run; timing is omitted.
TRACE = """LAPDm Channel Release acknowledged nr=2
PCH IMSI page transmitted channel=60 fn=2739
TX packet type=1b data=0080013f410627
RX enqueue type=80 payload=34 data=800000000c7b00010000032445030504046002008134015c0581551532f4
GSM service uplink sapi=0 pd=03 message=01 length=2
GSM service uplink sapi=0 pd=06 message=29 length=3
6250_call_input: step=1 pressed=1
GSM service uplink sapi=0 pd=03 message=07 length=2
RX enqueue type=80 payload=34 data=b000000010f200010000036009030f
6250_call_input: step=3 pressed=1
GSM service uplink sapi=0 pd=03 message=25 length=5
RX enqueue type=80 payload=34 data=b0000000145600010000038209032d
GSM service uplink sapi=0 pd=03 message=2a length=2
RX enqueue type=80 payload=34 data=b000000014580001000003a40d060d00
TX packet type=02 radio_phase=release_channel_change data=040000001117001a600000130000001400000001
RX enqueue type=89 payload=8 data=0000000000000000
6250_channel_confirmation: body=00 input=0409 expected=00 pending=00
RX enqueue type=80 payload=34 data=600000000a4d000100001506210001f0
"""


class CallCheckTests(unittest.TestCase):
    def test_incoming_trace_is_not_outgoing_proof(self):
        with self.assertRaises(ValueError):
            verify_outgoing(TRACE)

    def test_complete_signaling(self):
        verify(TRACE)

    def test_missing_confirmation(self):
        with self.assertRaisesRegex(ValueError, "release confirmation"):
            verify(TRACE.replace("RX enqueue type=89", "removed confirmation"))

    def test_speech_after_release_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "speech traffic continued"):
            verify(TRACE + "radio_l1: kind=speech\n")

    def test_empty_trace_is_not_success(self):
        with self.assertRaisesRegex(ValueError, "registration release"):
            verify("")

    def test_keypress_is_not_signaling(self):
        with self.assertRaises(ValueError):
            verify("6250_call_input: step=1 pressed=1\n"
                   "6250_call_input: step=3 pressed=1\n")


if __name__ == "__main__":
    unittest.main()
