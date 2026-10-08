import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from PIL import Image

from tools import run_noki8890_sip_cancel as check


class CleanupFramesTest(unittest.TestCase):
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
