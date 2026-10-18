import unittest

from tools.c54x_rom4_staged_check import SOURCE_OFFSET, TAIL_SOURCE_OFFSET, REPEAT_SOURCE_OFFSET, check


class StagedFrontierTest(unittest.TestCase):
    def setUp(self):
        self.words = ('7718,1ec6,7728,2000,f7be,7758,3146,7214,1f0e,7215,1f0f,7713,0060,f071,079f,8093,'
                      '7713,1000,f071,1fff,8093,76f8,2290,0432,76f8,2291,3232,7314,1f0e,7315,1f0f,f020,'
                      '252a,f010,23c4,8811,7712,0d00,47f8,0011')
        self.payload = bytes.fromhex(self.words.replace(',', ''))
        self.tail = '7683,0001,f020,0e05,7ef8,001a,0083,f072,0e1e,7ef8,0014,0093,7e8b,0093,e552,fc00,7722,c008,7722,c0c8,f7bb,e908,f074,4610'
        self.tail_payload = bytes.fromhex(self.tail.replace(',', ''))
        self.image = (bytes(SOURCE_OFFSET) + self.payload).ljust(TAIL_SOURCE_OFFSET, b'\0') + self.tail_payload
        self.loop = '771a,1388,f072,0e62,4920,4816,f620,f845,0e66,7721,0aaa,76f8,06f9,0001,f6bf,7713,06f9,ff45,7683,0000,e908,f074,4610,f030'
        self.loop_payload = bytes.fromhex(self.loop.replace(',', ''))
        self.image = self.image.ljust(REPEAT_SOURCE_OFFSET, b'\0') + self.loop_payload
        self.row = ('rom4_staged_frontier: pc=408d xpc=00 illegal=0 pmst=ffac swwsr=2000 bscr=0000 swcr=0000 '
                    f'words={self.words} t=0.500000000\n')
        self.text = (self.row + f'rom4_staged_uploaded_tail: base=0e10 words={self.tail}\n'
                     'rom4_staged_dagen: bk=00fa ar0=1987 ar2=06e4 st0=081f st1=6108\n'
                     'rom4_staged_accumulators: a=0000000000 b=0000000000 t=000b ar3=06e3 ar4=0000\n'
                     'rom4_staged_bsp_write: value=c008 mask=ffff t=0.133888750\n'
                     'rom4_staged_bsp_write: value=c0c8 mask=ffff t=0.133888808\n'
                     'rom4_staged_bsp_write: value=c008 mask=ffff t=0.134188962\n'
                     'rom4_staged_bsp_write: value=c008 mask=ffff t=0.134294769\n'
                     'rom4_staged_bsp_write: value=c008 mask=ffff t=0.158807577\n'
                     'rom4_staged_bsp_write: value=c008 mask=ffff t=0.159053500\n'
                     'rom4_staged_bsp_write: value=c008 mask=ffff t=2.291748731\n'
                     'rom4_staged_bsp_tx: index=1 value=0aaa mask=ffff t=0.133893750\n'
                     'rom4_staged_bsp_tx: index=2 value=ffd5 mask=ffff t=0.133940788\n'
                     'rom4_staged_bsp_tx_count: count=2\n'
                     'rom4_staged_bsp_rx: index=1 value=0aaa mask=ffff t=0.133909558\n'
                     'rom4_staged_bsp_rx: index=2 value=0aaa mask=ffff t=0.133937654\n'
                     'rom4_staged_bsp_rx_count: count=2\n'
                     f'rom4_staged_repeat_tail: base=0e58 words={self.loop}\n'
                     'rom4_staged_end: pc=408d xpc=00 illegal=0 reads=6 writes=5 gap_reads=9 t=3.016666667\n'
                     'rom4_interface_summary: completion_strobes=623 mailbox_writes=74 slot_expiries=624 frame_expiries=648 rf_reads=19808 rf_port32_writes=0 rf_port38_reads=0 rf_port39_reads=0 pc=408d pmst=ffac ifr=0020 imr=035f mode_aa=0000 mode_ac=0000 transfer_expiries=0\n')
        self.text += ''.join(f'rom4_rf_read: sample={i + 1} pc={pc} frame=30 t={0.162032 + i * 0.0000002:.6f}\n'
                             for i, pc in enumerate(('324e', '3254', '325a', '3260') * 4))
        observations = [
            ('low_write', '00f1', '0000', '0f10', '0.133253635'),
            ('low_write', '00f2', '0000', '0f10', '0.133253654'),
            ('low_write', '00f2', '0003', 'a5e5', '0.158817750'),
            ('low_read', '00f1', '0000', 'a5ed', '0.158817923'),
            ('low_read', '00f2', '0003', 'a5ef', '0.158817962'),
            ('gap_read', '0003', '0000', 'a5f4', '0.158818058'),
            ('gap_read', '0003', '0000', 'a5f8', '0.158818135'),
            ('gap_read', '0003', '0000', 'a601', '0.158818327'),
            ('low_write', '00f2', '0003', 'a5e5', '0.159063481'),
            ('low_read', '00f1', '0000', 'a5ed', '0.159063654'),
            ('low_read', '00f2', '0003', 'a5ef', '0.159063692'),
            ('gap_read', '0003', '0000', 'a5f4', '0.159063788'),
            ('gap_read', '0003', '0000', 'a5f8', '0.159063865'),
            ('gap_read', '0003', '0000', 'a601', '0.159064058'),
            ('low_write', '00f2', '0003', 'a5e5', '2.291758711'),
            ('low_read', '00f1', '0000', 'a5ed', '2.291758885'),
            ('low_read', '00f2', '0003', 'a5ef', '2.291758923'),
            ('gap_read', '0003', '0000', 'a5f4', '2.291759019'),
            ('gap_read', '0003', '0000', 'a5f8', '2.291759096'),
            ('gap_read', '0003', '0000', 'a601', '2.291759288'),
        ]
        self.text += ''.join(f'rom4_staged_{tag}: address={address} value={value} mask=ffff pc_context={pc} t={timestamp}\n'
                             for tag, address, value, pc, timestamp in observations)
        self.text += 'rom4_staged_low_count: reads=4 writes=4\nrom4_staged_gap_count: reads=6\n'

    def test_binds_observation_without_promoting_boot(self):
        result = check(self.text, self.image)
        self.assertEqual(result['source_offset'], SOURCE_OFFSET)
        self.assertEqual(result['observed_words'], 40)
        self.assertEqual(result['status'], 'bounded_staged_rf_loop')
        self.assertEqual(result['rf_sample_blocks'], 619)
        self.assertEqual(result['tail_source_offset'], TAIL_SOURCE_OFFSET)
        self.assertEqual(result['bsp_receive_words'], [0x0aaa, 0x0aaa])
        self.assertEqual(result['bsp_transmit_words'], [0x0aaa, 0xffd5])
        self.assertEqual(result['repeat_source_offset'], REPEAT_SOURCE_OFFSET)
        self.assertEqual(result['bsp_control_writes'], [0xc008, 0xc0c8] + [0xc008] * 5)
        self.assertEqual(result['completion_strobes'], 623)
        self.assertEqual(result['swwsr'], 0x2000)
        self.assertEqual(result['low_gap_bus_reads'], 9)
        self.assertEqual(result['rf_sample_reads'], 19808)
        for field in ('boot_claim', 'upload_complete_claim', 'fitted_bus_dialect_claim', 'physical_low_gap_identity_claim', 'rf_acquisition_claim', 'preceding_instruction_retirement_claim'):
            self.assertFalse(result[field])

    def test_rejects_changed_or_duplicate_frozen_accumulators(self):
        row = 'rom4_staged_accumulators: a=0000000000 b=0000000000 t=000b ar3=06e3 ar4=0000\n'
        for text in (self.text.replace(row, ''), self.text + row,
                     self.text.replace('t=000b', 't=000c'),
                     self.text.replace('a=0000000000', 'a=0000000001'),
                     self.text.replace('ar3=06e3', 'ar3=06e2'),
                     self.text.replace('ar4=0000', 'ar4=0001')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check(text, self.image)

    def test_rejects_missing_or_changed_native_rf_reads(self):
        for text in (self.text.replace('rom4_rf_read:', 'missing_rf:'),
                     self.text + 'rom4_rf_read: malformed\n',
                     self.text.replace('rf_reads=19808', 'rf_reads=19807'),
                     self.text.replace('sample=4', 'sample=3'),
                     self.text.replace('pc=3254', 'pc=3253'),
                     self.text.replace('frame=30', 'frame=29'),
                     self.text.replace('0.162032', '0.700000')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check(text, self.image)

    def test_rejects_changed_pointer_and_unowned_bus_evidence(self):
        for text in (self.text.replace('rom4_staged_gap_read:', 'missing_gap:'),
                     self.text + 'rom4_staged_low_read: malformed\n',
                     self.text.replace('address=00f1 value=0000', 'address=00f1 value=0001'),
                     self.text.replace('address=0003 value=0000', 'address=0003 value=0001'),
                     self.text.replace('pc_context=a5f4', 'pc_context=a5f3'),
                     self.text.replace('0.158818058', '0.158817961'),
                     self.text.replace('reads=6', 'reads=5'),
                     self.text + 'rom4_staged_low_count: reads=4 writes=4\n'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check(text, self.image)

    def test_rejects_missing_duplicate_or_changed_frontier(self):
        for text in ('', self.text + self.row,
                     self.text.replace('illegal=0', 'illegal=1'),
                     self.text.replace('7728,2000', '7728,7fff'),
                     self.text.replace('pc=408d', 'pc=0f1f'),
                     self.text.replace('st0=081f', 'st0=001f'),
                     self.text.replace('st1=6108', 'st1=4901'),
                     self.text + 'rom4_staged_dagen: malformed\n',
                     self.text + 'rom4_staged_dagen: bk=0010 ar0=0002 ar2=06d3 st0=081f st1=e900\n',
                     self.text.replace('rom4_staged_bsp_rx_count: count=2', 'rom4_staged_bsp_rx_count: count=3'),
                     self.text.replace('index=2 value=0aaa', 'index=2 value=0aab'),
                     self.text.replace('0.133937654', '0.133909557'),
                     self.text + 'rom4_staged_bsp_rx: malformed\n',
                     self.text + 'unsupported alternate instruction\n',
                     self.text.replace('value=0aaa', 'value=0aab'),
                     self.text.replace('rom4_staged_bsp_tx_count: count=2', 'rom4_staged_bsp_tx_count: count=3'),
                     self.text.replace('value=ffd5', 'value=ffd4'),
                     self.text.replace('0.133940788', '0.133937653'),
                     self.text.replace('base=0e58', 'base=0e59'),
                     self.text.replace('f845,0e66', 'f845,0e65'),
                     self.text + f'rom4_staged_repeat_tail: base=0e58 words={self.loop}\n',
                     self.text.replace('value=c008', 'value=c009'),
                     self.text.replace('mask=ffff', 'mask=00ff'),
                     self.text + 'rom4_staged_bsp_write: malformed\n',
                     self.text.replace('0.133888808', '0.133888749'),
                     self.text.replace('base=0e10', 'base=0e11'),
                     self.text.replace('7722,c008', '7722,c009'),
                     self.text + f'rom4_staged_uploaded_tail: base=0e10 words={self.tail}\n',
                     self.text.replace('completion_strobes=623', 'completion_strobes=0'),
                     self.text.replace('mailbox_writes=74', 'mailbox_writes=70'),
                     self.text.replace('swwsr=2000', 'swwsr=0000'),
                     self.text.replace('bscr=0000', 'bscr=f000'),
                     self.text.replace('swcr=0000', 'swcr=0001'),
                     self.text.replace('t=0.500000000', 't=0.100000000')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check(text, self.image)

    def test_requires_clean_full_duration_endpoint_and_transport(self):
        row = 'rom4_staged_end: pc=408d xpc=00 illegal=0 reads=6 writes=5 gap_reads=9 t=3.016666667\n'
        for text in (self.text.replace(row, ''), self.text + row,
                     self.text.replace('t=3.016666667', 't=0.516666667'),
                     self.text.replace('slot_expiries=624', 'slot_expiries=623'),
                     self.text.replace('frame_expiries=648', 'frame_expiries=647'),
                     self.text.replace('rf_port32_writes=0', 'rf_port32_writes=1'),
                     self.text.replace('ifr=0020', 'ifr=0225'),
                     self.text.replace('2.291758711', '0.291758711'),
                     self.text.replace('2.291759288', '3.291759288'),
                     self.text + 'unimplemented arithmetic\n'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check(text, self.image)

    def test_rejects_wrong_source_position_or_ambiguous_binding(self):
        for image in (b'', self.image[2:], self.image + self.payload, self.image + self.tail_payload,
                      self.image + self.loop_payload,
                      self.image[:-2] + b'\x00\x00'):
            with self.subTest(length=len(image)), self.assertRaises(ValueError):
                check(self.text, image)
