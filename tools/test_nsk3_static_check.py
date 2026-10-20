import unittest
from tools.nsk3_static_check import bootstrap_identity,peripheral_contract,PERIPHERAL_FRAGMENTS


class Nsk3StaticTest(unittest.TestCase):
    def test_peripherals_are_own_variant_anchored(self):
        for variant,shift in (('boot4',0),('boot3',-0x10)):
            image=bytearray(0x100000)
            image[5]=0xf4
            image[0xe4:0xec]=bytes.fromhex('e28f0001e12fff10')
            image[0x16c:0x170]=bytes.fromhex('0010fa5c')
            for address,encoded in PERIPHERAL_FRAGMENTS.values():
                start=address+shift-0x200000
                expected=bytes.fromhex(encoded)
                image[start:start+len(expected)]=expected
            result=peripheral_contract(image,variant)
            self.assertEqual(8192,result['eeprom_geometry']['capacity_bytes'])
            self.assertFalse(result['physical_wiring_or_timing_proved'])
            for address,_ in PERIPHERAL_FRAGMENTS.values():
                changed=bytearray(image)
                changed[address+shift-0x200000]^=1
                with self.assertRaises(ValueError):peripheral_contract(changed,variant)
            changed=bytearray(image)
            changed[5]=0x92
            with self.assertRaises(ValueError):peripheral_contract(changed,variant)

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
