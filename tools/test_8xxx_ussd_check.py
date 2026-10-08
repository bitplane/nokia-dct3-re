import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from PIL import Image

from tools import noki8850_ussd_check, noki8890_ussd_check
from tools.test_radio_ussd_trace_check import GOOD


class OwnProductUssdTest(unittest.TestCase):
    def check(self, product, module, mutation=lambda text: text,
              invalid_storage=False, invalid_frame=False):
        inputs = ''.join(f'{product}_ussd_physical: key={key}\n' for key in module.KEYS)
        text = mutation(inputs + GOOD + f'{product}_ussd_physical: key=Back\n')
        storage = bytearray(1611)
        storage[1604:1609] = bytes.fromhex('00f1100001')
        if invalid_storage:
            storage[1610] = 1
        with tempfile.TemporaryDirectory() as directory:
            frames = Path(directory)
            frame = Image.new('L', (84, 48), 255)
            digest = hashlib.sha256(frame.tobytes()).hexdigest()
            for phase in ('result', 'after_back'):
                frame.save(frames / f'{product}_ussd_{phase}.png')
            with patch.object(module, 'RESULT', '0' * 64 if invalid_frame else digest), \
                    patch.object(module, 'IDLE', digest), \
                    patch.object(module, 'verify_registration') as registration:
                module.verify(text, frames, storage)
                registration.assert_called_once()

    def test_complete_and_negative_contracts(self):
        for product, module in (('8850', noki8850_ussd_check), ('8890', noki8890_ussd_check)):
            with self.subTest(product=product):
                self.check(product, module)
                for options in (
                    {'mutation': lambda text: text.replace('key=Keypad *', 'key=Keypad #')},
                    {'mutation': lambda text: text.replace('pd=0b message=3b', 'pd=0b message=3a')},
                    {'mutation': lambda text: text.replace('key=Back', 'key=Other')},
                    {'mutation': lambda text: '[LUA ERROR]\n' + text},
                    {'invalid_storage': True}, {'invalid_frame': True},
                ):
                    with self.subTest(options=options), self.assertRaises(ValueError):
                        self.check(product, module, **options)


if __name__ == '__main__':
    unittest.main()
