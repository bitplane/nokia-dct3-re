import unittest

from tools.nsk1_native_upload_check import check_upload


class NativeUploadTest(unittest.TestCase):
    def setUp(self):
        self.trace = '\n'.join(f'nsk1_program_extent: address={address:04x} data={word:04x} mask=ffff'
                               for address, word in ((0x29d3,0x7714),(0x29d4,0x085e),(0x29d5,0x7194),(0x29d6,0x1f2b),(0x29d7,0x1284)))
        self.trace += '\nnsk1_program_extent_summary: writes=811 highest=2b2a observer_writes=0'
        self.code = '00:29D3: 7714 085E      STM     #$085E, $14\n00:29D5: 7194 1F2B      MVDK    *AR4+, $1F2B'

    def test_visible_own_upload(self):
        check_upload(self.trace, self.code)

    def test_rejects_discarded_or_different_upload(self):
        for trace, code in ((self.trace.replace('data=7714','data=0000'),self.code),
                            (self.trace.replace('writes=811','writes=810'),self.code),
                            (self.trace+'\nc54x_zero_entry:',self.code),
                            (self.trace,'00:29D3: 0000 ADD $00, A')):
            with self.assertRaises(ValueError):
                check_upload(trace, code)


if __name__ == '__main__':
    unittest.main()
