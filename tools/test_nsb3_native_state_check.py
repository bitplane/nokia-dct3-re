import hashlib
import unittest

from tools.nse2_native_state_check import check_state
from tools.test_nse2_native_state_check import trace


class Nsb3NativeStateTest(unittest.TestCase):
    def test_product_parameters_do_not_weaken_restoration(self):
        text = trace().replace('nse2_', 'nsb3_').replace('18.500000000', '40.000000000')
        card, pixels = bytes(3898), b'own Call log pixels'
        options = dict(prefix='nsb3', checkpoint='40.000000000', card_size=3898,
                       menu_frame=hashlib.sha256(pixels).hexdigest())
        result = check_state(text, card, card, pixels, pixels, **options)
        self.assertFalse(result['native_registration_verified'])
        for bad in (text.replace('restored=8180', 'restored=8179'),
                    text.replace('40.000000000', '18.500000000'),
                    text.replace('nsb3_state: completed=1', ''),
                    text.replace('nsb3_', 'nse2_')):
            with self.assertRaises(ValueError):
                check_state(bad, card, card, pixels, pixels, **options)
        with self.assertRaises(ValueError):
            check_state(text, card, card, pixels, b'other frame', **options)


if __name__ == '__main__':
    unittest.main()
