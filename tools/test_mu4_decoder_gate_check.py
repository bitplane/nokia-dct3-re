import unittest

from tools.mu4_decoder_gate_check import check


def fixture():
    return '\n'.join([
        'mu4_playback_execute_entry: address=026d62 opcode=fd00 t=0.2',
        'mu4_playback_execute_entry: address=026d62 opcode=fd00 t=9.9',
        'mu4_decoder_gate: ordinal=1 address=0293ae opcode=f830 call=375 t=9.65 '
        'identity=Execute_entry operands=context_only observer_writes=0',
        'mu4_decoder_gate: ordinal=2 address=0293b0 opcode=7211 call=375 t=9.66 '
        'identity=Execute_entry operands=context_only observer_writes=0',
        'mu4_decoder_gate_terminal: events=2 bad=0 capped=0 first=9.64 last=9.86 t=10 '
        'identity=Execute_entry operands=context_only observer_writes=0',
        'mu4_playback_parent_terminal: first_session_complete=1 overlay_verified=1 illegal=0 '
        'opcode_mismatch=0 selection=first_verified_aa99_session identity=Execute_entry '
        'audio_acceptance=0 t=10 observer_writes=0'])


class DecoderGateTests(unittest.TestCase):
    def test_successor_not_live_operands_defines_outcome(self):
        self.assertEqual(check(fixture())['branches'][0]['outcome'], 'pass')
        self.assertEqual(check(fixture().replace('address=0293b0 opcode=7211',
                                                'address=02940b opcode=10f8'))['branches'][0]['outcome'], 'skip')

    def test_rejects_mutated_or_incomplete_evidence(self):
        for before, after in [('bad=0', 'bad=1'), ('capped=0', 'capped=1'),
                              ('events=2', 'events=3'), ('opcode=f830', 'opcode=0000'),
                              ('ordinal=2', 'ordinal=3'), ('t=9.66', 't=9.63'),
                              ('t=9.9', 't=9.8'), ('illegal=0', 'illegal=1'),
                              ('operands=context_only', 'operands=retired'),
                              ('observer_writes=0', 'observer_writes=1'),
                              ('address=0293b0 opcode=7211', 'address=0293cd opcode=f982')]:
            with self.subTest(before=before, after=after), self.assertRaises(ValueError):
                check(fixture().replace(before, after))

    def test_rejects_censored_branch(self):
        text = fixture().replace('events=2', 'events=1')
        text = '\n'.join(line for line in text.splitlines() if 'ordinal=2' not in line)
        with self.assertRaisesRegex(ValueError, 'censored'):
            check(text)


if __name__ == '__main__':
    unittest.main()
