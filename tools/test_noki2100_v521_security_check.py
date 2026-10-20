import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from PIL import Image

from tools import noki2100_v521_security_check as check


class SecurityInteractionTests(unittest.TestCase):
    def text(self):
        rows = ['dsp_hle: bootstrap completion exchanges=992 publications=3']
        for index, key in enumerate(['Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad 4', 'Keypad 5', 'Left Softkey / Menu']):
            time = 8 + index * 0.5
            rows += [f'nam2_v521_security_physical: index={index + 1} key={key} t={time}',
                     f'keypad_route: state=1 before=000 after=001 mask=aa ctrl=05 pc=002fb714 t={time}',
                     f'keypad_route: state=0 before=000 after=000 mask=aa ctrl=05 pc=002fe4b4 t={time + 0.001}']
        return '\n'.join(rows)

    def test_early_contacts_are_physically_serviced(self):
        check.check_input(self.text())

    def test_active_low_mask_is_not_inverted(self):
        with self.assertRaisesRegex(ValueError, 'unmasked IRQ0'):
            check.check_input(self.text().replace('mask=aa', 'mask=ab'))

    def test_assertion_alone_is_not_input_acceptance(self):
        with self.assertRaisesRegex(ValueError, 'firmware IRQ0 service'):
            check.check_input(self.text().replace('state=0', 'state=1'))

    def test_changed_or_incomplete_keys_fail(self):
        for text in [self.text().replace('Keypad 5', 'Keypad 6'), self.text().replace('t=8.0', 't=14.0'),
                     self.text().replace('exchanges=992', 'exchanges=64'), self.text() + '\n[LUA ERROR]']:
            with self.assertRaises(ValueError):
                check.check_input(text)

    def test_frame_pixels_and_dimensions_are_checked(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            image = Image.new('RGB', (96, 65), 'white')
            image.save(directory / 'frame.png')
            expected = hashlib.sha256(image.tobytes()).hexdigest()
            with patch.object(check, 'FRAMES', {'frame.png': expected}):
                check.check_frames(directory)
                Image.new('RGB', (96, 65), 'black').save(directory / 'frame.png')
                with self.assertRaises(ValueError):
                    check.check_frames(directory)
                Image.new('RGB', (84, 48), 'white').save(directory / 'frame.png')
                with self.assertRaises(ValueError):
                    check.check_frames(directory)

    def test_runtime_storage_must_retain_own_executable(self):
        image = bytes(0x200000)
        flash = bytearray(image)
        flash[0x1FC142:0x1FC146] = bytes.fromhex('0000a0aa')
        with patch.object(check, 'V521_SHA1', hashlib.sha1(image).hexdigest()):
            check.check_storage(image, flash)
            changed = bytearray(flash)
            changed[0x100] = 1
            with self.assertRaisesRegex(ValueError, 'executable'):
                check.check_storage(image, changed)
            with self.assertRaisesRegex(ValueError, 'executable'):
                check.check_storage(image, flash[:-1])


if __name__ == '__main__':
    unittest.main()
