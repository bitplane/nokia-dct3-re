import array
import math
from pathlib import Path
import sys
import tempfile
import unittest
import wave

from tools.sip_handset_waveform_check import inspect_tone


class SipHandsetWaveformTest(unittest.TestCase):
    def recording(self, root, seconds):
        samples = array.array('h')
        for frequency, amplitude in seconds:
            samples.extend(round(amplitude * math.sin(2 * math.pi * frequency * index / 8000))
                           for index in range(8000))
        if sys.byteorder != 'little':
            samples.byteswap()
        path = Path(root) / 'recording.wav'
        with wave.open(str(path), 'wb') as output:
            output.setparams((1, 2, 8000, 0, 'NONE', 'not compressed'))
            output.writeframes(samples.tobytes())
        return path

    def test_sustained_tone_survives_louder_unrelated_beep(self):
        with tempfile.TemporaryDirectory() as root:
            path = self.recording(root, [(1000, 30000), (660, 8000), (660, 8000)])
            result = inspect_tone(path, 660)
            self.assertEqual(result['longest_seconds'], 2)
            self.assertGreater(result['windows'][0]['tone_energy_fraction'], 0.99)

    def test_silence_wrong_frequency_and_brief_tone_fail(self):
        with tempfile.TemporaryDirectory() as root:
            for seconds in ([(0, 0)] * 3, [(440, 8000)] * 3,
                            [(660, 8000), (0, 0), (660, 8000)]):
                with self.assertRaises(ValueError):
                    inspect_tone(self.recording(root, seconds), 660)

    def test_inaudible_tone_fails(self):
        with tempfile.TemporaryDirectory() as root:
            with self.assertRaises(ValueError):
                inspect_tone(self.recording(root, [(660, 50)] * 3), 660)

    def test_wrong_format_fails(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / 'wrong.wav'
            with wave.open(str(path), 'wb') as output:
                output.setparams((2, 2, 8000, 0, 'NONE', 'not compressed'))
                output.writeframes(bytes(64000))
            with self.assertRaises(ValueError):
                inspect_tone(path, 660)


if __name__ == '__main__':
    unittest.main()
