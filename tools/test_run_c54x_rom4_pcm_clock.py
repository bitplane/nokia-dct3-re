import unittest

from tools.run_c54x_rom4_pcm_clock import instruction_report


CAPTURE = ("[opcov] op=6f83 first_pc=0800 count=1\n"
           "[opdecode] op=6f83 supported=0 words=0\n"
           "[opwords] op=6f83 words=2 ext1=0c1e ext2=0000 first_pc=010800 count=1\n"
           "[optupledecode] op=6f83 words=2 ext1=0c1e ext2=0000 supported=1 decoded_words=2\n"
           "[opcov] op=f47f first_pc=a68c count=20\n"
           "[opdecode] op=f47f supported=1 words=1\n")


class NativePcmInstructionReportTests(unittest.TestCase):
    def test_decoder_observations_are_not_arithmetic_assertions(self):
        result = instruction_report(CAPTURE)
        self.assertEqual(result['opcodes'], 2)
        self.assertEqual(result['groups'], 2)
        self.assertEqual(len(result['sha256']), 64)
        self.assertFalse(result['independent_arithmetic_assertion_claim'])

    def test_missing_unsupported_and_truncated_records_fail(self):
        for bad in (CAPTURE.replace('[opdecode] op=f47f supported=1 words=1\n', ''),
                    CAPTURE.replace('op=f47f supported=1 words=1', 'op=f47f supported=0 words=0'),
                    CAPTURE.replace('ext1=0c1e ext2=0000 first_pc=010800 count=1', 'ext1=0c1e ext2=0000 first_pc=010800 count=2'),
                    CAPTURE.replace('supported=1 decoded_words=2', 'supported=0 decoded_words=0'),
                    CAPTURE.replace('[optupledecode]', '[lostdecode]')):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                instruction_report(bad)


if __name__ == '__main__':
    unittest.main()
