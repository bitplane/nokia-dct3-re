import unittest

from tools.nsk1_network_search import KEYS, check_trace, check_cancel_trace
from tools.test_nsk1_radio_trace_check import fixture as radio_fixture


def fixture():
    lines = [radio_fixture()]
    codes = [1,2,3,4,5,0x19,0x19] + [0x19 if key == 'menu' else 0x18 for key in KEYS]
    lines.extend(f'nsk1_code_key: value={code:02x}' for code in codes)
    for index, key in enumerate(KEYS, 1):
        for down in (1, 0):
            lines.append(f'nsk1_network_contact: index={index} key={key} down={down} t=23.0 physical_only=1')
    for context in ('00', '03'):
        lines.append(f'nsk1_radio_dynamic_request: input=05df object=00000000 field2={context}')
        lines.append(f'nsk1_radio_completion_server_receive: object=001014a4 header=05df{context}7f')
        lines.append(f'nsk1_radio_task16_receive: object=001014a4 header=05e8{context}a1')
    return '\n'.join(lines)


class NetworkSearchTest(unittest.TestCase):
    def cancel_fixture(self):
        lines = [fixture(), 'nsk1_network_late: t=80.0', 'nsk1_network_late: t=89.0',
                 'nsk1_network_quit: down=1 t=90.0 physical_only=1',
                 'nsk1_code_key: value=19 t=90.05',
                 'nsk1_network_quit: down=0 t=90.2 physical_only=1',
                 'nsk1_network_late: t=92.0', 'nsk1_network_late: t=99.0']
        return '\n'.join(lines)

    def test_initiation_is_not_operator_discovery(self):
        result = check_trace(fixture())
        self.assertTrue(result['physical_manual_search_initiated'])
        self.assertFalse(result['operator_discovery_proved'])
        self.assertFalse(result['registration_proved'])

    def test_missing_contact_release_fails(self):
        with self.assertRaises(ValueError):
            check_trace(fixture().replace('index=14 key=menu down=0', 'index=14 key=menu down=1'))

    def test_changed_decoded_key_fails(self):
        with self.assertRaisesRegex(ValueError, 'decoded key'):
            check_trace(fixture().replace('nsk1_code_key: value=18', 'nsk1_code_key: value=17', 1))

    def test_missing_context_reply_fails(self):
        with self.assertRaisesRegex(ValueError, 'replies'):
            check_trace(fixture().replace('header=05e803a1', 'header=05e703a1'))

    def test_different_request_does_not_count_as_search(self):
        with self.assertRaisesRegex(ValueError, 'requests'):
            check_trace(fixture().replace('input=05df', 'input=05dd'))

    def test_cancel_is_not_stable_idle(self):
        result = check_cancel_trace(self.cancel_fixture())
        self.assertTrue(result['physical_search_ui_cancelled'])
        self.assertFalse(result['stable_idle_return_proved'])

    def test_cancel_requires_physical_release_and_decode(self):
        for old, new in [('down=0 t=90.2', 'down=1 t=90.2'),
                         ('value=19 t=90.05', 'value=18 t=90.05')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_cancel_trace(self.cancel_fixture().replace(old, new))

    def test_cancel_requires_late_observation(self):
        with self.assertRaisesRegex(ValueError, 'late-search'):
            check_cancel_trace(self.cancel_fixture().replace('nsk1_network_late: t=99.0', ''))

    def test_late_reboot_is_not_a_successful_menu_return(self):
        reset = '\nmad2_clock: event=W off=01 data=05 old=01 counter=0402 pc=0029516e t=93.974106385'
        with self.assertRaisesRegex(ValueError, 'reboot'):
            check_cancel_trace(self.cancel_fixture() + reset)


if __name__ == '__main__':
    unittest.main()
