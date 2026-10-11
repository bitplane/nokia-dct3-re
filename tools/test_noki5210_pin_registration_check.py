import unittest
from unittest.mock import patch
from tools.make_sim_card_profile import make_profile
from tools.noki5210_pin_registration_check import verify, verify_late_boundary


def complete():
    return '\n'.join([
        *(f'5210_pin_physical: key=Keypad {digit}' for digit in '1234'),
        '5210_pin_physical: key=Left Softkey / Menu',
        'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
        'LAPDm Location Updating Accept acknowledged nr=1',
    ])


class PinRegistrationTest(unittest.TestCase):
    def late_trace(self):
        prefix = '\n'.join((
            'TX packet type=57 payload=4 words=3 radio_phase=serving_bcch data=03050000',
            'RX enqueue type=8b payload=166',
            '5210_measurement_entry: pc=0037019c',
            '5210_measurement_entry: pc=002e336c',
            '5210_measurement_entry: pc=002bd782',
            '5210_request_entry: pc=002e1f5e',
            '5210_request_entry: pc=002bd3d8 r0=00000005',
            '5210_request_entry: pc=002bd424',
            '5210_readiness_selector: argument=00000000 lr=00234f43 state=10 eligible=5 current=00149440 queued=00000000 t=10.209',
        ))
        return prefix + '\n' + complete().replace(
            'LAPDm Location Updating Accept acknowledged nr=1', '\n'.join((
                '5210_readiness_selector: argument=00000003 lr=00232069 state=6 eligible=5 current=00149440 queued=00149e90 t=21.025',
                '5210_readiness_root: final current=00149440 queued=00149e90 previous=00000000 context=00148380 writes=28 selections=2',
                '5210_pin_fixture: stopped_at=45.000000000')))

    def test_late_boundary(self):
        verify_late_boundary(self.late_trace(), make_profile(pin_enabled=True))

    def test_late_boundary_requires_consumption_and_full_window(self):
        for original, replacement in (
            ('pc=002bd782', 'pc=002bd784'),
            ('RX enqueue type=8b payload=166', 'RX missing'),
            ('pc=002bd3d8 r0=00000005', 'pc=002bd3d8 r0=00000004'),
            ('stopped_at=45.000000000', 'stopped_at=21.000000000'),
        ):
            with self.subTest(original=original), self.assertRaises(ValueError):
                verify_late_boundary(self.late_trace().replace(original, replacement), make_profile(pin_enabled=True))

    def test_late_boundary_rejects_new_radio_progress(self):
        with self.assertRaisesRegex(ValueError, 'boundary changed'):
            verify_late_boundary(self.late_trace() + '\nradio_phase=random_access', make_profile(pin_enabled=True))

    def test_late_request_ownership_rejects_missing_or_changed_evidence(self):
        for original, replacement in (
            ('selections=2', 'selections=3'), ('writes=28', 'writes=81'),
            ('argument=00000003', 'argument=00000000'),
            ('state=6 eligible=5', 'state=6 eligible=0'),
            ('final current=00149440 queued=00149e90', 'final current=00149440 queued=00000000'),
        ):
            with self.subTest(original=original), self.assertRaises(ValueError):
                verify_late_boundary(self.late_trace().replace(original, replacement), make_profile(pin_enabled=True))

    def test_complete(self):
        with patch('tools.noki5210_pin_registration_check.registration') as registration:
            verify(complete(), make_profile(pin_enabled=True))
        registration.assert_called_once_with(complete(), 'nsm5')

    def test_bad_pin_or_missing_physical_key(self):
        for text in (complete().replace('sw=9000', 'sw=9804'),
                     complete().replace('key=Keypad 3', 'key=Keypad 5'),
                     complete() + '\n[LUA ERROR]'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                verify(text, make_profile(pin_enabled=True))

    def test_registration_must_follow_pin(self):
        text = complete()
        registration = 'LAPDm Location Updating Accept acknowledged nr=1'
        text = registration + '\n' + text.replace(registration, '')
        with self.assertRaises(ValueError):
            verify(text, make_profile(pin_enabled=True))

    def test_unlocked_card_cannot_pass(self):
        with self.assertRaises(ValueError):
            verify(complete(), make_profile(pin_enabled=False))


if __name__ == '__main__':
    unittest.main()
