import unittest

from tools.dsp_speech_control_static_check import (
    FLASH_BASE,
    Profile,
    verify_profile,
)


class DspSpeechControlStaticCheckTests(unittest.TestCase):
    def setUp(self):
        self.profile = Profile(
            "test",
            FLASH_BASE + 0,
            FLASH_BASE + 2,
            FLASH_BASE + 4,
            FLASH_BASE + 6,
        )

    def test_accepts_independent_speech_and_channel_fields(self):
        image = bytes.fromhex("0102fefd0804f3fb")
        result = verify_profile(image, self.profile)
        self.assertEqual(0x0201, result["speech_add"])
        self.assertEqual(0x0408, result["channel_add"])

    def test_rejects_conflated_field(self):
        image = bytes.fromhex("0906fefd0804f3fb")
        with self.assertRaisesRegex(ValueError, "table mismatch"):
            verify_profile(image, self.profile)


if __name__ == "__main__":
    unittest.main()
