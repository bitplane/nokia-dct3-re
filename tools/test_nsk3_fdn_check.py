import unittest
from unittest.mock import patch

from tools.nsk3_fdn_check import check_trace
from tools.sim_fdn_protocol_check import check_phase


def trace():
    contacts = ''.join(f'nsk3_pin_contact: correct=1 key={key:02x} down={down} \n'
                       for key in (1, 2, 3, 4, 0x19) for down in (1, 0))
    phone = ''.join(f'nsk3_security_contact: key={key:02x} down=1 \n'
                    for key in (1, 2, 3, 4, 5, 0x19))
    return (contacts + 'SIM status ins=20 sw=9000\n' + phone
            + 'nsk3_security_submit: pointer=00100000 text=3132333435000000 caller=0028a1d7\n'
            + 'nsk3_security_gate: match=00000001 retry_pointer=0010c520 retry=00\n'
            + 'nsk3_security_accepted: result=00000001\n'
            + ''.join(f'nsk3_security_contact: key={key:02x} down=1 \n' for key in (0x19, 4, 5, 3)))


class Nsk3FdnTests(unittest.TestCase):
    def test_protocol_rejects_saturated_capture(self):
        def status(_, instruction):
            return [(0x9000, 3, 10)] if instruction == 0x20 else []
        with patch('tools.sim_fdn_protocol_check.statuses', side_effect=status), patch('tools.sim_fdn_protocol_check.transmitted', return_value=bytes(4096)):
            with self.assertRaisesRegex(ValueError, 'cap reached'):
                check_phase('', 3, 'nsk3_sim_tx')

    def test_own_input_and_shared_protocol_boundary(self):
        with patch('tools.nsk3_fdn_check.check_phase') as protocol, patch('tools.nsk3_fdn_check.check_upload_handoffs') as upload:
            check_trace(trace(), 1)
            protocol.assert_called_once_with(trace(), 1, 'nsk3_sim_tx')
            upload.assert_called_once_with(trace(), 1)

    def test_wrong_input_and_authentication_order_rejected(self):
        original = trace()
        variants = [original.replace('key=03 down=1', 'key=09 down=1', 1),
                    original.replace('caller=0028a1d7', 'caller=0028a1d9'),
                    original.replace('nsk3_security_accepted: result=00000001\n', '')
                    + 'nsk3_security_accepted: result=00000001\n']
        for value in variants:
            with self.subTest(value=value), self.assertRaises(ValueError):
                check_trace(value, 1)


if __name__ == '__main__':
    unittest.main()
