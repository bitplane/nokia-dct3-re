import unittest
from tools.nsm1_fdn_dial_check import disabled_card,RECORD
from tools.sim_fdn_profile import EXTENDED_LENGTH


class FdnDialControlsTests(unittest.TestCase):
    def test_off_control_changes_only_retained_validity(self):
        card=bytearray(EXTENDED_LENGTH);card[3570:3578]=b'FDN\x01\x01\x00\x01\x00'
        card[3578:3610]=RECORD;card[3600]=0xf6
        off=disabled_card(card)
        self.assertEqual([(i,a,b) for i,(a,b) in enumerate(zip(card,off)) if a!=b],[(3575,0,1)])
        with self.assertRaises(ValueError):disabled_card(off)
        with self.assertRaises(ValueError):disabled_card(card[:-1])
        card[3600]=0xf5
        with self.assertRaises(ValueError):disabled_card(card)


if __name__=='__main__':unittest.main()
