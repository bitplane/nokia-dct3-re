import unittest
from tools.run_c54x_backend_bsp import MARKER, OBSERVATION, RF_OBSERVATION, RECEIVE_PINS, COMPLETION, check_result


class BackendBspResultTest(unittest.TestCase):
    def output(self):
        return '\n'.join((MARKER, OBSERVATION, RF_OBSERVATION, RECEIVE_PINS, COMPLETION)) + '\n'

    def test_success(self):
        check_result(self.output(), 3)

    def test_observation_is_mandatory_and_exact(self):
        text = self.output()
        for changed in (text.replace(OBSERVATION, ''),
                        text.replace(OBSERVATION, OBSERVATION + '\n' + OBSERVATION),
                        text.replace('debugger_reads=8', 'debugger_reads=0'),
                        text.replace('overrun_restore=2', 'overrun_restore=0'),
                        text.replace('normal_acks=4', 'normal_acks=0')):
            with self.subTest(text=changed), self.assertRaises(ValueError):
                check_result(changed, 3)

    def test_rf_observation_is_mandatory_and_exact(self):
        text = self.output()
        alternatives = ['', RF_OBSERVATION + '\n' + RF_OBSERVATION]
        for before, after in (('debugger_reads=8', 'debugger_reads=0'),
                              ('ordinary_reads=3', 'ordinary_reads=0'),
                              ('source_restore=1', 'source_restore=0'),
                              ('reset=1', 'reset=0'),
                              ('physical_rf_claim=0', 'physical_rf_claim=1')):
            alternatives.append(RF_OBSERVATION.replace(before, after))
        for alternative in alternatives:
            with self.subTest(marker=alternative), self.assertRaisesRegex(ValueError, 'RF input'):
                check_result(text.replace(RF_OBSERVATION, alternative), 3)

    def test_receive_pins_are_mandatory_and_exact(self):
        text = self.output()
        alternatives = ['', RECEIVE_PINS + '\n' + RECEIVE_PINS]
        for before, after in (('words=3', 'words=0'), ('partial_restore=1', 'partial_restore=0'),
                              ('reset_abort=1', 'reset_abort=0'), ('vectors=3', 'vectors=0'),
                              ('handset_audio_claim=0', 'handset_audio_claim=1')):
            alternatives.append(RECEIVE_PINS.replace(before, after))
        for alternative in alternatives:
            with self.subTest(marker=alternative), self.assertRaisesRegex(ValueError, 'receive-pin'):
                check_result(text.replace(RECEIVE_PINS, alternative), 3)

    def test_incomplete_or_duplicate(self):
        for text in (COMPLETION, MARKER, MARKER + '\n' + self.output(),
                     'Fatal error: fixture failed\n' + self.output()):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check_result(text, 3)

    def test_normal_exit_does_not_prove_completion(self):
        with self.assertRaises(ValueError):
            check_result(self.output(), 0)


if __name__ == '__main__':
    unittest.main()
