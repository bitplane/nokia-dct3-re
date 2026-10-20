import unittest
from tools.nsk3_static_check import bootstrap_identity


class Nsk3StaticTest(unittest.TestCase):
    def test_bootstrap_identity_is_instruction_anchored(self):
        for identity in (3,4):
            payload=bytearray(372)
            payload[24:30]=bytes.fromhex('76f8 0803')+identity.to_bytes(2,'big')
            self.assertEqual(bootstrap_identity(payload),identity)
            for offset in (24,26,28):
                bad=bytearray(payload)
                bad[offset]^=0x80
                with self.assertRaises(ValueError):bootstrap_identity(bad)
            for bad in (payload[:-1],payload+b'\0\0'):
                with self.assertRaises(ValueError):bootstrap_identity(bad)


if __name__=='__main__':unittest.main()
