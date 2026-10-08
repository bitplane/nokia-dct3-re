import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from PIL import Image

from tools import noki8xxx_divert_check as check
from tools.test_radio_call_divert_trace_check import GOOD


class OwnProductDivertTest(unittest.TestCase):
    def check(self, product, mutation=lambda text: text, bad_storage=False):
        keys = ''.join(f'{product}_divert_physical: key={key}\n' for key in check.KEYS)
        text = mutation(keys + GOOD + f'{product}_divert_physical: key=Back\n')
        storage = bytearray(1611)
        storage[1604:1609] = bytes.fromhex('00f1100001')
        storage[1610] = int(bad_storage)
        with tempfile.TemporaryDirectory() as directory:
            frames = Path(directory)
            frame = Image.new('L', (84, 48), 255)
            digest = hashlib.sha256(frame.tobytes()).hexdigest()
            for phase in ('result', 'after_back'):
                frame.save(frames / f'{product}_divert_{phase}.png')
            with patch.dict(check.PROFILES, {product: ('test', digest, digest)}), \
                    patch.object(check, 'verify_8850_registration') as nsm2, \
                    patch.object(check, 'verify_8890_registration') as nsb6:
                check.verify(text, frames, storage, product)
                if product == '8850':
                    nsm2.assert_called_once_with(text, 'nsm2')
                    nsb6.assert_not_called()
                else:
                    nsb6.assert_called_once_with(text)
                    nsm2.assert_not_called()

    def test_complete_and_negative_contracts(self):
        for product in ('8850', '8890'):
            with self.subTest(product=product):
                self.check(product)
                for options in (
                    {'mutation': lambda text: text.replace('key=Keypad *', 'key=Keypad 0')},
                    {'mutation': lambda text: text.replace('service=21', 'service=22')},
                    {'mutation': lambda text: text.replace('active=0', 'active=1')},
                    {'mutation': lambda text: text.replace('key=Back', 'key=Other')},
                    {'mutation': lambda text: '[LUA ERROR]\n' + text},
                    {'bad_storage': True},
                ):
                    with self.subTest(options=options), self.assertRaises(ValueError):
                        self.check(product, **options)

    def test_unknown_product(self):
        with self.assertRaisesRegex(ValueError, 'unsupported'):
            check.verify('', Path('.'), b'', '8210')


if __name__ == '__main__':
    unittest.main()
