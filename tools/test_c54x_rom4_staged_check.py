import unittest

from tools.c54x_rom4_staged_check import SOURCE_OFFSET, STOP, check


class StagedFrontierTest(unittest.TestCase):
    def setUp(self):
        self.words = 'f7bb,7718,1ec6,771d,ffa8,7728,2000,7758,3146,f070,0140,f495,76f8,0803,0004,7df8'
        self.payload = bytes.fromhex(self.words.replace(',', ''))
        self.image = bytes(SOURCE_OFFSET) + self.payload
        self.row = ('rom4_staged_frontier: pc=0f07 xpc=00 illegal=1 pmst=ffa8 '
                    f'words={self.words} t=0.500000000\n')
        self.text = STOP + '\n' + self.row

    def test_binds_observation_without_promoting_boot(self):
        result = check(self.text, self.image)
        self.assertEqual(result['source_offset'], SOURCE_OFFSET)
        self.assertEqual(result['observed_words'], 16)
        for field in ('boot_claim', 'upload_complete_claim', 'fitted_bus_dialect_claim'):
            self.assertFalse(result[field])

    def test_rejects_missing_duplicate_or_changed_frontier(self):
        for text in ('', self.text + self.row, self.text + STOP,
                     self.text.replace('illegal=1', 'illegal=0'),
                     self.text.replace('7728,2000', '7728,7fff'),
                     self.text.replace('pc=0f07', 'pc=0f09'),
                     self.text.replace('t=0.500000000', 't=0.100000000')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check(text, self.image)

    def test_rejects_wrong_source_position_or_ambiguous_binding(self):
        for image in (b'', self.image[2:], self.image + self.payload,
                      self.image[:-2] + b'\x00\x00'):
            with self.subTest(length=len(image)), self.assertRaises(ValueError):
                check(self.text, image)
