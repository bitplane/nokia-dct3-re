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
    def test_pcs_profile_rejects_other_product_before_audio_setup(self):
        repository = Path(__file__).resolve().parents[1]
        for product, value in (('8210', '1'), ('8890', 'yes')):
            environment = dict(os.environ, SIP_PRODUCT=product, SIP_PCS1900=value,
                               SIP_LATE_PIN_DCS='0')
            result = subprocess.run(['bash', 'tools/run_sip_physical_audio_gate.sh'],
                                    cwd=repository, env=environment, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('requires 8890' if value == '1' else 'must be 0 or 1', result.stderr)

    def test_pcs_result_requires_matching_band_direction_and_media(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.recording(root, [(440, 8000)] * 2).rename(root / 'sip-microphone.wav')
            self.recording(root, [(660, 8000)] * 2).rename(root / 'sip-earpiece.wav')
            valid = dict(machine='nsb6hle', scenario='incoming-sip-media',
                         laboratory_carrier=600, native_dsp_complete=False,
                         hle_media_tested=True, result='pass')
            (root / 'sip-result.json').write_text('{"passed": true}')
            (root / 'acceptance.json').write_text(json.dumps(valid))
            verify(root, '8890', 'incoming', pcs1900=True)
            self.assertEqual(json.loads((root / 'sip-waveform-result.json').read_text())[
                'laboratory_carrier'], 600)
            for key, value in (('laboratory_carrier', 60), ('scenario', 'outgoing-sip-media'),
                               ('hle_media_tested', False), ('native_dsp_complete', True)):
                (root / 'acceptance.json').write_text(json.dumps({**valid, key: value}))
                with self.subTest(key=key), self.assertRaisesRegex(ValueError, 'matching own-band'):
                    verify(root, '8890', 'incoming', pcs1900=True)
            (root / 'acceptance.json').write_text(json.dumps(valid))
            (root / 'sip-result.json').write_text('{"passed": false}')
            with self.assertRaisesRegex(ValueError, 'SIP media acceptance'):
                verify(root, '8890', 'incoming', pcs1900=True)

    def test_late_pin_profile_is_product_explicit_before_audio_setup(self):
        repository = Path(__file__).resolve().parents[1]
        for product, value, error in (('8850', '1', 'requires 8210'),
                                       ('8890', '1', 'requires 8210'),
                                       ('8210', 'yes', 'must be 0 or 1')):
            environment = dict(os.environ, SIP_PRODUCT=product, SIP_LATE_PIN_DCS=value)
            result = subprocess.run(['bash', 'tools/run_sip_physical_audio_gate.sh'],
                                    cwd=repository, env=environment, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(error, result.stderr)

    def test_research_product_invalid_direction_is_rejected_before_audio_setup(self):
        repository = Path(__file__).resolve().parents[1]
        for product in ('8850', '8890'):
            environment = dict(os.environ, SIP_PRODUCT=product, SIP_DIRECTION='invalid')
            result = subprocess.run(['bash', 'tools/run_sip_physical_audio_gate.sh'],
                                    cwd=repository, env=environment, capture_output=True, text=True)
            with self.subTest(product=product):
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('unsupported SIP waveform direction', result.stderr)

    def test_audio_defaults_are_captured_before_modules_and_restored_on_failure(self):
        repository = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            commands = root / 'commands'
            state = root / 'loaded'
            pactl = root / 'pactl'
            pactl.write_text('''#!/bin/bash
echo "$*" >> "$TEST_COMMANDS"
case "$1" in
    get-default-sink) if [[ -f "$TEST_LOADED" ]]; then echo temporary; else echo original-sink; fi ;;
    get-default-source) if [[ -f "$TEST_LOADED" ]]; then echo temporary.monitor; else echo original-source; fi ;;
    load-module) touch "$TEST_LOADED"; echo 42 ;;
esac
''')
            pactl.chmod(0o755)
            for name in ('ffmpeg', 'python3'):
                executable = root / name
                executable.write_text('#!/bin/bash\nexit 1\n')
                executable.chmod(0o755)
            environment = dict(os.environ, SIP_PRODUCT='3410', RUN_DIR=str(root / 'run'),
                               PATH=f'{root}:{os.environ["PATH"]}', TEST_COMMANDS=str(commands),
                               TEST_LOADED=str(state))
            result = subprocess.run(['bash', 'tools/run_sip_physical_audio_gate.sh'],
                                    cwd=repository, env=environment, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            calls = commands.read_text().splitlines()
            self.assertEqual(calls[:2], ['get-default-sink', 'get-default-source'])
            self.assertIn('set-default-sink original-sink', calls)
            self.assertIn('set-default-source original-source', calls)
            self.assertEqual(calls.count('unload-module 42'), 2)
            self.assertEqual(calls[-2:], ['set-default-sink original-sink',
                                         'set-default-source original-source'])

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
            for product in ('3210', '3310', '3330', '3410', '5210', '8210', '8290', '8850', '8890'):
                with self.subTest(product=product):
                    verify(root, product, 'incoming')
                    result = json.loads((root / 'sip-waveform-result.json').read_text())
                    self.assertTrue(result['scope'].startswith(f'{product} HLE'))
                    self.assertIn('not native DSP speech', result['scope'])
                    self.assertEqual(result['direction'], 'incoming')

    def test_8290_requires_own_storage_before_audio_setup(self):
        repository = Path(__file__).resolve().parents[1]
        environment = dict(os.environ, SIP_PRODUCT='8290')
        environment.pop('SIP_WAVEFORM_NVRAM_DIR', None)
        result = subprocess.run(['bash', 'tools/run_sip_physical_audio_gate.sh'],
                                cwd=repository, env=environment, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('8290 requires its own qualified', result.stderr)
        with tempfile.TemporaryDirectory() as directory:
            environment['SIP_WAVEFORM_NVRAM_DIR'] = directory
            result = subprocess.run(['bash', 'tools/run_sip_physical_audio_gate.sh'],
                                    cwd=repository, env=environment, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('missing 8290 own qualified storage', result.stderr)


if __name__ == '__main__':
    unittest.main()
