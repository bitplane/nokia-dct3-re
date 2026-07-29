import unittest

from tools.radio_3330_incoming_call_boundary_check import CHECKPOINTS, verify


def trace(omit=None):
    lines = []
    for label, expression in CHECKPOINTS:
        if label != omit:
            expression = getattr(expression, "pattern", expression)
            lines.append(expression.replace(".*", " sample ").replace(
                "[0-9a-f]*", "").replace("[0-9a-f]{18}", "000000000000000000")
                .replace("[0-9a-f]{2}", "00").replace("[01]", "0")
                .replace("\\", ""))
    return "\n".join(lines)


class Radio3330IncomingCallBoundaryCheckTest(unittest.TestCase):
    def test_accepts_complete_lifecycle(self):
        verify(trace())

    def test_rejects_missing_release_transaction(self):
        with self.assertRaisesRegex(ValueError, "release transaction"):
            verify(trace("NHM-6 release transaction"))

    def test_rejects_duplicate_connect(self):
        text = trace()
        connect = "GSM service uplink sapi=0 pd=03 message=07 length=2"
        with self.assertRaisesRegex(ValueError, "exactly one Connect"):
            verify(text + "\n" + connect)


if __name__ == "__main__":
    unittest.main()
