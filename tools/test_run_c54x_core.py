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
    def test_ram_forwarding_scopes(self):
        for profile in ('imm-forward', 'rmw-forward'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for bad in (output.replace('restore=', 'missing_restore='),
                        output.replace('writes_each=1', 'writes_each=2').replace('silicon_phase_claim=0', 'silicon_phase_claim=1'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)
            with self.subTest(profile=profile, exit_status=0), self.assertRaises(ValueError):
                check_result(output, 0, profile=profile)

    def test_block_xc_scopes(self):
        for profile in ('block-xc', 'block-xc-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for bad in (output.replace('restore=5760', 'restore=0').replace('restore=24', 'restore=0'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_short_block_xc_protection_scope(self):
        marker, ending = EXTRA_PROFILES['block-xc-irq']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='block-xc-irq')
        for bad in (output.replace('restore=384', 'restore=0'),
                    output.replace('all_stores_before_vector=1', 'all_stores_before_vector=0'),
                    output.replace('vector_fetches=1', 'vector_fetches=2'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='block-xc-irq')

    def test_circular_load_scope(self):
        marker, ending = EXTRA_PROFILES['ld-circular']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='ld-circular')
        for bad in (output.replace('restore=393216', 'restore=0'),
                    output.replace('bk_sample=Decode', 'bk_sample=Access'),
                    output.replace('data_writes=0', 'data_writes=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='ld-circular')

    def test_memory_move_brc_exclusion_scope(self):
        marker, ending = EXTRA_PROFILES['move-brc-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='move-brc-edge')
        for bad in (output.replace('restore=24', 'restore=0'),
                    output.replace('source_reads=0', 'source_reads=1'),
                    output.replace('pointer_effects=0', 'pointer_effects=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='move-brc-edge')

    def test_memory_move_brc_scope(self):
        marker, ending = EXTRA_PROFILES['move-brc']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='move-brc')
        for bad in (output.replace('restore=960', 'restore=0'),
                    output.replace('latency=0', 'latency=1'),
                    output.replace('counter_writes=1', 'counter_writes=2'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='move-brc')

    def test_asm_store_bypass_scope(self):
        marker, ending = EXTRA_PROFILES['asm-store-bp']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='asm-store-bp')
        for bad in (output.replace('restore=24576', 'restore=0'),
                    output.replace('retained_shift=1', 'retained_shift=0'),
                    output.replace('alias_reads=0', 'alias_reads=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='asm-store-bp')

    def test_adjacent_push_pop_scope(self):
        marker, ending = EXTRA_PROFILES['push-pop-same']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='push-pop-same')
        for bad in (output.replace('restore=2048', 'restore=0'),
                    output.replace('logical_bypass=1', 'logical_bypass=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='push-pop-same')

    def test_active_repeat_context_scope(self):
        marker, ending = EXTRA_PROFILES['repeat-context']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='repeat-context')
        for bad in (output.replace('restore=384', 'restore=0'),
                    output.replace('source_reads=once', 'source_reads=twice'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='repeat-context')

    def test_accumulator_square_scope(self):
        marker, ending = EXTRA_PROFILES['squr']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='squr')
        for bad in (output.replace('restore=32768', 'restore=0'),
                    output.replace('operand_bits=17', 'operand_bits=16'),
                    output.replace('t_unchanged=1', 't_unchanged=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='squr')

    def test_software_saved_interrupt_repeat_scope(self):
        marker, ending = EXTRA_PROFILES['repeat-isr']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='repeat-isr')
        for bad in (output.replace('restore=3840', 'restore=0'),
                    output.replace('balanced_frames=1', 'balanced_frames=0'),
                    output.replace('leaf_return=inside,REA_plus_1', 'leaf_return=inside'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='repeat-isr')

    def test_delayed_conditional_call_block_scope(self):
        marker, ending = EXTRA_PROFILES['rptb-ccd']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rptb-ccd')
        for bad in (output.replace('restore=1024', 'restore=0'),
                    output.replace('balanced_frames=1', 'balanced_frames=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='rptb-ccd')

    def test_delayed_conditional_call_exclusion_scope(self):
        marker, ending = EXTRA_PROFILES['cond-cd-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='cond-cd-edge')
        for bad in (output.replace('restore=32', 'restore=0'),
                    output.replace('stack_reservation=0', 'stack_reservation=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='cond-cd-edge')

    def test_delayed_conditional_call_scope(self):
        marker, ending = EXTRA_PROFILES['conditional-cd']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='conditional-cd')
        for bad in (output.replace('restore=1920', 'restore=0'),
                    output.replace('taken_frame=PC+4', 'taken_frame=PC+2'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='conditional-cd')

    def test_mapped_mvdk_exclusion_scope(self):
        marker, ending = EXTRA_PROFILES['mvdk-map-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvdk-map-edge')
        for bad in (output.replace('restore=48', 'restore=0'),
                    output.replace('data_reads=0', 'data_reads=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvdk-map-edge')

    def test_mapped_mvdk_producer_scope(self):
        marker, ending = EXTRA_PROFILES['mvdk-producer']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvdk-producer')
        for bad in (output.replace('restore=5184', 'restore=0'),
                    output.replace('mapped_no_conflict=1', 'mapped_no_conflict=0'),
                    output.replace('direct_ld_gap0=projection_only', 'direct_ld_gap0=silicon'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvdk-producer')

    def test_mapped_mvdk_scope(self):
        marker, ending = EXTRA_PROFILES['mvdk-mapped']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvdk-mapped')
        for bad in (output.replace('restore=14256', 'restore=0'),
                    output.replace('source_reads=1', 'source_reads=2'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvdk-mapped')

    def test_delayed_block_repeat_interrupt_scope(self):
        marker, ending = EXTRA_PROFILES['rptbd-irq']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rptbd-irq')
        for bad in (output.replace('restore=768', 'restore=0'),
                    output.replace('balanced_frames=1', 'balanced_frames=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='rptbd-irq')

    def test_delayed_block_repeat_scope(self):
        marker, ending = EXTRA_PROFILES['rptbd']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rptbd')
        for bad in (output.replace('restore=512', 'restore=0'),
                    output.replace('delay_effects=once', 'delay_effects=repeated'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='rptbd')

    def test_delayed_block_repeat_exclusion_scope(self):
        marker, ending = EXTRA_PROFILES['rptbd-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rptbd-edge')
        for bad in (output.replace('restore=28', 'restore=0'),
                    output.replace('data_reads=0', 'data_reads=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='rptbd-edge')

    def test_mapped_mvdd_source_scope(self):
        for name in ('mvdd-source', 'mvdd-source-rpt', 'mvdd-source-edge'):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace('restore=', 'missing_restore='),
                        output.replace('source_opcode_fetches=1', 'source_opcode_fetches=2'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(name=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)

    def test_ts_load_scope(self):
        for name in ('ld-ts', 'ld-ts-producer'):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace('restore=', 'missing_restore='),
                        output.replace('read_owned_t=1', 'read_owned_t=0'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(name=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)

    def test_extended_ar_store_scope(self):
        for name in ('st-extended-ar', 'st-ar-consumer'):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace('restore=', 'missing_restore='),
                        output.replace('publish_clock=7', 'publish_clock=6'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(name=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)

    def test_smem_cmpm_scope(self):
        for name in ('cmpm-smem', 'cmpm-branch'):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace('restore=', 'missing_restore='),
                        output.replace('data_writes=0', 'data_writes=1'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(name=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)

    def test_delay_multiply_scope(self):
        for name in ('mpy-delay', 'mpy-delay-edge'):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace('restore=', 'missing_restore='),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(name=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)

    def test_t_register_move_scope(self):
        marker, ending = EXTRA_PROFILES['move-t']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='move-t')
        for bad in (output.replace('restore=1920', 'restore=0'),
                    output.replace('destination_writes=1', 'destination_writes=2'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='move-t')

    def test_delay_store_boundary_scope(self):
        marker, ending = EXTRA_PROFILES['st-delay-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='st-delay-edge')
        for bad in (output.replace('restore=16', 'restore=0'),
                    output.replace('memory_writes=0', 'memory_writes=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='st-delay-edge')

    def test_long_store_delay_scope(self):
        marker, ending = EXTRA_PROFILES['store-delay']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='store-delay')
        for bad in (output.replace('restore=2880', 'restore=0'),
                    output.replace('literal_fetches=1', 'literal_fetches=2'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='store-delay')

    def test_mvmd_bk_scope(self):
        marker, ending = EXTRA_PROFILES['mvmd-bk']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvmd-bk')
        for bad in (output.replace('restore=432', 'restore=0'),
                    output.replace('source_retained=1', 'source_retained=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvmd-bk')

    def test_absolute_add_bk_scope(self):
        for name in ('add-bk', 'add-absolute'):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace('restore=2048', 'restore=0'),
                        output.replace('independent_arithmetic=1', 'independent_arithmetic=0'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(name=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)

    def test_stlm_bk_scope(self):
        marker, ending = EXTRA_PROFILES['stlm-bk']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='stlm-bk')
        for bad in (output.replace('restore=384', 'restore=0'),
                    output.replace('circular_bk=Decode', 'circular_bk=Access'),
                    output.replace('forbidden_gaps_projection=1', 'forbidden_gaps_projection=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='stlm-bk')

    def test_store_bypass_scope(self):
        marker, ending = EXTRA_PROFILES['store-bypass']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='store-bypass')
        for bad in (output.replace('restore=288', 'restore=0'),
                    output.replace('alias_reads=0', 'alias_reads=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='store-bypass')

    def test_peripheral_ldu_scope(self):
        marker, ending = EXTRA_PROFILES['ldu-periph']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='ldu-periph')
        for bad in (output.replace('restore=9216', 'restore=0'),
                    output.replace('source_reads=1', 'source_reads=2'),
                    output.replace('physical_mapping=unvalidated', 'physical_mapping=validated'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='ldu-periph')

    def test_software_intr_scope(self):
        marker, ending = EXTRA_PROFILES['software-intr']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='software-intr')
        for bad in (output.replace('restore=256', 'restore=0'),
                    output.replace('frame_writes=1', 'frame_writes=2'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='software-intr')

    def test_block_branch_exclusions_and_existing_controls(self):
        for name in ('rptb-br-edges', 'signed-branch', 'tc-branch', 'bio-branch', 'plain-branch'):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace('restore=', 'missing_restore='),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(profile=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)

    def test_in_block_branch_scope(self):
        for name in ('rptb-branch', 'rptb'):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace('restore=', 'missing_restore='),
                        output.replace('counter=Decode', 'counter=Execute').replace('brc_decode=1', 'brc_decode=0'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(profile=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)

    def test_memory_immediate_multiply_scope(self):
        for name in ('mpy-mem-imm', 'mpy-t-imm', 'mpy-mapped'):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace('restore=', 'missing_restore='),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(profile=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)

    def test_absolute_and_negate_scope(self):
        for name in ('absolute', 'negate'):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace('restore=13824', 'restore=0'),
                        output.replace('producer_cases=1536', 'producer_cases=0'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(profile=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)

    def test_profile_names_fit_mame_bios_limit(self):
        for name in EXTRA_PROFILES:
            with self.subTest(profile=name):
                self.assertLessEqual(len(name), 16)

    def test_logic_controls_and_exclusions(self):
        for name in ('logic-edges', 'logic-short', 'logic-peripheral', 'load-offset'):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace('restore=', 'missing_restore='),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(profile=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)

    def test_long_memory_logic_scope(self):
        marker, ending = EXTRA_PROFILES['logic-offset']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='logic-offset')
        for bad in (output.replace('restore=294912', 'restore=0'),
                    output.replace('pointer_updates_once=1', 'pointer_updates_once=0'),
                    output.replace('mapped_acceptance=0', 'mapped_acceptance=1'),
                    output.replace('repeat_acceptance=0', 'repeat_acceptance=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='logic-offset')

    def test_accumulator_branch_scope(self):
        for name in ('bacc', 'fbacc', 'bacc-producer', 'fbacc-producer'):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace('frame_words=0', 'frame_words=1'),
                        output.replace('target_retained=1', 'target_retained=0'),
                        output.replace('delayed_acceptance=0', 'delayed_acceptance=1'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(profile=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)

    def test_popd_edges_scope(self):
        for name in ('popd-edges', 'popd-ram'):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            with self.subTest(profile=name), self.assertRaises(ValueError):
                check_result(output.replace('restore=', 'missing_restore='), 3, profile=name)

    def test_popd_scope(self):
        for name, cases in (('popd-mapped', 4032), ('popd-repeat', 6528)):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace(f'restore={cases}', 'restore=0'),
                        output.replace('writes=1', 'writes=2').replace('writes_each=1', 'writes_each=2'),
                        output.replace('active_block_acceptance=0', 'active_block_acceptance=1'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(profile=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)

    def test_absolute_logic_scope(self):
        marker, ending = EXTRA_PROFILES['logic-absolute']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='logic-absolute')
        for bad in (output.replace('restore=1536', 'restore=0'),
                    output.replace('retained_address=1', 'retained_address=0'),
                    output.replace('retained_source=1', 'retained_source=0'),
                    output.replace('mapped_acceptance=0', 'mapped_acceptance=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='logic-absolute')

    def test_mvdd_scope(self):
        for name, cases in (('mvdd', 49152), ('mvdd-repeat', 221184)):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace(f'restore={cases}', 'restore=0'),
                        output.replace('same_ar_xmod_priority=1', 'same_ar_xmod_priority=0'),
                        output.replace('mapped_acceptance=0', 'mapped_acceptance=1'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(profile=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)

    def test_near_accumulator_call_scope(self):
        for name in ('cala', 'cala-producer'):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace('restore=216', 'restore=0'),
                        output.replace('frame_words=1', 'frame_words=2'),
                        output.replace('ignored_upper_targets=3', 'ignored_upper_targets=0'),
                        output.replace('delayed_acceptance=0', 'delayed_acceptance=1'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(profile=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)

    def test_xmem_load_scope(self):
        marker, ending = EXTRA_PROFILES['xmem-load']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='xmem-load')
        for bad in (output.replace('restore=196608', 'restore=0'),
                    output.replace('captured_source=1', 'captured_source=0'),
                    output.replace('mapped_acceptance=0', 'mapped_acceptance=1'),
                    output.replace('repeat_acceptance=0', 'repeat_acceptance=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='xmem-load')

    def test_unsigned_add_scope(self):
        marker, ending = EXTRA_PROFILES['adds']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='adds')
        for bad in (output.replace('restore=55296', 'restore=0'),
                    output.replace('source_unsigned=1', 'source_unsigned=0'),
                    output.replace('mapped_acceptance=0', 'mapped_acceptance=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='adds')

    def test_exponent_t_scope(self):
        marker, ending = EXTRA_PROFILES['exp-t']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='exp-t')
        for bad in (output.replace('restore=12288', 'restore=0'),
                    output.replace('adjacent_T=old', 'adjacent_T=new'),
                    output.replace('retained_consumer=1', 'retained_consumer=0'),
                    output.replace('norm_acceptance=0', 'norm_acceptance=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='exp-t')

    def test_repeated_pshd_scope(self):
        marker, ending = EXTRA_PROFILES['pshd-repeat']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='pshd-repeat')
        for bad in (output.replace('restore=768', 'restore=0'),
                    output.replace('issue_interval=1', 'issue_interval=2'),
                    output.replace('pending_irq_deferred=1', 'pending_irq_deferred=0'),
                    output.replace('excluded_aliases=2', 'excluded_aliases=0'),
                    output.replace('memory_count_acceptance=0', 'memory_count_acceptance=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='pshd-repeat')

    def test_mapped_pshd_scope(self):
        marker, ending = EXTRA_PROFILES['pshd-mapped']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='pshd-mapped')
        for bad in (output.replace('restore=288', 'restore=0'),
                    output.replace('source_retained=1', 'source_retained=0'),
                    output.replace('repeat_coverage=0', 'repeat_coverage=1'),
                    output.replace('dagen_source_coverage=0', 'dagen_source_coverage=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='pshd-mapped')

    def test_timer_stm_scope(self):
        marker, ending = EXTRA_PROFILES['timer-stm']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='timer-stm')
        for bad in (output.replace('restore=36', 'restore=0'),
                    output.replace('publish_clock=8', 'publish_clock=5'),
                    output.replace('trb_reload=1', 'trb_reload=0'),
                    output.replace('psc_write_mask=1', 'psc_write_mask=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='timer-stm')

    def test_direct_cmpm_scope(self):
        marker, ending = EXTRA_PROFILES['cmpm-direct']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='cmpm-direct')
        for bad in (output.replace('restore=384', 'restore=0'),
                    output.replace('address_retained=1', 'address_retained=0'),
                    output.replace('mapped_source_acceptance=0', 'mapped_source_acceptance=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='cmpm-direct')

    def test_ldu_status_scope(self):
        marker, ending = EXTRA_PROFILES['ldu-st1']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='ldu-st1')
        for bad in (output.replace('restore=288', 'restore=0'),
                    output.replace('register_reads=1', 'register_reads=2'),
                    output.replace('unsigned_full_word=1', 'unsigned_full_word=0'),
                    output.replace('producer_latency_claim=0', 'producer_latency_claim=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='ldu-st1')

    def test_stack_irq_return_scope(self):
        marker, ending = EXTRA_PROFILES['stack-irq-return']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='stack-irq-return')
        for bad in (output.replace('restore=80', 'restore=0'),
                    output.replace('return_reads=1', 'return_reads=0'),
                    output.replace('resumed_idle=1', 'resumed_idle=0'),
                    output.replace('intm_cleared=1', 'intm_cleared=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='stack-irq-return')

    def test_stack_irq_order_scope(self):
        marker, ending = EXTRA_PROFILES['stack-irq']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='stack-irq')
        for bad in (output.replace('restore=48', 'restore=0'),
                    output.replace('older_first=1', 'older_first=0'),
                    output.replace('acknowledgements=1', 'acknowledgements=2'),
                    output.replace('isr_return_claim=0', 'isr_return_claim=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='stack-irq')

    def test_pop_brc_repeat_scope(self):
        marker, ending = EXTRA_PROFILES['pop-brc']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='pop-brc')
        for bad in (output.replace('restore=30', 'restore=0'),
                    output.replace('gap_nops=1', 'gap_nops=0'),
                    output.replace('count_plus_one=1', 'count_plus_one=0'),
                    output.replace('active_repeat_write_acceptance=0', 'active_repeat_write_acceptance=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='pop-brc')

    def test_repeat_control_pop_scope(self):
        marker, ending = EXTRA_PROFILES['pop-repeat']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='pop-repeat')
        for bad in (output.replace('restore=192', 'restore=0'),
                    output.replace('stack_reads=1', 'stack_reads=2'),
                    output.replace('active_repeat_acceptance=0', 'active_repeat_acceptance=1'),
                    output.replace('excluded_active=3', 'excluded_active=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='pop-repeat')

    def test_pmst_iptr_scope(self):
        marker, ending = EXTRA_PROFILES['pmst-iptr']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='pmst-iptr')
        for bad in (output.replace('restore=1024', 'restore=0'),
                    output.replace('irq_bits=16', 'irq_bits=1'),
                    output.replace('vector_prefetch_owned=1', 'vector_prefetch_owned=0'),
                    output.replace('pending_latch_fixture=1', 'pending_latch_fixture=0'),
                    output.replace('isr_return_claim=0', 'isr_return_claim=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='pmst-iptr')

    def test_pmst_prefetch_scope(self):
        marker, ending = EXTRA_PROFILES['pmst-prefetch']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='pmst-prefetch')
        for bad in (output.replace('restore=64', 'restore=0'),
                    output.replace('debugger_state_fixture=1', 'debugger_state_fixture=0'),
                    output.replace('firmware_ordering_claim=0', 'firmware_ordering_claim=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='pmst-prefetch')

    def test_pmst_pop_scope(self):
        marker, ending = EXTRA_PROFILES['pmst-pop']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='pmst-pop')
        for bad in (output.replace('restore=64', 'restore=0'),
                    output.replace('stack_reads=1', 'stack_reads=2'),
                    output.replace('source_retained=1', 'source_retained=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='pmst-pop')

    def test_pmst_popd_scope(self):
        marker, ending = EXTRA_PROFILES['pmst-popd']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='pmst-popd')
        for bad in (output.replace('restore=64', 'restore=0'),
                    output.replace('stack_reads=1', 'stack_reads=2'),
                    output.replace('active_repeat_claim=0', 'active_repeat_claim=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='pmst-popd')

    def test_repeat_pmst_scope(self):
        marker, ending = EXTRA_PROFILES['repeat-pmst']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='repeat-pmst')
        for bad in (output.replace('restore=384', 'restore=0'),
                    output.replace('pmst_unchanged=1', 'pmst_unchanged=0'),
                    output.replace('actual_iterations=1', 'actual_iterations=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='repeat-pmst')

    def test_repeat_popd_scope(self):
        marker, ending = EXTRA_PROFILES['repeat-popd']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='repeat-popd')
        for bad in (output.replace('restore=384', 'restore=0'),
                    output.replace('source_retained=1', 'source_retained=0'),
                    output.replace('actual_iterations=1', 'actual_iterations=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='repeat-popd')

    def test_dual_mac_scope(self):
        for profile in ('mac-dual', 'mac-dual-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for bad in (output.replace('restore=', 'restore=0'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_repeat_rearm_scope(self):
        marker, ending = EXTRA_PROFILES['repeat-rearm']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='repeat-rearm')
        for bad in (output.replace('restore=64', 'restore=0'),
                    output.replace('brc_publications=1', 'brc_publications=2'),
                    output.replace('write_owner=Access', 'write_owner=Execute'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='repeat-rearm')

    def test_repeat_rearm_edge_scope(self):
        marker, ending = EXTRA_PROFILES['rearm-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rearm-edge')
        for bad in (output.replace('restore=8', 'restore=0'),
                    output.replace('publications=0', 'publications=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='rearm-edge')

    def test_reverse_mar_scope(self):
        marker, ending = EXTRA_PROFILES['mar-reverse']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mar-reverse')
        for bad in (output.replace('restore=3072', 'restore=0'),
                    output.replace('independent_bit_serial_oracle=1', 'independent_bit_serial_oracle=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mar-reverse')

    def test_block_bcd_scope(self):
        marker, ending = EXTRA_PROFILES['rptb-bcd']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rptb-bcd')
        for bad in (output.replace('restore=3072', 'restore=0'),
                    output.replace('delay_decision_retained=1', 'delay_decision_retained=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='rptb-bcd')

    def test_immediate_asm_scope(self):
        marker, ending = EXTRA_PROFILES['asm-immediate']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='asm-immediate')
        for bad in (output.replace('restore=9216', 'restore=0'),
                    output.replace('data_reads=0', 'data_reads=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='asm-immediate')

    def test_short_asm_store_scopes(self):
        for profile in ('asm-store', 'asm-store-ld'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for bad in (output.replace('restore=122880', 'restore=0'),
                        output.replace('data_reads=0', 'data_reads=1'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_shifted_delay_load_scope(self):
        marker, ending = EXTRA_PROFILES['shift-delay']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='shift-delay')
        for bad in (output.replace('restore=110592', 'restore=0'),
                    output.replace('extension_fetches=1', 'extension_fetches=2'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='shift-delay')

    def test_shifted_delay_exclusions(self):
        marker, ending = EXTRA_PROFILES['shift-delay-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='shift-delay-edge')
        for bad in (output.replace('restore=16', 'restore=0'),
                    output.replace('data_reads=0', 'data_reads=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='shift-delay-edge')

    def test_pmst_mapping_scope(self):
        marker, ending = EXTRA_PROFILES['pmst-map']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='pmst-map')
        for bad in (output.replace('restore=64', 'restore=0'),
                    output.replace('duplicate_register_writes=0', 'duplicate_register_writes=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='pmst-map')

    def test_stlm_imr_scope(self):
        marker, ending = EXTRA_PROFILES['stlm-imr']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='stlm-imr')
        for bad in (output.replace('restore=9216', 'restore=0'),
                    output.replace('accumulators=2', 'accumulators=1'),
                    output.replace('source_execute_live=1', 'source_execute_live=0'),
                    output.replace('destination_reads=0', 'destination_reads=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='stlm-imr')

    def test_mcbsp_control_scope(self):
        marker, ending = EXTRA_PROFILES['mcbsp-control']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mcbsp-control')
        for bad in (output.replace('restore=16', 'restore=0'),
                    output.replace('busy_after_read=1', 'busy_after_read=0'),
                    output.replace('device_reset=1', 'device_reset=0'),
                    output.replace('physical_mapping=unvalidated', 'physical_mapping=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mcbsp-control')

    def test_long_peripheral_rmw_scope(self):
        marker, ending = EXTRA_PROFILES['rmw-long-periph']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rmw-long-periph')
        for bad in (output.replace('restore=368640', 'restore=0'),
                    output.replace('double_charge=0', 'double_charge=1'),
                    output.replace('ram_extra=0', 'ram_extra=1'),
                    output.replace('device_semantics=unvalidated', 'device_semantics=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='rmw-long-periph')

    def test_long_peripheral_store_scope(self):
        marker, ending = EXTRA_PROFILES['st-long-periph']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='st-long-periph')
        for bad in (output.replace('restore=6144', 'restore=0'),
                    output.replace('ram_extra=0', 'ram_extra=1'),
                    output.replace('peripheral_extra=1', 'peripheral_extra=0'),
                    output.replace('device_semantics=unvalidated', 'device_semantics=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='st-long-periph')

    def test_preincrement_immediate_scope(self):
        marker, ending = EXTRA_PROFILES['preinc-st']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='preinc-st')
        for bad in (output.replace('restore=1536', 'restore=0'),
                    output.replace('source_literal_retained=1', 'source_literal_retained=0'),
                    output.replace('single_repeat_acceptance=0', 'single_repeat_acceptance=1'),
                    output.replace('pointer_updates=1', 'pointer_updates=2'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='preinc-st')

    def test_stm_brc_scope(self):
        marker, ending = EXTRA_PROFILES['stm-brc']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='stm-brc')
        for bad in (output.replace('restore=30', 'restore=0'),
                    output.replace('latency=0', 'latency=1'),
                    output.replace('counter_writes=1', 'counter_writes=2'),
                    output.replace('in_loop_acceptance=0', 'in_loop_acceptance=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='stm-brc')

    def test_preincrement_store_scope(self):
        marker, ending = EXTRA_PROFILES['preinc-store']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='preinc-store')
        for bad in (output.replace('restore=6144', 'restore=0'),
                    output.replace('pointer_updates=1', 'pointer_updates=2'),
                    output.replace('reads=0', 'reads=1'),
                    output.replace('source_execute_live=1', 'source_execute_live=0'),
                    output.replace('interrupt_composition=unvalidated', 'interrupt_composition=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='preinc-store')

    def test_ldu_imr_scope(self):
        marker, ending = EXTRA_PROFILES['ldu-imr']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='ldu-imr')
        for bad in (output.replace('restore=384', 'restore=0'),
                    output.replace('source_retained=1', 'source_retained=0'),
                    output.replace('imr_reads=1', 'imr_reads=2'),
                    output.replace('interrupt_composition=unvalidated', 'interrupt_composition=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='ldu-imr')

    def test_memory_shift16_scope(self):
        marker, ending = EXTRA_PROFILES['memory-shift16']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='memory-shift16')
        for bad in (output.replace('restore=221184', 'restore=0'),
                    output.replace('partial_carry=1', 'partial_carry=0'),
                    output.replace('selectors=8', 'selectors=4'),
                    output.replace('mapped_acceptance=0', 'mapped_acceptance=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='memory-shift16')

    def test_mvmd_status_irq_scope(self):
        marker, ending = EXTRA_PROFILES['mvmd-status-irq']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvmd-status-irq')
        for bad in (output.replace('restore=576', 'restore=0'),
                    output.replace('masked_frame=0', 'masked_frame=1'),
                    output.replace('foreground_once=1', 'foreground_once=0'),
                    output.replace('rsbx_guard_inherited=0', 'rsbx_guard_inherited=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvmd-status-irq')

    def test_mvmd_cmpt_scope(self):
        marker, ending = EXTRA_PROFILES['mvmd-cmpt']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvmd-cmpt')
        for bad in (output.replace('restore=16', 'restore=0'),
                    output.replace('pointer_updates=1', 'pointer_updates=0'),
                    output.replace('latency=2', 'latency=0'),
                    output.replace('interrupt_composition=unvalidated', 'interrupt_composition=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvmd-cmpt')

    def test_mvmd_block_scope(self):
        marker, ending = EXTRA_PROFILES['mvmd-block']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvmd-block')
        for bad in (output.replace('restore=24', 'restore=0'),
                    output.replace('braf_preserved=1', 'braf_preserved=0'),
                    output.replace('interrupt_composition=unvalidated', 'interrupt_composition=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvmd-block')

    def test_mvmd_status_alu_scope(self):
        marker, ending = EXTRA_PROFILES['mvmd-status-alu']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvmd-status-alu')
        for bad in (output.replace('restore=768', 'restore=0'),
                    output.replace('asm_values=32', 'asm_values=1'),
                    output.replace('latency=0', 'latency=1'),
                    output.replace('interrupt_composition=unvalidated', 'interrupt_composition=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvmd-status-alu')

    def test_mvmd_cpl_scope(self):
        marker, ending = EXTRA_PROFILES['mvmd-cpl']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvmd-cpl')
        for bad in (output.replace('restore=16', 'restore=0'),
                    output.replace('latency=2', 'latency=0'),
                    output.replace('interrupt_composition=unvalidated', 'interrupt_composition=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvmd-cpl')

    def test_mvdd_peripheral_scope(self):
        marker, ending = EXTRA_PROFILES['mvdd-periph']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvdd-periph')
        for bad in (output.replace('restore=1536', 'restore=0'),
                    output.replace('extra_cycles=1', 'extra_cycles=0'),
                    output.replace('repeat_acceptance=0', 'repeat_acceptance=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvdd-periph')

    def test_mvmd_t_scope(self):
        marker, ending = EXTRA_PROFILES['mvmd-t']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvmd-t')
        for bad in (output.replace('restore=432', 'restore=0'),
                    output.replace('source_reads=1', 'source_reads=2'),
                    output.replace('producer_composition=unvalidated', 'producer_composition=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvmd-t')

    def test_mvmd_status_scope(self):
        marker, ending = EXTRA_PROFILES['mvmd-st1']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvmd-st1')
        for bad in (output.replace('restore=288', 'restore=0'),
                    output.replace('status_writes=1', 'status_writes=2'),
                    output.replace('consumer_latency=unvalidated', 'consumer_latency=1'),
                    output.replace('interrupt_composition=unvalidated', 'interrupt_composition=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvmd-st1')

    def test_mvmd_accumulator_scope(self):
        marker, ending = EXTRA_PROFILES['mvmd-acc']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvmd-acc')
        for bad in (output.replace('restore=288', 'restore=0'),
                    output.replace('mapped_no_conflict=1', 'mapped_no_conflict=0'),
                    output.replace('status_destination=unvalidated', 'status_destination=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvmd-acc')

    def test_subs_unsigned_scope(self):
        marker, ending = EXTRA_PROFILES['subs']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='subs')
        for bad in (output.replace('restore=55296', 'restore=0'),
                    output.replace('source_unsigned=1', 'source_unsigned=0'),
                    output.replace('mapped_acceptance=0', 'mapped_acceptance=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='subs')

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
        for bad in (output.replace('restore=768', 'restore=0'),
                    output.replace('patterns=8', 'patterns=4'),
                    output.replace('untaken_stack_accesses=0', 'untaken_stack_accesses=1'),
                    output.replace('stack_target_retained=1', 'stack_target_retained=0'),
                    output.replace('delayed_acceptance=0', 'delayed_acceptance=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='conditional-ret')

    def test_load_return_scope(self):
        marker, ending = EXTRA_PROFILES['ret-load']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='ret-load')
        for key, old, new in (('restore', '64', '0'), ('producer_reads', '1', '0'),
                              ('data_writes', '0', '1'), ('gaps', '2', '1')):
            with self.subTest(key=key), self.assertRaises(ValueError):
                check_result(output.replace(f'{key}={old}', f'{key}={new}'),
                             3, profile='ret-load')

    def test_carry_branch_scope(self):
        marker, ending = EXTRA_PROFILES['carry-branch']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='carry-branch')
        for old, new in (('restore=384', 'restore=0'),
                         ('conditions=C,NC', 'conditions=C'),
                         ('duplicate_fetches=0', 'duplicate_fetches=1'),
                         ('arithmetic_producers=unvalidated', 'arithmetic_producers=validated')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='carry-branch')

    def test_long_delayed_banz_scope(self):
        marker, ending = EXTRA_PROFILES['banzd-long']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='banzd-long')
        for old, new in (('restore=4608', 'restore=0'),
                         ('delay_forms=two_single,one_double', 'delay_forms=two_single'),
                         ('offset_fetches=1', 'offset_fetches=2'),
                         ('data_accesses=0', 'data_accesses=1')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='banzd-long')

    def test_write_a_scope(self):
        marker, ending = EXTRA_PROFILES['write-a']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='write-a')
        for old, new in (('restore=3584', 'restore=0'), ('mapping=2', 'mapping=1'),
                         ('program_writes=1', 'program_writes=2'),
                         ('repeat=unvalidated', 'repeat=validated')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='write-a')

    def test_write_a_exclusions(self):
        marker, ending = EXTRA_PROFILES['write-a-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='write-a-edge')
        for old, new in (('restore=12', 'restore=0'), ('source_reads=0', 'source_reads=1'),
                         ('program_writes=0', 'program_writes=1'), ('atomic_fallback=0', 'atomic_fallback=1')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='write-a-edge')

    def test_xmem_arithmetic_scope(self):
        marker, ending = EXTRA_PROFILES['xmem-alu']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='xmem-alu')
        for old, new in (('restore=393216', 'restore=0'), ('encodings=1024', 'encodings=512'),
                         ('operations=ADD,SUB', 'operations=ADD'),
                         ('mapped_acceptance=0', 'mapped_acceptance=1'),
                         ('repeat_acceptance=0', 'repeat_acceptance=1')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='xmem-alu')

    def test_xmem_store_scope(self):
        marker, ending = EXTRA_PROFILES['xmem-store']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='xmem-store')
        for old, new in (('restore=786432', 'restore=0'), ('operations=STL,STH', 'operations=STL'),
                         ('destination_reads=0', 'destination_reads=1'),
                         ('eab=Read', 'eab=Execute'), ('sst_sxm=1', 'sst_sxm=0'),
                         ('mapped_acceptance=0', 'mapped_acceptance=1')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='xmem-store')

    def test_xmem_store_bypass_scope(self):
        marker, ending = EXTRA_PROFILES['xmem-store-bp']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='xmem-store-bp')
        for old, new in (('restore=288', 'restore=0'), ('alias_reads=0', 'alias_reads=1'),
                         ('writes=1', 'writes=2'), ('gaps=0,1', 'gaps=1')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='xmem-store-bp')

    def test_long_pair_profile_scopes(self):
        for profile, restores in (('drsub', 6912), ('long-pair', 20736)):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for old, new in ((f'restore={restores}', 'restore=0'),
                             ('pair_reads=2', 'pair_reads=1'),
                             ('memory_writes=0', 'memory_writes=1')):
                with self.subTest(profile=profile, old=old), self.assertRaises(ValueError):
                    check_result(output.replace(old, new), 3, profile=profile)

    def test_normalization_profile_scopes(self):
        for profile, restores in (('norm', 98304), ('norm-edge', 256), ('exp-norm', 6144)):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            with self.subTest(profile=profile), self.assertRaises(ValueError):
                check_result(output.replace(f'restore={restores}', 'restore=0'), 3, profile=profile)
        marker, ending = EXTRA_PROFILES['exp-norm']
        with self.assertRaises(ValueError):
            check_result(marker.replace('zero_gap=projection_only', 'zero_gap=validated') + '\n' + ending + '\n', 3, profile='exp-norm')

    def test_signed_xc_profile_scopes(self):
        for profile, restores in (('xc-signed', 2304), ('xc-controls', 212)):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            with self.subTest(profile=profile), self.assertRaises(ValueError):
                check_result(output.replace(f'restore={restores}', 'restore=0'), 3, profile=profile)
        marker, ending = EXTRA_PROFILES['xc-signed']
        for old, new in (('predicate=Access', 'predicate=Read'),
                         ('rejected_writes=0', 'rejected_writes=1'),
                         ('signed_irq_composition=unvalidated', 'signed_irq_composition=validated')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(marker.replace(old, new) + '\n' + ending + '\n', 3, profile='xc-signed')

    def test_dual_multiply_profile_scopes(self):
        for profile, restores in (('mpy-dual', 43008), ('mpy-dual-edge', 16)):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            with self.subTest(profile=profile), self.assertRaises(ValueError):
                check_result(output.replace(f'restore={restores}', 'restore=0'), 3, profile=profile)
        marker, ending = EXTRA_PROFILES['mpy-dual']
        for old, new in (('x_alias_priority=1', 'x_alias_priority=0'),
                         ('t_publish=Read', 't_publish=Execute'),
                         ('physical_banks=unvalidated', 'physical_banks=validated')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(marker.replace(old, new) + '\n' + ending + '\n', 3, profile='mpy-dual')

    def test_indexed_load_profile_scope(self):
        marker, ending = EXTRA_PROFILES['ld-ar0-index']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='ld-ar0-index')
        for old, new in (('restore=98304', 'restore=0'),
                         ('address_index_sample=Access', 'address_index_sample=Read'),
                         ('self_index=1', 'self_index=0'),
                         ('excluded_cases=2304', 'excluded_cases=0')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='ld-ar0-index')

    def test_delayed_load_profile_scope(self):
        edge, end = EXTRA_PROFILES['ld-delay-edge']
        check_result(edge + '\n' + end + '\n', 3, profile='ld-delay-edge')
        with self.assertRaises(ValueError):
            check_result(edge.replace('spill_cases=16', 'spill_cases=0') + '\n' + end + '\n', 3, profile='ld-delay-edge')
        marker, ending = EXTRA_PROFILES['load-delay']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='load-delay')
        for old, new in (('restore=147456', 'restore=0'),
                         ('target_observes_result=1', 'target_observes_result=0'),
                         ('interrupt_composition=unvalidated', 'interrupt_composition=validated')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='load-delay')

    def test_block_backward_profile_scope(self):
        marker, ending = EXTRA_PROFILES['rptb-back']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rptb-back')
        for old, new in (('restore=1536', 'restore=0'),
                         ('outer_counts_preserved=1', 'outer_counts_preserved=0'),
                         ('held_words_restored=1', 'held_words_restored=0')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='rptb-back')

    def test_block_far_call_profile_scope(self):
        edge, end = EXTRA_PROFILES['rptb-fcall-edge']
        check_result(edge + '\n' + end + '\n', 3, profile='rptb-fcall-edge')
        with self.assertRaises(ValueError):
            check_result(edge.replace('frame_allocation=0', 'frame_allocation=1') + '\n' + end + '\n', 3, profile='rptb-fcall-edge')
        marker, ending = EXTRA_PROFILES['rptb-fcall']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rptb-fcall')
        for old, new in (('restore=768', 'restore=0'),
                         ('sources=none,IRQ,NMI', 'sources=none'),
                         ('caller_loop_preserved=1', 'caller_loop_preserved=0')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='rptb-fcall')

    def test_dual_macsu_profile_scope(self):
        for profile, count in (('macsu-dual', 215040), ('macsu-dual-edge', 16)):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            with self.subTest(profile=profile), self.assertRaises(ValueError):
                check_result(output.replace(f'restore={count}', 'restore=0'), 3, profile=profile)
        marker, ending = EXTRA_PROFILES['macsu-dual']
        for old, new in (('x_unsigned_y_signed=1', 'x_unsigned_y_signed=0'),
                         ('accumulator_sample=Execute', 'accumulator_sample=Read'),
                         ('t_publish=Read', 't_publish=Execute')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(marker.replace(old, new) + '\n' + ending + '\n', 3, profile='macsu-dual')

    def test_block_single_profile_scope(self):
        edge, end = EXTRA_PROFILES['rptb-rpt-edge']
        check_result(edge + '\n' + end + '\n', 3, profile='rptb-rpt-edge')
        with self.assertRaises(ValueError):
            check_result(edge.replace('restore=16', 'restore=0') + '\n' + end + '\n', 3, profile='rptb-rpt-edge')
        marker, ending = EXTRA_PROFILES['rptb-single']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rptb-single')
        for old, new in (('restore=768', 'restore=0'),
                         ('positions=middle,end', 'positions=middle'),
                         ('outer_counts_preserved=1', 'outer_counts_preserved=0'),
                         ('interrupt_composition=unvalidated', 'interrupt_composition=validated')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='rptb-single')

    def test_t_store_profile_scopes(self):
        for profile, restores in (('st-t', 49152), ('st-t-consumer', 15360), ('st-t-edge', 64)):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            with self.subTest(profile=profile), self.assertRaises(ValueError):
                check_result(output.replace(f'restore={restores}', 'restore=0'), 3, profile=profile)
        for profile, old, new in (('st-t', 'address=Read', 'address=Access'),
                                  ('st-t', 'data=Execute', 'data=Read'),
                                  ('st-t-consumer', 'short_gap0=projection', 'short_gap0=legal'),
                                  ('st-t-consumer', 'younger_ld_t_wins=1', 'younger_ld_t_wins=0')):
            marker, ending = EXTRA_PROFILES[profile]
            with self.subTest(profile=profile, old=old), self.assertRaises(ValueError):
                check_result(marker.replace(old, new) + '\n' + ending + '\n', 3, profile=profile)

    def test_ts_arithmetic_profile_scopes(self):
        for profile, restores in (('ts-alu', 154368), ('ts-alu-producer', 18432), ('ts-alu-edge', 320)):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            with self.subTest(profile=profile), self.assertRaises(ValueError):
                check_result(output.replace(f'restore={restores}', 'restore=0'), 3, profile=profile)
        for profile, old, new in (('ts-alu', 't_sample=Read', 't_sample=Execute'),
                                  ('ts-alu-producer', 'projection_only_stlm_gap=0', 'projection_only_stlm_gap=none'),
                                  ('ts-alu-edge', 'invalid_count_reads=1', 'invalid_count_reads=0')):
            marker, ending = EXTRA_PROFILES[profile]
            with self.subTest(profile=profile, old=old), self.assertRaises(ValueError):
                check_result(marker.replace(old, new) + '\n' + ending + '\n', 3, profile=profile)

    def test_t_load_control_scope(self):
        marker, ending = EXTRA_PROFILES['t-load-control']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='t-load-control')
        for old, new in (('restore=56064', 'restore=0'),
                         ('t_publish=Read', 't_publish=Execute'),
                         ('no_retirement_rewrite=1', 'no_retirement_rewrite=0')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='t-load-control')

    def test_asm_load_profile_scopes(self):
        for profile, restores in (('asm-load', 73728), ('asm-consumer', 9216), ('asm-edge', 16), ('asm-mapped', 49152)):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            with self.subTest(profile=profile), self.assertRaises(ValueError):
                check_result(output.replace(f'restore={restores}', 'restore=0'), 3, profile=profile)
        for profile, old, new in (('asm-load', 't_unchanged=1', 't_unchanged=0'),
                                  ('asm-load', 'publish=Execute', 'publish=Read'),
                                  ('asm-mapped', 'sources=16', 'sources=1'),
                                  ('asm-mapped', 'source_sample=Read', 'source_sample=Execute'),
                                  ('asm-consumer', 'latency=zero', 'latency=one_word')):
            marker, ending = EXTRA_PROFILES[profile]
            with self.subTest(profile=profile, old=old), self.assertRaises(ValueError):
                check_result(marker.replace(old, new) + '\n' + ending + '\n', 3, profile=profile)

    def test_delayed_comparison_profile_scope(self):
        marker, ending = EXTRA_PROFILES['cmp-delay-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='cmp-delay-edge')
        with self.assertRaises(ValueError):
            check_result(output.replace('restore=48', 'restore=0'), 3, profile='cmp-delay-edge')
        marker, ending = EXTRA_PROFILES['compare-delay']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='compare-delay')
        for old, new in (('restore=27648', 'restore=0'),
                         ('branch_decision=retained', 'branch_decision=reevaluated'),
                         ('literal_reads=once', 'literal_reads=twice'),
                         ('interrupt_after_publication=1', 'interrupt_after_publication=0')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='compare-delay')

    def test_indexed_store_profile_scope(self):
        marker, ending = EXTRA_PROFILES['store-index']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='store-index')
        for old, new in (('restore=98304', 'restore=0'),
                         ('address=Read', 'address=Access'),
                         ('data=Execute', 'data=Read'),
                         ('excluded_pointer_updates=0', 'excluded_pointer_updates=1'),
                         ('producer_composition=unvalidated', 'producer_composition=validated')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='store-index')

    def test_minmax_profile_scope(self):
        marker, ending = EXTRA_PROFILES['minmax']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='minmax')
        for old, new in (('restore=65536', 'restore=0'),
                         ('operands=Execute', 'operands=Read'),
                         ('signed_bits=40', 'signed_bits=32'),
                         ('equality=B', 'equality=A'),
                         ('producer_composition=unvalidated', 'producer_composition=validated')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='minmax')

    def test_indexed_mar_profile_scope(self):
        marker, ending = EXTRA_PROFILES['mar-index']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mar-index')
        for old, new in (('restore=3072', 'restore=0'),
                         ('publication=Access', 'publication=Execute'),
                         ('alias_AR0=1', 'alias_AR0=0'),
                         ('producer_latency=unvalidated', 'producer_latency=validated')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='mar-index')

    def test_immediate_mac_profile_scope(self):
        marker, ending = EXTRA_PROFILES['mac-t-imm']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mac-t-imm')
        for old, new in (('restore=172032', 'restore=0'),
                         ('t_retained=1', 't_retained=0'),
                         ('accumulator_latency=unvalidated', 'accumulator_latency=validated'),
                         ('projection_only_stlm_gap=0', 'projection_only_stlm_gap=none')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='mac-t-imm')

    def test_dual_arithmetic_profile_scopes(self):
        for profile, restores in (('alu-dual', 86016), ('alu-dual-edge', 32)):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            with self.subTest(profile=profile), self.assertRaises(ValueError):
                check_result(output.replace(f'restore={restores}', 'restore=0'), 3, profile=profile)
        marker, ending = EXTRA_PROFILES['alu-dual']
        for old, new in (('x_alias_priority=1', 'x_alias_priority=0'),
                         ('t_unchanged=1', 't_unchanged=0'),
                         ('physical_banks=unvalidated', 'physical_banks=validated')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_result(marker.replace(old, new) + '\n' + ending + '\n', 3, profile='alu-dual')

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
