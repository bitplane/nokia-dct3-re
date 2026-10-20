import unittest

from tools.nsk3_native_upload_check import check_upload


class NativeUploadTest(unittest.TestCase):
    def setUp(self):
        self.trace='nsk3_program_extent_summary: writes=811 unique=811 highest=2b2a retained=811 observer_writes=0\n'
        for address,word in enumerate((0x10f8,0x192e,0xf010,0x0200,0xf842,0x282f,0x61f8,0x21a1),0x2800):
            self.trace+=f'nsk3_program_visible: address={address:04x} uploaded={word:04x} program={word:04x} data={word:04x} observer_writes=0\n'

    def test_retained_upload(self):
        check_upload(self.trace)

    def test_rejects_incomplete_or_incoherent_upload(self):
        for trace in (self.trace.replace('retained=811','retained=9'),
                      self.trace.replace('program=10f8','program=a594'),
                      self.trace.replace('data=192e','data=0000'),
                      self.trace.replace('highest=2b2a','highest=2b29')):
            with self.assertRaises(ValueError):
                check_upload(trace)


if __name__=='__main__':
    unittest.main()
