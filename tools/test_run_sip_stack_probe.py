import tempfile
from pathlib import Path
import unittest
import wave

from tools.run_sip_stack_probe import tone_energy, write_tone


class SipStackProbeTest(unittest.TestCase):
    def test_exact_tone_and_wrong_frequency(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'tone.wav'
            write_tone(path, 440)
            result = tone_energy(path, 440)
            self.assertGreater(result['tone_energy_fraction'], 0.99)
            with self.assertRaises(ValueError):
                tone_energy(path, 660)

    def test_silence_is_not_media_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'silence.wav'
            with wave.open(str(path), 'wb') as output:
                output.setparams((1, 2, 8000, 0, 'NONE', 'not compressed'))
                output.writeframes(bytes(16000))
            with self.assertRaises(ValueError):
                tone_energy(path, 440)

    def test_wrong_format_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'wrong.wav'
            with wave.open(str(path), 'wb') as output:
                output.setparams((1, 1, 8000, 0, 'NONE', 'not compressed'))
                output.writeframes(bytes(8000))
            with self.assertRaises(ValueError):
                tone_energy(path, 440)


if __name__ == '__main__':
    unittest.main()
