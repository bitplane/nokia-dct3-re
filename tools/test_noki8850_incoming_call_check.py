import unittest
from tools.noki8850_incoming_call_check import verify
from tools.test_radio_5210_incoming_call_trace_check import GOOD as NSM5_GOOD

GOOD = NSM5_GOOD.replace("message=08 length=5", "message=08 length=11").replace(
    "data=040002000271012fc1", "data=041202000271012fc1",
).replace(
    "data=040000001117001a600000560000001400000001",
    "data=041202001117001a600000010000001400000001",
).replace(
    "GSM service uplink sapi=0 pd=03 message=07 length=2",
    "8850_incoming_physical: action=Call / Send\n8850_keypad_decoded key=0e\n"
    "GSM service uplink sapi=0 pd=03 message=07 length=2",
).replace(
    "GSM service uplink sapi=0 pd=03 message=25 length=5",
    "8850_incoming_physical: action=End\n8850_keypad_decoded key=0f\n"
    "GSM service uplink sapi=0 pd=03 message=25 length=5",
)


class Nokia8850IncomingCallCheckTest(unittest.TestCase):
    def test_complete(self):
        verify(GOOD)

    def test_wrong_send(self):
        with self.assertRaisesRegex(ValueError, "Send decode"):
            verify(GOOD.replace("key=0e", "key=11"))

    def test_missing_acknowledgement(self):
        with self.assertRaisesRegex(ValueError, "Connect Acknowledge"):
            verify(GOOD.replace("036009030f", "036009032d"))

    def test_wrong_release(self):
        with self.assertRaisesRegex(ValueError, "own release"):
            verify(GOOD.replace("041202001117001a60000001", "040000001117001a60000056"))


if __name__ == '__main__':
    unittest.main()
