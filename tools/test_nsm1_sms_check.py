import unittest
from tools.nsm1_sms_check import check_transition
from tools.nsk3_sms_check import check_streams,SMS_OFFSET,SMS_RECORD_SIZE,DRAFT
from tools.make_sim_card_profile import make_profile
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH


class SmsTest(unittest.TestCase):
    def test_only_fresh_sms_slot_changes(self):
        before=make_profile(True).ljust(CURRENT_NVRAM_LENGTH,b'\xff')
        after=bytearray(before);after[SMS_OFFSET:SMS_OFFSET+SMS_RECORD_SIZE]=DRAFT
        check_transition(before,after)
        for offset in (0,len(after)-1,SMS_OFFSET+SMS_RECORD_SIZE):
            bad=bytearray(after);bad[offset]^=1
            with self.assertRaises(ValueError):check_transition(before,bad)
        with self.assertRaises(ValueError):check_transition(after,after)

    def test_exact_sms_selection_update_and_read_only_cold(self):
        stream=bytes.fromhex('a0a40000026f3c')+bytes.fromhex('a0dc0104b0')+DRAFT
        status='SIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=1'
        read=bytes.fromhex('a0b20104b0')
        check_streams(status,'',stream,read)
        for bad in (stream.replace(bytes.fromhex('6f3c'),bytes.fromhex('6f3d')),stream[:-1],stream+bytes.fromhex('a0dc0104b0')+DRAFT):
            with self.assertRaises(ValueError):check_streams(status,'',bad,read)
        with self.assertRaises(ValueError):check_streams(status,status,stream,stream)


if __name__=='__main__':unittest.main()
