import unittest

from tools.c54x_rom4_rf_score_check import check


ZERO = "rom4_rf_score_summary: mode=zero reads=900000 results=5000 nonzero=0 maximum=0000000000 second=0 accepted=0 acquisition_claim=0"
TONE = "rom4_rf_score_summary: mode=tone reads=900000 results=5000 nonzero=5000 maximum=0000007fff second=400 accepted=8 acquisition_claim=0"


class ScoreTests(unittest.TestCase):
    def test_controls(self):
        self.assertEqual(check(ZERO), ("zero", 5000, 0))
        self.assertEqual(check(ZERO.replace("mode=zero", "mode=dc")), ("dc", 5000, 0))
        self.assertEqual(check(TONE), ("tone", 5000, 8))

    def test_reject_insufficient_inconsistent_and_duplicate(self):
        for text in ("", ZERO + "\n" + ZERO, ZERO.replace("results=5000", "results=1"),
                     ZERO.replace("nonzero=0", "nonzero=1"), TONE.replace("accepted=8", "accepted=0"),
                     TONE.replace("accepted=8", "accepted=401"), TONE.replace("nonzero=5000", "nonzero=4999"),
                     TONE.replace("0000007fff", "0000000574")):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check(text)


if __name__ == "__main__":
    unittest.main()
