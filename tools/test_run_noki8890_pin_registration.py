import unittest
from tools.run_noki8890_pin_registration import check_pin_inputs


class PinRegistrationTest(unittest.TestCase):
    def fixture(self):
        lines = []
        for key, code in (('Keypad 1', '01'), ('Keypad 2', '02'),
                          ('Keypad 3', '03'), ('Keypad 4', '04'), ('Menu', '19')):
            lines += ['8890_pin_physical: key=' + key,
                      '8890_keypad_decoded: key=' + code]
        measurement = ['TX packet type=57 payload=4 words=3 data=01140000',
                       'RX enqueue type=8b payload=166 producer=1 data=0010003c00c4',
                       '8890_band_rx: object=001168e8',
                       '8890_band_parse: object=001168e8 arfcn=003c rssi=c4']
        return '\n'.join(measurement + lines + ['SIM status ins=20 sw=9000',
                                 'LAPDm Location Updating Accept acknowledged nr=1'])

    def test_physical_pin_then_registration(self):
        check_pin_inputs(self.fixture())

    def test_missing_decode(self):
        with self.assertRaisesRegex(ValueError, 'physical PIN decode'):
            check_pin_inputs(self.fixture().replace('key=04', 'key=05'))

    def test_rejected_pin(self):
        with self.assertRaisesRegex(ValueError, 'physical PIN acceptance'):
            check_pin_inputs(self.fixture().replace('sw=9000', 'sw=9804'))

    def test_registration_before_acceptance(self):
        event = 'LAPDm Location Updating Accept acknowledged nr=1'
        with self.assertRaisesRegex(ValueError, 'physical PIN acceptance'):
            check_pin_inputs(event + '\n' + self.fixture().replace(event, ''))

    def test_unrelated_measurement_object(self):
        with self.assertRaisesRegex(ValueError, 'consumer correlation'):
            check_pin_inputs(self.fixture().replace('parse: object=001168e8', 'parse: object=001168ec'))


if __name__ == '__main__':
    unittest.main()
