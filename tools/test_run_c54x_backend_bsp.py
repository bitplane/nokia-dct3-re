import unittest
from tools.run_c54x_backend_bsp import MARKER, OBSERVATION, RF_OBSERVATION, RECEIVE_PINS, TRANSMIT_PINS, CLOCK_INPUTS, CONTINUOUS_RX, CONTINUOUS_TX, FRAME_IGNORE, JOINED_PINS, CLOCKED_STORE, COMPLETION, check_result
from tools.run_c54x_backend_bsp import REQUESTS, REQUEST_COMPLETION, STAGED_CLOCKED_STORE


class BackendBspResultTest(unittest.TestCase):
    def test_slot_requests_are_separate_from_bsp_acceptance(self):
        text = REQUESTS + '\n' + REQUEST_COMPLETION
        check_result(text, 3, host_requests=True)
        with self.assertRaises(ValueError):
            check_result(text, 3)
        with self.assertRaises(ValueError):
            check_result(self.output(), 3, host_requests=True)

    def test_slot_requests_require_all_cases_and_terminal_result(self):
        original = REQUESTS + '\n' + REQUEST_COMPLETION
        for token in ('cases=16', 'channels=2', 'pending_restore=16', 'armed_restore=16', 'overlap_restore=1',
                      'payload_preserved=1', 'masked_pending=1', 'ack_ports=1,2',
                      'reset=16', 'mapping_claim=0'):
            with self.subTest(token=token), self.assertRaises(ValueError):
                check_result(original.replace(token, token.split('=')[0] + '=9'), 3, host_requests=True)
        for text in (REQUEST_COMPLETION, original + '\n' + REQUEST_COMPLETION,
                     REQUESTS + '\n' + original, original + '\nunexpected',
                     'Fatal error: unexpected\n' + original):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check_result(text, 3, host_requests=True)
        with self.assertRaises(ValueError):
            check_result(original, 0, host_requests=True)

    def output(self):
        return '\n'.join((MARKER, OBSERVATION, RF_OBSERVATION, RECEIVE_PINS, TRANSMIT_PINS, CLOCK_INPUTS, CONTINUOUS_RX, CONTINUOUS_TX, FRAME_IGNORE, JOINED_PINS, CLOCKED_STORE, STAGED_CLOCKED_STORE, COMPLETION)) + '\n'

    def test_staged_clocked_store_is_mandatory_and_exact(self):
        for alternative in ('', STAGED_CLOCKED_STORE + '\n' + STAGED_CLOCKED_STORE,
                            STAGED_CLOCKED_STORE.replace('physical_phase_claim=0', 'physical_phase_claim=1')):
            with self.subTest(marker=alternative), self.assertRaisesRegex(ValueError, 'staged clocked-store'):
                check_result(self.output().replace(STAGED_CLOCKED_STORE, alternative), 3)

    def test_clocked_store_is_mandatory_and_exact(self):
        alternatives = ['', CLOCKED_STORE + '\n' + CLOCKED_STORE]
        for field in ('opcode=7721', 'pending_without_clocks=1', 'echo=0aaa', 'untimed=0', 'sources=4,5', 'handset_clock_claim=0'):
            alternatives.append(CLOCKED_STORE.replace(field, field.split('=')[0] + '=9'))
        for alternative in alternatives:
            with self.subTest(marker=alternative), self.assertRaisesRegex(ValueError, 'clocked-store'):
                check_result(self.output().replace(CLOCKED_STORE, alternative), 3)

    def test_only_explicit_verbose_teardown_is_accepted(self):
        text = self.output() + 'Enter sdlwindow_exit\nLeave sdlwindow_exit\n'
        check_result(text, 3, verbose_teardown=True)
        with self.assertRaisesRegex(ValueError, 'terminal'):
            check_result(text, 3)
        with self.assertRaisesRegex(ValueError, 'terminal'):
            check_result(text + 'unexpected\n', 3, verbose_teardown=True)

    def test_joined_pins_are_mandatory_and_exact(self):
        alternatives = ['', JOINED_PINS + '\n' + JOINED_PINS]
        for field in ('cases=128', 'formats=4', 'polarities=4', 'patterns=4', 'phases=2', 'partial_restore=128', 'sources=4,5'):
            alternatives.append(JOINED_PINS.replace(field, field.split('=')[0] + '=0'))
        alternatives.append(JOINED_PINS.replace('handset_clock_claim=0', 'handset_clock_claim=1'))
        for alternative in alternatives:
            with self.subTest(marker=alternative), self.assertRaisesRegex(ValueError, 'joined-pin'):
                check_result(self.output().replace(JOINED_PINS, alternative), 3)

    def test_frame_ignore_is_mandatory_and_exact(self):
        alternatives = ['', FRAME_IGNORE + '\n' + FRAME_IGNORE]
        for field in ('directions=2', 'words=4', 'partial_restore=1', 'ignored_sync=1', 'reset_abort=1', 'sources=4,5'):
            alternatives.append(FRAME_IGNORE.replace(field, field.split('=')[0] + '=0'))
        alternatives.append(FRAME_IGNORE.replace('handset_clock_claim=0', 'handset_clock_claim=1'))
        for alternative in alternatives:
            with self.subTest(marker=alternative), self.assertRaisesRegex(ValueError, 'frame-ignore'):
                check_result(self.output().replace(FRAME_IGNORE, alternative), 3)

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
