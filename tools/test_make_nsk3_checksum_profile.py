import unittest
from unittest.mock import patch
from tools.make_nsk3_checksum_profile import repair,NATIVE_EEPROM


class Nsk3ChecksumTest(unittest.TestCase):
    def test_unqualified_source_rejected(self):
        for data in (b'',b'\xff'*2048,b'\xff'*8192):
            with self.assertRaises(ValueError):repair(data)

    def test_arithmetic_and_write_scope_under_mocked_source_guard(self):
        original=b'\xff'*8192
        with patch('tools.make_nsk3_checksum_profile.hashlib.sha256') as digest:
            digest.return_value.hexdigest.return_value=NATIVE_EEPROM
            for low in (False,True):
                output=repair(original,low)
                expected=bytearray(original)
                expected[0x11e:0x120]=bytes.fromhex('db24')
                if low:expected[0x3c:0x40]=bytes.fromhex('00003bc4')
                self.assertEqual(expected,output)
            changed=bytearray(original)
            changed[0x74:0x76]=b'\0\0'
            self.assertEqual(bytes.fromhex('db24'),repair(changed)[0x11e:0x120])


if __name__=='__main__':unittest.main()
