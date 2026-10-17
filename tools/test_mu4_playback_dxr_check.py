import struct
import unittest

from tools.mu4_playback_dxr_check import (
    FILE, check_buffer_sequence, check_writes, parse_controls, parse_row,
)


class DxrOwnershipTests(unittest.TestCase):
    def test_buffer_model_accounts_for_reset_ignore_discard_repeat_and_endpoint(self):
        controls = [(10,0),(20,1),(32,0),(40,1)]
        writes = [(1,1,99,4,0),(21,2,7,4,0),(31,3,9,4,0),
                  (33,4,10,4,0),(42,5,11,4,0),(44,6,12,4,0)]
        result = check_buffer_sequence(controls,100,writes,[99,7,0,11],[5,30,41,43])
        self.assertEqual(result['fresh_write_completions'],2)
        self.assertEqual(result['retained_word_completions'],1)
        self.assertEqual(result['pre_reset_tx_prefix_unattributed'],1)
        self.assertEqual(result['pre_reset_writes_unattributed'],1)
        self.assertEqual(result['reset_discarded_ordinals'],[3])
        self.assertEqual(result['reset_ignored_ordinals'],[4])
        self.assertEqual(result['endpoint_pending_ordinals'],[6])
        self.assertFalse(result['internal_buffer_loads_observed'])
        self.assertFalse(result['native_audio_validated'])

    def test_buffer_model_rejects_mismatch_reset_completion_overflow_and_time_ties(self):
        controls = [(10,0),(20,1)]
        writes = [(21,1,7,4,0)]
        for config,stop,records,tx,times in (
                (controls,100,writes,[8],[30]),
                (controls,100,writes,[7],[15]),
                (controls,100,writes+[(22,2,8,4,0),(23,3,9,4,0)],[7],[30]),
                (controls,100,[(20,1,7,4,0)],[7],[30]),
                (controls,29,writes,[7],[30]),
                (controls,100,[(21,1,7,0,0)],[7],[30])):
            with self.subTest(records=records, times=times), self.assertRaises(ValueError):
                check_buffer_sequence(config,stop,records,[0]+tx,[5]+times)
        with self.assertRaises(ValueError):
            check_buffer_sequence([],100,writes,[7],[30])

    def test_indexed_control_observation_has_complete_count_and_selection(self):
        lines = [
            'mu4_transport_config: address=0038 index=01 value=0001 mask=ffff t=0.000000001 context_pc=000010 observer_writes=0',
            'mu4_transport_config: address=0039 index=01 value=0000 mask=ffff t=0.000000002 context_pc=000011 observer_writes=0',
            'mu4_transport_terminal: configs=2 dxr=0 first=0.000000003 last=0.000000004 t=0.000000100 observer_writes=0']
        text = '\n'.join(lines)
        self.assertEqual(parse_controls(text), ([(2,0)],100))
        for bad in ('\n'.join(lines[1:]),text+'\n'+lines[-1],
                    text.replace('configs=2','configs=3'),
                    text.replace('index=01 value=0000','index=02 value=0000'),
                    text.replace('t=0.000000002','t=0.000000000'),
                    text.replace('mask=ffff','mask=00ff'),
                    text.replace('last=0.000000004','last=0.000000101')):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                parse_controls(bad)

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
