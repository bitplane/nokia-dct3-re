import unittest

from tools.mu4_serial_epoch_check import bind_lifecycle


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


if __name__ == '__main__':
    unittest.main()
