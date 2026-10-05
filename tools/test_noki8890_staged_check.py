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

RUNTIME = GOOD.replace('observation_halt pc=2c75 ownership_retained=1',
                       'runtime_hle_handoff pc=2c75 native_suspended=1') + '\n' + '\n'.join((
    '8890_service_encoder: command=0032 argument=3fff commit=0001',
    'TX pending type=05 payload=10 data=1eff00d000030101e000',
    'RX enqueue type=8e payload=10 producer=086 data=1e0002d000030101e000',
    'RX enqueue type=8e payload=10 producer=08c data=1e0002d000030401c100',
    'TX pending type=05 payload=10 data=1e0200d0000305014100',
    'TX pending type=70 payload=6 data=1304eca05beb',
    'TX pending type=70 payload=26 data=16184bc0613636443a235d792fd4ba7e6b71defabf9b19e00f43',
    'TX pending type=70 payload=2 data=0d00',
))

SELFTEST = RUNTIME + '\n' + '\n'.join((
    'RX enqueue type=74 payload=2 producer=08e data=0d00',
    '8890_selftest_reply: command=0d faults=00 flag=c4',
    '8890_service_return: command=0d faults=00/00/00',
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

    def test_runtime_discovery(self):
        verify(RUNTIME, runtime=True)

    def test_runtime_requires_native_handoff(self):
        with self.assertRaises(ValueError):
            verify(RUNTIME.replace('native_suspended=1', 'native_suspended=0'), runtime=True)

    def test_runtime_requires_firmware_ack(self):
        with self.assertRaises(ValueError):
            verify(RUNTIME.replace('data=1e0200d0000305014100', 'data=unknown'), runtime=True)

    def test_selftest_consumer(self):
        verify(SELFTEST, runtime=True, selftest=True)

    def test_selftest_requires_consumer(self):
        with self.assertRaises(ValueError):
            verify(SELFTEST.replace('faults=00/00/00', 'faults=00/10/00'), runtime=True, selftest=True)
