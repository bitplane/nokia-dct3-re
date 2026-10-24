import unittest

from tools.nsb7_pcm_check import verify, verify_before_restore, verify_unanswered_restore


class PCMTests(unittest.TestCase):
    def setUp(self):
        self.text = '\n'.join([
            'nsb7_outgoing_call_input: key=Send state=1 ',
            'nsb7_key_decode: key=0e observer_writes=0',
            'dsp_control_write: data=860b pc=002cb93e r4=00000008 ',
            'dsp_hle: speech tick uplink=100 downlink=93 pcm=100 '
            'pcm_clock=512000/8000 pcm_shape=64 serial_clocks=1024000/752000',
            'nsb7_outgoing_call_input: key=End state=1 ',
            'nsb7_key_decode: key=0f observer_writes=0',
            'dsp_control_write: data=840a pc=002cb93e r4=00000008 ',
        ])

    def test_clocked_transport(self):
        self.assertEqual(verify(self.text)['downlink'], 93)

    def test_incoming_clocked_transport(self):
        text = self.text.replace('nsb7_outgoing_call_input', 'nsb7_call_input')
        self.assertEqual(verify(text, incoming=True)['downlink'], 93)
        with self.assertRaises(ValueError):
            verify(text)

    def test_invalid_transfers(self):
        for old, new in [('512000/8000', '256000/8000'),
                         ('pcm_shape=64', 'pcm_shape=32'),
                         ('1024000/752000', '1024000/752001'),
                         ('downlink=93', 'downlink=0'),
                         ('pcm=100', 'pcm=99')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify(self.text.replace(old, new))

    def test_no_transfers(self):
        with self.assertRaises(ValueError):
            verify('\n'.join(row for row in self.text.splitlines() if 'speech tick' not in row))

    def test_saved_call_requires_positive_exact_pre_save_pcm(self):
        text = self.text.replace('nsb7_outgoing_call_input: key=End',
                                 'sip_state: saved\nnsb7_outgoing_call_input: key=End')
        self.assertEqual(verify_before_restore(text)['uplink'], 100)
        for bad in (text.replace('sip_state: saved', ''),
                    text.replace('downlink=93', 'downlink=0'),
                    text.replace('002cb93e', '002c338e'),
                    text.replace('752000', '752001'),
                    'sip_state: saved\n' + self.text):
            with self.subTest(bad=bad[-60:]), self.assertRaises(ValueError):
                verify_before_restore(bad)

    def test_unanswered_local_pcm_is_only_bounded_zero_downlink_teardown(self):
        tick = next(row for row in self.text.splitlines() if 'speech tick' in row).replace('downlink=93', 'downlink=0')
        text = 'sip_state: restored\n' + tick + '\nLAPDm service Channel Release acknowledged'
        verify_unanswered_restore(text)
        verify_unanswered_restore('sip_state: restored\nLAPDm service Channel Release acknowledged')
        for bad in (text.replace('downlink=0', 'downlink=1'),
                    tick + '\n' + text,
                    text + '\n' + tick,
                    text.replace('752000', '752001'),
                    text + '\ngsm_call_adapter: media direction=uplink id=1 result=accepted'):
            with self.subTest(bad=bad[-80:]), self.assertRaises(ValueError):
                verify_unanswered_restore(bad)


if __name__ == '__main__':
    unittest.main()
