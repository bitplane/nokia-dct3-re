import unittest

from tools.c54x_rom4_staged_check import SOURCE_OFFSET, STOP, check


class StagedFrontierTest(unittest.TestCase):
    def setUp(self):
        self.words = ('7718,1ec6,7728,2000,f7be,7758,3146,7214,1f0e,7215,1f0f,7713,0060,f071,079f,8093,'
                      '7713,1000,f071,1fff,8093,76f8,2290,0432,76f8,2291,3232,7314,1f0e,7315,1f0f,f020,'
                      '252a,f010,23c4,8811,7712,0d00,47f8,0011')
        self.payload = bytes.fromhex(self.words.replace(',', ''))
        self.image = bytes(SOURCE_OFFSET) + self.payload
        self.row = ('rom4_staged_frontier: pc=31d8 xpc=00 illegal=1 pmst=ffac swwsr=2000 bscr=0000 swcr=0000 '
                    f'words={self.words} t=0.500000000\n')
        self.text = STOP + '\n' + self.row + 'rom4_interface_summary: completion_strobes=2 mailbox_writes=71 \n'

    def test_binds_observation_without_promoting_boot(self):
        result = check(self.text, self.image)
        self.assertEqual(result['source_offset'], SOURCE_OFFSET)
        self.assertEqual(result['observed_words'], 40)
        self.assertEqual(result['instruction'], 0x7f92)
        self.assertEqual(result['instruction_address'], 0x31d6)
        self.assertTrue(result['repeat_active'])
        self.assertEqual(result['completion_strobes'], 2)
        self.assertEqual(result['swwsr'], 0x2000)
        for field in ('boot_claim', 'upload_complete_claim', 'fitted_bus_dialect_claim'):
            self.assertFalse(result[field])

    def test_rejects_missing_duplicate_or_changed_frontier(self):
        for text in ('', self.text + self.row, self.text + STOP,
                     self.text.replace('illegal=1', 'illegal=0'),
                     self.text.replace('7728,2000', '7728,7fff'),
                     self.text.replace('pc=31d8', 'pc=0f1f'),
                     self.text.replace('repeat=1', 'repeat=0'),
                     self.text.replace('delay=0', 'delay=1'),
                     self.text.replace('completion_strobes=2', 'completion_strobes=0'),
                     self.text.replace('mailbox_writes=71', 'mailbox_writes=70'),
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
