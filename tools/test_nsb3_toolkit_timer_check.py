import unittest

from tools.nsb3_toolkit_timer_check import check_trace
from tools.nsb3_toolkit_check import TIMER, TIMER_REJECTION, command_contract
from tools.test_nsb3_toolkit_check import menu_trace, wire_bytes


def timer_trace():
    text = menu_trace().replace('ins=c2 sw=9000', 'ins=c2 sw=9113')
    text += wire_bytes('TX', bytes.fromhex('a012000013'))
    text += wire_bytes('RX', TIMER + b'\x90\x00')
    text += wire_bytes('TX', bytes.fromhex('a01400000c') + TIMER_REJECTION)
    text += '\nSIM status ins=14 sw=9000 chv=3/3 puk=10/10 enabled=1'
    return text


class Nsb3ToolkitTimerTest(unittest.TestCase):
    def test_timer_is_rejected_not_started(self):
        check_trace(timer_trace())

    def test_success_result_bad_payload_expiry_and_missing_input_fail(self):
        good = timer_trace()
        rejected = wire_bytes('TX', bytes.fromhex('a01400000c') + TIMER_REJECTION)
        accepted = wire_bytes('TX', bytes.fromhex('a01400000c') + TIMER_REJECTION[:-1] + b'\x00')
        for bad in (good.replace(rejected, accepted),
                    good.replace(wire_bytes('RX', TIMER + b'\x90\x00'),
                                 wire_bytes('RX', TIMER[:-1] + b'\x04\x90\x00')),
                    good + wire_bytes('TX', bytes.fromhex('a0c200000ed70c820282812401012503000005')),
                    good.replace('nsb3_toolkit_menu: action=select', 'nsb3_toolkit_menu: action=missing'),
                    good.replace('ins=c2 sw=9113', 'ins=c2 sw=9000')):
            with self.assertRaises(ValueError):
                check_trace(bad)

    def test_only_observed_command_contracts_are_available(self):
        with self.assertRaises(ValueError):
            command_contract(99)
        with self.assertRaises(ValueError):
            command_contract(14, menu_item=2)


if __name__ == '__main__':
    unittest.main()
