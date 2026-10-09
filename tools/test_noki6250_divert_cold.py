import unittest
from tools.run_dct3_divert_cold import check_query


class ColdQueryTests(unittest.TestCase):
    def trace(self, active=True):
        keys = ['Keypad *', 'Keypad #', 'Keypad 2', 'Keypad 1', 'Keypad #', 'Send']
        return '\n'.join(['6250_divert_physical: key=' + key for key in keys] + [
            f'gsm_ss: request=interrogate transaction=1b invoke=1 service=21 active={int(active)}',
            f'GSM service downlink kind=27 sapi=0 pd=0b message=2a length={21 if active else 17}',
            'radio_phase=release_deconfigure', '6250_divert_physical: key=Back'])

    def test_active_and_fresh_inactive_queries(self):
        check_query(self.trace(), True)
        check_query(self.trace(False), False)
        check_query(self.trace().replace('6250_', '6210_'), True, '6210')
        with self.assertRaisesRegex(ValueError, 'physical cold query'):
            check_query(self.trace(), True, '6210')

    def test_registration_cannot_substitute_for_restoration(self):
        with self.assertRaisesRegex(ValueError, 're-registered'):
            check_query('gsm_ss: request=register\n' + self.trace(), True)

    def test_boot_release_cannot_satisfy_query_release(self):
        with self.assertRaisesRegex(ValueError, 'RR release'):
            check_query('radio_phase=release_deconfigure\n' +
                        self.trace().replace('radio_phase=release_deconfigure', ''), True)

    def test_wrong_status_and_missing_physical_keys_fail(self):
        with self.assertRaisesRegex(ValueError, 'missing status'):
            check_query(self.trace(False), True)
        with self.assertRaisesRegex(ValueError, 'physical cold query'):
            check_query(self.trace().replace('key=Keypad 2', 'key=Keypad 3'), True)


if __name__ == '__main__':
    unittest.main()
