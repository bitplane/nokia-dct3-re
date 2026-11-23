import unittest

from tools.make_rom6_record_profile import PRODUCTS, replace_low_records
from tools.noki6250_pmm_check import record_checksum
from tools.run_rom6_lab_call import MODELS, check_inputs, check_registration, schedule

NSM2 = PRODUCTS['nsm2']
EEPROM = NSM2.eeprom
BASE = {'source': 0x26}

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
            check_inputs('rom6_call_input: key=Keypad 1 t=12.000', schedule(MODELS['nsm2'], 'lab')[0])

    def test_replaces_only_low_records_and_sum(self):
        before = image()
        after = replace_low_records(NSM2, bytes(before), b'\x01' * 0x110)
        at = EEPROM + BASE['source']
        self.assertEqual(after[at:at + 0x110], b'\x01' * 0x110)
        self.assertEqual(after[at + 0x11c:at + 0x120], (0x110).to_bytes(4, 'big'))
        self.assertEqual(after[:at], before[:at])
        self.assertEqual(after[at + 0x120:], before[at + 0x120:])

    def test_keeps_a_valid_envelope_valid(self):
        before = image()
        payload = slice(EEPROM + 0x26, EEPROM + 0x26 + NSM2.length)
        before[EEPROM + 0x21] = record_checksum(0, before[payload])
        after = replace_low_records(NSM2, bytes(before), b'\x03' * 0x110)
        self.assertEqual(after[EEPROM + 0x21], record_checksum(0, after[payload]))
        before[EEPROM + 0x21] = 2
        self.assertEqual(replace_low_records(NSM2, bytes(before), b'\x03' * 0x110)[EEPROM + 0x21], 2)

    def test_rejects_wrong_sum_and_size(self):
        before = image()
        before[EEPROM + BASE['source'] + 0x11f] = 1
        with self.assertRaises(ValueError):
            replace_low_records(NSM2, bytes(before), b'\x01' * 0x110)
        with self.assertRaises(ValueError):
            replace_low_records(NSM2, bytes(image()), b'\x01' * 0x10f)


if __name__ == '__main__':
    unittest.main()
