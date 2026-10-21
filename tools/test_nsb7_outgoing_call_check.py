"""Positive and negative NSB-7 outgoing signalling fixtures."""
import unittest

from tools.nsb7_outgoing_call_check import check


def fixture():
    steps = [(20, 'Keypad 5'), (21, 'Keypad 5'), (22, 'Keypad 5'),
             (23, 'Keypad 1'), (24, 'Keypad 2'), (25, 'Keypad 3'),
             (26, 'Keypad 4'), (28, 'Send'), (40, 'End')]
    lines = [f'nsb7_outgoing_call_input: key={key} state={state} t={second}.{fraction}'
             for second, key in steps[:-1]
             for state, fraction in [('1', '000000000'), ('0', '200000000')]]
    lines += ['GSM service establish sapi=0 pd=05 message=24 length=16 data=05247103205881080910101032547698',
              'GSM service downlink kind=5 sapi=0 pd=05 message=21 length=2',
              'GSM service uplink sapi=0 pd=03 message=05 length=18 data=03450404600201805e0581551532f4150101',
              'GSM service downlink kind=10 sapi=0 pd=03 message=02 length=2',
              'GSM service downlink kind=14 sapi=0 pd=06 message=2e length=8',
              'GSM service uplink sapi=0 pd=06 message=29 length=3',
              'GSM service downlink kind=11 sapi=0 pd=03 message=01 length=2',
              'GSM service downlink kind=12 sapi=0 pd=03 message=07 length=2',
              'GSM service uplink sapi=0 pd=03 message=0f length=2',
              'nsb7_outgoing_call_input: key=End state=1 t=40.000000000',
              'GSM service uplink sapi=0 pd=03 message=25 length=5',
              'GSM service downlink kind=25 sapi=0 pd=03 message=2d length=2',
              'GSM service uplink sapi=0 pd=03 message=2a length=2',
              'LAPDm service Channel Release acknowledged',
              'nsb7_outgoing_call_input: key=End state=0 t=40.200000000',
              'nsb7_radio_tx: index=86 type=02 length=20 data=0412020200000016600002580000000001297000',
              'nsb7_radio_tx: index=88 type=57 length=4 data=03050000',
              'nsb7_outgoing_call_done: t=50.000000000']
    return '\n'.join(lines)


class OutgoingCallTest(unittest.TestCase):
    def test_signalling_without_speech(self):
        check(fixture())

    def test_missing_checkpoints(self):
        for token in ['key=Send', 'message=24', 'message=21', 'message=05',
                      'message=02', 'message=2e', 'message=29', 'message=01',
                      'message=07', 'message=0f', 'key=End', 'message=25',
                      'message=2d', 'message=2a', 'Release acknowledged',
                      'type=02', 'type=57', 't=50.']:
            with self.subTest(token=token), self.assertRaises(ValueError):
                check(fixture().replace(token, 'incorrect'))

    def test_wrong_called_number(self):
        with self.assertRaises(ValueError):
            check(fixture().replace('5e0581551532f4', '5e0581551532f5'))

    def test_duplicate_setup(self):
        setup = next(line for line in fixture().splitlines() if 'message=05' in line)
        with self.assertRaises(ValueError):
            check(fixture() + '\n' + setup)

    def test_incoming_page_is_not_outgoing_evidence(self):
        with self.assertRaises(ValueError):
            check(fixture() + '\nPCH IMSI page transmitted')


if __name__ == '__main__':
    unittest.main()
