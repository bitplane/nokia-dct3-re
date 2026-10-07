import array
import json
import math
import os
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest
import wave

from tools.sip_handset_waveform_check import inspect_tone, verify


class SipHandsetWaveformTest(unittest.TestCase):
    def test_3330_requires_own_provisioned_storage_before_audio_setup(self):
        repository = Path(__file__).resolve().parents[1]
        environment = dict(os.environ, SIP_PRODUCT='3330')
        environment.pop('SIP_WAVEFORM_NVRAM_DIR', None)
        result = subprocess.run(['bash', 'tools/run_sip_physical_audio_gate.sh'],
                                cwd=repository, env=environment, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('3330 requires its separately provisioned NVRAM', result.stderr)
        with tempfile.TemporaryDirectory() as directory:
            environment['SIP_WAVEFORM_NVRAM_DIR'] = directory
            result = subprocess.run(['bash', 'tools/run_sip_physical_audio_gate.sh'],
                                    cwd=repository, env=environment, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('missing 3330 provisioned storage', result.stderr)

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

    def test_result_names_the_tested_product(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.recording(root, [(440, 8000)] * 2).rename(root / 'sip-microphone.wav')
            self.recording(root, [(660, 8000)] * 2).rename(root / 'sip-earpiece.wav')
            for product in ('3210', '3310', '3330', '3410', '5210'):
                with self.subTest(product=product):
                    verify(root, product, 'incoming')
                    result = json.loads((root / 'sip-waveform-result.json').read_text())
                    self.assertTrue(result['scope'].startswith(f'{product} HLE'))
                    self.assertIn('not native DSP speech', result['scope'])
                    self.assertEqual(result['direction'], 'incoming')


if __name__ == '__main__':
    unittest.main()
