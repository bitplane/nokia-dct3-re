import unittest
from unittest.mock import patch
from tools import noki6250_power_check as checker


def fixture():
    ticks = ''.join(f'ccont_rtc: event=second time=12:00:{s} day=0 t={s}.000000\n'
                    for s in range(32, 43))
    return ('6250_power_physical: step=1 pressed=1 t=25\n'
            '6250_power_physical: step=2 pressed=0 t=29\n'
            'ccont_power: event=off t=31.665598\n' + ticks +
            '6250_power_physical: step=3 pressed=1 t=43\n'
            'ccont_power: event=wake cause=02 t=43\n'
            'ccont_power: event=cause_read data=13 t=43.1\n'
            '6250_power_physical: step=4 pressed=0 t=45\n'
            'read-binary fid=6f7e offset=0 length=11\n'
            'TX packet type=1b data=0080013f4905087200f110000133080910101032547698\n'
            'gsm_call_adapter: network registered=1 arfcn=19\n'
            f'6250_power_endpoint: phase=restart faults={"00" * 24}\n'
            f'6250_power_endpoint: phase=settled faults={"00" * 24}\n')


class PowerTests(unittest.TestCase):
    def verify(self, text):
        with patch.object(checker, 'check_cold') as cold, patch.object(
                checker, 'check_uploads') as uploads, patch.object(
                checker, 'check_registration') as registration:
            checker.verify(text, b'storage')
            cold.assert_called_once()
            uploads.assert_called_once()
            registration.assert_called_once_with(
                registration.call_args.args[0], 'nhm3', preserved=True, configured_carrier=True)

    def test_warm_contract_is_explicit(self):
        self.verify(fixture())

    def test_powered_activity_rejected(self):
        with self.assertRaisesRegex(ValueError, 'powered endpoint'):
            self.verify(fixture().replace('ccont_power: event=off t=31.665598\n',
                'ccont_power: event=off t=31.665598\ndspif_transport: RX enqueue\n'))

    def test_bad_or_missing_nv_endpoint_rejected(self):
        for text in (fixture().replace('phase=settled', 'phase=other'),
                     fixture().replace('faults=' + '00' * 24,
                                       'faults=' + '00' * 12 + '01' + '00' * 11)):
            with self.assertRaisesRegex(ValueError, 'NV fault'):
                self.verify(text)

    def test_wrong_cause_and_rtc_gap(self):
        for text in (fixture().replace('wake cause=02', 'wake cause=04'),
                     fixture().replace('time=12:00:37 day=0 t=37.000000',
                                       'time=12:00:39 day=0 t=37.000000')):
            with self.assertRaises(ValueError):
                self.verify(text)

    def test_lua_failure(self):
        with self.assertRaisesRegex(ValueError, 'observer'):
            self.verify(fixture() + '[LUA ERROR]')


if __name__ == '__main__':
    unittest.main()
