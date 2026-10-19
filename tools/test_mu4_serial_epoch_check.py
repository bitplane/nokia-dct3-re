import struct
import unittest

from tools.mu4_serial_epoch_check import (bind_lifecycle, codec_control_history,
    startup_report, frame_wait_din, nonshifting_din, tx_event_rows, check_tx_event_capture,
    full_lifecycle_report, codec_capture_horizon)


BASE = 1_000_000_000


def fixture(specification):
    events,writes,tx,times = [],[],[],[]
    for kind,time,word,source in specification:
        ns = BASE+time
        write = kind in ('accept','overwrite','ignore')
        events.append(dict(ordinal=str(len(events)+1),kind=kind,word=f'{word:04x}',ns=str(ns),
                           dma_owner='4' if write else '0',dma_read=str(source if write else 0),
                           observer_writes='0',audio_acceptance='0'))
        if write:
            writes.append((ns,source,word,4,0))
        if kind == 'complete':
            tx.append(word)
            times.append(ns)
    return events,writes,tx,times


class SerialEpochTest(unittest.TestCase):
    def test_active_codec_capture_cannot_end_on_an_early_valid_prefix(self):
        text = '\n'.join([
            'mu4_codec_control: word=0e53 ns=1 power=007 analog=00a digital=008 format=053 rate=020 active=000 hp_left=079 hp_right=079 observer_writes=0 audio_acceptance=0',
            'mu4_codec_control: word=1023 ns=2 power=007 analog=00a digital=008 format=053 rate=023 active=000 hp_left=079 hp_right=079 observer_writes=0 audio_acceptance=0',
            'mu4_codec_control: word=1201 ns=3 power=007 analog=00a digital=008 format=053 rate=023 active=001 hp_left=079 hp_right=079 observer_writes=0 audio_acceptance=0'])
        self.assertEqual(codec_capture_horizon(text,struct.pack('<QB',1_000_000,1),1_015_000)['gap_ns'],15000)
        divided = text.replace('word=1023','word=1063').replace('rate=023','rate=063')
        self.assertEqual(codec_capture_horizon(divided,struct.pack('<QB',1_000_000,0),1_030_000)['maximum_gap_ns'],45352)
        for last in (900_000,1_015_001):
            with self.assertRaises(ValueError):
                codec_capture_horizon(text,struct.pack('<QB',last,1),1_015_000)
        with self.assertRaises(ValueError):
            codec_capture_horizon(text,b'',1_015_000)

    def test_streamed_owned_epochs_cover_all_writes(self):
        specification = []
        for source in range(1,2001):
            specification.extend([('accept',source*3,source,source),
                                  ('load_fresh',source*3+1,source,0),
                                  ('complete',source*3+2,source,0)])
        events,writes,tx,times = fixture(specification)
        report,completed = bind_lifecycle(iter(events),writes,tx,times)
        self.assertEqual(report['accepted'],2000)
        self.assertEqual(report['events'],6000)
        self.assertEqual(len(completed),2000)
        self.assertEqual(report['endpoint_pending'],[])

    def test_full_lifecycle_rejects_missing_event_with_recomputed_checksum(self):
        def capture(events):
            data = b''.join(struct.pack('<QHBBI',*event) for event in events)
            checksum = 2166136261
            for value in data:
                checksum = ((checksum ^ value)*16777619)&0xffffffff
            text = (f'mu4_tx_event_capture: records={len(events)} bytes={len(data)} fnv1a={checksum:08x} capped=0 '
                    'encoding=le64ns_u16word_u8event_u8owner_u32source file=mu4_playback_tx.events '
                    'save_restore=unsupported observer_writes=0 audio_acceptance=0')
            return text,data
        events = [(1,0,0,0,0),(10,1,2,4,1),(11,1,4,0,0),(20,1,6,0,0),
                  (21,1,8,0,0),(30,1,5,0,0),(40,1,6,0,0)]
        args = ([(10,1,1,4,0)],[1,1],[20,40],[1,1],
                struct.pack('<QBQB',20,0,40,1),50)
        report = full_lifecycle_report(*capture(events),*args)
        self.assertEqual(report['unknown_prefix_completions'],0)
        self.assertEqual(report['source_epochs_completed'],1)
        with self.assertRaises(ValueError):
            full_lifecycle_report(*capture(events[:-1]),*args)
        changed = list(args)
        changed[3] = [1,0]
        with self.assertRaises(ValueError):
            full_lifecycle_report(*capture(events),*changed)

    def test_binary_event_integrity_and_decoder(self):
        data = struct.pack('<QHBBI',1,0x1234,2,4,1) + struct.pack('<QHBBI',2,0x1234,4,0,0)
        checksum = 2166136261
        for value in data:
            checksum = ((checksum ^ value)*16777619)&0xffffffff
        text = (f'mu4_tx_event_capture: records=2 bytes=32 fnv1a={checksum:08x} capped=0 '
                'encoding=le64ns_u16word_u8event_u8owner_u32source file=mu4_playback_tx.events '
                'save_restore=unsupported observer_writes=0 audio_acceptance=0')
        self.assertEqual(check_tx_event_capture(text,data)['records'],2)
        decoded = list(tx_event_rows(data))
        self.assertEqual([row['kind'] for row in decoded],['accept','load_fresh'])
        self.assertEqual(decoded[0]['dma_read'],'1')
        for changed,binary in [(text.replace('capped=0','capped=1'),data),
                               (text,data[:-1]),(text,data[:-1]+b'\x01'),
                               (text.replace('records=2','records=1'),data)]:
            with self.assertRaises(ValueError):
                check_tx_event_capture(changed,binary)
        with self.assertRaises(ValueError):
            list(tx_event_rows(struct.pack('<QHBBI',1,0,9,0,0)))

    def test_duplicate_completion_cannot_hide_missing_event(self):
        data = fixture([('accept',1,1,1),('load_fresh',2,1,0),('complete',3,1,0)])
        events,writes,tx,times = data
        events.append(dict(events[-1],ordinal='4'))
        with self.assertRaises(ValueError):
            bind_lifecycle(iter(events),writes,tx,times)

    def test_nonshifting_ownership_needs_completion_not_zero_matching(self):
        events = [dict(kind=kind,ns=str(time),word=f'{word:04x}') for kind,time,word in [
            ('load_fresh',5,0x1235),('complete',10,0x1235),('wait_frame',11,0x1235),
            ('reset',15,0),('accept',16,0),('reset',17,0),
            ('load_fresh',30,0x1234),('complete',40,0x1234)]]
        slots = b''.join(struct.pack('<QB',time,0) for time in (10,12,16,20,40))
        result = nonshifting_din(events,[10,40],[0x1235,65535,65535,65535,0x1234],slots)
        self.assertEqual(result['covered_by_transition'],{'wait_frame':1,'reset':2})
        self.assertEqual(result['uncovered_words'],0)
        with self.assertRaises(ValueError):
            nonshifting_din(events,[10,40],[0x1235,65535,0,65535,0x1234],slots)
        unknown = [dict(kind='load_fresh',ns='1',word='0000'),
                   dict(kind='reset',ns='2',word='0000'),
                   dict(kind='load_fresh',ns='5',word='0000')]
        result = nonshifting_din(unknown,[],[0],struct.pack('<QB',3,0))
        self.assertEqual(result['uncovered_words'],1)
        self.assertEqual(result['covered_by_transition'],{})

    def test_frame_wait_din_uses_final_bit_not_sample_matching(self):
        events = [{'kind':'wait_frame','ns':'10','word':'1235'},
                  {'kind':'accept','ns':'11','word':'0000'},
                  {'kind':'load_fresh','ns':'20','word':'0000'}]
        slots = b''.join(struct.pack('<QB',time,0) for time in (9,10,19,20))
        self.assertEqual(frame_wait_din(events,[7,65535,65535,8],slots)['din_words'],2)
        with self.assertRaises(ValueError):
            frame_wait_din(events,[7,0,65535,8],slots)
        self.assertTrue(frame_wait_din(events[:1],[],b'')['open_endpoint_interval'])

    def test_wait_frame_observes_underrun_without_a_completion(self):
        result,completed = bind_lifecycle(*fixture([
            ('accept',1,0x1234,1),('load_fresh',2,0x1234,0),
            ('complete',3,0x1234,0),('wait_frame',4,0x1234,0),
            ('accept',5,0x5678,2),('load_fresh',6,0x5678,0),
            ('complete',7,0x5678,0)]))
        self.assertEqual(set(completed),{1,2})
        for specification in [
            [('accept',1,1,1),('wait_frame',2,1,0)],
            [('accept',1,1,1),('load_fresh',2,1,0),('wait_frame',3,1,0)],
            [('accept',1,1,1),('load_fresh',2,1,0),('complete',3,1,0),('wait_frame',4,2,0)]]:
            with self.assertRaises(ValueError):
                bind_lifecycle(*fixture(specification))

    def test_actual_load_completion_and_retained_repeat(self):
        data = fixture([('accept',10,0x1234,1),('load_fresh',11,0x1234,0),
                        ('complete',20,0x1234,0),('load_repeat',21,0x1234,0),
                        ('complete',30,0x1234,0)])
        result,completed = bind_lifecycle(*data)
        self.assertEqual(result['source_epochs_completed'],1)
        self.assertEqual(len(completed[1]),2)
        self.assertFalse(result['native_audio_validated'])

    def test_overwrite_discards_only_buffer_not_active_shift(self):
        data = fixture([('accept',10,0x1234,1),('load_fresh',11,0x1234,0),
                        ('accept',12,0x5678,2),('overwrite',13,0x9abc,3),
                        ('complete',20,0x1234,0),('load_fresh',21,0x9abc,0),
                        ('complete',30,0x9abc,0)])
        result,completed = bind_lifecycle(*data)
        self.assertEqual(set(completed),{1,3})
        self.assertEqual(result['discarded'],1)
        self.assertEqual(result['endpoint_pending'],[])

    def test_restart_retains_word_and_reset_discards_both_stages(self):
        result,completed = bind_lifecycle(*fixture([
            ('accept',10,1,1),('load_fresh',11,1,0),('restart',12,1,0),
            ('accept',13,2,2),('reset',14,0,0),('ignore',15,3,3)]))
        self.assertEqual(result['discarded'],2)
        self.assertEqual(result['ignored'],1)
        self.assertEqual(completed,{})

    def test_unknown_prefix_is_never_assigned_a_future_write(self):
        result,completed = bind_lifecycle(*fixture([
            ('load_fresh',1,0xabcd,0),('complete',2,0xabcd,0),
            ('accept',3,0xabcd,1),('load_fresh',4,0xabcd,0),('complete',5,0xabcd,0)]))
        self.assertEqual(result['unknown_prefix_completions'],1)
        self.assertEqual(completed[1],[(BASE+5,0xabcd)])

    def test_wrong_values_ownership_and_buffer_states_are_rejected(self):
        original = fixture([('accept',10,0x1234,1),('load_fresh',11,0x1234,0),('complete',20,0x1234,0)])
        for index,key,value in [(0,'dma_read','2'),(0,'dma_owner','0'),
                                (1,'word','1235'),(1,'kind','load_repeat'),
                                (2,'word','1235'),(2,'ordinal','4'),(2,'kind','bogus')]:
            events = [dict(row) for row in original[0]]
            events[index][key] = value
            with self.subTest(index=index,key=key),self.assertRaises(ValueError):
                bind_lifecycle(events,*original[1:])

    def test_startup_reset_discards_first_source_without_value_fitting(self):
        events,writes,tx,times = fixture([
            ('reset',10,0,0),('accept',11,0,1),('reset',12,0,0),
            ('accept',13,0,2),('load_fresh',14,0,0),('complete',20,0,0)])
        delta = 176_700_000
        for row in events:
            row['ns'] = str(int(row['ns'])-delta)
            row.update(pcr='000e',spcr2='0101',xcr1='0140',xcr2='0044')
        writes = [(time-delta,ordinal,word,owner,reserved) for time,ordinal,word,owner,reserved in writes]
        times = [time-delta for time in times]
        text = '\n'.join('mu4_tx_startup: '+' '.join(f'{key}={value}' for key,value in row.items()) for row in events)
        text += ('\nmu4_startup_frame: state=1 ns=823299900 observer_writes=0'
                 '\nmu4_startup_frame: state=0 ns=823299983 observer_writes=0'
                 '\nmu4_tx_startup_terminal: events=6 frames=2 first=0.817000000 last=0.824000000 capped=0 t=2.016666667 observer_writes=0')
        report = startup_report(text,writes,tx,times,[0],struct.pack('<QB',times[0]+41,0))
        self.assertEqual(report['discarded_source_ordinals'],[1])
        self.assertEqual(report['first_completed_source'],2)
        self.assertEqual(report['first_completed_slot'],0)

    def test_codec_simultaneous_mute_and_independent_control_bits(self):
        prefix = 'mu4_codec_control: word='
        suffix = (' power=007 analog=00a digital=008 format=001 rate=020 active=000 '
                  'observer_writes=0 audio_acceptance=0')
        muted = prefix+'0580 ns=10 hp_left=180 hp_right=000'+suffix
        self.assertEqual(codec_control_history(muted)[-1][1][2:4],(0x180,0))
        restored = muted+'\n'+prefix+'05f0 ns=20 hp_left=1f0 hp_right=070'+suffix
        self.assertEqual(codec_control_history(restored)[-1][1][2:4],(0x1f0,0x70))
        independent = (prefix+'0401 ns=10 hp_left=001 hp_right=079'+suffix+'\n'+
                       prefix+'0790 ns=20 hp_left=010 hp_right=190'+suffix)
        self.assertEqual(codec_control_history(independent)[-1][1][2:4],(0x10,0x190))
        with self.assertRaisesRegex(ValueError,'latch contract'):
            codec_control_history(muted.replace('hp_right=000','hp_right=079'))


if __name__ == '__main__':
    unittest.main()
