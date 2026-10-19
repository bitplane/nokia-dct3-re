import unittest

from tools.mu4_metadata_extent_check import check
from tools.mu4_context_contract import reference_metadata_size_updates


def fixture():
    def tag(title,artist):
        frames = b''
        for name,value in ((b'TIT2',title),(b'TPE1',artist)):
            payload = b'\0'+value
            frames += name+len(payload).to_bytes(4,'big')+b'\0\0'+payload
        return b'ID3\x03\0\0\0\0\0\x64'+frames+bytes(100-len(frames))
    before = tag(b'000908      ',b'            ')+bytes([0x55])*(143982-110)
    after = tag(b'MAME',b'TEST')+before[110:]+b'\xff'*210
    lines = [
        'mu4_original_resident_program: PASS checked_words=48261 mismatch_words=0 excluded_data_words=3660 source_words=51921 before_resident_execution=1 physical_mapping=unvalidated full_boot=0',
        'mu4_original_resident_data: PASS checked_words=3656 mismatch_words=0 excluded_register_words=4 source_data_words=3660 before_resident_execution=1 physical_mapping=unvalidated full_boot=0',
        'mu4_metadata_extent_scope: first=14.9 last=20 first_address=1456 last_address=145a cap=4096 boundary=data_write context_pc=not_owner observer_writes=0 audio_acceptance=0']
    ordinal = 0
    for index,step in enumerate(reference_metadata_size_updates(len(before),100)):
        for address,value in ((0x1456,step['extent']>>16),(0x1457,step['extent']&0xffff)):
            ordinal += 1
            lines.append(f'mu4_metadata_extent_write: ordinal={ordinal} address={address:04x} value={value:04x} '
                         f'mask=ffff t={15.1+index/50:.9f} context_pc=03c3ff ar1=1444 '
                         f'ar7={step["words"]:04x} observer_writes=0')
    lines += ['mu4_native_recorder_metadata: PASS native_reply=1 success=1 playback=0 firmware_state_forcing=0',
              f'mu4_metadata_extent_terminal: events={ordinal} capped=0 illegal=0 t=20.000000000 observer_writes=0 audio_acceptance=0']
    return '\n'.join(lines),before,after


class MetadataExtentTest(unittest.TestCase):
    def test_source_progression_and_independent_file_extent_agree(self):
        result = check(*fixture())
        self.assertEqual(result['accounting_updates'],52)
        self.assertEqual(result['growth_bytes'],210)
        self.assertEqual(result['clear_phase_extent'],144082)
        self.assertEqual(result['header_phase_extent'],144092)
        self.assertEqual(result['file_delta']['added_byte_counts'],{'ff':210})

    def test_exposed_bytes_are_reported_not_assumed_to_be_valid_padding(self):
        text,before,after = fixture()
        result = check(text,before,after[:-210]+b'Z'*210)
        self.assertEqual(result['file_delta']['added_byte_counts'],{'5a':210})

    def test_rejects_partial_wrong_progression_or_scope(self):
        text,before,after = fixture()
        for old,new in (('events=104','events=103'),('capped=0','capped=1'),('illegal=0','illegal=1'),
                        ('last=20','last=21'),('mask=ffff','mask=ff00'),('value=3270','value=3271'),
                        ('ordinal=2 ','ordinal=1 '),('context_pc=03c3ff','context_pc=03c4000'),
                        ('ar7=0001','ar7=10000'),('success=1','success=0'),
                        ('t=20.000000000','t=19.0'),('mismatch_words=0','mismatch_words=1')):
            self.assertIn(old,text)
            with self.subTest(old=old), self.assertRaises(ValueError):
                check(text.replace(old,new),before,after)
        for bad in (after[:-1],after[:110]+b'x'+after[111:]):
            with self.assertRaises(ValueError):
                check(text,before,bad)
        lines = text.splitlines()
        with self.assertRaises(ValueError):
            check('\n'.join([lines[-1]]+lines[:-1]),before,after)


if __name__ == '__main__':
    unittest.main()
