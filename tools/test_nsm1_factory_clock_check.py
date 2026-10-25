import math
from pathlib import Path
import unittest

from tools import nsm1_factory_clock_check as checker
from tools.nsm1_native_state_check import check_storage,signature


def words(frequency=1750):
    result=[]
    for i in range(16000):
        time=85_000_062_500+i*125000
        value=0x55
        if 86_140_000_000<=time<86_350_000_000:
            value=0x92 if math.sin(2*math.pi*frequency*i/8000)>0 else 0xb8
        result.append((value,time))
    return result


class FactoryClockTest(unittest.TestCase):
    def test_periodic_raw_bytes_not_assumed_analog_pcm(self):
        result=checker.measure_wave(words())
        self.assertEqual(result['active_samples'],1680)
        self.assertGreater(result['centered_raw_byte_projection'],.80)
        self.assertFalse(result['analog_encoding_proved'])

    def test_wrong_frequency_flat_data_and_clock_discontinuity_are_rejected(self):
        good=words()
        for bad in (words(900),[(0x55,t) for _,t in good],
                    [(v,t+1) for v,t in good],good[:100]+good[101:],
                    good[:100]+[good[100]]+good[100:]):
            with self.subTest(count=len(bad)),self.assertRaises(ValueError):
                checker.measure_wave(bad)

    def test_shared_own_product_validation_rejects_undeclared_machines(self):
        for function in (check_storage,signature):
            with self.subTest(function=function.__name__),self.assertRaises(ValueError):
                function(Path('unused'),machine='donor')

    def test_timestamp_precision_and_malformed_input_fail_cleanly(self):
        self.assertEqual(checker.nanoseconds('86.000062500'),86_000_062_500)
        for value in ('1.2.3','86.0000000001'):
            with self.subTest(value=value),self.assertRaises(ValueError):
                checker.nanoseconds(value)


if __name__=='__main__':
    unittest.main()
