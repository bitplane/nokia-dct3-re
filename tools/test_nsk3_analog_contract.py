import unittest
from tools.nsk3_analog_contract import FRAGMENTS,inspect


class AnalogContractTest(unittest.TestCase):
    def image(self):
        # Synthetic opcode fixture tests extraction, not firmware qualification.
        image=bytearray(0x100000)
        for address,encoded in FRAGMENTS.items():
            data=bytes.fromhex(encoded)
            image[address-0x200000:address-0x200000+len(data)]=data
        return image

    def test_contract_and_each_anchor(self):
        image=self.image()
        result=inspect(image)
        self.assertEqual(result['source7_channel'],2)
        self.assertEqual(result['saturated_identity_conversion'],6614)
        self.assertEqual(result['signed_inclusive_limits'],[1800,5500])
        self.assertFalse(result['physical_units_proved'])
        for address in FRAGMENTS:
            bad=bytearray(image)
            bad[address-0x200000]^=1
            with self.assertRaises(ValueError):inspect(bad)


if __name__=='__main__':unittest.main()
