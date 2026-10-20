import unittest
from tools.nsk3_sms_check import check_card,DRAFT,SMS_OFFSET,SMS_RECORD_SIZE
from tools.make_sim_card_profile import make_profile
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH
from tools.nsk3_phonebook_check import transmitted


class OfflineSmsTest(unittest.TestCase):
    def test_exact_sms_not_phonebook_save(self):
        card=bytearray(make_profile(False).ljust(CURRENT_NVRAM_LENGTH,b'\xff'))
        card[SMS_OFFSET:SMS_OFFSET+SMS_RECORD_SIZE]=DRAFT
        check_card(card)
        self.assertEqual(DRAFT[:12],bytes.fromhex('070281f001ff008100000141'))
        for offset in (0,SMS_OFFSET,SMS_OFFSET+11,SMS_OFFSET+SMS_RECORD_SIZE):
            bad=bytearray(card)
            bad[offset]^=1
            with self.assertRaises(ValueError):check_card(bad)
        with self.assertRaises(ValueError):check_card(card[:-1])

    def test_longer_tx_capture_is_explicit(self):
        trace='\n'.join(f'nsk3_sim_tx: index={i} value=00' for i in range(1,1025))
        with self.assertRaises(ValueError):transmitted(trace)
        self.assertEqual(len(transmitted(trace,4096)),1024)


if __name__=='__main__':unittest.main()
