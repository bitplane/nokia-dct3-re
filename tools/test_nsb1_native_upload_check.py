import unittest

from tools.nsb1_native_upload_check import check_upload


class NativeUploadTest(unittest.TestCase):
    def setUp(self):
        self.trace = 'nsb1_program_extent_summary: writes=825 highest=2b38 observer_writes=0\n'
        for address,word in ((0x29e1,0x7714),(0x29e2,0x085e),(0x29e3,0x7194),(0x29e4,0x1f2b),(0x29e5,0x1284)):
            self.trace += f'nsb1_program_extent: address={address:04x} data={word:04x} mask=ffff\n'
            self.trace += f'nsb1_program_visible: address={address:04x} program={word:04x} data={word:04x} observer_writes=0\n'

    def test_retained_upload(self):
        check_upload(self.trace)

    def test_rejects_missing_or_discarded_words(self):
        for text in (self.trace.replace('program=7714','program=0000'),
                     self.trace.replace('data=085e','data=0000'),
                     self.trace.replace('writes=825','writes=824'),
                     self.trace.replace('mask=ffff','mask=ff00')):
            with self.assertRaises(ValueError):
                check_upload(text)


if __name__ == '__main__':
    unittest.main()
