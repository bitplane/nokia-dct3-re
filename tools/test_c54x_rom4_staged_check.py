import unittest

from tools.c54x_rom4_staged_check import SOURCE_OFFSET, TAIL_SOURCE_OFFSET, STOP, check


class StagedFrontierTest(unittest.TestCase):
    def setUp(self):
        self.words = ('7718,1ec6,7728,2000,f7be,7758,3146,7214,1f0e,7215,1f0f,7713,0060,f071,079f,8093,'
                      '7713,1000,f071,1fff,8093,76f8,2290,0432,76f8,2291,3232,7314,1f0e,7315,1f0f,f020,'
                      '252a,f010,23c4,8811,7712,0d00,47f8,0011')
        self.payload = bytes.fromhex(self.words.replace(',', ''))
        self.tail = '0e1e,7ef8,0014,0093,7e8b,0093,e552,fc00,7722,c008,7722,c0c8,f7bb,e908,f074,4610'
        self.tail_payload = bytes.fromhex(self.tail.replace(',', ''))
        self.image = (bytes(SOURCE_OFFSET) + self.payload).ljust(TAIL_SOURCE_OFFSET, b'\0') + self.tail_payload
        self.row = ('rom4_staged_frontier: pc=0e32 xpc=00 illegal=1 pmst=ffac swwsr=2000 bscr=0000 swcr=0000 '
                    f'words={self.words} t=0.500000000\n')
        self.text = (STOP + '\n' + self.row + f'rom4_staged_uploaded_tail: base=0e18 words={self.tail}\n'
                     'rom4_staged_bsp_write: value=c008 mask=ffff t=0.133888750\n'
                     'rom4_staged_bsp_write: value=c0c8 mask=ffff t=0.133888808\n'
                     'rom4_interface_summary: completion_strobes=2 mailbox_writes=71 \n')

    def test_binds_observation_without_promoting_boot(self):
        result = check(self.text, self.image)
        self.assertEqual(result['source_offset'], SOURCE_OFFSET)
        self.assertEqual(result['observed_words'], 40)
        self.assertEqual(result['instruction'], 0x8821)
        self.assertEqual(result['instruction_address'], 0x0e30)
        self.assertEqual(result['tail_source_offset'], TAIL_SOURCE_OFFSET)
        self.assertEqual(result['destination'], 0x21)
        self.assertEqual(result['bsp_control_writes'], [0xc008, 0xc0c8])
        self.assertEqual(result['completion_strobes'], 2)
        self.assertEqual(result['swwsr'], 0x2000)
        for field in ('boot_claim', 'upload_complete_claim', 'fitted_bus_dialect_claim'):
            self.assertFalse(result[field])

    def test_rejects_missing_duplicate_or_changed_frontier(self):
        for text in ('', self.text + self.row, self.text + STOP,
                     self.text.replace('illegal=1', 'illegal=0'),
                     self.text.replace('7728,2000', '7728,7fff'),
                     self.text.replace('pc=0e32', 'pc=0f1f'),
                     self.text.replace('value=c008', 'value=c009'),
                     self.text.replace('mask=ffff', 'mask=00ff'),
                     self.text + 'rom4_staged_bsp_write: malformed\n',
                     self.text.replace('0.133888808', '0.133888749'),
                     self.text.replace('base=0e18', 'base=0e19'),
                     self.text.replace('7722,c008', '7722,c009'),
                     self.text + f'rom4_staged_uploaded_tail: base=0e18 words={self.tail}\n',
                     self.text.replace('completion_strobes=2', 'completion_strobes=0'),
                     self.text.replace('mailbox_writes=71', 'mailbox_writes=70'),
                     self.text.replace('swwsr=2000', 'swwsr=0000'),
                     self.text.replace('bscr=0000', 'bscr=f000'),
                     self.text.replace('swcr=0000', 'swcr=0001'),
                     self.text.replace('t=0.500000000', 't=0.100000000')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check(text, self.image)

    def test_rejects_wrong_source_position_or_ambiguous_binding(self):
        for image in (b'', self.image[2:], self.image + self.payload, self.image + self.tail_payload,
                      self.image[:-2] + b'\x00\x00'):
            with self.subTest(length=len(image)), self.assertRaises(ValueError):
                check(self.text, image)
