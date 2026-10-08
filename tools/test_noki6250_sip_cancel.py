from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from PIL import Image
from tools import run_noki6250_sip_cancel as check


class SipCancelTest(unittest.TestCase):
    def test_restored_dialog_requires_fresh_epoch_and_no_media(self):
        good = {'passed': True, 'sip_status': 487, 'epoch': 2,
                'media': dict.fromkeys(('uplink', 'downlink', 'pcm_transmitted', 'pcm_received', 'dropped'), 0)}
        check.check_restored_dialog(good)
        for key, value in (('epoch', 1), ('sip_status', 200), ('passed', False), ('media', {})):
            with self.subTest(key=key), self.assertRaises(ValueError):
                check.check_restored_dialog({**good, key: value})
        with self.assertRaisesRegex(ValueError, 'produced media'):
            check.check_restored_dialog({**good, 'media': {**good['media'], 'uplink': 1}})

    def test_product_result_requires_own_coherence_and_one_setup(self):
        text = ('SETUP caller=5551234\nTX packet type=02 payload=20 '
                'radio_phase=release_channel_change data=041202001117001a600000130000001400000001\n'
                '6250_channel_confirmation: body=00 input=0409 expected=00 pending=00\n'
                'RX enqueue type=80 payload=34 data=600000000a4d000100001506210001f0\n'
                '6250_sip_cancel: physical Exit\n6250_raw_matrix_key: value=15\n')
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            (run / 'nvram/nhm3hle').mkdir(parents=True)
            (run / 'nvram/nhm3hle/sim_card').write_bytes(b'card')
            (run / 'console.log').write_text('completed')
            with patch.object(check, 'check_registration') as registration, \
                    patch.object(check, 'INCOMING_SETUP', re.compile('SETUP caller=5551234')), \
                    patch.object(check, 'check_frames'):
                (run / 'error.log').write_text(text)
                check.check_product_result(run)
                registration.assert_called_once_with(text, b'card')
                for broken in (text.replace('physical Exit', 'physical Answer'),
                               text.replace('value=15', 'value=06'),
                               text.replace('041202001117', '040000001117'),
                               text + 'GSM service uplink sapi=0 pd=03 message=07\n',
                               text + 'SETUP caller=5551234\n', text + '[LUA ERROR] failed\n'):
                    (run / 'error.log').write_text(broken)
                    with self.subTest(text=broken), self.assertRaises(ValueError):
                        check.check_product_result(run)

    def test_blank_and_wrong_geometry_frames_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            for size in ((96, 60), (84, 48)):
                for name in check.FRAMES:
                    Image.new('L', size, 255).save(path / name)
                with self.subTest(size=size), self.assertRaises(ValueError):
                    check.check_frames(path)

    def test_generic_runner_rejects_unproved_answered_media_and_restore(self):
        root = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory:
            for options in ([], ['--incoming'], ['--incoming', '--cancel-incoming', '--record-media'],
                            ['--incoming', '--cancel-incoming', '--restore-idle'],
                            ['--incoming', '--cancel-incoming', '--restore-call']):
                result = subprocess.run([sys.executable, str(root / 'tools/run_sip_handset_gate.py'),
                    '--pjsua', 'absent', '--run-dir', directory, '--product', '6250', *options],
                    capture_output=True, text=True)
                with self.subTest(options=options):
                    self.assertEqual(result.returncode, 2)
                    self.assertIn('limited to unanswered incoming CANCEL', result.stderr)


if __name__ == '__main__':
    unittest.main()
