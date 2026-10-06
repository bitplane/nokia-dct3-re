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

    def test_runtime_discovery(self):
        runtime = GOOD.replace('observation_halt pc=2c75 ownership_retained=1',
                               'runtime_hle_handoff pc=2c75 native_suspended=1')
        runtime += '\nTX pending type=05 payload=10 data=1eff00d000030101e000'
        runtime += '\nRX enqueue type=8e data=1e0002d000030401c100'
        runtime += '\nTX pending type=05 payload=10 data=1e0200d0000305014100'
        verify(runtime, runtime=True)
        selftest = runtime + '\nTX pending type=70 payload=2 data=0d00'
        selftest += '\n8210_selftest_reply: command=0d faults=00 flag=84'
        selftest += '\n8210_service_return: command=0d faults=00/00/00'
        verify(selftest, runtime=True, selftest=True)
        with self.assertRaises(ValueError):
            verify(selftest.replace('faults=00/00/00', 'faults=00/10/00'),
                   runtime=True, selftest=True)
        with self.assertRaises(ValueError):
            verify(runtime.replace('0305014100', '0300014100'), runtime=True)

    def test_selftest_requires_runtime(self):
        with self.assertRaises(ValueError):
            verify(GOOD, selftest=True)
