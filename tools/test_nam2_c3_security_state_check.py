import unittest

from tools.nam2_c3_security_state_check import KEYS, check_log


def observation(event, time):
    cpu = ','.join(['00000000'] * 37)
    return f'nam2_security_state: event={event} pc=00200000 sp=00100000 ram=00000001 cpu={cpu} t={time:.9f}\n'


def valid_log():
    lines = []
    for phase, start, end in (('reference', 'saved', 'reference_end'), ('replay', 'restored', 'replay_end')):
        lines.append(observation(start, 7.75))
        for index, key in enumerate(KEYS):
            lines.append(f'nam2_security_state_key: phase={phase} index={index + 1} key={key} t={8 + index * 0.5:.9f}\n')
        lines.append(observation(end, 11))
    lines.append('nam2_security_state: result=pass physical_replay=1 idle_claim=0\n')
    return ''.join(lines)


class Nam2SecurityStateTest(unittest.TestCase):
    def test_complete_replay(self):
        check_log(valid_log())

    def test_restoration_mismatch(self):
        text = valid_log().replace('event=restored pc=00200000', 'event=restored pc=00200002')
        with self.assertRaisesRegex(ValueError, 'restoration'):
            check_log(text)

    def test_continuation_mismatch(self):
        text = valid_log().replace('event=replay_end pc=00200000', 'event=replay_end pc=00200002')
        with self.assertRaisesRegex(ValueError, 'continuation'):
            check_log(text)

    def test_wrong_key(self):
        with self.assertRaisesRegex(ValueError, 'contacts'):
            check_log(valid_log().replace('key=Keypad 5', 'key=Keypad 6'))

    def test_missing_result(self):
        with self.assertRaisesRegex(ValueError, 'terminal'):
            check_log(valid_log().split('nam2_security_state: result=')[0])

    def test_incomplete_cpu(self):
        with self.assertRaisesRegex(ValueError, 'incomplete CPU'):
            check_log(valid_log().replace('cpu=00000000,', 'cpu='))

    def test_lua_error_after_success(self):
        with self.assertRaisesRegex(ValueError, 'fixture error'):
            check_log(valid_log() + '[LUA ERROR] failed\n')

    def test_truncated_register(self):
        with self.assertRaisesRegex(ValueError, 'incomplete CPU'):
            check_log(valid_log().replace('cpu=00000000,', 'cpu=0,'))


if __name__ == '__main__':
    unittest.main()
