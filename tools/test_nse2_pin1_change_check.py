import hashlib
import unittest
from unittest.mock import patch

from tools.make_sim_card_profile import make_profile
from tools.nse2_pin1_change_check import check_change


class Pin1ChangeTest(unittest.TestCase):
    def test_enabled_change_and_disabled_refusal(self):
        for enabled in (False,True):
            before=make_profile(enabled).ljust(3570,b'\xff')
            after=bytearray(before)
            rows=[]
            if enabled:
                after[3484:3492]=b'2468'+b'\xff'*4
                rows.extend(('sim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08 selected=6fad t=7.66',
                             'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
                             'sim_device: header cla=a0 ins=24 p1=00 p2=01 p3=10 selected=6f3a t=28.72',
                             'SIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=1'))
                for i in range(10):
                    when=(6+i//2*.35 if i<8 else 7.6)+i%2*.15
                    rows.append(f'nse2_pin_contact: event={i+1} down={1-i%2} t={when:.3f} physical_only=1')
            for i in range(60 if enabled else 30):
                when=18.5+i//2*.35+i%2*.15
                rows.append(f'nse2_pin1_change_contact: event={i+1} down={1-i%2} t={when:.3f} physical_only=1')
            text='\n'.join(rows)
            pixels=b'result'
            with patch('tools.nse2_pin1_change_check.FRAMES',{enabled:hashlib.sha256(pixels).hexdigest()}):
                self.assertEqual(check_change(text,before,after,pixels,enabled=enabled)['pin1_changed'],enabled)
                corrupt=bytearray(after)
                corrupt[3500]^=1
                for trace,card,image in ((text,corrupt,pixels),(text.replace('event=1 ','event=2 ',1),after,pixels),
                                          (text,after,b'wrong')):
                    with self.assertRaises(ValueError):
                        check_change(trace,before,card,image,enabled=enabled)
                if enabled:
                    for bad in (text.replace('p2=01','p2=02'),text.replace('sw=9000','sw=9804',1),
                                text.replace('t=28.72','t=7.7')):
                        with self.assertRaises(ValueError):
                            check_change(bad,before,after,pixels,enabled=enabled)


if __name__=='__main__':
    unittest.main()
