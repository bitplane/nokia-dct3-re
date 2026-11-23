import unittest

from tools.make_nsm2_record_profile import BASE, EEPROM, replace_low_records
from tools.run_nsm2_call import check_inputs, check_registration

EXCHANGE = '\n'.join((
    'npe3_raw_agch: ra=0f', 'npe3_bts_uplink: start_frame=1 octets=013f490508', 'npe3_raw_ua: x',
    'npe3_raw_lu_accept: x', 'npe3_bts_uplink: start_frame=2 octets=032101', 'npe3_raw_release: x',
    'npe3_raw_disc_ua: x', 'npe3_bts_page_access: x', 'npe3_raw_call_setup: x', 'npe3_raw_assignment: x'))


def image():
    """A minimal version-3 journal holding only the base record."""
    pmm = bytearray(b'\xff' * 0x30000)
    pmm[EEPROM:EEPROM + 0x20] = bytes.fromhex('f0f0fff80001454550524f4d00000002ffff0000000000020003ffffffffffff')
    pmm[EEPROM + 0x20:EEPROM + 0x26] = bytes.fromhex('000280000000')
    at = EEPROM + BASE['source']
    pmm[at:at + 0x8000] = bytes(0x8000)
    return pmm


class CallTest(unittest.TestCase):
    def test_exchange_order(self):
        check_registration(EXCHANGE)
        lines = EXCHANGE.split('\n')
        for missing in range(len(lines)):
            with self.assertRaises(ValueError):
                check_registration('\n'.join(lines[:missing] + lines[missing + 1:]))
        with self.assertRaises(ValueError):
            check_registration('\n'.join(reversed(lines)))

    def test_inputs_must_complete(self):
        with self.assertRaises(ValueError):
            check_inputs('nsm2_call_input: key=Keypad 1 t=12.000')

    def test_replaces_only_low_records_and_sum(self):
        before = image()
        after = replace_low_records(bytes(before), b'\x01' * 0x110)
        at = EEPROM + BASE['source']
        self.assertEqual(after[at:at + 0x110], b'\x01' * 0x110)
        self.assertEqual(after[at + 0x11c:at + 0x120], (0x110).to_bytes(4, 'big'))
        self.assertEqual(after[:at], before[:at])
        self.assertEqual(after[at + 0x120:], before[at + 0x120:])

    def test_rejects_wrong_sum_and_size(self):
        before = image()
        before[EEPROM + BASE['source'] + 0x11f] = 1
        with self.assertRaises(ValueError):
            replace_low_records(bytes(before), b'\x01' * 0x110)
        with self.assertRaises(ValueError):
            replace_low_records(bytes(image()), b'\x01' * 0x10f)


if __name__ == '__main__':
    unittest.main()
