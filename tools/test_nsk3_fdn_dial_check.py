import unittest
from unittest.mock import patch
from tools.nsk3_fdn_dial_check import check_dial


def trace(blocked=False):
    result = ''.join(f'nsk3_pin_contact: correct=1 key={key:02x} down={down} \n'
                     for key in (1, 2, 3, 4, 0x19) for down in (1, 0))
    result += 'SIM status ins=20 sw=9000\n'
    result += ''.join(f'nsk3_security_contact: key={key:02x} down=1 \n'
                      for key in (1, 2, 3, 4, 5, 0x19))
    result += ('nsk3_security_submit: pointer=00100000 text=3132333435000000 caller=0028a1d7\n'
               'nsk3_security_gate: match=00000001 retry_pointer=0010c520 retry=00\n'
               'nsk3_security_accepted: result=00000001\n')
    keys = (5, 4, 3, 2, 1, 0x0e) if blocked else (1, 2, 3, 4, 6, 0x0e)
    return result + ''.join(f'nsk3_security_contact: key={key:02x} down=1 \n' for key in keys)


class Nsk3FdnDialTests(unittest.TestCase):
    def test_correct_physical_cases_select_card_contract(self):
        for blocked, enabled in ((False, True), (True, True), (True, False)):
            with self.subTest(blocked=blocked, enabled=enabled), patch('tools.nsk3_fdn_dial_check.check_phase') as protocol, patch('tools.nsk3_fdn_dial_check.check_upload_handoffs'):
                value = trace(blocked)
                check_dial(value, blocked, enabled)
                protocol.assert_called_once_with(value, 1 if enabled else 3, 'nsk3_sim_tx')

    def test_wrong_number_or_send_rejected(self):
        for value in (trace(True), trace().replace('key=0e down=1', 'key=0f down=1')):
            with self.assertRaises(ValueError):
                check_dial(value, False, True)

    def test_input_before_phone_acceptance_rejected(self):
        value = trace().replace('nsk3_security_accepted: result=00000001\n', '')
        value += 'nsk3_security_accepted: result=00000001\n'
        with self.assertRaisesRegex(ValueError, 'preceded'):
            check_dial(value, False, True)


if __name__ == '__main__':
    unittest.main()
