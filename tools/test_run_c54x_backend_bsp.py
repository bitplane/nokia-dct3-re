import unittest
from tools.run_c54x_backend_bsp import MARKER, OBSERVATION, RF_OBSERVATION, RECEIVE_PINS, TRANSMIT_PINS, CLOCK_INPUTS, CONTINUOUS_RX, CONTINUOUS_TX, COMPLETION, check_result


class BackendBspResultTest(unittest.TestCase):
    def output(self):
        return '\n'.join((MARKER, OBSERVATION, RF_OBSERVATION, RECEIVE_PINS, TRANSMIT_PINS, CLOCK_INPUTS, CONTINUOUS_RX, CONTINUOUS_TX, COMPLETION)) + '\n'

    def test_continuous_transmit_is_mandatory_and_exact(self):
        alternatives = ['', CONTINUOUS_TX + '\n' + CONTINUOUS_TX]
        for field in ('completed_words=6', 'load_irqs=5', 'partial_restore=1',
                      'queued_boundary=1', 'underrun_halt=1', 'external_restart=1',
                      'reset_abort=1', 'source=5'):
            alternatives.append(CONTINUOUS_TX.replace(field, field.split('=')[0] + '=0'))
        alternatives.append(CONTINUOUS_TX.replace('handset_clock_claim=0', 'handset_clock_claim=1'))
        for alternative in alternatives:
            with self.subTest(marker=alternative), self.assertRaisesRegex(ValueError, 'continuous-TX'):
                check_result(self.output().replace(CONTINUOUS_TX, alternative), 3)

    def test_continuous_receive_is_mandatory_and_exact(self):
        alternatives = ['', CONTINUOUS_RX + '\n' + CONTINUOUS_RX]
        for field in ('words=5', 'partial_restore=1', 'single_sync_continuation=1',
                      'overrun_restart=1', 'reset_abort=1', 'source=4'):
            alternatives.append(CONTINUOUS_RX.replace(field, field.split('=')[0] + '=0'))
        alternatives.append(CONTINUOUS_RX.replace('handset_clock_claim=0', 'handset_clock_claim=1'))
        for alternative in alternatives:
            with self.subTest(marker=alternative), self.assertRaisesRegex(ValueError, 'continuous-RX'):
                check_result(self.output().replace(CONTINUOUS_RX, alternative), 3)

    def test_clock_inputs_are_mandatory_and_exact(self):
        alternatives = ['', CLOCK_INPUTS + '\n' + CLOCK_INPUTS]
        for field in ('levels=4', 'save_replay=4', 'reset_inputs=1'):
            alternatives.append(CLOCK_INPUTS.replace(field, field.split('=')[0] + '=0'))
        alternatives.append(CLOCK_INPUTS.replace('handset_clock_claim=0', 'handset_clock_claim=1'))
        for alternative in alternatives:
            with self.subTest(marker=alternative), self.assertRaisesRegex(ValueError, 'clock-input'):
                check_result(self.output().replace(CLOCK_INPUTS, alternative), 3)

    def test_transmit_pins_are_mandatory_and_exact(self):
        alternatives = ['', TRANSMIT_PINS + '\n' + TRANSMIT_PINS]
        for field in ('words=5', 'partial_restore=1', 'queued_word=1',
                      'underrun_no_irq=1', 'shift_status=1', 'reset_abort=1', 'reset_irq=1', 'vectors=4'):
            alternatives.append(TRANSMIT_PINS.replace(field, field.split('=')[0] + '=0'))
        alternatives.append(TRANSMIT_PINS.replace('handset_audio_claim=0', 'handset_audio_claim=1'))
        for alternative in alternatives:
            with self.subTest(marker=alternative), self.assertRaisesRegex(ValueError, 'transmit-pin'):
                check_result(self.output().replace(TRANSMIT_PINS, alternative), 3)

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
