import unittest
from unittest.mock import patch
from tools.nsm1_sms_delete_check import check_case
from tools.nsk3_sms_delete_check import deleted_card
from tools.nsk3_sms_check import DRAFT,SMS_OFFSET,SMS_RECORD_SIZE,READ_KEYS
from tools.make_sim_card_profile import make_profile
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH


def tx(data,status=''):
    return '\n'.join(f'nsm1_sim_tx: index={i} value={value:02x}' for i,value in enumerate(data,1))+status


class SmsEraseTest(unittest.TestCase):
    def test_exact_free_status_not_tpdu_wipe(self):
        saved=bytearray(make_profile(True).ljust(CURRENT_NVRAM_LENGTH,b'\xff'))
        saved[SMS_OFFSET:SMS_OFFSET+SMS_RECORD_SIZE]=DRAFT
        after=deleted_card(saved)
        self.assertEqual(after[SMS_OFFSET],0)
        self.assertEqual(after[:SMS_OFFSET]+after[SMS_OFFSET+1:],saved[:SMS_OFFSET]+saved[SMS_OFFSET+1:])

    def test_phase_specific_update_and_input_contract(self):
        command=bytes.fromhex('a0dc0104b0')+bytes(1)+DRAFT[1:]
        erase=tx(command,'\nSIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=1')
        read=tx(bytes.fromhex('a0b20104b0'))
        with patch('tools.nsm1_sms_delete_check.check_authenticated') as authenticate:
            check_case(erase,'erase')
            authenticate.assert_called_once_with(erase,READ_KEYS+(0x19,)*3)
            check_case(read,'cancel');check_case(read,'cold')
            for bad,mode in ((erase,'cancel'),(erase,'cold'),(erase.replace('sw=9000','sw=9402'),'erase'),
                             (tx(bytes.fromhex('a0dc0104b0')+DRAFT,'\nSIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=1'),'erase')):
                with self.assertRaises(ValueError):check_case(bad,mode)
        with self.assertRaises(ValueError):check_case(read,'unknown')


if __name__=='__main__':unittest.main()
