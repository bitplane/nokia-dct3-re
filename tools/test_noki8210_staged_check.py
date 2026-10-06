import unittest
from tools.noki8210_staged_check import verify

GOOD = '\n'.join((
    '8210_verifier_descriptor: pointer=0031bcf0',
    'release entry=0f00 words=223 prom_input=0006 clock=13000000 stage=verifier',
    'publication word0=0000 word1=0006 word2=0006 word3=0006',
    '8210_verifier_result: result0=0000 result1=0006',
    'release entry=0f00 words=126 prom_input=0006 clock=13000000 stage=loader',
    'request selector=0014 ack=0000',
    *(['request selector=0001'] * 133),
    'loader2_verified words=623 entry=0a00',
    'installed_program words=422 first=0590 last=0735',
    'outside_uploaded_code pc=2c75',
    'observation_halt pc=2c75 ownership_retained=1',
))


class StagedTest(unittest.TestCase):
    def test_complete(self):
        verify(GOOD)

    def test_wrong_extent(self):
        with self.assertRaises(ValueError):
            verify(GOOD.replace('words=623', 'words=613'))

    def test_missing_consumption(self):
        with self.assertRaises(ValueError):
            verify(GOOD.replace('result1=0006', 'result1=ffff'))

    def test_extra_chunk(self):
        with self.assertRaises(ValueError):
            verify(GOOD + '\nrequest selector=0001')

    def test_handoff_rejected(self):
        with self.assertRaises(ValueError):
            verify(GOOD + '\nruntime_hle_handoff')
