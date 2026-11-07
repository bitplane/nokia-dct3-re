import unittest
import math

from tools.gsm_sch_signal import HYPERFRAME,TRAINING,sch_information_bits,sch_uncoded_bits,sch_coded_bits,sch_burst_bits,gaussian_phase_response,gmsk_burst_samples


class SchSignalTest(unittest.TestCase):
    def test_gaussian_pulse_integral_against_numerical_filter(self):
        sigma=math.sqrt(math.log(2))/(2*math.pi*0.3)
        def pulse(time):
            return (math.erf((time+0.5)/(sigma*math.sqrt(2)))-math.erf((time-0.5)/(sigma*math.sqrt(2))))/2
        for time in (-2,-0.5,0,0.5,2):
            steps=4000
            width=(time+8)/steps
            total=sum(pulse(-8+(index+0.5)*width) for index in range(steps))*width
            self.assertAlmostEqual(gaussian_phase_response(time),total,places=6)
        self.assertAlmostEqual(gaussian_phase_response(0),0.5)
        self.assertAlmostEqual(gaussian_phase_response(8),1)
        self.assertAlmostEqual(gaussian_phase_response(-8),0)

    def test_gmsk_constant_data_and_differential_frequency(self):
        tone=gmsk_burst_samples((1,)*32)
        for index,(i,q) in enumerate(tone):
            self.assertEqual((i,q),(round(1024*math.cos(math.pi*index/8)),round(1024*math.sin(math.pi*index/8))))
        alternating=gmsk_burst_samples(tuple(index&1 for index in range(32)))
        for index in range(32,96):
            a=complex(*alternating[index]); b=complex(*alternating[index+1])
            self.assertAlmostEqual(math.atan2((b*a.conjugate()).imag,(b*a.conjugate()).real),-math.pi/8,delta=0.002)

    def test_gmsk_phase_against_integrated_modulation_frequency(self):
        bits=(0,1,0,0,1,1,0,1)
        samples=gmsk_burst_samples(bits,8)
        sigma=math.sqrt(math.log(2))/(2*math.pi*0.3)
        def frequency(time):
            result=0
            for index in range(-8,len(bits)+9):
                current=bits[index] if 0<=index<len(bits) else 1
                previous=bits[index-1] if 0<=index-1<len(bits) else 1
                alpha=1-2*(current^previous)
                x=time-index
                result+=alpha*(math.erf((x+0.5)/(sigma*math.sqrt(2)))-math.erf((x-0.5)/(sigma*math.sqrt(2))))/2
            return result*math.pi/2
        for index in (0,8,28,63):
            time=index/8
            steps=1000
            phase=sum(frequency((step+0.5)*time/steps) for step in range(steps))*time/steps
            self.assertAlmostEqual(samples[index][0],1024*math.cos(phase),delta=0.51)
            self.assertAlmostEqual(samples[index][1],1024*math.sin(phase),delta=0.51)

    def test_sch_gmsk_has_constant_envelope_and_explicit_cadence(self):
        bits=sch_burst_bits(sch_information_bits(11,42))
        samples=gmsk_burst_samples(bits)
        self.assertEqual(len(samples),592)
        self.assertEqual(samples[0],(1024,0))
        for i,q in samples: self.assertAlmostEqual(math.hypot(i,q),1024,delta=0.71)
        for args in [([],4,1024),([2],4,1024),([0],1,1024),([0],4,32768)]:
            with self.subTest(args=args),self.assertRaises(ValueError): gmsk_burst_samples(*args)

    def test_frame_information_with_independent_bit_position_decoder(self):
        for t1 in (0,1,1023,2047):
            for multiframe in (0,12,25):
                for position in (1,11,21,31,41):
                    frame=t1*26*51+multiframe*51+position
                    for bsic in (0,1,42,63):
                        bits=sch_information_bits(frame,bsic)
                        def field(positions):
                            value=0
                            for at in positions: value=(value<<1)|bits[at]
                            return value
                        decoded_bsic=field((7,6,5,4,3,2))
                        decoded_t1=field((1,0,15,14,13,12,11,10,9,8,23))
                        decoded_t2=field((22,21,20,19,18))
                        decoded_t3=10*field((17,16,24))+1
                        reconstructed=26*51*decoded_t1+51*((decoded_t3-decoded_t2)%26)+decoded_t3
                        self.assertEqual((decoded_bsic,reconstructed),(bsic,frame))

    def test_frame_information_octets_and_bounds(self):
        for frame,bsic,expected in [(1,0,'00000400'),(11,42,'a8002c01'),(HYPERFRAME-10,63,'ffffc200')]:
            bits=sch_information_bits(frame,bsic)
            octets=bytes(sum(bits[index]<< (index%8) for index in range(start,min(start+8,25))) for start in (0,8,16,24))
            self.assertEqual(octets.hex(),expected)
        for frame,bsic in [(0,0),(2,0),(-1,0),(HYPERFRAME,0),(1,-1),(1,64),(True,0)]:
            with self.subTest(frame=frame,bsic=bsic),self.assertRaises(ValueError): sch_information_bits(frame,bsic)

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
