import unittest

from tools.mu4_recorder_read_census import check


def fixture():
    return '\n'.join([
        'mu4_recorder_read_scope: address=0021 context_cap=128 dma_identity=active_source_read context_pc=not_instruction_owner side_effects_disabled=excluded observer_writes=0',
        'mu4_recorder_ac_input: generated=32,32,32,32 received=10,10,10,10 other=0 other_owner_reads=5 shape=square periods=64,32 amplitudes=8192,4096 ownership=actual_channel2_source_read analog_adc=0 decoded_audio=0',
        'mu4_recorder_read_owner: dma_mask=00 reads=3 observer_writes=0',
        'mu4_recorder_read_owner: dma_mask=02 reads=2 observer_writes=0',
        'mu4_recorder_read_owner: dma_mask=04 reads=40 observer_writes=0',
        'mu4_recorder_read_context: context_pc=020123 reads=1 observer_writes=0',
        'mu4_recorder_read_context: context_pc=020456 reads=2 observer_writes=0',
        'mu4_recorder_read_terminal: reads=45 contexts=2 dropped=0 context_cap=128 t=4.000000000 save_restore=unsupported observer_writes=0'])


class RecorderReadCensusTests(unittest.TestCase):
    def test_publication_freshness_reconciles_every_owner_and_pending_epoch(self):
        text = fixture()+'\n'+'\n'.join([
            'mu4_recorder_rx_owner: dma_mask=00 fresh=0 stale=3 observer_writes=0',
            'mu4_recorder_rx_owner: dma_mask=02 fresh=2 stale=0 observer_writes=0',
            'mu4_recorder_rx_owner: dma_mask=04 fresh=40 stale=0 observer_writes=0',
            'mu4_recorder_rx_terminal: publications=43 discarded=0 pending=1 resets=1 bad=0 unknown_stale=1 save_restore=unsupported observer_writes=0 encoder_consumption=0'])
        result = check(text,True)['drr_freshness']
        self.assertEqual(result['fresh_reads'],42)
        self.assertEqual(result['stale_reads'],3)
        self.assertTrue(result['published_drr_delivery_validated'])
        for before,after in [('publications=43','publications=42'),('bad=0','bad=1'),
                             ('pending=1','pending=2'),('unknown_stale=1','unknown_stale=4'),
                             ('dma_mask=02 fresh=2','dma_mask=02 fresh=1')]:
            with self.assertRaises(ValueError):
                check(text.replace(before,after),True)
        with self.assertRaises(ValueError):
            check(fixture(),True)

    def test_distinguishes_other_dma_from_unowned_context(self):
        result = check(fixture())
        self.assertEqual(result['channel2_selected_reads'],40)
        self.assertEqual(result['unowned_bus_reads'],3)
        self.assertEqual(result['dma_masks']['02'],2)
        self.assertFalse(result['encoder_consumption_validated'])

    def test_missing_capped_and_inconsistent_evidence_fails(self):
        for before,after in [('dropped=0','dropped=1'),('reads=45','reads=46'),
                             ('contexts=2','contexts=1'),('dma_mask=04','dma_mask=40'),
                             ('other_owner_reads=5','other_owner_reads=6'),
                             ('context_pc=020456','context_pc=020123'),
                             ('t=4.000000000','t=NaN')]:
            with self.subTest(before=before),self.assertRaises(ValueError):
                check(fixture().replace(before,after))
        with self.assertRaises(ValueError):
            check(fixture()+'\nmu4_recorder_read_owner: dma_mask=04 reads=40 observer_writes=0')

    def test_nonunique_owner_mask_is_exposed_not_silently_relabelled(self):
        result = check(fixture().replace('dma_mask=04','dma_mask=06'))
        self.assertEqual(result['multi_owner_masks'],['06'])


if __name__ == '__main__':
    unittest.main()
