import gzip
import unittest
from tools.nam1_installer_audit import inspect


def stream(address, payload):
    return b'\x0b' + address.to_bytes(3, 'big') + b'\x00' + len(payload).to_bytes(3, 'big') + b'\x00' + payload


class Nam1InstallerAuditTest(unittest.TestCase):
    def test_members_compare_and_preserve_outside_product_data(self):
        source = b'MZ' + gzip.compress(b'installer text', mtime=0)
        source += gzip.compress(stream(0x200000, b'ab'), mtime=0)
        source += gzip.compress(stream(0x5f0000, b'product state'), mtime=0)
        result = inspect(source, b'ab')
        self.assertEqual(result['gzip_members'], 3)
        self.assertFalse(result['members'][0]['wintesla_records'])
        self.assertEqual(result['members'][1]['different_existing_bytes'], 0)
        self.assertEqual(result['members'][2]['outside_image_bytes'], 13)
        self.assertFalse(result['product_state_proved'])

    def test_mismatches_and_truncated_member_are_not_silently_accepted(self):
        source = gzip.compress(stream(0x200000, b'ab'), mtime=0)
        self.assertEqual(inspect(source, b'ac')['members'][0]['different_existing_bytes'], 1)
        with self.assertRaises(ValueError):
            inspect(source[:-3], b'ab')


if __name__ == '__main__':
    unittest.main()
