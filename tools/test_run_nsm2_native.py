import unittest

from tools.run_nsm2_native import LOADER2, RESIDENT, check_execution


class Native8850EvidenceTest(unittest.TestCase):
    def setUp(self):
        self.flash = bytearray(0x120000)
        self.flash[LOADER2[0]:LOADER2[0] + 1226] = bytes([0xa5]) * 1226
        self.flash[RESIDENT[2]:RESIDENT[2] + 844] = bytes(range(256)) * 3 + bytes(76)
        self.loader = bytes(self.flash[LOADER2[0]:LOADER2[0] + 1226])
        self.program = bytearray(0x20000)
        self.program[0x590 * 2:0x736 * 2] = self.flash[RESIDENT[2]:RESIDENT[2] + 844]
        self.text = '\n'.join((
            'nsm2_native_composition: hle=0 staged=0 clock_policy=45500000 fitted_mask_claim=0 observer_writes=0',
            'nsm2_native_entry: kind=reset observer_writes=0',
            'nsm2_native_entry: kind=loader observer_writes=0',
            'nsm2_native_verifier: result0=0000 result1=0006 observer_writes=0',
            'nsm2_native_entry: kind=reset observer_writes=0',
            'nsm2_native_entry: kind=loader observer_writes=0',
            'nsm2_native_entry: kind=resident observer_writes=0',
            '8850_service_reply class=74 command=0d status=00',
            'nsm2_native_terminal: t=30.016667 arm=002e6438 dsp=003999 illegal=0 idle=1 pmst=07ac observer_writes=0',
            'nsm2_native_coverage: missing_program_reads=0 observer_writes=0',
        ))

    def check(self, text=None, loader=None, program=None):
        check_execution(self.text if text is None else text,
                        self.loader if loader is None else loader,
                        self.program if program is None else program, self.flash)

    def test_complete_own_uploads(self):
        self.check()

    def test_restarts_faults_and_illegal_execution_reject(self):
        for text in (self.text + '\nnsm2_native_entry: kind=reset observer_writes=0',
                     self.text + '\nnsm2_native_mcu_reset: t=31.965116 pc=00309d9e lr=002439d7 observer_writes=0',
                     self.text + '\n' + self.text,
                     self.text.replace('illegal=0', 'illegal=1'),
                     self.text.replace('status=00', 'status=01'),
                     self.text.replace('result1=0006', 'result1=ffff'),
                     self.text.replace('missing_program_reads=0', 'missing_program_reads=1'),
                     self.text.replace('hle=0', 'hle=1')):
            with self.subTest(text=text[-80:]), self.assertRaises(ValueError):
                self.check(text)

    def test_foreign_uploads_reject(self):
        loader = bytearray(self.loader)
        loader[0] ^= 1
        program = bytearray(self.program)
        program[0x590 * 2 + 10] ^= 1
        for kwargs in ({'loader': bytes(loader)}, {'program': program}, {'program': self.program[:-2]}):
            with self.subTest(kwargs=list(kwargs)), self.assertRaises(ValueError):
                self.check(**kwargs)


if __name__ == '__main__':
    unittest.main()
