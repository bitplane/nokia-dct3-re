import unittest
from tools.rom6_adc_lab_check import check,reference_word,continuous_queue_bound


class AdcLabCheckTest(unittest.TestCase):
    def fixture(self,tone=True,divider=6,burst=False):
        header=f'rf_adc_lab: direct_adc=1 tuned_air_source=0 tone={int(tone)} reference=13000000 source_phase_divider=12 word_divider={divider} amplitude=1024 physical_clock_claim=0'
        lines=[header,'mfi_rx_enable: state=1 produced=0 consumed=0 overrun=0 underrun=0 t=0.010000000']
        if burst:
            lines.append('rf_adc_burst: frame_ticks=60000 multiframe=51 frames=0,10,20,30,40 active_ticks=7104 epoch=0 envelope=rectangular other_slots=zero tuned_air_source=0')
        for sequence in range(1,65):
            tick=(7008 if burst else 130008)+(sequence-1)*divider
            stamp=((tick+384)*1_000_000_000+12_999_999)//13_000_000
            seconds,fraction=divmod(stamp,1_000_000_000)
            lines.append(f'mfi_rx_word: sequence={sequence} value={reference_word(sequence,tick,tone,divider,burst):04x} source_tick={tick} t={seconds}.{fraction:09d}')
            if sequence==1:
                lines.append(f'rom6_adc_first_read: t={seconds}.{fraction:09d} cpu_pc=002446 observer_writes=0')
        lines.extend(['rom6_adc_lab_complete: result_words=0 cap=32768 reads=64 observer_writes=0',
                      'mfi_rx_capacity_observation: configured=64 peak=64 physical_capacity_claim=0',
                      'mfi_rx_trace_complete: words=64 blocks=0 word_cap=4096 block_cap=128',
                      'rom4_interface_summary: completion_strobes=0 rf_reads=64',
                      f'mfi_rx_summary: produced=64 consumed=64 queued=0 overrun=0 underrun=0 fault=0 divider={divider} physical_clock_claim=0'])
        return '\n'.join(lines)

    def test_tone_and_silence_do_not_claim_decoder_success(self):
        for tone in (False,True):
            for divider in (6,12):
                result=check(self.fixture(tone,divider),tone)
                self.assertEqual(result['checked_prefix_words'],64)
                self.assertEqual(result['word_divider'],divider)
                self.assertFalse(result['decoder_qualified'])
                self.assertFalse(result['tuned_rf_qualified'])

    def test_finite_burst_edges_and_latched_q(self):
        text=self.fixture(True,12,True)
        self.assertTrue(check(text,True,True)['finite_fcch_bursts'])
        self.assertEqual(reference_word(1,7104,True,12,True),0)
        self.assertEqual(reference_word(2,7104,True,12,True),(-392)&0xffff)
        self.assertEqual(reference_word(1,600000,True,12,True),1024)
        self.assertEqual(reference_word(1,60000,True,12,True),0)
        with self.assertRaises(ValueError): check(text,True)
        with self.assertRaises(ValueError): check(text.replace('active_ticks=7104','active_ticks=7103'),True,True)

    def test_fault_is_not_masked_by_good_prefix(self):
        result=check(self.fixture()+'\nmfi_rx_fault: overrun=1 produced=64 consumed=0 t=0.100000000'
                     +'\nrf_adc_lab_fault_context: cpu_pc=00b55d st1=c903 imr=035f ifr=0021 observer_writes=0 t=0.100000000',True)
        self.assertFalse(result['transport_fault_free'])
        self.assertEqual(result['fault_cpu_contexts'][0]['st1'],'c903')

    def test_closed_capture_counts_before_disable_resets_counters(self):
        text=self.fixture(False).replace(
            'mfi_rx_summary: produced=64 consumed=64 queued=0',
            'mfi_rx_enable: state=0 produced=64 consumed=64 overrun=0 underrun=0 t=0.020000000\n'
            'mfi_rx_summary: produced=0 consumed=0 queued=0')
        self.assertTrue(check(text,False)['all_reads_from_clocked_queue'])
        with self.assertRaises(ValueError):
            check(text.replace('state=0 produced=64 consumed=64','state=0 produced=64 consumed=32'),False)
        with self.assertRaises(ValueError):
            check(text.replace('state=1 produced=0 consumed=0','state=1 produced=1 consumed=0'),False)

    def test_service_gap_bound_and_clock_edge_uncertainty(self):
        self.assertEqual(continuous_queue_bound(32992152,12,11616,2548643758),(69,69))
        self.assertEqual(continuous_queue_bound(0,12,0,923),(1,2))
        with self.assertRaises(ValueError): continuous_queue_bound(0,0,0,0)

    def test_bad_word_provenance_or_missing_source_fails(self):
        text=self.fixture(False)
        for old,new in [('value=0000','value=ffff'),('source_tick=130008','source_tick=130014'),
                        ('tuned_air_source=0','tuned_air_source=1'),('sequence=64','sequence=65'),
                        ('rf_reads=64','rf_reads=0'),('word_divider=6','word_divider=12'),
                        ('divider=6 physical','divider=12 physical'),
                        ('rom6_adc_first_read: t=0.','rom6_adc_first_read: t=1.'),
                        ('consumed=64 queued=0','consumed=32 queued=32'),
                        ('consumed=64 queued=0','consumed=64 queued=1'),
                        ('state=1 produced=0','state=0 produced=0'),
                        ('words=64 blocks=0','words=63 blocks=0'),
                        ('words=64 blocks=0','words=64 blocks=129'),
                        ('configured=64 peak=64','configured=64 peak=65'),
                        ('configured=64 peak=64','configured=63 peak=63')]:
            with self.subTest(old=old),self.assertRaises(ValueError): check(text.replace(old,new,1),False)
        with self.assertRaises(ValueError):
            check('\n'.join(line for line in text.splitlines() if not line.startswith('mfi_rx_word: sequence=1 ')),False)
        with self.assertRaises(ValueError):
            check(text+'\nmfi_rx_fault: overrun=1 t=malformed'
                  +'\nrf_adc_lab_fault_context: cpu_pc=00b55d st1=c903 imr=035f ifr=0021 observer_writes=0 t=0.100000000',False)


if __name__=='__main__': unittest.main()
