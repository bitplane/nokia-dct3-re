from pathlib import Path
import json
import re
import tempfile
import unittest
from unittest.mock import patch
from PIL import Image
from tools import run_noki8210_sip_cancel as check


def sample():
    text = 'gsm_call_adapter: network registered=1 arfcn=4\nSETUP caller=5551234\n'
    for name, code in [('Keypad ' + str(key), f'{key:02x}') for key in range(1, 6)] + [('Menu', '19')]:
        text += f'8210_security_physical: key={name} t=12\n8210_keypad_decoded: key={code}\n'
    return text + '8210_sip_cancel: physical Exit\n8210_keypad_decoded: key=1a\n'


class SipCancelTest(unittest.TestCase):
    def verify(self, text, *, restore_idle=False, epoch=2):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            (run / 'error.log').write_text(text)
            (run / 'console.log').write_text('completed')
            (run / 'sip-result.json').write_text(json.dumps({'epoch': epoch}))
            (run / 'nvram/nsm3hle').mkdir(parents=True)
            (run / 'nvram/nsm3hle/sim_card').write_bytes(b'card')
            with patch.object(check, 'verify_stage') as stage, \
                    patch.object(check, 'verify_registration') as registration, \
                    patch.object(check, 'INCOMING_SETUP', re.compile('SETUP caller=5551234')), \
                    patch.object(check, 'check_frames'), \
                    patch('tools.noki8210_state_check.verify') as state, \
                    patch('tools.noki8210_state_check.check_frames') as frames:
                check.check_product_result(run, restore_idle)
                stage.assert_called_once_with(text, runtime=True, selftest=True, base_record=True)
                registration.assert_called_once_with(text, b'card', configured_carrier=True)
                if restore_idle:
                    state.assert_called_once_with(text, sip_cancel=True)
                    frames.assert_called_once_with(run / 'snap', sip_cancel=True)
                else:
                    state.assert_not_called()

    def test_restored_idle_requires_fresh_epoch_after_restore(self):
        restored = 'state_roundtrip: result=pass scenario=8210_idle\n'
        paging = 'gsm_call_adapter: incoming state id=1 epoch=2 phase=paging\n'
        text = restored + paging + sample()
        self.verify(text, restore_idle=True)
        for broken in (paging + restored + sample(), paging + sample()):
            with self.assertRaisesRegex(ValueError, 'follow exact idle'):
                self.verify(broken, restore_idle=True)
        with self.assertRaisesRegex(ValueError, 'existed before idle'):
            self.verify('gsm_call_adapter: request id=2 epoch=1 digits=123\n' + text,
                        restore_idle=True)
        with self.assertRaisesRegex(ValueError, 'fresh host epoch'):
            self.verify(text, restore_idle=True, epoch=1)

    def test_declared_base_record_and_own_carrier_security_exit(self):
        self.verify(sample())

    def test_wrong_carrier_missing_security_or_duplicate_setup_rejected(self):
        for text in (sample().replace('arfcn=4', 'arfcn=1'),
                     sample().replace('key=03', 'key=04'),
                     sample().replace('physical Exit', 'physical Answer'),
                     sample().replace('key=1a', 'key=0e'),
                     sample() + 'SETUP caller=5551234\n',
                     sample() + '[LUA ERROR] failed\n'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                self.verify(text)

    def test_blank_wrong_geometry_and_unreviewed_frames_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            frames = Path(directory)
            for name in check.FRAMES:
                Image.new('L', (84, 48), 255).save(frames / name)
            with self.assertRaises(ValueError):
                check.check_frames(frames)
            Image.new('L', (96, 60), 255).save(frames / next(iter(check.FRAMES)))
            with self.assertRaises(ValueError):
                check.check_frames(frames)


if __name__ == '__main__':
    unittest.main()
