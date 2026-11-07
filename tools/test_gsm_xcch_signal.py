import unittest
from tools.gsm_xcch_signal import control_bits,coded_control,check_vector,TSC2


class XcchSourceTest(unittest.TestCase):
    def fixture(self):
        octets=bytes(23)
        coded=coded_control(octets)
        bursts=[list('000'+'0'*57+'1'+TSC2+'1'+'0'*57+'000') for _ in range(4)]
        for index,bit in enumerate(coded):
            position=2*((49*index)%57)+(index%8)//4
            bursts[index%4][3+position if position<57 else 88+position-57]=str(bit)
        return dict(octets=octets.hex(),tsc=2,information='0'*184,
                    coded=''.join(map(str,coded)),normal_bursts=[''.join(burst) for burst in bursts])

    def test_zero_block_parity_and_octet_bit_order(self):
        self.assertEqual(control_bits(bytes(23)),(0,)*184+(1,)*40+(0,)*4)
        self.assertEqual(control_bits(b'\x01'+bytes(22))[:8],(1,0,0,0,0,0,0,0))
        self.assertEqual(control_bits(b'\x80'+bytes(22))[:8],(0,0,0,0,0,0,0,1))
        self.assertTrue(check_vector(self.fixture())['source_bits_qualified'])

    def test_corrupted_source_layers_reject(self):
        for field in ('information','coded','normal_bursts'):
            vector=self.fixture()
            if field=='normal_bursts':
                vector[field][0]=vector[field][0][:3]+str(1-int(vector[field][0][3]))+vector[field][0][4:]
            else:
                vector[field]=str(1-int(vector[field][0]))+vector[field][1:]
            with self.subTest(field=field),self.assertRaises(ValueError):check_vector(vector)

    def test_bad_geometry_and_training_reject(self):
        for octets in (b'',bytes(22),bytes(24)):
            with self.assertRaises(ValueError):control_bits(octets)
        vector=self.fixture();vector['normal_bursts'][0]=vector['normal_bursts'][0][:61]+'1'+vector['normal_bursts'][0][62:]
        with self.assertRaises(ValueError):check_vector(vector)


if __name__=='__main__':unittest.main()
