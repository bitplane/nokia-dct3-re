import unittest
from tools.npe3_rach_transmit_check import check,transactions
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


if __name__=='__main__':unittest.main()
