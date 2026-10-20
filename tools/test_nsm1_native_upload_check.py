import unittest

from tools.nsm1_native_upload_check import check_upload


class NativeUploadTest(unittest.TestCase):
    def setUp(self):
        self.trace='nsm1_program_extent_summary: writes=605 unique=605 highest=2a5c retained=605 observer_writes=0\n'
        for address,word in enumerate((0x1221,0xf062,0x005a,0x8184,0xf074,0x48f0,0xf479,0x80f8),0x2800):
            self.trace+=f'nsm1_program_visible: address={address:04x} uploaded={word:04x} program={word:04x} data={word:04x} observer_writes=0\n'

    def test_complete_retention(self):
        check_upload(self.trace)

    def test_rejects_old_mapping_and_wrong_extent(self):
        for text in (self.trace.replace('retained=605','retained=6'),
                     self.trace.replace('program=1221','program=a594'),
                     self.trace.replace('data=1221','data=0000'),
                     self.trace.replace('highest=2a5c','highest=2a5b')):
            with self.assertRaises(ValueError):
                check_upload(text)


if __name__=='__main__':
    unittest.main()
