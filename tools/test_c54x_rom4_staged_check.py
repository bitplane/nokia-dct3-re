import unittest

from tools.c54x_rom4_staged_check import SOURCE_OFFSET, STOP, check


class StagedFrontierTest(unittest.TestCase):
    def setUp(self):
        self.words = ('f7bb,7718,1ec6,771d,ffa8,7728,2000,7758,3146,f070,0140,f495,76f8,0803,0004,7df8,'
                      '0803,ff87,7cf8,0802,ff87,f074,0f6d,10f8,087f,f844,0f17,7213,087b,7212,0881,6bf8,'
                      '0013,0800,f074,8023,76f8,087f,0001,44f8')
        self.payload = bytes.fromhex(self.words.replace(',', ''))
        self.image = bytes(SOURCE_OFFSET) + self.payload
        self.row = ('rom4_staged_frontier: pc=0f23 xpc=00 illegal=1 pmst=ffa8 swwsr=2000 bscr=0000 swcr=0000 '
                    f'words={self.words} t=0.500000000\n')
        self.text = STOP + '\n' + self.row

    def test_binds_observation_without_promoting_boot(self):
        result = check(self.text, self.image)
        self.assertEqual(result['source_offset'], SOURCE_OFFSET)
        self.assertEqual(result['observed_words'], 40)
        self.assertEqual(result['instruction'], 0x6bf8)
        self.assertEqual(result['instruction_address'], 0x0f1f)
        self.assertEqual(result['swwsr'], 0x2000)
        for field in ('boot_claim', 'upload_complete_claim', 'fitted_bus_dialect_claim'):
            self.assertFalse(result[field])

    def test_rejects_missing_duplicate_or_changed_frontier(self):
        for text in ('', self.text + self.row, self.text + STOP,
                     self.text.replace('illegal=1', 'illegal=0'),
                     self.text.replace('7728,2000', '7728,7fff'),
                     self.text.replace('pc=0f23', 'pc=0f1f'),
                     self.text.replace('swwsr=2000', 'swwsr=0000'),
                     self.text.replace('bscr=0000', 'bscr=f000'),
                     self.text.replace('swcr=0000', 'swcr=0001'),
                     self.text.replace('t=0.500000000', 't=0.100000000')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check(text, self.image)

    def test_rejects_wrong_source_position_or_ambiguous_binding(self):
        for image in (b'', self.image[2:], self.image + self.payload,
                      self.image[:-2] + b'\x00\x00'):
            with self.subTest(length=len(image)), self.assertRaises(ValueError):
                check(self.text, image)
