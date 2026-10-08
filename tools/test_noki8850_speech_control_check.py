import unittest

from tools.noki8850_speech_control_check import recover, verify_call, verify_pcm


class SpeechControlTests(unittest.TestCase):
    def test_wrong_rom_rejected(self):
        with self.assertRaisesRegex(ValueError, 'NSM-2'):
            recover(b'not the acquired image')

    def test_ordered_own_publisher(self):
        text = ('8850_call_physical: action=send\n'
                'dsp_control_write: data=860b pc=002cb3ca r4=00000008 rest\n'
                '8850_call_physical: action=end\n'
                'dsp_control_write: data=840a pc=002cb3ca r4=00000008 rest\n')
        self.assertTrue(verify_call(text)['runtime_validated'])
        for altered in (text.replace('data=840a', 'data=860b'),
                        text.replace('002cb3ca', '002cb2ea'),
                        text.replace('action=end', 'action=send'),
                        text.replace('r4=00000008', 'r4=00000009')):
            with self.assertRaises(ValueError):
                verify_call(altered)

    def test_pcm_requires_transport_and_stop(self):
        text = ('8850_call_physical: action=send\n'
                'dsp_control_write: data=860b pc=002cb3ca r4=00000008 rest\n'
                'dsp_hle: speech tick uplink=100 downlink=93 pcm=100 '
                'pcm_clock=1000000/8000 pcm_shape=125 rest\n'
                '8850_call_physical: action=end\n'
                'dsp_control_write: data=840a pc=002cb3ca r4=00000008 rest\n'
                'dsp_hle: speech stop control=040a uplink=345 downlink=338\n')
        self.assertEqual(verify_pcm(text)['downlink'], 338)
        for altered in (text.replace('pcm=100 ', 'pcm=99 '),
                        text.replace('8000', '0'),
                        text.replace('downlink=338', 'downlink=0'),
                        text + 'speech blocked by unsupported PCM link'):
            with self.assertRaises(ValueError):
                verify_pcm(altered)


if __name__ == '__main__':
    unittest.main()
