import unittest

from tools.mu4_output_scale_check import check


def fixture():
    return '\n'.join([
        'mu4_scale_scope: first=1.06 last=1.10 cap=100000 read_context=029fea..029ff1 write_context=029fee boundary=data_bus observer_writes=0 audio_acceptance=0',
        'mu4_pcm_copy_bus: kind=write address=1000 word=fffd t=1.065 observer_writes=0',
        'mu4_scale_bus: ordinal=1 kind=read address=1000 word=fffd mask=ffff t=1.070 context_pc=029fee st1=0100 pmst=0021 last_write_t=1.065 last_write_pc=02a367 last_write_word=fffd last_write_mask=ffff observer_writes=0',
        'mu4_scale_bus: ordinal=2 kind=read address=1202 word=0800 mask=ffff t=1.070 context_pc=029fee st1=0100 pmst=0021 observer_writes=0',
        'mu4_scale_bus: ordinal=3 kind=write address=1000 word=fffe mask=ffff t=1.071 context_pc=029fee st1=0100 pmst=0021 observer_writes=0',
        'mu4_processing_bus: kind=read address=1000 word=fffe last_write_pc=029fee last_write_t=1.071 observer_writes=0',
        'mu4_scale_terminal: events=3 closed=1 capped=0 illegal=0 t=2.016666667 observer_writes=0',
    ])


class OutputScaleTest(unittest.TestCase):
    def test_signed_floor_from_actual_bus_words(self):
        result = check(fixture())
        self.assertEqual(result['outputs'],1)
        self.assertEqual(result['changed_samples'],1)
        self.assertEqual(result['input_origins'],{'copy':1})
        self.assertEqual(result['processing_reads_matched'],1)
        self.assertFalse(result['native_audio_validated'])

    def test_rejects_wrong_arithmetic_modes_or_provenance(self):
        for old,new in [('kind=write address=1000 word=fffe','kind=write address=1000 word=ffff'),
                        ('word=0800','word=1000'),('st1=0100','st1=0140'),
                        ('pmst=0021','pmst=0020'),('last_write_word=fffd','last_write_word=fffc'),
                        ('word=fffd t=1.065','word=fffc t=1.065'),
                        ('capped=0','capped=1'),('ordinal=3','ordinal=4')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(fixture().replace(old,new))

    def test_changed_upstream_writer_is_reported_not_promoted(self):
        result = check(fixture().replace('last_write_pc=02a367','last_write_pc=029000'))
        self.assertEqual(result['input_origins'],{'other_029000':1})

    def test_actual_dma_read_is_excluded_not_consumed_as_cpu_operand(self):
        text = fixture()
        extra = ('mu4_scale_bus: ordinal=3 kind=read address=10c0 word=0000 mask=ffff t=1.0705 '
                 'context_pc=029fef st1=0100 pmst=0021 observer_writes=0\n'
                 'mu4_playback_dma_epoch: address=10c0 word=0000 t=1.0705 channel=3 ownership=active_source_read observer_writes=0\n')
        text = text.replace('mu4_scale_bus: ordinal=3',extra+'mu4_scale_bus: ordinal=4').replace('events=3','events=4')
        self.assertEqual(check(text)['excluded_actual_dma_reads'],1)

    def test_ambiguous_cpu_dma_collision_is_rejected(self):
        text = fixture()
        read = next(line for line in text.splitlines() if 'ordinal=1 kind=read' in line)
        text = text.replace(read,read+'\n'+read)
        text += '\nmu4_playback_dma_epoch: address=1000 word=fffd t=1.070 channel=3 ownership=active_source_read observer_writes=0'
        text = text.replace('events=3','events=4')
        with self.assertRaisesRegex(ValueError,'ambiguous ownership'):
            check(text)


if __name__ == '__main__':
    unittest.main()
