import unittest
from unittest.mock import patch

from tools.npe3_dedicated_check import check,packets,check_sacch


class DedicatedSubmissionTest(unittest.TestCase):
    def test_sacch_binds_cell_content_schedule_and_native_decodes(self):
        si2=bytes([0x59,6,0x1a])+bytes(20)
        si3=bytearray(23);si3[:3]=bytes([0x49,6,0x1b]);si3[3:10]=bytes.fromhex('000100f1100001')
        blocks=[bytes.fromhex('0500030349061d')+bytes(16),
                bytes.fromhex('050003032d061e000100f110000100ff')+b'\x2b'*7]
        text=(f'rf_adc_broadcast_si: index=1 arfcn=35 octets={si2.hex()} physical_claim=0\n'
              f'rf_adc_broadcast_si: index=2 arfcn=35 octets={si3.hex()} physical_claim=0\n')
        text+='\n'.join(f'npe3_bts_sacch: index={i} starts=42 period=102 octets={b.hex()} tsc=0 raw_adc_only=1 physical_claim=0'
                         for i,b in enumerate(blocks))
        native=[dict(type=128,payload='702a00'+frame.to_bytes(3,'big').hex()+'00230001'+block.hex(),time=str(t))
                for frame,block,t in ((45,blocks[0],0.21),(147,blocks[1],0.68))]
        with patch('tools.rom6_adc_measure_check.check_rx_publications',return_value=dict(packets=native)):
            self.assertEqual(check_sacch(text,5)['successful_end_frames'],[45,147])
            for bad in ('',text+text,text.replace('05000303','04000303'),text.replace('period=102','period=51')):
                with self.assertRaises(ValueError):check_sacch(bad,5)
            with self.assertRaises(ValueError):check_sacch(text,0)
            with patch('tools.rom6_adc_measure_check.check_rx_publications',return_value=dict(packets=native[:1])):
                with self.assertRaises(ValueError):check_sacch(text,5)
            with patch('tools.rom6_adc_measure_check.check_rx_publications',return_value=dict(packets=[dict(native[0],time='0.01'),native[1]])):
                with self.assertRaises(ValueError):check_sacch(text,5)

    def text(self):
        return ('npe3_mcu_tx_commit: sequence=1 t=12.098916000 previous=0000 producer=000d '
                'words=1802,042a,0000,0000,0040,8000,0023,4000,0000,0000,0000,0000,0000 '
                'pc_context=00426a20 observer_writes=0\n'
                'npe3_mcu_tx_commit: sequence=2 t=12.156672231 previous=000d producer=001c '
                'words=1c1b,0080,013f,4905,0870,00f0,00ff,fe33,0809,1010,1032,5476,982b,2b00,0000 '
                'pc_context=00426a20 observer_writes=0\n'
                'npe3_mcu_tx_complete: commits=2 cap=256 invalid=0 begin=0 end=24 observer_writes=0')

    @patch('tools.npe3_dedicated_check.check_agch',return_value={'native_publication_times':['12.094406923']})
    def test_ordered_assignment_and_lu_submission(self,_):
        result=check(self.text())
        self.assertTrue(result['mcu_assignment_acceptance_qualified'])
        self.assertEqual(result['location_updating_request'],'05087000f000fffe33080910101032547698')
        self.assertFalse(result['native_dedicated_transmission_qualified'])
        self.assertFalse(result['registration_qualified'])
        for bad in (self.text().replace('8000','6000'),self.text().replace('013f','0173'),
                    self.text().replace('0870','0270'),self.text().replace('4905','4505'),
                    self.text().replace('12.098916000','12.090000000')):
            with self.assertRaises(ValueError):check(bad)

    def test_complete_uncapped_ring_and_header_lengths_required(self):
        self.assertEqual(len(packets(self.text())),2)
        for bad in ('',self.text().replace('commits=2','commits=3'),
                    self.text().replace('previous=000d','previous=000c'),
                    self.text().replace('producer=001c','producer=001b'),
                    self.text().replace('1c1b','1a1b'),self.text().replace('begin=0','begin=2'),
                    self.text().replace('invalid=0','invalid=1')):
            with self.assertRaises(ValueError):packets(bad)

    def test_long_observation_checks_declared_horizon_and_full_ring(self):
        long=self.text().replace('cap=256','cap=2048').replace('end=24','end=60').replace('12.156672231','59.999000000')
        self.assertEqual(len(packets(long)),2)
        for bad in (long.replace('end=60','end=24'),long.replace('cap=2048','cap=256'),
                    long.replace('59.999000000','60.001000000'),long.replace('end=60','end=121'),
                    long.replace('commits=2','commits=2049')):
            with self.assertRaises(ValueError):packets(bad)


if __name__=='__main__':unittest.main()
