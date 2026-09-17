import unittest

from tools.radio_3330_incoming_call_boundary_check import CHECKPOINTS, verify
from tools.radio_call_lifecycle_common import RR_CHANNEL_RELEASE


def trace(omit=None):
    lines = []
    for label, expression in CHECKPOINTS:
        if label == "traffic release UA":
            lines.append(RR_CHANNEL_RELEASE.pattern.replace(
                ".*", " sample ").replace(
                "[0-9a-f]{18}", "000000000000000000").replace(
                "[0-9a-f]{2}", "00").replace("\\", ""))
        if label != omit:
            expression = getattr(expression, "pattern", expression)
            lines.append(expression.replace(".*", " sample ").replace(
                "[0-9a-f]*", "").replace("[0-9a-f]{18}", "000000000000000000")
                .replace("[0-9a-f]{4}", "0000")
                .replace("[0-9a-f]{2}", "00").replace("[01]", "0")
                .replace("(?:00|12)", "12")
                .replace("(?:04120200|04000000)", "04120200")
                .replace("\\", ""))
    return "\n".join(lines)


class Radio3330IncomingCallBoundaryCheckTest(unittest.TestCase):
    def test_accepts_complete_lifecycle(self):
        verify(trace())

    def test_rejects_missing_release_transaction(self):
        with self.assertRaisesRegex(ValueError, "release transaction"):
            verify(trace("NHM-6 release transaction"))

    def test_accepts_physically_observed_paced_selector(self):
        verify(trace()
               .replace("041202000271012fc1", "040002000271012fc1")
               .replace("041202001117001a", "040000001117001a"))

    def test_rejects_duplicate_connect(self):
        text = trace()
        connect = "GSM service uplink sapi=0 pd=03 message=07 length=2"
        with self.assertRaisesRegex(ValueError, "exactly one Connect"):
            verify(text + "\n" + connect)


if __name__ == "__main__":
    unittest.main()
