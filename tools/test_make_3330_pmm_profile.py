import hashlib
import unittest

from tools.make_3330_pmm_profile import (
    FLASH_SIZE,
    PMM_OFFSET,
    PMM_SIZE,
    SECURITY_VERIFIER_OFFSET,
    SETTLED_SECURITY_VERIFIER,
    VIRGIN_SECURITY_VERIFIER,
    make_profile,
)


class Make3330PmmProfileTest(unittest.TestCase):
    def fixture(self):
        base = bytes((index * 7) & 0xff for index in range(0x350000))
        pmm = bytearray(b"\xff" * PMM_SIZE)
        pmm[
            SECURITY_VERIFIER_OFFSET:SECURITY_VERIFIER_OFFSET + len(VIRGIN_SECURITY_VERIFIER)
        ] = VIRGIN_SECURITY_VERIFIER
        return base, pmm

    def test_changes_only_the_evidenced_verifier(self):
        base, pmm = self.fixture()

        result = make_profile(base, bytes(pmm))

        self.assertEqual(FLASH_SIZE, len(result))
        self.assertEqual(base, result[:len(base)])
        expected_pmm = bytearray(pmm)
        expected_pmm[
            SECURITY_VERIFIER_OFFSET:SECURITY_VERIFIER_OFFSET + len(SETTLED_SECURITY_VERIFIER)
        ] = SETTLED_SECURITY_VERIFIER
        self.assertEqual(bytes(expected_pmm), result[PMM_OFFSET:PMM_OFFSET + PMM_SIZE])
        self.assertEqual(b"\xff" * (PMM_OFFSET - len(base)), result[len(base):PMM_OFFSET])

    def test_rejects_unexpected_verifier(self):
        base, pmm = self.fixture()
        pmm[SECURITY_VERIFIER_OFFSET] ^= 1
        with self.assertRaisesRegex(ValueError, "unexpected virgin security verifier"):
            make_profile(base, bytes(pmm))

    def test_rejects_unexpected_digest_when_requested(self):
        base, pmm = self.fixture()
        with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
            make_profile(base, bytes(pmm), hashlib.sha256(b"other").hexdigest())

    def test_rejects_wrong_sizes(self):
        base, pmm = self.fixture()
        with self.assertRaisesRegex(ValueError, "MCU/PPM"):
            make_profile(base[:-1], bytes(pmm))
        with self.assertRaisesRegex(ValueError, "PMM image"):
            make_profile(base, bytes(pmm[:-1]))


if __name__ == "__main__":
    unittest.main()
