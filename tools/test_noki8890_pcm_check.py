import unittest

from tools.noki8890_pcm_check import verify


def trace(incoming=False):
    return (
        ('8890_incoming_physical: action=Call / Send\n' if incoming else
         '8890_call_physical: action=send\n') +
        '8890_keypad_decoded: key=0e\n'
        'dsp_control_write: data=860b pc=002c338e r4=00000008 t=1\n'
        'dsp_hle: speech tick uplink=1 downlink=0 pcm=1 '
        'pcm_clock=512000/8000 pcm_shape=64 serial_clocks=10240/7520\n'
        'dsp_hle: speech tick uplink=150 downlink=140 pcm=150 '
        'pcm_clock=512000/8000 pcm_shape=64 serial_clocks=1536000/1128000\n' +
        ('8890_incoming_physical: action=End\n' if incoming else
         '8890_call_physical: action=end\n') +
        '8890_keypad_decoded: key=0f\n'
        'dsp_control_write: data=840a pc=002c338e r4=00000008 t=4\n')


class Nsb6PcmTest(unittest.TestCase):
    def test_both_own_physical_call_directions(self):
        for incoming in (False, True):
            result = verify(trace(incoming), incoming=incoming)
            self.assertEqual(result['pcm_clock'], [512000, 8000])
            self.assertFalse(result['native_dsp_speech'])

    def test_sibling_clock_shape_and_accounting_fail(self):
        for old, new in (('512000', '1000000'), ('shape=64', 'shape=125'),
                         ('1536000', '1536001'), ('1128000', '1128001'),
                         ('pcm=150', 'pcm=149'), ('downlink=140', 'downlink=151')):
            with self.subTest(new=new), self.assertRaises(ValueError):
                verify(trace().replace(old, new))

    def test_missing_or_wrong_own_control_fails(self):
        for old, new in (('data=860b', 'data=840b'), ('data=840a', 'data=860a'),
                         ('pc=002c338e', 'pc=002cb3ca'), ('key=0e', 'key=0f')):
            with self.subTest(new=new), self.assertRaises(ValueError):
                verify(trace().replace(old, new))

    def test_post_disable_or_unsustained_transfers_fail(self):
        lines = trace().splitlines(True)
        with self.assertRaises(ValueError):
            verify(''.join(lines[:4] + lines[5:] + [lines[4]]))
        with self.assertRaises(ValueError):
            verify(''.join(lines[:4] + lines[5:]))
        with self.assertRaises(ValueError):
            verify(trace().replace('downlink=140', 'downlink=0'))


if __name__ == '__main__':
    unittest.main()
