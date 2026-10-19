import unittest

from tools.mu4_pcm_dma_epoch_check import check


def fixture():
    return ('mu4_pcm_copy_bus: kind=write address=1000 word=1234 t=1.010000000 observer_writes=0\n'
            'mu4_playback_dma_epoch: read=10 address=1000 word=1234 t=1.020000000 '
            'last_write_t=1.010000000 last_write_pc=02a367 last_write_word=1234 '
            'last_write_mask=ffff channel=3 ownership=active_source_read observer_writes=0 audio_acceptance=0\n'
            'mu4_pcm_copy_terminal: closed=1 capped=0 illegal=0 t=2.016666667 observer_writes=0\n')


class DmaEpochTest(unittest.TestCase):
    def test_actual_copy_epoch(self):
        result = check(fixture())
        self.assertEqual(result['copy_origin_reads'],1)
        self.assertFalse(result['native_audio_validated'])

    def test_bad_values_ownership_and_missing_copy_are_rejected(self):
        for old,new in [('last_write_word=1234','last_write_word=5678'),
                        ('last_write_mask=ffff','last_write_mask=00ff'),
                        ('last_write_t=1.010000000','last_write_t=1.030000000'),
                        ('channel=3','channel=2'),('ownership=active_source_read','ownership=enabled'),
                        ('kind=write address=1000','kind=write address=1001'),
                        ('closed=1','closed=0'),('illegal=0','illegal=1')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(fixture().replace(old,new))

    def test_unknown_prior_writer_is_not_decoder_provenance(self):
        result = check(fixture().replace('last_write_t=1.010000000','last_write_t=-1'))
        self.assertEqual(result['unobserved_prior_writes'],1)
        self.assertEqual(result['copy_origin_reads'],0)


if __name__ == '__main__':
    unittest.main()
