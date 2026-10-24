import unittest

from tools.nsb3_native_check import check_upload, check_native


class Nsb3NativeTest(unittest.TestCase):
    def test_all_uploaded_words_are_checked(self):
        source = bytearray(0x100000)
        offset = 0xbd53c + 12 + (0x2800 - 0x25b4) * 2
        payload = b'AB' * 811
        source[offset:offset+len(payload)] = payload
        window = bytearray(payload + bytes(0x1000 - len(payload)))
        trace = 'nsb3_program_extent: writes=811 highest=2b2a observer_writes=0'
        check_upload(source, trace, window)
        window[1620] ^= 1
        with self.assertRaises(ValueError):
            check_upload(source, trace, window)
        with self.assertRaises(ValueError):
            check_upload(source, trace.replace('811', '810'), window)

    def test_native_liveness_does_not_ignore_illegal_flag(self):
        trace = ('nsb3_native_state: pc=408d illegal=0 idle=1 observer_writes=0\n'
                 'rom4_interface_summary: completion_strobes=1 mailbox_writes=1 slot_expiries=1 frame_expiries=1')
        check_native(trace)
        for bad in (trace.replace('illegal=0', 'illegal=1'), trace.replace('mailbox_writes=1', 'mailbox_writes=0'), ''):
            with self.assertRaises(ValueError):
                check_native(bad)


if __name__ == '__main__':
    unittest.main()
