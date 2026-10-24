import unittest

from tools.nsb3_toolkit_state_check import check_replay_protocol
from tools.nsb3_toolkit_check import RESPONSE
from tools.test_nsb3_toolkit_check import toolkit_trace, wire_bytes


def replay_trace():
    text = toolkit_trace().replace(
        'nsb3_toolkit_physical: action=dismiss down=1 t=68.000000000',
        'nsb3_state_contact: phase=reference down=1').replace(
        'nsb3_toolkit_physical: action=dismiss down=0 t=68.150000000',
        'nsb3_state_contact: phase=reference down=0')
    text += '\nnsb3_state_contact: phase=restored down=1'
    text += wire_bytes('TX', bytes.fromhex('a01400000c') + RESPONSE)
    text += '\nSIM status ins=14 sw=9000 chv=3/3 puk=10/10 enabled=1'
    text += '\nnsb3_state_contact: phase=restored down=0'
    return text


class Nsb3ToolkitStateTest(unittest.TestCase):
    def test_one_fetched_command_completes_after_each_physical_action(self):
        check_replay_protocol(replay_trace())

    def test_missing_restore_pending_card_failure_and_refetch_fail(self):
        good = replay_trace()
        for bad in (good.replace('phase=restored down=1', 'phase=missing down=1'),
                    good.replace('ins=14 sw=9000', 'ins=14 sw=6a80', 1),
                    good + wire_bytes('TX', bytes.fromhex('a012000016')),
                    good.replace('sw=9116', 'sw=9000'),
                    good + wire_bytes('TX', bytes.fromhex('a0dc010420') + b'\xff' * 32)):
            with self.assertRaises(ValueError):
                check_replay_protocol(bad)

    def test_response_before_restored_key_is_not_replay_acceptance(self):
        text = replay_trace().replace('\nnsb3_state_contact: phase=restored down=1', '')
        text += '\nnsb3_state_contact: phase=restored down=1'
        with self.assertRaises(ValueError):
            check_replay_protocol(text)


if __name__ == '__main__':
    unittest.main()
