import pathlib
import tempfile
import unittest

from tools.radio_two_call_negative_check import verify


BASE = """
GSM service uplink sapi=0 pd=03 message=07 length=2 data=8347 t=1
GSM service uplink sapi=0 pd=03 message=08 length=9 data=93080802e091150101 t=2
GSM service uplink sapi=0 pd=03 message=01 length=2 data=9341 t=3
"""


class TwoCallNegativeCheckTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.frames = pathlib.Path(self.directory.name)

    def tearDown(self):
        self.directory.cleanup()

    def test_duplicate_is_deduplicated(self):
        text = BASE + "malformed=0 duplicate=0\nmalformed=0 duplicate=1\n"
        with self.assertRaisesRegex(ValueError, "presentation"):
            verify(text, self.frames, "duplicate")

    def test_duplicate_rejects_second_confirmation(self):
        text = BASE + "malformed=0 duplicate=0\nmalformed=0 duplicate=1\n" + \
            "GSM service uplink data=9308\n"
        with self.assertRaisesRegex(ValueError, "another call-control"):
            verify(text, self.frames, "duplicate")

    def test_malformed_requires_observed_normalization(self):
        text = BASE + "malformed=1 duplicate=0\n"
        with self.assertRaisesRegex(ValueError, "bounded malformed"):
            verify(text, self.frames, "malformed")

    def test_rejects_rr_teardown(self):
        text = BASE + "malformed=0 duplicate=0\nmalformed=0 duplicate=1\n" + \
            "radio_phase=release_channel_change\n"
        with self.assertRaisesRegex(ValueError, "released"):
            verify(text, self.frames, "duplicate")


if __name__ == "__main__":
    unittest.main()
