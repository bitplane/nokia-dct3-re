import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from PIL import Image

from tools import run_noki8850_sip_cancel as check


class SipCancelTest(unittest.TestCase):
    def test_media_cannot_share_cancel_restore_or_failure(self):
        for options in (['--outgoing-media', '--restore-idle'],
                        ['--incoming-media', '--restore-idle'],
                        ['--incoming-media', '--outgoing-media'],
                        ['--record-media'],
                        ['--restore-outgoing'],
                        ['--restore-incoming'],
                        ['--restore-phase', 'alerting'],
                        ['--outgoing-media', '--restore-incoming'],
                        ['--incoming-media', '--restore-incoming', '--record-media'],
                        ['--incoming-media', '--restore-outgoing'],
                        ['--outgoing-media', '--restore-outgoing', '--record-media'],
                        ['--outgoing-media', '--outgoing-busy'],
                        ['--outgoing-media', '--outgoing-unavailable']):
            with self.subTest(options=options), \
                    patch('sys.argv', ['runner', 'unused', '--pjsua', 'unused', *options]), \
                    self.assertRaises(SystemExit) as result:
                check.main()
            self.assertEqual(result.exception.code, 2)

    def test_outgoing_send_must_follow_physical_input_on_own_carrier(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            (run / 'console.log').write_text('')
            valid = ('gsm_call_adapter: network registered=1 arfcn=1\n'
                     '8850_call_physical: action=send\n8850_keypad_decoded key=0e\n')
            with patch.object(check, 'check_trace', return_value=[]), \
                    patch.object(check, 'verify_registration'), \
                    patch('tools.noki8850_outgoing_call_check.verify_frames'):
                (run / 'error.log').write_text(valid)
                check.check_outgoing_result(run)
                for invalid in (valid.replace('arfcn=1', 'arfcn=13'),
                                valid.replace('key=0e', 'key=0f'),
                                '8850_keypad_decoded key=0e\n' + valid.replace(
                                    '8850_keypad_decoded key=0e\n', '')):
                    (run / 'error.log').write_text(invalid)
                    with self.assertRaises(ValueError):
                        check.check_outgoing_result(run)

    def test_idle_restore_requires_fresh_epoch_after_load(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            (run / 'console.log').write_text('')
            restored = 'state_roundtrip: result=pass scenario=8850_idle\n'
            paging = 'gsm_call_adapter: incoming state id=1 epoch=2\n'
            with patch.object(check, 'check_trace', return_value=[]), \
                    patch.object(check, 'check_correlated_inputs', return_value=[]), \
                    patch('tools.noki8850_state_check.verify'), \
                    patch('tools.noki8850_state_check.check_frames'):
                for text, epoch, message in ((paging + restored, 2, 'follow exact'),
                                             (restored + paging, 1, 'fresh host epoch')):
                    (run / 'error.log').write_text(text)
                    (run / 'sip-result.json').write_text(json.dumps({'epoch': epoch}))
                    with self.assertRaisesRegex(ValueError, message):
                        check.check_product_result(run, restore_idle=True)

    def test_own_geometry_and_reviewed_content(self):
        with tempfile.TemporaryDirectory() as directory:
            frames = Path(directory)
            image = Image.new('L', (84, 48), 255)
            digest = hashlib.sha256(image.crop((0, 8, 84, 48)).tobytes()).hexdigest()
            expected = dict.fromkeys(check.FRAMES, digest)
            for name in expected:
                image.save(frames / name)
            with patch.object(check, 'FRAMES', expected):
                check.check_frames(frames)
                Image.new('L', (96, 60), 255).save(frames / '8850_sip_missed_call.png')
                with self.assertRaises(ValueError):
                    check.check_frames(frames)
            image.save(frames / '8850_sip_missed_call.png')
            expected['8850_sip_missed_call.png'] = '0' * 64
            with patch.object(check, 'FRAMES', expected), self.assertRaises(ValueError):
                check.check_frames(frames)

    def product(self, mutation=lambda text: text, status=0, stage_errors=()):
        text = ('gsm_call_adapter: network registered=1 arfcn=1\n'
                'RX enqueue type=80 payload=34 data=80000000000000000000032445030504046002008134015c0581551532f4\n'
                '8850_sip_cancel: physical Exit\n8850_keypad_decoded key=1a\n')
        storage = bytearray(1611)
        storage[1604:1609] = bytes.fromhex('00f1100001')
        storage[1610] = status
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            (run / 'nvram/nsm2hle').mkdir(parents=True)
            (run / 'nvram/nsm2hle/sim_card').write_bytes(storage)
            (run / 'console.log').write_text('')
            (run / 'error.log').write_text(mutation(text))
            with patch.object(check, 'check_trace', return_value=list(stage_errors)), \
                    patch.object(check, 'check_correlated_inputs', return_value=[]), \
                    patch.object(check, 'verify_registration') as registration, \
                    patch.object(check, 'check_frames'):
                check.check_product_result(run)
                registration.assert_called_once_with(mutation(text), 'nsm2')

    def test_product_checks_are_not_just_cancel_marker(self):
        self.product()
        for options in (
            {'mutation': lambda text: text.replace('arfcn=1', 'arfcn=10')},
            {'mutation': lambda text: text.replace('0581551532f4', '0581551532f5')},
            {'mutation': lambda text: text.replace('key=1a', 'key=19')},
            {'mutation': lambda text: text.replace('physical Exit', 'physical Other')},
            {'status': 1}, {'stage_errors': ('missing exclusive HLE handoff',)},
        ):
            with self.subTest(options=options), self.assertRaises(ValueError):
                self.product(**options)


if __name__ == '__main__':
    unittest.main()
