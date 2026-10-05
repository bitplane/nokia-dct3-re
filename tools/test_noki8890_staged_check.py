import unittest
from tools.noki8890_staged_check import verify

GOOD = '\n'.join((
    'release entry=0f00 words=223 prom_input=0006 clock=13000000 stage=verifier',
    'publication word0=0000 word1=0006 word2=0006 word3=0006',
    'release entry=0f00 words=126 prom_input=0006 clock=13000000 stage=loader',
    'request selector=0014 ack=0000',
    *(['request selector=0001'] * 118),
    'loader2_verified words=613 entry=0a00',
    'outside_uploaded_code pc=2c75',
    'observation_halt pc=2c75 ownership_retained=1',
))


class StagedTest(unittest.TestCase):
    def test_complete(self):
        verify(GOOD)

    def test_wrong_product_request_count(self):
        with self.assertRaises(ValueError):
            verify(GOOD + '\nrequest selector=0001')

    def test_missing_native_publication(self):
        with self.assertRaises(ValueError):
            verify(GOOD.replace('publication word0=', 'synthetic word0='))

    def test_unexpected_handoff(self):
        with self.assertRaises(ValueError):
            verify(GOOD + '\nruntime_hle_handoff')
