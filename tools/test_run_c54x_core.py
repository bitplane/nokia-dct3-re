import unittest

from tools.run_c54x_core import (COMPLETION, REQUIRED, check_result,
                                BUS_REGISTER_REQUIRED, BUS_REGISTER_COMPLETION,
                                BUS_IO_REQUIRED, BUS_IO_COMPLETION,
                                ATOMIC_IO_REQUIRED, ATOMIC_IO_COMPLETION,
                                COMPOSE_REQUIRED, COMPOSE_COMPLETION,
                                BUS_STM_REQUIRED, BUS_STM_COMPLETION,
                                READ_A_MAPPED_REQUIRED, READ_A_MAPPED_COMPLETION,
                                DP_ABSOLUTE_REQUIRED, DP_ABSOLUTE_COMPLETION)


class CoreResultTests(unittest.TestCase):
    def test_absolute_dp_scope_is_separate(self):
        output = DP_ABSOLUTE_REQUIRED + '\n' + DP_ABSOLUTE_COMPLETION + '\n'
        check_result(output, 3, dp_absolute=True)
        for bad in (output.replace('pages=512', 'pages=511'),
                    output.replace('consumer_execute=10', 'consumer_execute=11'),
                    output.replace('extension_fetches=1', 'extension_fetches=2'),
                    DP_ABSOLUTE_REQUIRED + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, dp_absolute=True)
        with self.assertRaises(ValueError):
            check_result(output, 3)

    def test_mapped_read_a_scope_is_separate(self):
        output = READ_A_MAPPED_REQUIRED + '\n' + READ_A_MAPPED_COMPLETION + '\n'
        check_result(output, 3, mapped_read_a=True)
        for bad in (output.replace('restore=288', 'restore=0'),
                    output.replace('status_preserved=1', 'status_preserved=0'),
                    output.replace('silicon_phase_claim=0', 'silicon_phase_claim=1'),
                    READ_A_MAPPED_REQUIRED + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, mapped_read_a=True)
        with self.assertRaises(ValueError):
            check_result(output, 3)

    def test_bus_stm_scope_is_separate(self):
        output = BUS_STM_REQUIRED + '\n' + BUS_STM_COMPLETION + '\n'
        check_result(output, 3, bus_stm=True)
        for bad in (output.replace('publish_clock=8', 'publish_clock=7'),
                    output.replace('peripheral_extra_clock=1', 'peripheral_extra_clock=0'),
                    output.replace('destination_reads=0', 'destination_reads=1'),
                    BUS_STM_REQUIRED + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, bus_stm=True)
        with self.assertRaises(ValueError):
            check_result(output, 3)

    def test_conflicting_scopes_are_rejected(self):
        output = COMPOSE_REQUIRED + '\n' + COMPOSE_COMPLETION + '\n'
        with self.assertRaises(ValueError):
            check_result(output, 3, compose=True, atomic_io=True)

    def test_composition_scope_is_separate(self):
        output = COMPOSE_REQUIRED + '\n' + COMPOSE_COMPLETION + '\n'
        check_result(output, 3, compose=True)
        for bad in (output.replace('restore=24', 'restore=0'),
                    output.replace('pointer_updates=once', 'pointer_updates=twice'),
                    output.replace('interrupt_after_transfer=1', 'interrupt_after_transfer=0'),
                    COMPOSE_REQUIRED + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, compose=True)
        for kwargs in ({}, {'atomic_io': True}, {'bus_io': True}):
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                check_result(output, 3, **kwargs)

    def test_atomic_bus_io_scope_is_separate(self):
        output = ATOMIC_IO_REQUIRED + '\n' + ATOMIC_IO_COMPLETION + '\n'
        check_result(output, 3, atomic_io=True)
        for bad in (output.replace('cases=2880', 'cases=1728'),
                    output.replace('forms=short,offset,absolute,peripheral,AR_alias', 'forms=short,offset,absolute'),
                    output.replace('ready_threshold=2', 'ready_threshold=0'),
                    output.replace('duplicate_io=0', 'duplicate_io=1'),
                    output.replace('physical_mapping=unvalidated', 'physical_mapping=validated'),
                    ATOMIC_IO_REQUIRED + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, atomic_io=True)
        for kwargs in ({}, {'bus_io': True}, {'bus_registers': True}):
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                check_result(output, 3, **kwargs)

    def test_bus_register_scope_is_separate(self):
        output = BUS_REGISTER_REQUIRED + '\n' + BUS_REGISTER_COMPLETION + '\n'
        check_result(output, 3, bus_registers=True)
        with self.assertRaises(ValueError):
            check_result(output, 3)
        for bad in (output.replace('bus_timing_acceptance=0', 'bus_timing_acceptance=1'),
                    output.replace('physical_mapping=unvalidated', 'physical_mapping=validated'),
                    output.replace('encodings=65536', 'encodings=65535'),
                    BUS_REGISTER_REQUIRED + '\n' + output,
                    output + 'late output\n', output.replace(BUS_REGISTER_COMPLETION, COMPLETION)):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, bus_registers=True)

    def test_staged_bus_io_scope_is_separate(self):
        output = BUS_IO_REQUIRED + '\n' + BUS_IO_COMPLETION + '\n'
        check_result(output, 3, bus_io=True)
        for bad in (output.replace('cases=576', 'cases=575'),
                    output.replace('ready_threshold=2', 'ready_threshold=0'),
                    output.replace('atomic_acceptance=separate', 'atomic_acceptance=validated'),
                    BUS_IO_REQUIRED + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, bus_io=True)
        with self.assertRaises(ValueError):
            check_result(output, 3)

    def setUp(self):
        self.output = '\n'.join([*REQUIRED, COMPLETION]) + '\n'

    def test_complete(self):
        check_result(self.output, 3)

    def test_existing_prefix_markers_allow_detail(self):
        check_result(self.output.replace(
            'stack address latency conformance: PASS\n',
            'stack address latency conformance: PASS variants=24\n'), 3)

    def test_wrong_exit(self):
        for status in (0, 1, -11):
            with self.subTest(status=status), self.assertRaises(ValueError):
                check_result(self.output, status)

    def test_each_marker_required(self):
        for marker in REQUIRED:
            with self.subTest(marker=marker), self.assertRaises(ValueError):
                check_result(self.output.replace(marker + '\n', ''), 3)

    def test_duplicate_marker(self):
        with self.assertRaises(ValueError):
            check_result(REQUIRED[0] + '\n' + self.output, 3)

    def test_old_interrupt_checkpoint_is_insufficient(self):
        with self.assertRaises(ValueError):
            check_result(self.output.replace(' intm_at_iack=1', ''), 3)

    def test_checkpoint_without_decode_order_is_insufficient(self):
        with self.assertRaises(ValueError):
            check_result(self.output.replace(' decode_before_access=1', ''), 3)

    def test_completion_must_be_terminal(self):
        for output in (self.output.replace(COMPLETION, ''), self.output + 'crash\n'):
            with self.assertRaises(ValueError):
                check_result(output, 3)

    def test_other_fatal_error_rejected(self):
        with self.assertRaises(ValueError):
            check_result('Fatal error: unexpected failure\n' + self.output, 3)


if __name__ == '__main__':
    unittest.main()
