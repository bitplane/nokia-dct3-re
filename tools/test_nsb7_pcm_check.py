import unittest

from tools.nsb7_pcm_check import verify


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


if __name__ == '__main__':
    unittest.main()
