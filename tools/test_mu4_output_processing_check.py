import unittest

from tools.mu4_output_processing_check import check


def fixture():
    return '\n'.join([
        'mu4_processing_scope: first=1.06 last=1.10 cap=100000 read_context=02a2c6..02a2ea read_address=ar7 write_context=02a2ea boundary=data_bus observer_writes=0 audio_acceptance=0',
        'mu4_pcm_copy_bus: kind=write address=1000 word=1234 t=1.025 observer_writes=0',
        'mu4_processing_bus: ordinal=1 kind=read address=1000 word=1234 mask=ffff t=1.070 context_pc=02a2d2 ar7=1000 last_write_t=1.025 last_write_pc=02a367 last_write_word=1234 last_write_mask=ffff observer_writes=0',
        'mu4_processing_bus: ordinal=2 kind=write address=1000 word=5678 mask=ffff t=1.071 context_pc=02a2ea ar7=1001 observer_writes=0',
        'mu4_playback_dma_epoch: address=1000 word=5678 t=1.080 last_write_t=1.071 last_write_pc=02a2ea last_write_word=5678 last_write_mask=ffff channel=3 ownership=active_source_read observer_writes=0',
        'mu4_processing_terminal: events=2 closed=1 capped=0 illegal=0 t=2.016666667 observer_writes=0',
    ])


class ProcessingTest(unittest.TestCase):
    def test_fixed_actual_epoch_chain(self):
        result = check(fixture())
        self.assertEqual(result['input_origins'],{'copy':1})
        self.assertEqual(result['changed_sample_writes'],1)
        self.assertEqual(result['dma_reads_matched'],1)
        self.assertFalse(result['native_audio_validated'])

    def test_rejects_bad_values_missing_reads_and_bad_terminal(self):
        for old,new in [('last_write_word=1234','last_write_word=1235'),
                        ('last_write_word=5678','last_write_word=5679'),
                        ('word=1234 t=1.025','word=1235 t=1.025'),
                        ('ar7=1000','ar7=1001'),('ordinal=2','ordinal=3'),
                        ('capped=0','capped=1'),('illegal=0','illegal=1')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(fixture().replace(old,new))

    def test_other_writer_is_not_promoted_to_copy_provenance(self):
        result = check(fixture().replace('last_write_pc=02a367','last_write_pc=029fee'))
        self.assertEqual(result['input_origins'],{'other_029fee':1})

    def test_single_boundary_write_is_censored_not_claimed_as_transformed(self):
        text = fixture()
        text = '\n'.join(line for line in text.splitlines() if 'ordinal=1 kind=read' not in line)
        text = text.replace('ordinal=2','ordinal=1').replace('events=2','events=1')
        result = check(text)
        self.assertEqual(result['left_censored_sample_writes'],1)
        self.assertEqual(result['changed_sample_writes'],0)


if __name__ == '__main__':
    unittest.main()
