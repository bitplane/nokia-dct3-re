import unittest

from tools.nse1_scan_continuation_contract import FRAGMENTS, check_fragments


class ScanContinuationContractTest(unittest.TestCase):
    def image(self):
        image = bytearray(0x100000)
        for address, data in FRAGMENTS.items():
            offset = address - 0x200000
            image[offset:offset + len(data)] = data
        return image

    def test_bounded_fragments(self):
        self.assertEqual(check_fragments(self.image()), 32)

    def test_every_fragment_byte_is_protected(self):
        image = self.image()
        for address, data in FRAGMENTS.items():
            for index in range(len(data)):
                offset = address - 0x200000 + index
                image[offset] ^= 1
                with self.subTest(address=address, index=index), self.assertRaises(ValueError):
                    check_fragments(image)
                image[offset] ^= 1

    def test_truncation_is_not_coverage(self):
        with self.assertRaises(ValueError):
            check_fragments(self.image()[:0x100000 - 0x90000])


if __name__ == '__main__':
    unittest.main()
