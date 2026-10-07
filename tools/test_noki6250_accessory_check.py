import unittest

from tools.noki6250_accessory_check import verify


class AccessoryCheckTest(unittest.TestCase):
    def setUp(self):
        self.text = ('6250_accessory_decision: state=0f sample=03ff\n'
                     '6250_accessory_endpoint: state=0f sample=03ff t=20.000001\n')
        self.pixels = bytes([255]) * (96 * 60)

    def test_unattached(self):
        verify(self.text, self.pixels, (96, 60))

    def test_headset_state_rejected(self):
        with self.assertRaisesRegex(ValueError, 'decision'):
            verify(self.text.replace('state=0f', 'state=10'), self.pixels, (96, 60))

    def test_missing_settlement(self):
        with self.assertRaisesRegex(ValueError, 'endpoint'):
            verify(self.text.splitlines()[0], self.pixels, (96, 60))

    def test_nonblank_label_rejected(self):
        pixels = bytearray(self.pixels)
        pixels[30 * 96 + 30] = 0
        with self.assertRaisesRegex(ValueError, 'label'):
            verify(self.text, bytes(pixels), (96, 60))


if __name__ == '__main__':
    unittest.main()
