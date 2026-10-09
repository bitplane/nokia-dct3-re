import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from PIL import Image

from tools import run_noki8890_sip_cancel as check


class CleanupFramesTest(unittest.TestCase):
    def test_alerting_restore_requires_exact_architecture_and_order(self):
        state = 'pc=00412345 sp=00170000 ram=12345678 t=52.000000000'
        text = ('incoming state id=1 epoch=1 phase=alerting\n'
                '8890_state: event=saved ' + state + '\nsip_state: saved\n'
                '8890_state: event=restored ' + state + '\nsip_state: restored\n'
                'state_roundtrip: result=pass scenario=8890_incoming_alerting\n'
                '8890_sip_cancel: physical Exit\n')
        check.check_alerting_restoration(text)
        for invalid in (text.replace('event=restored pc=00412345', 'event=restored pc=00412346'),
                        text.replace('phase=alerting', 'phase=connected'),
                        text.replace('sip_state: restored', ''),
                        text + '8890_state: FAIL incomplete\n'):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                check.check_alerting_restoration(invalid)

    def test_outgoing_requires_own_carrier_send_and_idle_pixels(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            (run / 'snap').mkdir()
            (run / 'console.log').write_text('')
            image = Image.new('L', (84, 48), 255)
            digest = hashlib.sha256(image.crop((0, 8, 84, 48)).tobytes()).hexdigest()
            for name in ('8890_registered_idle.png', '8890_after_outgoing_call.png'):
                image.save(run / 'snap' / name)
            valid = ('gsm_call_adapter: network registered=1 arfcn=60\n'
                     '8890_call_physical: action=send\n8890_keypad_decoded: key=0e\n')
            with patch.object(check, 'verify_stage'), patch.dict(
                    check.FRAMES, {'8890_sip_registered_idle.png': digest}):
                (run / 'error.log').write_text(valid)
                check.check_outgoing_result(run)
                for invalid in (valid.replace('arfcn=60', 'arfcn=1'),
                                valid.replace('key=0e', 'key=0f')):
                    (run / 'error.log').write_text(invalid)
                    with self.assertRaises(ValueError):
                        check.check_outgoing_result(run)
                (run / 'error.log').write_text(valid)
                Image.new('L', (84, 48), 0).save(run / 'snap/8890_after_outgoing_call.png')
                with self.assertRaisesRegex(ValueError, 'reviewed idle'):
                    check.check_outgoing_result(run)

    def test_restored_call_requires_new_epoch_and_post_load_paging(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            (run / 'console.log').write_text('')
            restored = 'state_roundtrip: result=pass scenario=8890_idle\n'
            paging = 'gsm_call_adapter: incoming state id=1 epoch=2\n'
            with patch.object(check, 'verify_stage'), patch('tools.noki8890_state_check.verify'):
                for text, epoch, message in ((paging + restored, 2, 'follow exact'),
                                             (restored + paging, 1, 'fresh host epoch')):
                    (run / 'error.log').write_text(text)
                    (run / 'sip-result.json').write_text(json.dumps({'epoch': epoch}))
                    with self.assertRaisesRegex(ValueError, message):
                        check.check_product_result(run, restore_idle=True)

    def product_result(self, mutation=lambda text: text, bad_storage=False):
        setup = '80000000000000000000032445030504046002008134015c0581551532f4'
        text = ('gsm_call_adapter: network registered=1 arfcn=60\n'
                'RX enqueue type=80 payload=34 producer=abc data=' + setup + '\n'
                '8890_sip_cancel: physical Exit\n8890_keypad_decoded: key=1a\n')
        storage = bytearray(1611)
        storage[1604:1609] = bytes.fromhex('00f1100001')
        storage[1610] = int(bad_storage)
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            (run / 'nvram/nsb6hle').mkdir(parents=True)
            (run / 'nvram/nsb6hle/sim_card').write_bytes(storage)
            (run / 'console.log').write_text('')
            (run / 'error.log').write_text(mutation(text))
            with patch.object(check, 'verify_stage') as stage, patch.object(check, 'check_frames'):
                check.check_product_result(run)
                stage.assert_called_once_with(mutation(text), runtime=True, selftest=True)

    def test_product_boundary_is_not_just_exit_marker(self):
        self.product_result()
        for options in (
            {'mutation': lambda text: text.replace('arfcn=60', 'arfcn=600')},
            {'mutation': lambda text: text.replace('0581551532f4', '0581551532f5')},
            {'mutation': lambda text: text.replace('key=1a', 'key=19')},
            {'mutation': lambda text: text.replace('physical Exit', 'physical Other')},
            {'bad_storage': True},
        ):
            with self.subTest(options=options), self.assertRaises(ValueError):
                self.product_result(**options)

    def check(self, size=(84, 48), bad_frame=False):
        with tempfile.TemporaryDirectory() as directory:
            frames = Path(directory)
            image = Image.new('L', size, 255)
            digest = hashlib.sha256(image.crop((0, 8, 84, 48)).tobytes()).hexdigest()
            expected = {name: digest for name in check.FRAMES}
            for name in expected:
                image.save(frames / name)
            if bad_frame:
                expected['8890_sip_missed_call.png'] = '0' * 64
            with patch.object(check, 'FRAMES', expected):
                check.check_frames(frames)

    def test_reviewed_content_and_geometry_required(self):
        self.check()
        for options in ({'size': (96, 60)}, {'bad_frame': True}):
            with self.subTest(options=options), self.assertRaises(ValueError):
                self.check(**options)

    def test_missing_notification_is_not_cleanup(self):
        with tempfile.TemporaryDirectory() as directory, self.assertRaises(OSError):
            check.check_frames(Path(directory))


if __name__ == '__main__':
    unittest.main()
