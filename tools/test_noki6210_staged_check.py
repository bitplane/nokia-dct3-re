import unittest
from tools.noki6210_staged_check import verify

GOOD = '\n'.join((
    'release entry=0f00 words=223 prom_input=0006 clock=13000000 stage=verifier',
    'publication word0=0000 word1=0006 word2=0006 word3=0006',
    '6210_verifier_result: result0=0000 result1=0006',
    'release entry=0f00 words=126 prom_input=0006 clock=13000000 stage=loader',
    'request selector=0014 ack=0000',
    *(['request selector=0001'] * 156),
    'loader2_verified words=629 entry=0a00',
    'installed_program words=422 first=0590 last=0735',
    'outside_uploaded_code pc=2c75',
    'observation_halt pc=2c75 ownership_retained=1',
))


class StagedCheckTest(unittest.TestCase):
    def test_complete(self):
        verify(GOOD)

    def test_sibling_extent_rejected(self):
        with self.assertRaises(ValueError):
            verify(GOOD.replace('words=629', 'words=623'))

    def test_missing_chunk_rejected(self):
        with self.assertRaises(ValueError):
            verify(GOOD.replace('request selector=0001', '', 1))

    def test_missing_consumption_rejected(self):
        with self.assertRaises(ValueError):
            verify(GOOD.replace('result1=0006', 'result1=ffff'))

    def test_accidental_handoff_rejected(self):
        with self.assertRaises(ValueError):
            verify(GOOD + '\nruntime_hle_handoff')

    def test_runtime(self):
        text = GOOD.replace('observation_halt pc=2c75 ownership_retained=1',
                            'runtime_hle_handoff pc=2c75 native_suspended=1')
        text += '\nTX pending type=05 payload=10 data=1eff00d000030101e000'
        text += '\nRX enqueue data=1e0002d000030401c100'
        text += '\nTX pending type=05 payload=10 data=1e0200d0000305014100'
        verify(text, runtime=True)
        text += '\nTX pending type=70 payload=2 data=0d00'
        text += '\n6210_selftest_reply: command=0d faults=00 flag=c4'
        text += '\n6210_service_return: command=0d faults=00/00/00'
        text += '\n6210_analog_window: accepted=01'
        verify(text, runtime=True, selftest=True)
        with self.assertRaises(ValueError):
            verify(text.replace('accepted=01', 'accepted=00'), runtime=True, selftest=True)


if __name__ == '__main__':
    unittest.main()
