import unittest

from tools.nsb3_toolkit_check import check_trace, DISPLAY, RESPONSE, INKEY, INPUT, INKEY_RESPONSE, INPUT_RESPONSE
from tools.test_nsb3_pin_request_check import trace


def wire_bytes(direction, data):
    return '\n' + '\n'.join(
        f'nsb3_sim_wire: direction={direction} data={value:02x} t=62.0 observer_writes=0'
        for value in data)


def toolkit_trace():
    text = trace(3) + wire_bytes('TX', bytes.fromhex('a0100000050b41977700'))
    text += '\nSIM status ins=10 sw=9000 chv=3/3 puk=10/10 enabled=1'
    text += '\nSIM completion ins=f2 sw=9116'
    text += wire_bytes('TX', bytes.fromhex('a012000016'))
    text += wire_bytes('RX', DISPLAY + b'\x90\x00')
    text += '\nnsb3_toolkit_physical: action=dismiss down=1 t=68.000000000'
    text += wire_bytes('TX', bytes.fromhex('a01400000c') + RESPONSE)
    text += '\nSIM status ins=14 sw=9000 chv=3/3 puk=10/10 enabled=1'
    text += '\nnsb3_toolkit_physical: action=dismiss down=0 t=68.150000000'
    return text


class Nsb3ToolkitTest(unittest.TestCase):
    def test_own_profile_fetch_and_physical_response(self):
        check_trace(toolkit_trace())

    def test_wrong_profile_payload_result_input_and_mutation_fail(self):
        good = toolkit_trace()
        for bad in (good.replace('data=97 ', 'data=96 ', 1),
                    good.replace('direction=RX data=44 ', 'direction=RX data=45 ', 1),
                    good.replace('ins=14 sw=9000', 'ins=14 sw=6a80'),
                    good.replace('down=1 t=68.000000000', 'down=1 t=61.000000000'),
                    good.replace('sw=9116', 'sw=9000'),
                    good + wire_bytes('TX', bytes.fromhex('a0dc010420') + b'\xff' * 32),
                    good + '\nnsb3_board_reset_request: data=05'):
            with self.assertRaises(ValueError):
                check_trace(bad)

    def test_automatic_response_is_not_physical_completion(self):
        text = toolkit_trace().replace('\nnsb3_toolkit_physical: action=dismiss down=1 t=68.000000000', '')
        text += '\nnsb3_toolkit_physical: action=dismiss down=1 t=68.000000000'
        with self.assertRaises(ValueError):
            check_trace(text)

    def test_interactive_commands_require_exact_values_and_physical_order(self):
        text = toolkit_trace().replace('SIM status ins=14 sw=9000', 'SIM status ins=14 sw=9115')
        text += wire_bytes('TX', bytes.fromhex('a012000015')) + wire_bytes('RX', INKEY + b'\x90\x00')
        def action(name):
            return '\n' + '\n'.join(f'nsb3_toolkit_input: action={name} down={state}' for state in (1, 0))
        text += action('inkey_5') + action('inkey_confirm')
        text += wire_bytes('TX', bytes.fromhex('a014000010') + INKEY_RESPONSE)
        text += '\nSIM status ins=14 sw=911a chv=3/3 puk=10/10 enabled=1'
        text += wire_bytes('TX', bytes.fromhex('a01200001a')) + wire_bytes('RX', INPUT + b'\x90\x00')
        text += action('input_4') + action('input_2') + action('input_confirm')
        text += wire_bytes('TX', bytes.fromhex('a014000011') + INPUT_RESPONSE)
        text += '\nSIM status ins=14 sw=9000 chv=3/3 puk=10/10 enabled=1'
        check_trace(text, interactive=True)
        for bad in (text.replace('data=35 ', 'data=36 ', 1),
                    text.replace('action=input_2', 'action=input_3'),
                    text.replace('sw=911a', 'sw=9000'),
                    text.replace('action=input_confirm down=1', 'action=missing down=1')):
            with self.assertRaises(ValueError):
                check_trace(bad, interactive=True)
        with self.assertRaises(ValueError):
            check_trace(text)


if __name__ == '__main__':
    unittest.main()
