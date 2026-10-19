import unittest

from tools.mu4_recorder_read_census import check,check_buffers,check_read_identity,pin_read_instructions,check_copies,check_workspace,check_ingress,check_consumer,decode_ingress_loop,INGRESS_BASE,INGRESS_WORDS,INGRESS_PROLOGUE_BASE,INGRESS_PROLOGUE


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
    def test_stack_consumer_binds_each_dynamic_call_and_retires_origins(self):
        text = '\n'.join([
            'mu4_recorder_ingress_frame: ordinal=1 argument=2000 observer_writes=0',
            'mu4_recorder_ingress_frame: ordinal=2 argument=2100 observer_writes=0',
            'mu4_recorder_consumer_frame: ordinal=1 base=2000 origins=2048 known_reads=2048 read_epochs=1024 retired_unread=512 unread_return=512 unknown_reads=8 writes=600 alias_writes=2 missing_data_identity=1 observer_writes=0',
            'mu4_recorder_consumer_frame: ordinal=2 base=2100 origins=2048 known_reads=2048 read_epochs=2048 retired_unread=0 unread_return=0 unknown_reads=4 writes=0 alias_writes=0 missing_data_identity=0 observer_writes=0',
            'mu4_recorder_consumer_reader: program_alias=0 dma_mask=00 reads=4092 observer_writes=0',
            'mu4_recorder_consumer_reader: program_alias=1 dma_mask=02 reads=4 observer_writes=0',
            'mu4_recorder_consumer_instruction: instruction=03c123 opcode=a493 reads=4091 observer_writes=0',
            'mu4_recorder_consumer_terminal: entries=2 returns=2 origins=4096 known_reads=4096 read_epochs=3072 retired_unread=512 unread_return=512 unknown_reads=12 writes=600 alias_writes=2 missing_data_identity=1 bad=0 dropped=0 active=0 cap=256 frame_cap=512 save_restore=unsupported observer_writes=0 encoder_consumption=0'])
        ingress = dict(frames=2,paired_stores=4096)
        self.assertEqual(check_consumer(text,ingress)['read_epochs'],3072)
        for before,after in [('returns=2','returns=1'),('active=0','active=1'),('bad=0','bad=1'),
                             ('base=2100','base=2200'),('retired_unread=512','retired_unread=511'),
                             ('alias_writes=2','alias_writes=601'),('reads=4092','reads=4091')]:
            with self.subTest(before=before),self.assertRaises(ValueError):
                check_consumer(text.replace(before,after),ingress)

    def test_sample_repack_binds_complete_known_frames_and_source_geometry(self):
        text = '\n'.join([
            'mu4_recorder_ingress_frame: ordinal=1 stores=1024,1024 known=1024,1024 source_base=7000,7400 destination_base=2000,2001 argument=2000 identity=Execute_entry observer_writes=0',
            'mu4_recorder_ingress_frame: ordinal=2 stores=1024,1024 known=1024,1024 source_base=7800,7c00 destination_base=2000,2001 argument=2000 identity=Execute_entry observer_writes=0',
            'mu4_recorder_ingress_terminal: frames=2 entries=2 loads=2048,2048 stores=2048,2048 known=2048,2048 pending_reads=0 pending_write=0 pending_call=0 partial=0,0 bad=0 dropped=0 frame_cap=512 queue_cap=4 save_restore=unsupported observer_writes=0 encoder_consumption=0'])
        workspace = dict(known_reads=4096,instructions=[dict(instruction='039823',opcode='10eb',reads=2048),dict(instruction='03982b',opcode='10e3',reads=2048)])
        self.assertEqual(check_ingress(text,workspace)['paired_stores'],4096)
        for before,after in [('entries=2','entries=1'),('bad=0','bad=1'),('pending_write=0','pending_write=1'),
                             ('known=1024,1024','known=1024,1023'),('7000,7400','7000,7401'),
                             ('2000,2001','2000,2002'),('partial=0,0','partial=1,0'),('argument=2000','argument=2001')]:
            with self.subTest(before=before),self.assertRaises(ValueError):
                check_ingress(text.replace(before,after),workspace)
        words = {INGRESS_BASE+index:word for index,word in enumerate(INGRESS_WORDS)}
        words.update({INGRESS_PROLOGUE_BASE+index:word for index,word in enumerate(INGRESS_PROLOGUE)})
        words[0x3983d] = 0xf7b8
        self.assertEqual(decode_ingress_loop(words)['iterations'],1024)
        words[0x3982c] ^= 1
        with self.assertRaises(ValueError):
            decode_ingress_loop(words)

    def test_workspace_accounts_alias_reads_writes_and_missing_operand_identity(self):
        text = '\n'.join([
            'mu4_recorder_workspace_reader: program_alias=0 dma_mask=00 reads=6 observer_writes=0',
            'mu4_recorder_workspace_reader: program_alias=1 dma_mask=02 reads=2 observer_writes=0',
            'mu4_recorder_workspace_instruction: instruction=03a000 opcode=e598 reads=5 observer_writes=0',
            'mu4_recorder_workspace_terminal: origin_writes=10 total_writes=20 alias_writes=3 read_epochs=7 retired_unread=2 pending_unread=1 known_reads=8 unknown_reads=12 bad=0 dropped=0 missing_data_identity=1 cap=256 save_restore=unsupported observer_writes=0 encoder_consumption=0'])
        copies = dict(destinations=[dict(destination_min='7000',destination_max='7fff',known_writes=10)])
        result = check_workspace(text,copies)
        self.assertEqual(result['read_epochs'],7)
        self.assertEqual(result['missing_data_identity'],1)
        for before,after in [('origin_writes=10','origin_writes=11'),('alias_writes=3','alias_writes=21'),
                             ('bad=0','bad=1'),('dropped=0','dropped=1'),('known_reads=8','known_reads=9'),
                             ('missing_data_identity=1','missing_data_identity=0'),('retired_unread=2','retired_unread=1')]:
            with self.subTest(before=before),self.assertRaises(ValueError):
                check_workspace(text.replace(before,after),copies)
        with self.assertRaises(ValueError):
            check_workspace(text+'\n'+text.splitlines()[0],copies)

    def test_copy_pairing_requires_exact_known_and_unknown_source_counts(self):
        text = '\n'.join([
            'mu4_recorder_copy_destination: bank=0 instruction=038eb5 opcode=e598 known_writes=10 unknown_writes=2 source_min=1800 source_max=18ff destination_min=0db5 destination_max=0e34 observer_writes=0',
            'mu4_recorder_copy_terminal: known_reads=10 unknown_reads=2 writes=12 pending=0 bad=0 dropped=0 pending_cap=64 site_cap=256 identity=staged_read_write save_restore=unsupported observer_writes=0 encoder_consumption=0'])
        identities = dict(instructions=[dict(bank=0,instruction='038eb5',opcode='e598',reads=10)])
        self.assertEqual(check_copies(text,identities)['paired_words'],12)
        for before,after in [('known_writes=10','known_writes=9'),('writes=12','writes=11'),
                             ('unknown_reads=2','unknown_reads=1'),('pending=0','pending=1'),
                             ('bad=0','bad=1'),('dropped=0','dropped=1'),
                             ('opcode=e598','opcode=f495'),('source_min=1800','source_min=1700')]:
            with self.subTest(before=before),self.assertRaises(ValueError):
                check_copies(text.replace(before,after),identities)
        with self.assertRaises(ValueError):
            check_copies(text+'\n'+text.splitlines()[0],identities)

    def test_read_instruction_identity_requires_exact_bus_coverage(self):
        text = '\n'.join([
            'mu4_recorder_buffer_instruction: bank=0 instruction=038eb5 opcode=e598 reads=10 observer_writes=0',
            'mu4_recorder_buffer_identity_terminal: missing=0 dropped=0 cap=256 identity=staged_operand_bus_read save_restore=unsupported observer_writes=0 retirement=0'])
        buffers = dict(banks={0:dict(readers={'00':10}),1:dict(readers={})})
        self.assertEqual(check_read_identity(text,buffers)['instructions'][0]['opcode'],'e598')
        identities = check_read_identity(text,buffers)
        self.assertEqual(pin_read_instructions(identities,{0x38eb5:0xe598})['instructions'][0]['operation'],'MVDD Xmem,Ymem')
        loads = dict(instructions=[dict(instruction='039823',opcode='10eb',reads=1)])
        self.assertEqual(pin_read_instructions(loads,{0x39823:0x10eb})['instructions'][0]['operation'],'LD Smem,A')
        for words in ({},{0x38eb5:0xf495}):
            with self.assertRaises(ValueError):
                pin_read_instructions(identities,words)
        for before,after in [('reads=10','reads=9'),('missing=0','missing=1'),
                             ('dropped=0','dropped=1'),('retirement=0','retirement=1'),
                             ('instruction=038eb5','instruction=838eb5')]:
            with self.subTest(before=before),self.assertRaises(ValueError):
                check_read_identity(text.replace(before,after),buffers)
        with self.assertRaises(ValueError):
            check_read_identity(text+'\n'+text.splitlines()[0],buffers)

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
