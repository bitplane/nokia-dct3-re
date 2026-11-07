import unittest

from tools.gsm_sch_signal import TRAINING,sch_uncoded_bits,sch_coded_bits,sch_burst_bits


class SchSignalTest(unittest.TestCase):
    def test_independent_osmocom_channel_coding_vector(self):
        # Published test data, not implementation code:
        # https://github.com/osmocom/libosmocore/blob/master/tests/coding/coding_test.ok
        # "Encoding: 01 02 03 00", accessed 2026-11-07; packed bits are LSB-first.
        packed=bytes.fromhex('01020300')
        information=tuple((packed[index//8]>>(index%8))&1 for index in range(25))
        expected='110100111100000000110100111100001110011100110000000011101010001101011100110000'
        self.assertEqual(''.join(map(str,sch_coded_bits(information))),expected)

    def test_independent_polynomial_division_and_tail(self):
        for information in [(0,)*25,(1,)*25,*[tuple(int(index==bit) for index in range(25)) for bit in range(25)]]:
            uncoded=sch_uncoded_bits(information)
            work=list(uncoded[:35])
            generator=[1,0,1,0,1,1,1,0,1,0,1]
            for index in range(25):
                if work[index]:
                    for offset,value in enumerate(generator): work[index+offset]^=value
            self.assertEqual(work[-10:],[1]*10)
            self.assertEqual(uncoded[-4:],(0,)*4)

    def test_burst_placement_and_normative_training(self):
        information=(0,)*25
        burst=sch_burst_bits(information)
        coded=sch_coded_bits(information)
        self.assertEqual(len(burst),148)
        self.assertEqual(burst[:3]+burst[-3:],(0,)*6)
        self.assertEqual(burst[3:42]+burst[106:145],coded)
        self.assertEqual(burst[42:106],TRAINING)
        self.assertEqual(''.join(map(str,TRAINING)),f'{0xb962040f2d45761b:064b}')

    def test_information_shape_is_explicit(self):
        for value in [[],[0]*24,[0]*26,[2]*25,[0.0]*25]:
            with self.subTest(value=value),self.assertRaises(ValueError): sch_coded_bits(value)


if __name__=='__main__': unittest.main()
