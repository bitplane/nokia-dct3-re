import unittest
from unittest.mock import patch

from tools.npe3_dedicated_check import check,packets


class DedicatedSubmissionTest(unittest.TestCase):
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


if __name__=='__main__':unittest.main()
