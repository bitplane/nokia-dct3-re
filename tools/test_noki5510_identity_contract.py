import unittest

from tools.noki5510_identity_contract import wire_identity, display_identity, check_formatting, identity_contract


def decode_mobile_identity(data):
    digits = [data[0] >> 4]
    for byte in data[1:]:
        digits.extend((byte & 15,byte >> 4))
    if not data[0] & 8:
        assert digits.pop() == 15
    assert all(digit <= 9 for digit in digits)
    return data[0] & 7, ''.join(map(str,digits))


def fixture():
    return '\n'.join([
        '5510_identity_scope: entry=002a05cc cap=16 identity=debugger_entry operands=context_only observer_writes=0',
        '5510_nv_read: ordinal=42 offset=0000000c destination=0012eadc bytes=00000008 observer_writes=0',
        '5510_identity_entry: ordinal=1 selector=1 sp=001279c0 source=0012eadc destination=0012ea6d packed=ffffffffffffffff observer_writes=0',
        '5510_identity_return: ordinal=1 selector=1 address=002a060a sp=001279b0 source=0012eadc destination=0012ea6d output=faffffffffffff0f observer_writes=0',
        '5510_identity_terminal: entries=1 cap=16 observer_writes=0'])


class IdentityContractTests(unittest.TestCase):
    def test_independent_identity_value_decode(self):
        packed = bytes.fromhex('12345678901234ff')
        self.assertEqual(wire_identity(packed,1).hex(),'1a32547698103204')
        self.assertEqual(wire_identity(packed,2).hex(),'1332547698103204f6')
        self.assertEqual(decode_mobile_identity(wire_identity(packed,1)),(2,'123456789012340'))
        for svn in range(100):
            self.assertEqual(decode_mobile_identity(wire_identity(packed,2,svn)),
                             (3,'12345678901234'+f'{svn:02d}'))
        for last in range(256):
            self.assertEqual(wire_identity(packed[:7]+bytes([last]),1),wire_identity(packed,1))

    def test_qualified_display_forms(self):
        packed = bytes.fromhex('12345678901234ff')
        self.assertEqual(display_identity(packed,1),'12345678901234')
        self.assertEqual(display_identity(packed,3),'123456789012347')
        self.assertEqual(display_identity(packed,5),'123456789012340')
        self.assertEqual(display_identity(packed,9),'1234567890123406')
        for flags in (0,2,15):
            with self.assertRaises(ValueError):
                display_identity(packed,flags)
        with self.assertRaises(ValueError):
            display_identity(b'\xff'*8,3)

    def test_erased_runtime_is_negative_identity_not_provisioning(self):
        result = check_formatting(fixture(),{'software_version_decimal':6})
        self.assertFalse(result['calls'][0]['decimal_domain_valid'])
        self.assertEqual(result['calls'][0]['wire'],'faffffffffffff0f')

    def test_reject_changed_frames_scope_or_bytes(self):
        for before,after in [('cap=16','cap=1'),('entries=1','entries=2'),
                             ('output=faff','output=0aff'),('sp=001279b0','sp=001279b2'),
                             ('address=002a060a','address=002a063e'),
                             ('bytes=00000008','bytes=00000007'),
                             ('offset=0000000c','offset=00000014'),
                             ('destination=0012ea6d','destination=0022ea6d'),
                             ('observer_writes=0','observer_writes=1')]:
            with self.subTest(before=before),self.assertRaises(ValueError):
                check_formatting(fixture().replace(before,after),{'software_version_decimal':6})

    def test_reject_unsupported_reference_or_source(self):
        with self.assertRaises(ValueError):
            wire_identity(bytes(7),1)
        with self.assertRaises(ValueError):
            wire_identity(bytes(8),2,100)
        with self.assertRaises(ValueError):
            identity_contract(bytes(0x350000))


if __name__ == '__main__':
    unittest.main()
