import unittest
from tools.gsm_rach_signal import coded_access,decode_p27_access

WORDS=(0xb9ff,0x1a4b,0x33fd,0x0ab3,0x9c78,0x1f89,0xaf9f,0x033a,
       0xfffe,0x1fff,0xffff,0x1fff,0xffff,0xffff,0x01ff)


class AccessCodingTest(unittest.TestCase):
    def test_native_words_match_standard_codeword_without_report_input(self):
        self.assertEqual(''.join(map(str,coded_access(11,42))),'111001000111111111100111110101110011')
        self.assertEqual(decode_p27_access(WORDS,42),dict(ra=11,framing_offset=9,active_bits=88,coded_bits=36))

    def test_each_coded_bit_error_is_rejected(self):
        for index in range(58,94):
            changed=list(WORDS)
            changed[index//12]^=1<<(index%12)
            with self.subTest(index=index),self.assertRaises(ValueError):
                decode_p27_access(changed,42)

    def test_framing_tail_bsic_and_length_reject(self):
        for index in (9,17,94):
            changed=list(WORDS);changed[index//12]^=1<<(index%12)
            with self.assertRaises(ValueError):decode_p27_access(changed,42)
        for words in (WORDS[:-1],WORDS+(0,),(-1,)+WORDS[1:],(65536,)+WORDS[1:]):
            with self.assertRaises(ValueError):decode_p27_access(words,42)
        with self.assertRaises(ValueError):decode_p27_access(WORDS,43)
        for ra,bsic in ((-1,42),(256,42),(True,42),(11,64),(11,-1)):
            with self.assertRaises(ValueError):coded_access(ra,bsic)


if __name__=='__main__':unittest.main()
