import unittest
from tools.run_noki8850_pin_registration import check_pin_inputs


class PinRegistrationTest(unittest.TestCase):
    def fixture(self):
        records = []
        for key, decoded in (('Keypad 1', '01'), ('Keypad 2', '02'),
                             ('Keypad 3', '03'), ('Keypad 4', '04'), ('Menu', '19')):
            records.extend(['8850_pin_physical: key=' + key,
                            '8850_keypad_decoded key=' + decoded])
        records.extend(['SIM status ins=20 sw=9000',
                        'LAPDm Location Updating Accept acknowledged nr=1'])
        return '\n'.join(records)

    def test_physical_pin_precedes_registration(self):
        check_pin_inputs(self.fixture())

    def test_host_key_without_decode_is_insufficient(self):
        with self.assertRaisesRegex(ValueError, 'did not decode'):
            check_pin_inputs(self.fixture().replace('decoded key=04', 'decoded key=05'))

    def test_rejected_pin_is_not_registration(self):
        with self.assertRaisesRegex(ValueError, 'physical PIN acceptance'):
            check_pin_inputs(self.fixture().replace('sw=9000', 'sw=9804'))

    def test_registration_before_pin_is_not_success(self):
        text = self.fixture()
        event = 'LAPDm Location Updating Accept acknowledged nr=1'
        with self.assertRaisesRegex(ValueError, 'physical PIN acceptance'):
            check_pin_inputs(event + '\n' + text.replace(event, ''))


if __name__ == '__main__':
    unittest.main()
