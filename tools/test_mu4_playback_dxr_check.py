import struct
import unittest

from tools.mu4_playback_dxr_check import FILE, check_writes, parse_row


class DxrOwnershipTests(unittest.TestCase):
    @staticmethod
    def fixture(records):
        data = b''.join(struct.pack('<QIHBB', *record) for record in records)
        fingerprint = 2166136261
        for byte in data:
            fingerprint = ((fingerprint ^ byte)*16777619) & 0xffffffff
        row = {'records':len(records), 'bytes':len(data), 'hash':f'{fingerprint:08x}'}
        text = (f'mu4_playback_dxr_capture: records={len(records)} bytes={len(data)} '
                f'fnv1a={fingerprint:08x} encoding=le64ns_u32ordinal_u16word_u8owner_u8reserved '
                f'file={FILE} save_restore=unsupported observer_writes=0 audio_acceptance=0')
        return row, data, text

    def test_source_binding_distinguishes_other_writers_and_zero_words(self):
        row, data, text = self.fixture([(1,0,9,0,0),(2,1,0,4,0),
            (3,0,7,2,0),(4,2,0xffff,4,0)])
        self.assertEqual(parse_row(text), row)
        result = check_writes(row,data,[0,0xffff])
        self.assertEqual(result['channel_3_source_reads_bound'],2)
        self.assertEqual(result['owner_counts'],{'no_active_dma':1,'dma_channel_3':2,'dma_channel_1':1})
        self.assertTrue(result['dma_to_dxr_validated'])
        self.assertFalse(result['dxr_to_tx_validated'])
        self.assertFalse(result['native_audio_validated'])

    def test_rows_reject_scope_changes_and_duplicates(self):
        _,_,text = self.fixture([(1,1,2,4,0)])
        for bad in ('',text+'\n'+text,text+' extra',text.replace(FILE,'other.writes'),
                    text.replace('observer_writes=0','observer_writes=1'),
                    text.replace('save_restore=unsupported','save_restore=supported')):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                parse_row(bad)

    def test_integrity_and_rehashed_contract_corruptions_are_rejected(self):
        good = [(1,1,2,4,0),(2,2,3,4,0)]
        row,data,_ = self.fixture(good)
        for bad in (data[:-1], data+bytes(16), data[:12]+b'\x01'+data[13:]):
            with self.assertRaises(ValueError):
                check_writes(row,bad,[2,3])
        variants = [[],good[:1],good+[good[-1]],
            [(1,2,2,4,0),good[1]], [good[0],(2,1,3,4,0)],
            [good[0],(2,2,4,4,0)], [good[0],(0,2,3,4,0)],
            [good[0],(2,2,3,7,0)], [good[0],(2,2,3,4,1)],
            [good[0],(2,2,3,0,0)], [good[0],(2,0,3,0,0)]]
        for records in variants:
            changed,payload,_ = self.fixture(records)
            with self.subTest(records=records), self.assertRaises(ValueError):
                check_writes(changed,payload,[2,3])
        with self.assertRaises(ValueError):
            check_writes(row,data,[])


if __name__ == '__main__':
    unittest.main()
