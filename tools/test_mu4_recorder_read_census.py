import unittest

from tools.mu4_recorder_read_census import check,check_buffers


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
    def test_buffer_lifetimes_distinguish_read_retired_and_pending_epochs(self):
        text = '\n'.join([
            'mu4_recorder_buffer: bank=0 writes=4 read_epochs=2 retired_unread=1 pending_unread=1 unknown_reads=5 observer_writes=0',
            'mu4_recorder_buffer: bank=1 writes=3 read_epochs=1 retired_unread=2 pending_unread=0 unknown_reads=0 observer_writes=0',
            'mu4_recorder_buffer_reader: bank=0 dma_mask=00 reads=3 observer_writes=0',
            'mu4_recorder_buffer_reader: bank=1 dma_mask=02 reads=2 observer_writes=0',
            'mu4_recorder_buffer_context: bank=0 context_pc=029000 reads=3 observer_writes=0',
            'mu4_recorder_buffer_terminal: bad=0 dropped=0 context_cap=256 context_pc=not_instruction_owner save_restore=unsupported observer_writes=0 encoder_consumption=0'])
        destinations = dict(destinations=[dict(space='data',minimum='001800',maximum='0018ff',writes=4),
                                          dict(space='data',minimum='001900',maximum='0019ff',writes=3)])
        result = check_buffers(text,destinations)
        self.assertEqual(result['banks'][0]['known_reads'],3)
        self.assertEqual(result['banks'][1]['read_epochs'],1)
        for before,after in [('read_epochs=2','read_epochs=3'),('bad=0','bad=1'),
                             ('dropped=0','dropped=1'),('context_pc=029000 reads=3','context_pc=029000 reads=2'),
                             ('bank=1 dma_mask=02 reads=2','bank=1 dma_mask=02 reads=0'),
                             ('pending_unread=1','pending_unread=257')]:
            with self.subTest(before=before),self.assertRaises(ValueError):
                check_buffers(text.replace(before,after),destinations)
        with self.assertRaises(ValueError):
            check_buffers(text,None)

    def test_destination_pairing_rejects_missing_changed_and_duplicate_writes(self):
        text = fixture()+'\n'+'\n'.join([
            'mu4_recorder_destination: channel=1 space=0 writes=2 minimum=020000 maximum=020001 observer_writes=0',
            'mu4_recorder_destination: channel=2 space=1 writes=40 minimum=001000 maximum=001027 observer_writes=0',
            'mu4_recorder_destination_terminal: bad=0 pending=0 save_restore=unsupported observer_writes=0 encoder_consumption=0'])
        result = check(text)['dma_destinations']
        self.assertTrue(result['paired_values_validated'])
        self.assertEqual(result['destinations'][1]['space'],'data')
        for before,after in [('writes=40','writes=39'),('pending=0','pending=1'),
                             ('bad=0','bad=1'),('channel=1','channel=2'),
                             ('minimum=001000','minimum=002000'),('space=1','space=3')]:
            with self.subTest(before=before),self.assertRaises(ValueError):
                check(text.replace(before,after))
        with self.assertRaises(ValueError):
            check(text+'\nmu4_recorder_destination: channel=2 space=1 writes=40 minimum=001000 maximum=001027 observer_writes=0')

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
