import unittest

from tools.nse5_verifier_check import check_boundary, check_publication


class VerifierCheckTest(unittest.TestCase):
    def setUp(self):
        self.trace = "\n".join(
            f"nsm3_verifier: block={i} flag=087f pc=0f17" for i in range(228))

    def test_peripheral_boundary(self):
        check_boundary("requires peripheral read: port=002d pc=0f92 blocks=228",
                       self.trace, 1)

    def test_missing_block_rejected(self):
        with self.assertRaisesRegex(ValueError, "228"):
            check_boundary("", self.trace.rsplit("\n", 1)[0], 1)

    def test_fixture_publication(self):
        output = ("publication: blocks=228 word0=0016 word1=0004 word2=0004 "
                  "word3=0004 pc=0f6b fingerprint=a98692ad pmst=ffa8")
        self.assertEqual(check_publication(output, self.trace, 3, 0x16), "a98692ad")
        with self.assertRaisesRegex(ValueError, "supplied"):
            check_publication(output, self.trace, 3, 0)

    def test_foreign_version_rejected(self):
        output = ("publication: blocks=228 word0=0000 word1=0006 word2=0006 "
                  "word3=0006 pc=0f6a fingerprint=12345678 pmst=ffa8")
        with self.assertRaisesRegex(ValueError, "supplied"):
            check_publication(output, self.trace, 3, 0)
