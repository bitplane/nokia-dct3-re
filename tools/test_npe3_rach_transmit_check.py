import unittest
from tools.npe3_rach_transmit_check import check,transactions,check_windows,check_releases
from tools.test_gsm_rach_signal import WORDS


class NativeAccessTransmitTest(unittest.TestCase):
    text=('rom6_measure_rx_commit: sequence=1 t=1.000000000 previous=0080 producer=0083 consumer=0080 words=0484,0b00,0a36 mask=ffff cpu_pc=002d08 observer_writes=0\n'
          'rom6_measure_rx_complete: commits=1 cap=64 observer_writes=0')

    def journal(self):
        return '\n'.join(['2\t003b7d\t0021\t0ac3\tffff']+
            [f'{2.01+i/100:.2f}\t003b95\t0027\t{word:04x}\tffff' for i,word in enumerate(WORDS)]+
            ['2.2\t003bdf\t0021\t06c3\tffff'])

    def test_complete_serialization_and_report_correspondence(self):
        result=check(self.text,self.journal())
        self.assertEqual(result['random_access_octets'],[11])
        self.assertEqual(result['reported_frames'],[2614])
        self.assertFalse(result['physical_release_qualified'])
        self.assertFalse(result['registration_qualified'])

    def test_incomplete_aborted_or_unframed_transactions_reject(self):
        rows=self.journal().splitlines()
        for bad in ('', '\n'.join(rows[1:]),'\n'.join(rows[:-1]),
                    '\n'.join([rows[0]]+rows),self.journal().replace('06c3','04c3'),
                    self.journal().replace('b9ff\tffff','b9ff\t00ff'),
                    self.journal().replace('2.01\t','1.99\t'),
                    self.journal().replace('2.01\t','nan\t'),
                    '\n'.join(rows[:-1]+[rows[1],rows[-1]])):
            with self.subTest(bad=bad[:80]),self.assertRaises(ValueError):
                check(self.text,bad)

    def test_native_report_does_not_supply_decoder_result(self):
        for text in (self.text.replace('0b00','0800'),self.text.replace('t=1.000000000','t=3.000000000'),
                     self.text.replace('0484','0485'),self.text.replace('0a36','ffff').replace('0b00','0bff')):
            with self.assertRaises(ValueError):check(text,self.journal())
        self.assertEqual(len(transactions(self.journal())),1)

    def test_counter_projection_is_bound_to_commit_and_actual_controls(self):
        controls=(0x71f,0x8aa,0x67c,0x90e)
        due=[2.2+((target-0x62c)%5000)*12/13000000-0.2e-6 for target in controls]
        text=('npe3_tx_window: sequence=1 t=2.200000000 words=15 counter=062c '
              'controls=071f,08aa,067c,090e deadlines='+','.join(f'{at:.9f}' for at in due)+
              ' observer_writes=0 physical_claim=0')
        bursts=[(2,2.2,WORDS,controls)]
        result=check_windows(text,bursts,[476])
        self.assertFalse(result['physical_release_qualified'])
        for bad in ('',text+'\n'+text,text.replace('words=15','words=14'),
                    text.replace('counter=062c','counter=062d'),
                    text.replace('071f,08aa','071e,08aa'),
                    text.replace('physical_claim=0','physical_claim=1'),
                    text.replace(f'{due[0]:.9f}','2.200000000')):
            with self.assertRaises(ValueError):check_windows(bad,bursts,[476])
        with self.assertRaises(ValueError):check_windows(text,bursts,[477])
        with self.assertRaises(ValueError):check(self.text,self.journal(),require_windows=True)

    def test_release_requires_explicit_candidate_exact_buffer_and_deadline(self):
        marker='npe3_tx_candidate: p05_start=1 port_buffer_only=1 network_reply=0 physical_claim=0\n'
        row=('npe3_tx_release: sequence=1 t=2.300000000 target=071f counter=071f words='+
             ','.join(f'{word:04x}' for word in WORDS)+' network_reply=0 physical_claim=0')
        burst=(2,2.2,WORDS,(0x71f,0x8aa,0x67c,0x90e))
        windows=dict(p05_deadlines=[2.3])
        self.assertTrue(check_releases(marker+row,[burst],windows)['timed_candidate_release_qualified'])
        for bad in (row,marker,marker+row+'\n'+row,marker+row.replace('counter=071f','counter=071e'),
                    marker+row.replace('2.300000000','2.300000010'),marker+row.replace('b9ff','b9fe'),
                    marker+row.replace('network_reply=0','network_reply=1')):
            with self.assertRaises(ValueError):check_releases(bad,[burst],windows)
        with self.assertRaises(ValueError):check(self.text+'\n'+marker,self.journal())


if __name__=='__main__':unittest.main()
