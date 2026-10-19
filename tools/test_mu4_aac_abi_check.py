import struct
import unittest

from mu4_aac_abi_check import check_capture


TEXT = '''mu4_aac_abi_setup: original_upload_words=49843 original_initializers=228 input_bytes=2048 constructor=023d27 process=023d5d isolated_abi=1 handset_boot=0 conformance=0
mu4_aac_abi_header: ordinal=1 sync=0fff id=0001 layer=0000 protection=0001 profile=0001 frequency=0004 channels=0002 frame_length=0134 blocks=0001 identity=Execute_entry observer_writes=0
mu4_aac_abi_sources: left=f000 right=e800 sp=8fe8 result=0000 identity=Execute_entry observer_writes=0
mu4_aac_abi_result: returned=1 result=0000 base=162e index=009b remaining=0010 illegal=0 pc=001018 isolated_abi=1 conformance=0
mu4_aac_abi_capture: frames=1024 channels=2 raw_bytes=8192 converted_bytes=4096 source=decoder_return_pointers memory=post_wrapper_return conformance=0
'''


class AbiCheckTest(unittest.TestCase):
    def test_waveform_error_is_reported_not_accepted_as_tolerance(self):
        raw = struct.pack('<2048i', *([16384]*2048))
        converted = struct.pack('<2048h', *([1]*2048))
        result = check_capture(TEXT, raw, converted, bytes(4096))
        self.assertEqual(result['channels'][0]['mismatches'],1024)
        self.assertEqual(result['channels'][0]['rms'],1)
        self.assertNotIn('conformant',result)

    def test_wrong_source_failure_fault_and_duplicate_reject(self):
        for text in (TEXT.replace('left=f000','left=e800'), TEXT.replace('illegal=0','illegal=1'),
                     TEXT.replace('returned=1','returned=0'), TEXT+TEXT,
                     TEXT+'Fatal error: incomplete', TEXT.replace('frames=1024','frames=1023'),
                     TEXT.replace('frequency=0004','frequency=0003')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check_capture(text,bytes(8192),bytes(4096),bytes(4096))

    def test_changed_conversion_and_truncated_buffers_reject(self):
        for raw, converted, reference in ((bytes(8191),bytes(4096),bytes(4096)),
                                           (bytes(8192),bytes(4095),bytes(4096)),
                                           (bytes(8192),bytes(4096),bytes(4095)),
                                           (bytes(8192),b'\x01'+bytes(4095),bytes(4096))):
            with self.assertRaises(ValueError):
                check_capture(TEXT,raw,converted,reference)


if __name__ == '__main__':
    unittest.main()
