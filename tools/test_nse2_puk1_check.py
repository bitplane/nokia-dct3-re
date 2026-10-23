import hashlib
import unittest
from unittest.mock import patch

from tools.make_sim_card_profile import make_profile
from tools.nse2_puk1_check import check_wrong, check_cold, check_permanent


class Puk1CheckTest(unittest.TestCase):
    def test_reduced_retry_recovery_is_explicit_and_resets_puk_counter(self):
        from tools.nse2_sim_check import verify_puk_recovery
        before=bytearray(make_profile(True).ljust(3570,b'\xff'))
        before[3516]=0
        before[3518]=9
        after=bytearray(before)
        after[3484:3492]=b'2468'+b'\xff'*4
        after[3516]=3
        after[3518]=10
        rows=['sim_device: header cla=a0 ins=2c p1=00 p2=00 p3=10 selected=6fad t=18.26',
              'SIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1']
        for i in range(38):
            rows.append(f'nse2_puk_contact: event={i+1} down={1-i%2} t={11.2+i*.18:.3f} physical_only=1')
        text='\n'.join(rows)
        with patch.multiple('tools.nse2_sim_check',PUK_PROMPT_FRAME=hashlib.sha256(b'puk').hexdigest(),
                            SECURITY_FRAME=hashlib.sha256(b'phone').hexdigest()):
            self.assertTrue(verify_puk_recovery(text,before,after,b'puk',b'phone',puk_attempts=9)['puk_verified'])
            with self.assertRaises(ValueError):
                verify_puk_recovery(text,before,after,b'puk',b'phone')
            after[3518]=9
            with self.assertRaises(ValueError):
                verify_puk_recovery(text,before,after,b'puk',b'phone',puk_attempts=9)

    def test_wrong_puk_and_cold_retry_preservation(self):
        before=bytearray(make_profile(True).ljust(3570,b'\xff'))
        before[3516]=0
        after=bytearray(before)
        after[3518]=9
        rows=['sim_device: header cla=a0 ins=2c p1=00 p2=00 p3=10 selected=6fad t=18.26',
              'SIM status ins=2c sw=9804 chv=0/3 puk=9/10 enabled=1']
        starts=[11.2+i*.3 for i in range(8)]+[13.8]+[14.4+i*.35 for i in range(4)]+[15.9]+[16.6+i*.35 for i in range(4)]+[18.2]
        for i in range(38):
            when=starts[i//2]+i%2*.15
            rows.append(f'nse2_puk_contact: event={i+1} down={1-i%2} t={when:.3f} physical_only=1')
        text='\n'.join(rows)
        digest=lambda value:hashlib.sha256(value).hexdigest()
        with patch.multiple('tools.nse2_puk1_check',PUK_PROMPT_FRAME=digest(b'puk'),PIN_BLOCKED_FRAME=digest(b'blocked'),REJECTED_FRAME=digest(b'rejected')):
            self.assertEqual(check_wrong(text,before,after,b'puk',b'puk')['puk1_attempts_remaining'],9)
            self.assertEqual(check_cold('',after,after,b'blocked',b'puk')['puk1_attempts_remaining'],9)
            bad_card=bytearray(after)
            bad_card[3484]^=1
            for trace,card in ((text,bad_card),(text.replace('sw=9804','sw=9000'),after),
                               (text.replace('p2=00','p2=02'),after),(text.replace('event=1 ','event=2 ',1),after)):
                with self.assertRaises(ValueError):
                    check_wrong(trace,before,card,b'puk',b'puk')
            for trace,card in ((text,after),('',bad_card)):
                with self.assertRaises(ValueError):
                    check_cold(trace,after,card,b'blocked',b'puk')
            terminal=bytearray(before)
            terminal[3518]=1
            exhausted=bytearray(terminal)
            exhausted[3518]=0
            terminal_text=text.replace('sw=9804','sw=9840').replace('puk=9/10','puk=0/10')
            self.assertEqual(check_wrong(terminal_text,terminal,exhausted,b'puk',b'rejected')['puk1_attempts_remaining'],0)
            cold_text='\n'.join(rows[2:])
            self.assertTrue(check_permanent(cold_text,exhausted,exhausted,b'rejected',b'rejected')['credential_input_refused'])
            for bad_trace,card,pixels in ((terminal_text,exhausted,b'rejected'),(cold_text,bad_card,b'rejected'),
                                           (cold_text,exhausted,b'puk')):
                with self.assertRaises(ValueError):
                    check_permanent(bad_trace,exhausted,card,b'rejected',pixels)
            with self.assertRaises(ValueError):
                check_wrong(text,terminal,after,b'puk',b'puk')


if __name__=='__main__':
    unittest.main()
