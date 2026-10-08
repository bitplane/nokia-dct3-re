import unittest
from unittest.mock import patch

from tools.noki6250_slow_pin_check import verify


TEXT = ''.join(f'6250_pin_physical: column={c} row={r} t={8+i}.000000\n'
               for i, (c, r) in enumerate([(2, 1), (3, 1), (4, 1), (2, 2), (1, 1)])) + (
    'header cla=a0 ins=20 p1=00 p2=01 p3=08 selected=7f40 t=12.024\n'
    'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1\n'
    '6250_pin_rssi_completion: caller=002d2141 message=00112704 t=10.365198\n'
    'network registered=1 arfcn=19 t=13.120000\n'
)


class SlowPinTest(unittest.TestCase):
    def setUp(self):
        registration = patch('tools.noki6250_slow_pin_check.registration')
        delivery = patch('tools.noki6250_slow_pin_check.delivery', return_value={
            'routed': 10.365085, 'message': 0x112704})
        self.registration = registration.start()
        delivery.start()
        self.addCleanup(registration.stop)
        self.addCleanup(delivery.stop)

    def test_success(self):
        verify(TEXT, b'card')
        self.registration.assert_called_once_with(TEXT, b'card', require_host=True)

    def test_local_acceptance_after_pin(self):
        text = TEXT.replace('network registered=1 arfcn=19 t=13.120000',
            'RX enqueue type=80 payload=34 producer=0ab data=801200000b1400130000030045050200f1100001170809101010325476982b2b2b2b t=13.120000')
        verify(text, b'card', require_host=False)
        self.registration.assert_called_once_with(text, b'card', require_host=False)
        for altered in (text.replace('t=13.120000', 't=11.120000'),
                        text.replace('00130000030045', '00140000030045'), TEXT):
            with self.subTest(text=altered), self.assertRaisesRegex(ValueError, 'follow late PIN'):
                verify(altered, b'card', require_host=False)

    def test_fast_input_rejected(self):
        with self.assertRaisesRegex(ValueError, 'slow entry'):
            verify(TEXT.replace('t=9.000000', 't=8.200000'), b'card')

    def test_bad_pin_rejected(self):
        with self.assertRaisesRegex(ValueError, 'CHV1'):
            verify(TEXT.replace('sw=9000', 'sw=9804'), b'card')

    def test_early_verification_rejected(self):
        with self.assertRaisesRegex(ValueError, 'follow the background'):
            verify(TEXT.replace('t=12.024', 't=9.024'), b'card')

    def test_wrong_completion_object_rejected(self):
        with self.assertRaisesRegex(ValueError, 'completion consumer'):
            verify(TEXT.replace('message=00112704', 'message=00112708'), b'card')

    def test_registration_before_verification_rejected(self):
        with self.assertRaisesRegex(ValueError, 'follow late PIN'):
            verify(TEXT.replace('t=13.120000', 't=11.120000'), b'card')


if __name__ == '__main__':
    unittest.main()
