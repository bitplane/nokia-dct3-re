import unittest

from tools.run_c54x_rom4_crc import COMPLETE, PASS, check_result


class Rom4HelperResultTest(unittest.TestCase):
    def test_completion(self):
        check_result(PASS + "\n" + COMPLETE + "\n", 3)

    def test_reject_incomplete_or_ambiguous_results(self):
        for text, status in [(PASS, 3), (COMPLETE, 3),
                             (PASS + "\n" + COMPLETE, 0),
                             (PASS + "\n" + PASS + "\n" + COMPLETE, 3),
                             (PASS + "\n" + COMPLETE + "\nextra", 3),
                             ("WRONG CHECKSUMS\n" + PASS + "\n" + COMPLETE, 3),
                             ("Fatal error: failure\n" + PASS + "\n" + COMPLETE, 3)]:
            with self.subTest(text=text, status=status):
                with self.assertRaises(ValueError):
                    check_result(text, status)


if __name__ == "__main__":
    unittest.main()
