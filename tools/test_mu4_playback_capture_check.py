import struct
import tempfile
import unittest
from array import array
from pathlib import Path
from unittest.mock import patch

from tools.mu4_playback_capture_check import (
    FILES, SLOT_FILE, TX_TIME_FILE, compare_reference, compare_streams, framed_words,
    compare_serial_callbacks, locate_sink_mismatches, parse_rows, parse_slot_row,
    parse_tx_time_row, validate_slots, validate_stream, validate_tx_times, verify,
)


class PlaybackCaptureTests(unittest.TestCase):
    @staticmethod
    def times(values):
        data = b''.join(struct.pack('<Q', value) for value in values)
        fingerprint = 2166136261
        for byte in data:
            fingerprint = ((fingerprint ^ byte)*16777619) & 0xffffffff
        row = {'records':len(values), 'bytes':len(data), 'hash':f'{fingerprint:08x}'}
        text = (f'mu4_playback_tx_time_capture: records={len(values)} bytes={len(data)} '
                f'fnv1a={fingerprint:08x} encoding=le64ns file={TX_TIME_FILE} '
                'save_restore=unsupported observer_writes=0 audio_acceptance=0')
        return data, row, text

    def test_tx_metadata_is_strict_and_hash_bound(self):
        data, row, text = self.times([10, 20, 30])
        self.assertEqual(list(validate_tx_times(row, data, 3)), [10,20,30])
        self.assertEqual(parse_tx_time_row(text), row)
        self.assertIsNone(parse_tx_time_row('legacy'))
        for bad in ('', text+' extra', text+'\n'+text,
                    text.replace(TX_TIME_FILE, 'other.times'),
                    text.replace('observer_writes=0', 'observer_writes=1')):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                parse_tx_time_row(bad, True)
        for bad in (data[:-1], data+b'\0', data[:8]+bytes(8)+data[16:]):
            with self.assertRaises(ValueError):
                validate_tx_times(row,bad,3)
        for values in ([10,10], [20,10]):
            bad, changed, _ = self.times(values)
            with self.assertRaises(ValueError):
                validate_tx_times(changed,bad,2)

    def test_timestamp_pairing_handles_underflow_gaps_without_fitting(self):
        data, _, _ = self.slots([(5,0),(15,1),(25,0),(35,1),(45,0)])
        result = compare_serial_callbacks([2,4,6], [1,21,41], [2,0,4,0,6], data, 4)
        self.assertTrue(result['all_completed_tx_paired'])
        self.assertEqual(result['paired_words'], 3)
        self.assertEqual(result['din_without_completed_tx'], 2)
        self.assertEqual(result['delay_ns'], {4:3})
        self.assertEqual(result['unmatched_din']['consistent_with_held_last_bit'], 2)
        self.assertEqual(result['unmatched_din']['held_level_mismatches'], 0)
        self.assertFalse(result['native_audio_validated'])

    def test_unmatched_din_distinguishes_initial_level_and_held_bit_exceptions(self):
        data, _, _ = self.slots([(1,0),(5,1),(10,0),(15,1),(20,0)])
        result = compare_serial_callbacks([1], [4], [7,1,0xffff,9,0xffff], data, 1)
        report = result['unmatched_din']
        self.assertEqual(report['before_first_tx'], 1)
        self.assertEqual(report['preceded_by_tx'], 3)
        self.assertEqual(report['consistent_with_held_last_bit'], 2)
        self.assertEqual(report['held_level_mismatches'], 1)
        self.assertEqual(report['first_held_level_exceptions'][0]['din_index'], 3)
        self.assertEqual(report['values_by_slot']['slot_0'], {'0007':1,'ffff':2})
        self.assertFalse(report['pin_ownership_validated'])
        # A stream without TX has no preceding-word evidence, not a held-zero proof.
        report = compare_serial_callbacks([], [], [7,1,0xffff,9,0xffff], data)['unmatched_din']
        self.assertEqual(report['before_first_tx'], 5)
        self.assertEqual(report['preceded_by_tx'], 0)

    def test_timestamp_pairing_does_not_skip_a_wrong_word_for_a_match(self):
        data, _, _ = self.slots([(5,0),(6,1),(25,0)])
        result = compare_serial_callbacks([1,2], [1,21], [9,1,2], data, 4)
        self.assertEqual(result['mismatched_words'], 1)
        self.assertEqual(result['first_mismatches'][0]['din_index'], 0)
        self.assertFalse(result['all_completed_tx_paired'])
        result = compare_serial_callbacks([1,2], [1,21], [9,1,2], data, 3)
        self.assertEqual(result['unpaired_tx_words'], 2)
        with self.assertRaises(ValueError):
            compare_serial_callbacks([1], [2,3], [], b'')
        with self.assertRaises(ValueError):
            compare_serial_callbacks([1], [2], [], b'', -1)

    def test_mismatch_localization_preserves_fixed_offset_and_actual_slot(self):
        source = array('H', [0, 1, 2, 3, 4, 5])
        sink = array('H', [0, 0, 1, 2, 3, 9, 5])
        comparison = compare_streams(source, sink, anchor=3)
        data, _, _ = self.slots([(10+i*10, (i+1)%2) for i in range(7)])
        located = locate_sink_mismatches(comparison, data, 7)
        self.assertEqual(comparison['sink_offset_words'], 1)
        self.assertEqual(located, [dict(source_index=4, sink_index=5,
            source_word='0004', sink_word='0009', sink_time_ns=60, sink_slot=0)])
        self.assertNotIn('sink_time_ns', comparison['first_mismatches'][0])
        self.assertFalse(comparison['transport_pairing_validated'])
        self.assertEqual(locate_sink_mismatches({}, data, 7), [])
        with self.assertRaises(ValueError):
            locate_sink_mismatches(comparison, data[:-1], 7)
        with self.assertRaises(ValueError):
            locate_sink_mismatches({'first_mismatches':[{'sink_index':7}]}, data, 7)
        damaged = data[:53]+b'\x02'+data[54:]
        with self.assertRaises(ValueError):
            locate_sink_mismatches(comparison, damaged, 7)

    @staticmethod
    def slots(records):
        payload = b''.join(struct.pack('<QB',*record) for record in records)
        fingerprint = 2166136261
        for byte in payload:
            fingerprint = ((fingerprint ^ byte)*16777619) & 0xffffffff
        row = {'records':len(records),'bytes':len(payload),'hash':f'{fingerprint:08x}'}
        text = (f'mu4_playback_slot_capture: records={len(records)} bytes={len(payload)} '
                f'fnv1a={fingerprint:08x} encoding=le64ns_u8slot file={SLOT_FILE} '
                'save_restore=unsupported observer_writes=0 audio_acceptance=0')
        return payload,row,text

    @staticmethod
    def stream(words):
        payload = struct.pack('<'+'H'*len(words),*words)
        fingerprint = 2166136261
        for byte in struct.pack('>'+'H'*len(words),*words):
            fingerprint = ((fingerprint ^ byte)*16777619) & 0xffffffff
        return payload, {'words':len(words),'bytes':len(payload),'hash':f'{fingerprint:08x}'}

    def fixture(self):
        payload,row = self.stream([0,1,0xffff,0x1234])
        text = '\n'.join(
            f'[:] mu4_playback_word_capture: boundary={name} words={row["words"]} bytes={row["bytes"]} '
            f'hash={row["hash"]} encoding=le16 file={filename} save_restore=unsupported observer_writes=0 audio_acceptance=0'
            for name,filename in FILES.items())
        return payload,text

    def test_exact_word_encoding_and_observed_hash(self):
        payload,row = self.stream([0,0x1234,0x8000,0xffff])
        words,report = validate_stream(row,payload)
        self.assertEqual(list(words),[0,0x1234,0x8000,0xffff])
        self.assertEqual(report['nonzero'],3)
        self.assertEqual(report['observed_hash'],row['hash'])

    def test_incomplete_changed_or_byte_swapped_stream_is_rejected(self):
        payload,row = self.stream([0,0x1234,0x8000,0xffff])
        for bad in (payload[:-1],payload[:-2],payload+b'\0\0',
                    payload[:2]+b'\0\0'+payload[4:],
                    b''.join(payload[i:i+2][::-1] for i in range(0,len(payload),2))):
            with self.subTest(bad=bad),self.assertRaises(ValueError):
                validate_stream(row,bad)
        for key in ('words','bytes'):
            changed = dict(row)
            changed[key] += 1
            with self.assertRaises(ValueError):
                validate_stream(changed,payload)

    def test_boundary_rows_are_complete_unique_and_fixed(self):
        _,text = self.fixture()
        self.assertEqual(set(parse_rows(text)),set(FILES))
        for bad in ('\n'.join(text.splitlines()[1:]),text+'\n'+text.splitlines()[0],
                    text.replace('boundary=dma_source','boundary=cpu_read'),
                    text.replace('mu4_playback_dma.le16','wrong.le16'),
                    text.replace('observer_writes=0','observer_writes=1'),
                    text.replace('save_restore=unsupported','save_restore=supported'),
                    text.replace('audio_acceptance=0','audio_acceptance=1'),
                    text+' extra'):
            with self.subTest(bad=bad),self.assertRaises(ValueError):
                parse_rows(bad)

    def test_slot_rows_are_strict_but_legacy_absence_is_explicit(self):
        _,row,text = self.slots([(10,0),(20,1)])
        self.assertEqual(parse_slot_row('[:] '+text,True),row)
        self.assertIsNone(parse_slot_row('legacy capture'))
        for bad in ('',text+'\n'+text,text+' extra',
                    text.replace(SLOT_FILE,'other.slots'),
                    text.replace('le64ns_u8slot','native_endian'),
                    text.replace('observer_writes=0','observer_writes=1'),
                    text.replace('audio_acceptance=0','audio_acceptance=1')):
            with self.subTest(bad=bad),self.assertRaises(ValueError):
                parse_slot_row(bad,True)

    def test_slot_metadata_validates_time_domain_hash_and_counts(self):
        data,row,_ = self.slots([(100,0),(110,1),(200,0),(210,1)])
        result = validate_slots(row,data,4)
        self.assertEqual(result['observed_frame_pairs'],2)
        self.assertEqual(result['orphan_words'],0)
        self.assertEqual(result['pair_interval_ns'],{10:2})
        self.assertEqual(result['frame_interval_ns'],{100:1})
        for bad in (data[:-1],data+b'\0',data[:-1]+b'\0'):
            with self.assertRaises(ValueError):
                validate_slots(row,bad,4)
        with self.assertRaises(ValueError):
            validate_slots(row,data,3)
        for records in ([(10,0),(10,1)],[(10,0),(9,1)],[(10,0),(20,2)]):
            bad,changed,_ = self.slots(records)
            with self.assertRaises(ValueError):
                validate_slots(changed,bad,2)

    def test_slot_discontinuities_and_orphans_are_not_hidden_by_parity(self):
        data,row,_ = self.slots([(10,1),(20,0),(30,0),(40,1),(50,0)])
        result = validate_slots(row,data,5)
        self.assertEqual(result['observed_frame_pairs'],1)
        self.assertEqual(result['orphan_words'],3)
        self.assertEqual(result['slot_discontinuities'],1)
        self.assertEqual(list(framed_words([1,2,3,4,5],data)),[(2,30,(3,4))])
        with self.assertRaises(ValueError):
            list(framed_words([1,2],data))

    def test_reference_comparison_uses_slots_and_does_not_select_a_channel_swap(self):
        pairs = [(0,0),(10,-20),(30,-40),(50,-60)]
        records = [(500_000,1)]
        words = [0]
        for i,pair in enumerate(pairs):
            records.extend([(1_000_000+i*1_000_000,0),(1_000_100+i*1_000_000,1)])
            words.extend(value & 0xffff for value in pair)
        data,_,_ = self.slots(records)
        reference = struct.pack('<8h',*(value for pair in pairs for value in pair))
        result = compare_reference(words,data,reference,1000)
        self.assertEqual(result['compared_frames'],3)
        self.assertEqual(result['native_first_nonzero_frame']['word_index'],3)
        self.assertEqual(result['reference_first_nonzero_frame'],1)
        for channel in (0,1):
            self.assertEqual(result['metrics'][f'slot_{channel}_reference_{channel}']['mismatched_samples'],0)
        self.assertEqual(result['metrics']['slot_0_reference_1']['mismatched_samples'],3)
        self.assertFalse(result['packet_frame_alignment_validated'])
        self.assertFalse(result['native_audio_validated'])
        swapped = struct.pack('<8h',*(value for pair in pairs for value in pair[::-1]))
        result = compare_reference(words,data,swapped,1000)
        self.assertEqual(result['metrics']['slot_0_reference_1']['mismatched_samples'],0)
        self.assertEqual(result['metrics']['slot_0_reference_0']['mismatched_samples'],3)
        self.assertFalse(result['native_audio_validated'])

    def test_reference_comparison_stops_at_cadence_break_without_searching(self):
        records = [(100,0),(110,1),(1_000_100,0),(1_000_110,1),
                   (3_000_100,0),(3_000_110,1)]
        data,_,_ = self.slots(records)
        result = compare_reference([1,2,3,4,5,6],data,struct.pack('<6h',1,2,3,4,5,6),1000)
        self.assertEqual(result['compared_frames'],2)
        self.assertEqual(result['window_stop'],'native_cadence_discontinuity')
        for rate,reference in ((0,b'\0'*8),(1000,b'\0'*8),(1000,b'\0'*7)):
            with self.assertRaises(ValueError):
                compare_reference([1,2,3,4,5,6],data,reference,rate)

    def test_first_nonzero_alignment_checks_all_overlap_and_extras(self):
        source = array('H',[0,0,1,2,3,4,5,6])
        sink = array('H',[0,0,0,0,1,2,3,4,5,6,0])
        result = compare_streams(source,sink,anchor=4)
        self.assertTrue(result['alignment_established'])
        self.assertEqual(result['sink_offset_words'],2)
        self.assertEqual(result['compared_words'],8)
        self.assertEqual(result['mismatched_words'],0)
        self.assertEqual(result['exact_prefix_words'],8)
        self.assertEqual(result['sink_suffix_words'],1)
        self.assertEqual(result['sink_extra_nonzero'],0)
        self.assertFalse(result['transport_pairing_validated'])

    def test_tail_corruption_is_not_hidden_by_a_matching_anchor(self):
        source = array('H',[0,1,2,3,4,5,6,7])
        sink = array('H',[0,0,1,2,3,4,5,99,7])
        result = compare_streams(source,sink,anchor=4)
        self.assertEqual(result['mismatched_words'],1)
        self.assertEqual(result['exact_prefix_words'],6)
        self.assertEqual(result['first_mismatches'][0]['source_index'],6)
        self.assertFalse(result['transport_pairing_validated'])

    def test_alignment_does_not_search_past_an_initial_disagreement(self):
        source = array('H',[0,1,2,3,4,5])
        sink = array('H',[0,9,9,9,1,2,3,4,5])
        self.assertFalse(compare_streams(source,sink,anchor=4)['alignment_established'])
        for words in ([],[0]*8,[0,1]):
            self.assertFalse(compare_streams(array('H',words),source,anchor=4)['alignment_established'])
        with self.assertRaises(ValueError):
            compare_streams(source,sink,anchor=0)

    def test_wrapper_requires_existing_playback_validation_and_boundary_counters(self):
        payload,text = self.fixture()
        observation = {'tx_words':4,'din_words':4,'dma_reads':4,
                       'tx_nonzero':3,'din_nonzero':3,'dma_nonzero':3}
        observation.update(tx_hash=parse_rows(text)['mcbsp_tx']['hash'],
                           din_hash=parse_rows(text)['codec_din']['hash'])
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for filename in FILES.values():
                (root/filename).write_bytes(payload)
            with patch('tools.mu4_playback_capture_check.verify_playback_dma',return_value=observation) as checked:
                report = verify(text,root)
                checked.assert_called_once_with(text)
                self.assertTrue(report['capture_integrity_validated'])
                self.assertFalse(report['native_audio_validated'])
                self.assertFalse(report['codec_slots']['capture_integrity_validated'])
                with self.assertRaises(ValueError):
                    verify(text,root,require_framing=True)
                slots,_,slot_text = self.slots([(10,0),(20,1),(30,0),(40,1)])
                (root/SLOT_FILE).write_bytes(slots)
                report = verify(text+'\n'+slot_text,root,require_framing=True)
                self.assertTrue(report['codec_slots']['capture_integrity_validated'])
                with self.assertRaises(ValueError):
                    verify(text+'\n'+slot_text,root,require_tx_times=True)
                tx_times, _, tx_text = self.times([9,19,29,39])
                (root/TX_TIME_FILE).write_bytes(tx_times)
                report = verify(text+'\n'+slot_text+'\n'+tx_text,root,require_tx_times=True)
                self.assertTrue(report['serial_callbacks']['all_completed_tx_paired'])
                self.assertEqual(report['serial_callbacks']['delay_ns'], {1:4})
                self.assertFalse(report['native_audio_validated'])
                with self.assertRaises(ValueError):
                    verify(text+'\n'+tx_text,root)
                (root/TX_TIME_FILE).write_bytes(tx_times[:-1])
                with self.assertRaises(ValueError):
                    verify(text+'\n'+slot_text+'\n'+tx_text,root)
                report = verify(text+'\n'+slot_text,root,reference=payload,reference_rate=50_000_000)
                self.assertEqual(report['reference_comparison']['compared_frames'],2)
                self.assertFalse(report['native_audio_validated'])
                (root/SLOT_FILE).write_bytes(slots[:-1])
                with self.assertRaises(ValueError):
                    verify(text+'\n'+slot_text,root,require_framing=True)
            with patch('tools.mu4_playback_capture_check.verify_playback_dma',side_effect=ValueError('illegal')):
                with self.assertRaises(ValueError):
                    verify(text,root)
            for key in observation:
                changed = dict(observation)
                changed[key] = f'{int(changed[key],16)^1:08x}' if key.endswith('_hash') else changed[key]+1
                with patch('tools.mu4_playback_capture_check.verify_playback_dma',return_value=changed):
                    with self.subTest(key=key),self.assertRaises(ValueError):
                        verify(text,root)


if __name__ == '__main__':
    unittest.main()
