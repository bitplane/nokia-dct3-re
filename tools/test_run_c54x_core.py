import unittest

from tools.run_c54x_core import (COMPLETION, REQUIRED, check_result,
                                BUS_REGISTER_REQUIRED, BUS_REGISTER_COMPLETION,
                                BUS_IO_REQUIRED, BUS_IO_COMPLETION,
                                ATOMIC_IO_REQUIRED, ATOMIC_IO_COMPLETION,
                                COMPOSE_REQUIRED, COMPOSE_COMPLETION,
                                BUS_STM_REQUIRED, BUS_STM_COMPLETION,
                                READ_A_MAPPED_REQUIRED, READ_A_MAPPED_COMPLETION,
                                DP_ABSOLUTE_REQUIRED, DP_ABSOLUTE_COMPLETION,
                                EXTRA_PROFILES)


class CoreResultTests(unittest.TestCase):
    def test_driver_validity_errors_reject_pass_marker(self):
        marker, ending = EXTRA_PROFILES['conditional-ret']
        output = ('Driver tms54test (file tms320c54x_test.cpp): 1 errors, 0 warnings\n'
                  'Errors:\n   BIOS name exceeds maximum 16 characters\n' +
                  marker + '\n' + ending + '\n')
        with self.assertRaisesRegex(ValueError, 'validity errors'):
            check_result(output, 3, profile='conditional-ret')

    def test_conditional_return_scope(self):
        marker, ending = EXTRA_PROFILES['conditional-ret']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='conditional-ret')
        for bad in (output.replace('restore=64', 'restore=0'),
                    output.replace('untaken_stack_accesses=0', 'untaken_stack_accesses=1'),
                    output.replace('stack_target_retained=1', 'stack_target_retained=0'),
                    output.replace('delayed_acceptance=0', 'delayed_acceptance=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='conditional-ret')

    def test_conditional_call_scope(self):
        marker, ending = EXTRA_PROFILES['conditional-call']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='conditional-call')
        for bad in (output.replace('restore=64', 'restore=0'),
                    output.replace('untaken_stack_accesses=0', 'untaken_stack_accesses=1'),
                    output.replace('predicate_retained=1', 'predicate_retained=0'),
                    output.replace('delayed_acceptance=0', 'delayed_acceptance=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='conditional-call')

    def test_fcala_producer_scope(self):
        marker, ending = EXTRA_PROFILES['fcala-producer']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='fcala-producer')
        for bad in (output.replace('restore=216', 'restore=0'),
                    output.replace('ram_reads=2', 'ram_reads=1'),
                    output.replace('target_retained=1', 'target_retained=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='fcala-producer')

    def test_staged_fcala_scope(self):
        marker, ending = EXTRA_PROFILES['fcala']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='fcala')
        for bad in (output.replace('restore=216', 'restore=0'),
                    output.replace('callee_execute=12', 'callee_execute=11'),
                    output.replace('stack_writes=5,6', 'stack_writes=5,5'),
                    output.replace('producer_latency=unvalidated', 'producer_latency=1'),
                    output.replace('irq_composition=1', 'irq_composition=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='fcala')

    def test_atomic_far_reference_scope(self):
        marker, ending = EXTRA_PROFILES['far-atomic']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='far-atomic')
        for bad in (output.replace('encodings=16', 'encodings=1'),
                    output.replace('staged_acceptance=0', 'staged_acceptance=1'),
                    output.replace('save_replay=0', 'save_replay=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='far-atomic')

    def test_bitf_peripheral_read_contract(self):
        marker, ending = EXTRA_PROFILES['bitf-peripheral']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='bitf-peripheral')
        for bad in (output.replace('restore=384', 'restore=0'),
                    output.replace('peripheral_extra_clock=1', 'peripheral_extra_clock=0'),
                    output.replace('reads=1', 'reads=2'),
                    output.replace('tc_publish=9', 'tc_publish=8'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='bitf-peripheral')

    def test_t_peripheral_producer_contract(self):
        marker, ending = EXTRA_PROFILES['t-peripheral']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='t-peripheral')
        for bad in (output.replace('restore=864', 'restore=0'),
                    output.replace('peripheral_extra_clock=1', 'peripheral_extra_clock=0'),
                    output.replace('destination_reads=0', 'destination_reads=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='t-peripheral')

    def test_mvkd_peripheral_contract(self):
        marker, ending = EXTRA_PROFILES['mvkd-peripheral']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvkd-peripheral')
        for bad in (output.replace('restore=648', 'restore=0'),
                    output.replace('bk_producer_latency=unvalidated', 'bk_producer_latency=1'),
                    output.replace('peripheral_extra_clock=1', 'peripheral_extra_clock=0'),
                    output.replace('writes=1', 'writes=2'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvkd-peripheral')

    def test_cmpm_absolute_scope(self):
        marker, ending = EXTRA_PROFILES['cmpm-absolute']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='cmpm-absolute')
        for bad in (output.replace('restore=96', 'restore=0'),
                    output.replace('source_read=7', 'source_read=6'),
                    output.replace('data_writes=0', 'data_writes=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='cmpm-absolute')

    def test_ifr_pair_scope_is_explicit(self):
        marker, ending = EXTRA_PROFILES['ifr-pair']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='ifr-pair')
        for bad in (output.replace('restore=96', 'restore=0'),
                    output.replace('late_irq=1', 'late_irq=0'),
                    output.replace('accepted_irq=1', 'accepted_irq=0'),
                    output.replace('nmi=1', 'nmi=0'),
                    output.replace('interrupt_return=1', 'interrupt_return=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='ifr-pair')

    def test_mvmd_profile_is_separate(self):
        marker, ending = EXTRA_PROFILES['mvmd-ar']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvmd-ar')
        for bad in (output.replace('restore=576', 'restore=0'),
                    output.replace('source_read=5', 'source_read=6'),
                    output.replace('repeat_acceptance=0', 'repeat_acceptance=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvmd-ar')
        with self.assertRaises(ValueError):
            check_result(output, 3, profile='mvdm-ar')

    def test_mvkd_delay_profile_is_separate(self):
        marker, ending = EXTRA_PROFILES['mvkd-delay']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvkd-delay')
        for bad in (output.replace('restore=1728', 'restore=0'),
                    output.replace('ram_write=10', 'ram_write=9'),
                    output.replace('forms=AR_source,postincrement,same_RAM', 'forms=AR_source'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvkd-delay')
        with self.assertRaises(ValueError):
            check_result(output, 3, profile='mvdm-delay')

    def test_mac_profile_is_separate(self):
        marker, ending = EXTRA_PROFILES['mac-smem']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mac-smem')
        for bad in (output.replace('restore=73728', 'restore=0'),
                    output.replace('flags=16', 'flags=8'),
                    output.replace('t_retained=1', 't_retained=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mac-smem')
        with self.assertRaises(ValueError):
            check_result(output, 3)

    def test_delayed_far_call_scope_is_separate(self):
        marker, ending = EXTRA_PROFILES['far-call-delay']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='far-call-delay')
        for bad in (output.replace('restore=243', 'restore=0'),
                    output.replace('complete_delays=2', 'complete_delays=1'),
                    output.replace('xpc_delay_visibility=unvalidated', 'xpc_delay_visibility=validated'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='far-call-delay')
        with self.assertRaises(ValueError):
            check_result(output, 3, profile='mvdm-delay')

    def test_mvdm_delay_profile_is_separate(self):
        marker, ending = EXTRA_PROFILES['mvdm-delay']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvdm-delay')
        for bad in (output.replace('restore=576', 'restore=0'),
                    output.replace('outcomes=taken,untaken', 'outcomes=taken'),
                    output.replace('interrupt_return=1', 'interrupt_return=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvdm-delay')
        with self.assertRaises(ValueError):
            check_result(output, 3, profile='mvdm-ar')

    def test_extra_mvdm_profile_is_separate(self):
        marker, ending = EXTRA_PROFILES['mvdm-ar']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvdm-ar')
        for bad in (output.replace('restore=1152', 'restore=0'),
                    output.replace('repeat_acceptance=0', 'repeat_acceptance=1'),
                    output.replace('gaps=0,1', 'gaps=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvdm-ar')
        for kwargs in ({}, {'profile': 'unknown'}, {'profile': 'mvdm-ar', 'bus_stm': True}):
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                check_result(output, 3, **kwargs)

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
