from pathlib import Path
import re
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
    def test_source_domain_census_requires_all_forms_and_no_side_effects(self):
        marker, ending = EXTRA_PROFILES['source-domain']
        text = marker + '\n' + ending
        check_result(text, 3, profile='source-domain')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'),
                                 3, profile='source-domain')
        for bad in (marker, ending, text + '\nunhandled opcode'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='source-domain')

    def test_long_memory_alias_exclusions_require_complete_matrix(self):
        profile = 'long-abs-edge'
        marker,ending = EXTRA_PROFILES[profile]
        text = marker+'\n'+ending
        check_result(text,3,profile=profile)
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field),self.assertRaises(ValueError):
                    check_result(text.replace(field,field.split('=')[0]+'=invalid'),3,profile=profile)
        for bad in (marker,ending,text+'\nunhandled opcode'):
            with self.assertRaises(ValueError):
                check_result(bad,3,profile=profile)

    def test_arithmetic_alias_exclusions_require_all_families_and_no_operand_effects(self):
        profile = 'arith-abs-edge'
        marker, ending = EXTRA_PROFILES[profile]
        text = marker + '\n' + ending
        check_result(text, 3, profile=profile)
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'),
                                 3, profile=profile)
        for bad in (marker, ending, text + '\nunhandled opcode'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile=profile)

    def test_logic_alias_exclusions_require_every_class_and_no_operand_effects(self):
        marker,ending=EXTRA_PROFILES['logic-abs-edge']
        text=marker+'\n'+ending
        check_result(text,3,profile='logic-abs-edge')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field),self.assertRaises(ValueError):
                    check_result(text.replace(field,field.split('=')[0]+'=invalid'),3,profile='logic-abs-edge')
        for bad in (marker,ending,text+'\nunhandled opcode'):
            with self.assertRaises(ValueError):
                check_result(bad,3,profile='logic-abs-edge')

    def test_rmw_alias_exclusions_require_all_classes_and_no_operand_effects(self):
        marker, ending = EXTRA_PROFILES['rmw-abs-edge']
        text = marker + '\n' + ending
        check_result(text, 3, profile='rmw-abs-edge')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3,
                                 profile='rmw-abs-edge')
        for bad in (marker, ending, text + '\nunhandled opcode'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='rmw-abs-edge')

    def test_load_alias_exclusions_require_all_families_and_no_operand_effects(self):
        marker, ending = EXTRA_PROFILES['load-abs-edge']
        text = marker + '\n' + ending
        check_result(text, 3, profile='load-abs-edge')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3,
                                 profile='load-abs-edge')
        for bad in (marker, ending, text + '\nunhandled opcode'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='load-abs-edge')

    def test_memory_t_alias_exclusions_require_every_family_and_both_frontends(self):
        marker, ending = EXTRA_PROFILES['mem-t-abs-edge']
        text = marker + '\n' + ending
        check_result(text, 3, profile='mem-t-abs-edge')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3,
                                 profile='mem-t-abs-edge')
        for bad in (marker, ending, text + '\nunhandled opcode'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='mem-t-abs-edge')

    def test_program_multiply_alias_exclusions_require_both_instructions_and_destinations(self):
        marker, ending = EXTRA_PROFILES['pmac-abs-edge']
        text = marker + '\n' + ending
        check_result(text, 3, profile='pmac-abs-edge')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3,
                                 profile='pmac-abs-edge')

    def test_delay_alias_exclusions_require_both_instructions_and_no_data_effects(self):
        marker, ending = EXTRA_PROFILES['delay-abs-edge']
        text = marker + '\n' + ending
        check_result(text, 3, profile='delay-abs-edge')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3,
                                 profile='delay-abs-edge')

    def test_mac_alias_exclusions_require_both_sources_and_no_operand_effects(self):
        marker, ending = EXTRA_PROFILES['mac-abs-edge']
        text = marker + '\n' + ending
        check_result(text, 3, profile='mac-abs-edge')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3,
                                 profile='mac-abs-edge')

    def test_mpy_alias_exclusions_require_both_frontends_and_no_operand_effects(self):
        marker, ending = EXTRA_PROFILES['mpy-abs-edge']
        text = marker + '\n' + ending
        check_result(text, 3, profile='mpy-abs-edge')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3,
                                 profile='mpy-abs-edge')

    def test_interrupt_pop_exclusions_require_no_ack_or_stack_side_effects(self):
        marker,ending=EXTRA_PROFILES['pop-irq-edge']
        text=marker+'\n'+ending
        check_result(text,3,profile='pop-irq-edge')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field),self.assertRaises(ValueError):
                    check_result(text.replace(field,field.split('=')[0]+'=invalid'),3,profile='pop-irq-edge')

    def test_interrupt_pop_requires_w1c_retained_stack_and_restore_scope(self):
        marker,ending=EXTRA_PROFILES['pop-irq']
        text=marker+'\n'+ending
        check_result(text,3,profile='pop-irq')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field),self.assertRaises(ValueError):
                    check_result(text.replace(field,field.split('=')[0]+'=invalid'),3,profile='pop-irq')

    def test_clkmd_move_exclusions_require_no_operand_or_clock_effects(self):
        marker,ending=EXTRA_PROFILES['clock-mvdk-edge']
        text=marker+'\n'+ending
        check_result(text,3,profile='clock-mvdk-edge')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field),self.assertRaises(ValueError):
                    check_result(text.replace(field,field.split('=')[0]+'=invalid'),3,profile='clock-mvdk-edge')

    def test_clkmd_move_requires_retained_access_wait_and_restore(self):
        marker,ending=EXTRA_PROFILES['clock-mvdk']
        text=marker+'\n'+ending
        check_result(text,3,profile='clock-mvdk')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field),self.assertRaises(ValueError):
                    check_result(text.replace(field,field.split('=')[0]+'=invalid'),3,profile='clock-mvdk')

    def test_staged_idle_modes_require_clock_wake_and_replay_scope(self):
        marker,ending=EXTRA_PROFILES['idle-modes']
        text=marker+'\n'+ending
        check_result(text,3,profile='idle-modes')
        for field in ('cases=96','modes=3','restore=96','timer_stop=1','real_irq_return=1'):
            with self.subTest(field=field),self.assertRaises(ValueError):
                check_result(text.replace(field,field.split('=')[0]+'=0'),3,profile='idle-modes')
        with self.assertRaises(ValueError):
            check_result('c54x_failure: phase=252230\n'+text,3,profile='idle-modes')

    def test_delay_mod3_requires_no_source_or_implicit_destination_effect(self):
        marker, ending = EXTRA_PROFILES['delay-mod3-edge']
        text = marker + '\n' + ending
        check_result(text, 3, profile='delay-mod3-edge')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0]+'=invalid'),
                                 3, profile='delay-mod3-edge')
        with self.assertRaises(ValueError):
            check_result('c54x_failure: phase=1 case=0\n'+text, 3, profile='delay-mod3-edge')

    def test_circular_mvdk_requires_executed_opcode_attribution(self):
        marker, ending = EXTRA_PROFILES['mvdk-circular']
        text = marker + '\n' + ending
        check_result(text, 3, profile='mvdk-circular')
        for bad in (text.replace(' asserted_encodings=32', ''),
                    text.replace('asserted_encodings=32', 'asserted_encodings=31')):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvdk-circular')

    def test_native_dual_sub_requires_exact_scope_and_completion(self):
        marker, ending = EXTRA_PROFILES['dual-sub-native']
        text = marker + '\n' + ending
        check_result(text, 3, profile='dual-sub-native')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'),
                                 3, profile='dual-sub-native')
        for bad in (marker, text + '\n' + marker):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='dual-sub-native')

    def test_store_observation_requires_retained_address_and_restore_scope(self):
        marker,ending = EXTRA_PROFILES['store-observe']
        text = marker+'\n'+ending
        check_result(text,3,profile='store-observe')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field),self.assertRaises(ValueError):
                    check_result(text.replace(field,field.split('=')[0]+'=invalid'),3,profile='store-observe')

    def test_program_multiply_mod3_requires_full_scope(self):
        marker, ending = EXTRA_PROFILES['pmac-mod3-edge']
        text = marker + '\n' + ending
        check_result(text, 3, profile='pmac-mod3-edge')
        with self.assertRaises(ValueError):
            check_result('c54x_failure: phase=1 case=0\n' + text, 3, profile='pmac-mod3-edge')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'),
                                 3, profile='pmac-mod3-edge')
        for bad in (marker, text + '\n' + marker, text + '\nunexpected'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='pmac-mod3-edge')

    def test_move_repeat_ownership_requires_full_scope(self):
        marker, ending = EXTRA_PROFILES['move-rpt-owner']
        text = marker + '\n' + ending
        check_result(text, 3, profile='move-rpt-owner')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'),
                                 3, profile='move-rpt-owner')
        for bad in (marker, text + '\n' + marker, text + '\nunexpected'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='move-rpt-owner')

    def test_staged_overflow_branch_scope(self):
        marker, ending = EXTRA_PROFILES['ov-branch-pipe']
        text = marker + '\n' + ending
        check_result(text, 3, profile='ov-branch-pipe')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='ov-branch-pipe')
        for bad in (marker, text + '\n' + marker, text + '\nunexpected'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='ov-branch-pipe')

    def test_mapped_dual_alu_scope(self):
        for profile in ('alu-map', 'alu-map-edge', 'alu-map-irq', 'alu-low-bus'):
            marker, ending = EXTRA_PROFILES[profile]
            text = marker + '\n' + ending
            check_result(text, 3, profile=profile)
            for field in marker.split():
                if '=' in field:
                    with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                        check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile=profile)
            for bad in (marker, text + '\n' + marker, text + '\nunexpected'):
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_bios_fallback_cannot_satisfy_profile(self):
        marker, ending = EXTRA_PROFILES['stm-cpl']
        for warning in ('invalid BIOS "stm-cpl", reverting to default', 'INVALID BIOS', 'reverting to default'):
            with self.subTest(warning=warning), self.assertRaises(ValueError):
                check_result(warning + '\n' + marker + '\n' + ending, 3, profile='stm-cpl')

    def test_stm_status_profiles_require_scope_and_replay(self):
        for profile in ('stm-st1', 'stm-st1-edge', 'stm-cpl', 'stm-cmpt'):
            marker, ending = EXTRA_PROFILES[profile]
            text = marker + '\n' + ending
            check_result(text, 3, profile=profile)
            for field in marker.split():
                if '=' in field:
                    with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                        check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile=profile)
            for bad in (marker, text + '\n' + marker, text + '\nunexpected'):
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_retd_delay_repeat_scope(self):
        marker, ending = EXTRA_PROFILES['retd-rpt']
        text = marker + '\n' + ending
        check_result(text, 3, profile='retd-rpt')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='retd-rpt')
        for bad in (marker, text + '\n' + marker, text + '\nunexpected'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='retd-rpt')

    def test_rptbd_delay_repeat_scope(self):
        marker, ending = EXTRA_PROFILES['rptbd-rpt']
        text = marker + '\n' + ending
        check_result(text, 3, profile='rptbd-rpt')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='rptbd-rpt')
        for bad in (marker, text + '\n' + marker, text + '\nunexpected'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='rptbd-rpt')

    def test_rptbd_delay_repeat_exclusion_scope(self):
        marker, ending = EXTRA_PROFILES['rptbd-rpt-edge']
        text = marker + '\n' + ending
        check_result(text, 3, profile='rptbd-rpt-edge')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='rptbd-rpt-edge')

    def test_dagen_age_profiles_require_complete_matrix_and_replay(self):
        for profile in ('st-mvmm', 'ldm-ar-age', 'mapped-load-age'):
            marker, ending = EXTRA_PROFILES[profile]
            text = marker + '\n' + ending + '\n'
            check_result(text, 3, profile=profile)
            for field in marker.split():
                if '=' in field:
                    with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                        check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile=profile)
            with self.assertRaises(ValueError):
                check_result(marker, 3, profile=profile)

    def test_repeated_writa_requires_retained_par_and_memory_count(self):
        for profile in ('rpt-writa', 'rpt-writa16'):
            marker, ending = EXTRA_PROFILES[profile]
            text = marker + '\n' + ending + '\n'
            check_result(text, 3, profile=profile)
            for field in marker.split():
                if '=' in field:
                    with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                        check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile=profile)

    def test_signed_calls_require_all_predicates_and_owned_frames(self):
        marker, ending = EXTRA_PROFILES['signed-call']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='signed-call')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='signed-call')

    def test_bcd_arithmetic_requires_predicate_and_operand_retention(self):
        for profile in ('bcd-arithmetic', 'bcd-arith-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            text = marker + '\n' + ending + '\n'
            check_result(text, 3, profile=profile)
            for field in marker.split():
                if '=' in field:
                    with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                        check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile=profile)

    def test_circular_mar_requires_legal_domain_and_independent_oracle(self):
        marker, ending = EXTRA_PROFILES['mar-circular']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='mar-circular')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='mar-circular')

    def test_tc_returns_require_producer_and_retention_evidence(self):
        marker, ending = EXTRA_PROFILES['tc-return']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='tc-return')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='tc-return')

    def test_bitt_requires_retained_selector_and_single_read(self):
        for profile in ('bitt', 'bitt-retd'):
            marker, ending = EXTRA_PROFILES[profile]
            text = marker + '\n' + ending + '\n'
            check_result(text, 3, profile=profile)
            for field in marker.split():
                if '=' in field:
                    with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                        check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile=profile)

    def test_repeated_stores_require_live_source_and_each_write(self):
        for profile in ('rpt-store', 'rpt-store-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            text = marker + '\n' + ending + '\n'
            check_result(text, 3, profile=profile)
            for field in marker.split():
                if '=' in field:
                    with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                        check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile=profile)

    def test_rptz_requires_accumulator_clear_and_count_retention(self):
        marker, ending = EXTRA_PROFILES['rptz']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='rptz')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='rptz')

    def test_rotate_requires_carry_guard_and_restore_evidence(self):
        marker, ending = EXTRA_PROFILES['rotate']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='rotate')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='rotate')

    def test_mapped_ar_addm_requires_flags_retention_and_legal_consumers(self):
        for profile in ('addm-ar', 'addm-ar-load'):
            marker, ending = EXTRA_PROFILES[profile]
            text = marker + '\n' + ending + '\n'
            check_result(text, 3, profile=profile)
            for field in marker.split():
                if '=' not in field:
                    continue
                with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile=profile)

    def test_literal_guard_requires_all_aliases_and_block_restoration(self):
        marker, ending = EXTRA_PROFILES['literal-guard']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='literal-guard')
        for field in ('cases=54', 'restore=54', 'literals=7cf8,7df8,7ff8',
                      'block_counts=0,1,2', 'pages=2', 'snapshots=3',
                      'instruction=ST_absolute', 'writes=count+1', 'transfer_admission=0'):
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='literal-guard')

    def test_mvpd_controls_require_all_existing_banks(self):
        markers, ending = EXTRA_PROFILES['mvpd-controls']
        text = '\n'.join((*markers, ending))
        check_result(text, 3, profile='mvpd-controls')
        for marker in markers:
            with self.subTest(marker=marker), self.assertRaises(ValueError):
                check_result(text.replace(marker, ''), 3, profile='mvpd-controls')

    def test_extended_mvpd_exclusions_require_zero_address_and_bus_effects(self):
        marker, ending = EXTRA_PROFILES['mvpd-ext-edge']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='mvpd-ext-edge')
        for field in marker.split():
            if '=' not in field:
                continue
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='mvpd-ext-edge')

    def test_extended_mvpd_requires_address_mapping_and_replay_scope(self):
        marker, ending = EXTRA_PROFILES['mvpd-extended']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='mvpd-extended')
        for field in marker.split():
            if '=' not in field:
                continue
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='mvpd-extended')

    def test_st1_rules_require_primary_constraints_and_unverified_clear_label(self):
        for profile in ('st1-rules', 'st1-bits', 'stlm-st1', 'stlm-st1-edge', 'stlm-st1-irq', 'stlm-cpl', 'stlm-cmpt', 'repeat-stlm'):
            marker, ending = EXTRA_PROFILES[profile]
            text = marker + '\n' + ending + '\n'
            check_result(text, 3, profile=profile)
            for field in marker.split():
                if '=' in field:
                    with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                        check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile=profile)

    def test_staged_loader_profiles_require_complete_qualification_markers(self):
        for profile in ('sat', 'store-circular', 'tc-call', 'cmpm-acc', 'rpt-mvkd', 'rpt-mvkd-edge', 'stm-acc', 'st-t-long', 'st-t-long-edge', 'st-t-branch', 'portr-circular', 'portr-block', 'mvdm-acc', 'mvdm-acc-edge', 'stack-page0', 'stack-page0-edge', 'stld', 'stld-edge', 'unsigned-map', 'unsigned-edge', 'cmpm-map', 'cmpm-map-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            text = marker + '\n' + ending + '\n'
            check_result(text, 3, profile=profile)
            for field in marker.split():
                if '=' in field:
                    with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                        check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile=profile)

    def test_poll_interrupt_requires_complete_qualification_marker(self):
        marker, ending = EXTRA_PROFILES['poll-interrupt']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='poll-interrupt')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='poll-interrupt')

    def test_brc_addm_requires_complete_qualification_markers(self):
        for profile in ('addm-brc', 'addm-brc-loop'):
            marker, ending = EXTRA_PROFILES[profile]
            text = marker + '\n' + ending + '\n'
            check_result(text, 3, profile=profile)
            for field in marker.split():
                if '=' in field:
                    with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                        check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile=profile)

    def test_rcd_requires_complete_qualification_marker(self):
        marker, ending = EXTRA_PROFILES['rcd']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='rcd')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='rcd')

    def test_block_tc_delay_requires_complete_qualification_marker(self):
        marker, ending = EXTRA_PROFILES['rptb-tc-delay']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='rptb-tc-delay')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='rptb-tc-delay')

    def test_shifted_peripheral_requires_retention_and_honest_boundary_labels(self):
        for profile in ('shift-periph', 'shift-bsp', 'shift-per-edge', 'shift-unowned'):
            marker, ending = EXTRA_PROFILES[profile]
            text = marker + '\n' + ending + '\n'
            check_result(text, 3, profile=profile)
            for field in marker.split():
                if '=' in field:
                    with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                        check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile=profile)

    def test_reada_brc_requires_address_width_retention_and_exclusions(self):
        for profile in ('reada-brc', 'reada-brc16', 'reada-brc-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            text = marker + '\n' + ending + '\n'
            check_result(text, 3, profile=profile)
            for field in marker.split():
                if '=' in field:
                    with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                        check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile=profile)

    def test_forward_block_exit_requires_braf_lifetime_and_replay(self):
        marker, ending = EXTRA_PROFILES['rptb-exit']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='rptb-exit')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='rptb-exit')

    def test_bsp_stlm_requires_publication_scope_and_exclusions(self):
        for profile in ('stlm-bsp', 'stlm-bsp-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            text = marker + '\n' + ending + '\n'
            check_result(text, 3, profile=profile)
            for field in marker.split():
                if '=' in field:
                    with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                        check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile=profile)

    def test_status_bitf_requires_retained_sampling_and_replay(self):
        marker, ending = EXTRA_PROFILES['status-bitf']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='status-bitf')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='status-bitf')

    def test_serial_stm_requires_retained_publication_without_endpoint_claim(self):
        marker, ending = EXTRA_PROFILES['serial-stm']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='serial-stm')
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='serial-stm')

    def test_clkmd_stm_does_not_promote_a_physical_pll_clock(self):
        marker, ending = EXTRA_PROFILES['clock-stm']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='clock-stm')
        for field in ('cases=12', 'restore=12', 'writes=1', 'publish_clock=8',
                      'pll_reference=unconfigured', 'physical_clock_claim=0'):
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile='clock-stm')

    def test_standard_swwsr_scope_does_not_promote_c5416_controls(self):
        for profile in ('std-swwsr', 'std-stm', 'std-io', 'std-io-atomic'):
            marker, ending = EXTRA_PROFILES[profile]
            text = marker + '\n' + ending + '\n'
            check_result(text, 3, profile=profile)
            for field in marker.split():
                if '=' not in field:
                    continue
                with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                    check_result(text.replace(field, field.split('=')[0] + '=invalid'), 3, profile=profile)

    def test_staged_cold_requires_exact_reset_frontier(self):
        marker, ending = EXTRA_PROFILES['cold-pipeline']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='cold-pipeline')
        for field in ('fetch=000f00', 'pc=0f01', 'sp=0000', 'pmst=ffa8', 'cycles=12'):
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(text.replace(field, field.split('=')[0] + '=ffff'), 3, profile='cold-pipeline')

    def test_repeated_mvdp_interrupt_scope_is_required(self):
        marker, ending = EXTRA_PROFILES['rpt-mvdp-irq']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='rpt-mvdp-irq')
        for field in ('cases=288', 'restore=288', 'sources=IRQ,NMI',
                      'counts=0,1,7,255', 'count_forms=2', 'pages=2',
                      'arrivals=3', 'snapshots=3', 'deferred_until_final_write=1',
                      'balanced_stack=1', 'duplicate_transfers=0'):
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(text.replace(field, field.split('=')[0] + '=99'), 3, profile='rpt-mvdp-irq')

    def test_repeated_mvdp_requires_count_plus_one_and_replay(self):
        marker, ending = EXTRA_PROFILES['rpt-mvdp']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='rpt-mvdp')
        for field in ('cases=9600', 'restore=9600', 'forms=5', 'registers=8',
                      'counts=0,1,2,7,255', 'count_forms=2', 'pages=2', 'snapshots=3',
                      'destinations=ordinary,wrap', 'ovly=2', 'stream_clocks=1',
                      'source_reads=count+1', 'program_writes=count+1', 'silicon_phase_claim=0'):
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(text.replace(field, field.split('=')[0] + '=99'), 3, profile='rpt-mvdp')

    def test_mvdp_pair_requires_both_transfers_and_replay(self):
        marker, ending = EXTRA_PROFILES['mvdp-pair']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='mvdp-pair')
        for field in ('cases=12', 'restore=12', 'pages=2', 'snapshots=6',
                      'source_reads=2', 'program_writes=2'):
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(text.replace(field, field.split('=')[0] + '=99'), 3, profile='mvdp-pair')

    def test_mvdp_requires_complete_standalone_scope(self):
        marker, ending = EXTRA_PROFILES['mvdp']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='mvdp')
        for field in ('cases=336', 'restore=336', 'forms=7', 'registers=8', 'pages=2', 'snapshots=3',
                      'source_reads=1', 'program_writes=1',
                      'repeat=unvalidated', 'silicon_phase_claim=0'):
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(text.replace(field, field.split('=')[0] + '=99'), 3, profile='mvdp')
        with self.assertRaises(ValueError):
            check_result(marker + '\n' + text, 3, profile='mvdp')

    def test_repeated_transfer_rejection_requires_full_scope(self):
        marker, ending = EXTRA_PROFILES['rpt-xfer-edge']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='rpt-xfer-edge')
        for field in ('cases=9600', 'restore=9600', 'opcode_classes=6', 'forbidden_modes=25',
                      'repeat_counts=0,2', 'count_forms=short,long', 'cmpt=2', 'pages=2',
                      'frontends=2', 'snapshots=2', 'data_reads=0', 'data_writes=0',
                      'pointer_changes=0', 'stack_changes=0', 'silicon_trap_claim=0'):
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(text.replace(field, field.split('=')[0] + '=99'), 3, profile='rpt-xfer-edge')
        with self.assertRaises(ValueError):
            check_result(marker + '\n' + text, 3, profile='rpt-xfer-edge')

    def test_mmr_long_rejection_requires_full_domain_and_no_effects(self):
        marker, ending = EXTRA_PROFILES['mmr-long-edge']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='mmr-long-edge')
        for field in ('cases=4608', 'restore=4608', 'opcode_classes=9', 'forbidden_modes=25',
                      'reserved_modes=7', 'cmpt=2', 'pages=2', 'frontends=2', 'snapshots=2',
                      'data_reads=0', 'data_writes=0', 'pointer_changes=0', 'stack_changes=0',
                      'silicon_trap_claim=0'):
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(text.replace(field, field.split('=')[0] + '=99'), 3, profile='mmr-long-edge')
        with self.assertRaises(ValueError):
            check_result(marker + '\n' + text, 3, profile='mmr-long-edge')

    def test_parallel_sub_fraction_scope_and_replay(self):
        marker, ending = EXTRA_PROFILES['parallel-sub']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='parallel-sub')
        for before, after in (('restore=128', 'restore=0'), ('source_dest_pairs=4', 'source_dest_pairs=1'),
                              ('fractional_patterns=8', 'fractional_patterns=1'),
                              ('ordinary_ram=1', 'ordinary_ram=0')):
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_result(text.replace(before, after), 3, profile='parallel-sub')

    def test_xc_condition_profiles_require_both_frontends_and_replay(self):
        for name in ('xc-status', 'xc-latency', 'xc-barrier'):
            markers, ending = EXTRA_PROFILES[name]
            text = '\n'.join((*markers, ending)) + '\n'
            check_result(text, 3, profile=name)
            for marker in markers:
                with self.subTest(profile=name, marker=marker), self.assertRaises(ValueError):
                    check_result(text.replace(marker, ''), 3, profile=name)
            for old, new in (('restore=64', 'restore=0'), ('restore=384', 'restore=0'),
                             ('restore=12', 'restore=0')):
                if old in text:
                    with self.subTest(profile=name, mutation=old), self.assertRaises(ValueError):
                        check_result(text.replace(old, new), 3, profile=name)

    def test_atomic_xorm_scope(self):
        markers, ending = EXTRA_PROFILES['xorm']
        text = '\n'.join((*markers, ending)) + '\n'
        check_result(text, 3, profile='xorm')
        for marker in markers:
            with self.subTest(missing=marker), self.assertRaises(ValueError):
                check_result(text.replace(marker + '\n', ''), 3, profile='xorm')
            with self.subTest(duplicate=marker), self.assertRaises(ValueError):
                check_result(text.replace(marker, marker + '\n' + marker), 3, profile='xorm')
        for before, after in (('restore=1024', 'restore=0'), ('reads=1', 'reads=2'),
                              ('flags_preserved=1', 'flags_preserved=0'), ('staged_acceptance=0', 'staged_acceptance=1'),
                              ('data_reads=0', 'data_reads=1'), ('pointer_changes=0', 'pointer_changes=1')):
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_result(text.replace(before, after), 3, profile='xorm')

    def test_conditional_register_store_scope(self):
        markers, ending = EXTRA_PROFILES['cond-reg-store']
        text = '\n'.join((*markers, ending)) + '\n'
        check_result(text, 3, profile='cond-reg-store')
        for before, after in (('restore=18432', 'restore=0'),
                              ('false_read_writeback=1', 'false_read_writeback=0'),
                              ('circular_update=1', 'circular_update=0')):
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_result(text.replace(before, after), 3, profile='cond-reg-store')
        for marker in markers:
            with self.subTest(marker=marker), self.assertRaises(ValueError):
                check_result(text.replace(marker, ''), 3, profile='cond-reg-store')

    def test_atomic_return_scope(self):
        marker, ending = EXTRA_PROFILES['atomic-ret']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='atomic-ret')
        for before, after in (('restore=144', 'restore=0'), ('untaken_reads=0', 'untaken_reads=1'), ('stack_writes=0', 'stack_writes=1')):
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_result(text.replace(before, after), 3, profile='atomic-ret')

    def test_saccd_requires_both_existing_banks(self):
        markers, ending = EXTRA_PROFILES['saccd']
        text = '\n'.join((*markers, ending)) + '\n'
        check_result(text, 3, profile='saccd')
        for marker in markers:
            with self.subTest(marker=marker), self.assertRaises(ValueError):
                check_result(text.replace(marker, ''), 3, profile='saccd')

    def test_overflow_branch_scope_and_replay(self):
        marker, ending = EXTRA_PROFILES['ov-branch']
        text = marker + '\n' + ending + '\n'
        check_result(text, 3, profile='ov-branch')
        for before, after in (('restore=64', 'restore=0'),
                              ('selected_flag_clear=1', 'selected_flag_clear=0'),
                              ('delay_condition_retained=1', 'delay_condition_retained=0')):
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_result(text.replace(before, after), 3, profile='ov-branch')
        with self.assertRaises(ValueError):
            check_result(marker + '\n' + text, 3, profile='ov-branch')

    def test_stlm_t_requires_both_register_banks_and_replay(self):
        markers, ending = EXTRA_PROFILES['stlm-t']
        output = '\n'.join((*markers, ending)) + '\n'
        check_result(output, 3, profile='stlm-t')
        for marker in markers:
            for changed in (output.replace(marker, ''), marker + '\n' + output):
                with self.subTest(marker=marker), self.assertRaises(ValueError):
                    check_result(changed, 3, profile='stlm-t')
        with self.assertRaises(ValueError):
            check_result(output.replace('restore=12288', 'restore=0'), 3, profile='stlm-t')

    def test_long_store_move_profile_requires_every_shared_address_consumer(self):
        markers, ending = EXTRA_PROFILES['long-mvkd']
        self.assertEqual(len(markers), 7)
        output = '\n'.join((*markers, ending)) + '\n'
        check_result(output, 3, profile='long-mvkd')
        for marker in markers:
            self.assertIn(marker, REQUIRED)
            for changed in (output.replace(marker, ''), marker + '\n' + output):
                with self.subTest(marker=marker), self.assertRaises(ValueError):
                    check_result(changed, 3, profile='long-mvkd')
        for before, after in (('restore=6144', 'restore=0'),
                              ('restore=172032', 'restore=0'),
                              ('source_alias=1', 'source_alias=0'),
                              ('zero_gap_cmpr=1', 'zero_gap_cmpr=0'),
                              ('reads_each=1', 'reads_each=2')):
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_result(output.replace(before, after), 3, profile='long-mvkd')

    def test_paired_memory_profile_requires_store_load_and_domain_guard_evidence(self):
        markers, ending = EXTRA_PROFILES['indirect-dld']
        self.assertEqual(len(markers), 5)
        output = '\n'.join((*markers, ending)) + '\n'
        check_result(output, 3, profile='indirect-dld')
        for marker in markers:
            for changed in (output.replace(marker, ''), marker+'\n'+output):
                with self.subTest(marker=marker), self.assertRaises(ValueError):
                    check_result(changed, 3, profile='indirect-dld')
        for before, after in (('nonzero_bk_domain=SPRU131G_5.5.3.4', 'nonzero_bk_domain=unvalidated'),
                              ('cases=23040', 'cases=24576'), ('restore=3072', 'restore=0')):
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_result(output.replace(before, after), 3, profile='indirect-dld')

    def test_mmr_push_mod3_scope(self):
        markers, ending = EXTRA_PROFILES['stack-mod3-edge']
        output = '\n'.join((*markers, ending)) + '\n'
        check_result(output, 3, profile='stack-mod3-edge')
        for marker in markers:
            with self.subTest(marker=marker), self.assertRaises(ValueError):
                check_result(output.replace(marker, ''), 3, profile='stack-mod3-edge')
        for old, new in (('restore=512', 'restore=0'), ('opcode_classes=4', 'opcode_classes=2'),
                         ('frontends=2', 'frontends=1'), ('stack_changes=0', 'stack_changes=1'),
                         ('pointer_changes=0', 'pointer_changes=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='stack-mod3-edge')

    def test_multiply_load_mod3_exclusion_scope(self):
        marker, ending = EXTRA_PROFILES['mul-mod3-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mul-mod3-edge')
        for old, new in (('restore=4352', 'restore=0'), ('opcode_classes=34', 'opcode_classes=2'),
                         ('frontends=2', 'frontends=1'), ('data_reads=0', 'data_reads=1'),
                         ('asm_changes=0', 'asm_changes=1'), ('rc_changes=0', 'rc_changes=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='mul-mod3-edge')

    def test_multiply_t_source_scope(self):
        markers, ending = EXTRA_PROFILES['mul-t-sources']
        output = '\n'.join((*markers, ending)) + '\n'
        check_result(output, 3, profile='mul-t-sources')
        for marker in markers:
            for bad in (output.replace(marker, ''), marker + '\n' + output):
                with self.subTest(marker=marker), self.assertRaises(ValueError):
                    check_result(bad, 3, profile='mul-t-sources')
        for old, new in (('restore=52224', 'restore=0'), ('ram_cases=27648', 'ram_cases=30720'),
                         ('mapped_cases=24576', 'mapped_cases=0')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='mul-t-sources')

    def test_arithmetic_mod3_exclusion_scope(self):
        marker, ending = EXTRA_PROFILES['arith-mod3-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='arith-mod3-edge')
        for old, new in (('restore=35840', 'restore=0'), ('scalar_classes=24', 'scalar_classes=2'),
                         ('variable_selectors=8', 'variable_selectors=2'), ('shifts=32', 'shifts=1'),
                         ('frontends=2', 'frontends=1'), ('data_reads=0', 'data_reads=1'),
                         ('pointer_changes=0', 'pointer_changes=1'), ('alu_changes=0', 'alu_changes=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='arith-mod3-edge')

    def test_scalar_arithmetic_encoding_scope(self):
        for profile, count in (('memory-alu', 104448), ('adds', 52224), ('subs', 52224),
                               ('memory-shift16', 208896)):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for old, new in ((f'restore={count}', 'restore=0'), ('forms=17', 'forms=18')):
                with self.subTest(profile=profile, mutation=old), self.assertRaises(ValueError):
                    check_result(output.replace(old, new), 3, profile=profile)

    def test_mapped_bitfield_scope(self):
        marker, ending = EXTRA_PROFILES['mapped-bitf']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mapped-bitf')
        for old, new in (('restore=13824', 'restore=0'), ('reads_each=1', 'reads_each=2'),
                         ('data_writes=0', 'data_writes=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='mapped-bitf')

    def test_bitfield_mod3_exclusion_scope(self):
        marker, ending = EXTRA_PROFILES['bitf-mod3-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='bitf-mod3-edge')
        for old, new in (('restore=512', 'restore=0'), ('operations=CMPM,BITF', 'operations=BITF'),
                         ('initial_tc=2', 'initial_tc=1'), ('frontends=2', 'frontends=1'),
                         ('data_reads=0', 'data_reads=1'), ('data_writes=0', 'data_writes=1'),
                         ('pointer_changes=0', 'pointer_changes=1'), ('tc_changes=0', 'tc_changes=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='bitf-mod3-edge')

    def test_smem_test_compare_scope(self):
        for profile in ('bitf-smem', 'cmpm-smem'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for old, new in (('restore=52224', 'restore=0'), ('forms=17', 'forms=18'),
                             ('duplicate_reads=0', 'duplicate_reads=1') if profile == 'bitf-smem' else
                             ('source_reads=1', 'source_reads=2')):
                with self.subTest(profile=profile, mutation=old), self.assertRaises(ValueError):
                    check_result(output.replace(old, new), 3, profile=profile)

    def test_load_mod3_exclusion_scope(self):
        marker, ending = EXTRA_PROFILES['load-mod3-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='load-mod3-edge')
        for old, new in (('restore=9728', 'restore=0'), ('families=7', 'families=1'),
                         ('variable_shifts=32', 'variable_shifts=1'),
                         ('frontends=2', 'frontends=1'), ('data_reads=0', 'data_reads=1'),
                         ('data_writes=0', 'data_writes=1'), ('pointer_changes=0', 'pointer_changes=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='load-mod3-edge')

    def test_indirect_load_scope(self):
        marker, ending = EXTRA_PROFILES['indirect-load']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='indirect-load')
        with self.assertRaises(ValueError):
            check_result(output.replace('captured_address_restore=6', 'captured_address_restore=0'),
                         3, profile='indirect-load')

    def test_subc_mod3_exclusion_scope(self):
        marker, ending = EXTRA_PROFILES['subc-mod3-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='subc-mod3-edge')
        for old, new in (('restore=512', 'restore=0'), ('repeat=none,short', 'repeat=none'),
                         ('frontends=2', 'frontends=1'), ('data_reads=0', 'data_reads=1'),
                         ('data_writes=0', 'data_writes=1'), ('pointer_changes=0', 'pointer_changes=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='subc-mod3-edge')

    def test_repeated_subc_store_scope(self):
        markers, ending = EXTRA_PROFILES['rpt-subc-st']
        output = '\n'.join((*markers, ending)) + '\n'
        check_result(output, 3, profile='rpt-subc-st')
        for marker in markers:
            with self.assertRaises(ValueError):
                check_result(output.replace(marker, ''), 3, profile='rpt-subc-st')
        with self.assertRaises(ValueError):
            check_result(output.replace('forms=5', 'forms=6'), 3, profile='rpt-subc-st')

    def test_memory_multiply_mod3_exclusion_scope(self):
        marker, ending = EXTRA_PROFILES['mpy-mod3-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mpy-mod3-edge')
        for old, new in (('restore=256', 'restore=0'), ('frontends=2', 'frontends=1'),
                         ('data_reads=0', 'data_reads=1'), ('data_writes=0', 'data_writes=1'),
                         ('pointer_changes=0', 'pointer_changes=1'), ('silicon_trap_claim=0', 'silicon_trap_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='mpy-mod3-edge')

    def test_extended_push_scope(self):
        for profile, restores in (('pshd-long', 99696), ('pshd-long-map', 3696)):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for old, new in ((f'restore={restores}', 'restore=0'), ('literal_fetches=1', 'literal_fetches=2'),
                             ('source_reads=1', 'source_reads=2'), ('stack_writes=1', 'stack_writes=2'),
                             ('pointer_updates=once', 'pointer_updates=twice'),
                             ('physical_phase_claim=0', 'physical_phase_claim=1')):
                with self.subTest(profile=profile, mutation=old), self.assertRaises(ValueError):
                    check_result(output.replace(old, new), 3, profile=profile)
            for bad in (marker + '\n' + output, output + 'late output\n'):
                with self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_extended_push_exclusion_scope(self):
        marker, ending = EXTRA_PROFILES['pshd-long-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='pshd-long-edge')
        for old, new in (('restore=64', 'restore=0'), ('data_reads=0', 'data_reads=1'),
                         ('data_writes=0', 'data_writes=1'), ('stack_changes=0', 'stack_changes=1'),
                         ('pointer_changes=0', 'pointer_changes=1'), ('silicon_trap_claim=0', 'silicon_trap_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='pshd-long-edge')

    def test_fixture_bios_metadata_fits_mame_encoding(self):
        source = (Path(__file__).resolve().parents[1] / 'driver' / 'tms320c54x_test.cpp').read_text()
        blocks = re.findall(r'ROM_START\(([^)]+)\)(.*?)ROM_END', source, re.S)
        self.assertTrue(blocks)
        for driver, body in blocks:
            entries = re.findall(r'ROM_SYSTEM_BIOS\((\d+),\s*"([^"]+)"', body)
            indices = [int(index) for index, _ in entries]
            names = [name for _, name in entries]
            with self.subTest(driver=driver):
                self.assertEqual(len(indices), len(set(indices)))
                self.assertEqual(len(names), len(set(names)))
                # MAME ROM_BIOS encodes (index + 1) in eight bits; 255 wraps to zero.
                self.assertTrue(all(0 <= index < 255 for index in indices))
                self.assertTrue(all(0 < len(name) <= 16 for name in names))

    def test_stlm_pmst_scope(self):
        marker, ending = EXTRA_PROFILES['stlm-pmst']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='stlm-pmst')
        for old, new in (('restore=38912', 'restore=0'), ('writes=1', 'writes=2'),
                         ('source_sample=Execute', 'source_sample=Decode'),
                         ('prefetched_mapping=retained', 'prefetched_mapping=remapped'),
                         ('next_mapping=new', 'next_mapping=old'), ('vector_mapping=Prefetch', 'vector_mapping=Execute'),
                         ('balanced_frames=1', 'balanced_frames=0'), ('physical_phase_claim=0', 'physical_phase_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='stlm-pmst')
        for bad in (marker + '\n' + output, output + 'late output\n'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='stlm-pmst')

    def test_stlm_pmst_exclusion_scope(self):
        marker, ending = EXTRA_PROFILES['stlm-pmst-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='stlm-pmst-edge')
        for old, new in (('restore=32', 'restore=0'), ('data_reads=0', 'data_reads=1'),
                         ('data_writes=0', 'data_writes=1'), ('control_changes=0', 'control_changes=1'),
                         ('pointer_changes=0', 'pointer_changes=1'), ('silicon_trap_claim=0', 'silicon_trap_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='stlm-pmst-edge')

    def test_repeated_accumulator_load_block_scope(self):
        marker, ending = EXTRA_PROFILES['rpt-ld-block']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rpt-ld-block')
        for old, new in (('restore=2048', 'restore=0'), ('each_iteration=1', 'each_iteration=0'),
                         ('inner_continuations=outer+1', 'inner_continuations=outer'),
                         ('outer_counter_preserved=1', 'outer_counter_preserved=0'),
                         ('physical_phase_claim=0', 'physical_phase_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='rpt-ld-block')

    def test_repeated_accumulator_load_scope(self):
        marker, ending = EXTRA_PROFILES['rpt-ld']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rpt-ld')
        for old, new in (('restore=200704', 'restore=0'), ('block_cases=2048', 'block_cases=0'),
                         ('each_iteration=1', 'each_iteration=0'),
                         ('retained_count=1', 'retained_count=0'), ('retained_opcode=1', 'retained_opcode=0'),
                         ('fresh_continuation=1', 'fresh_continuation=0'), ('balanced_frames=1', 'balanced_frames=0'),
                         ('physical_phase_claim=0', 'physical_phase_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='rpt-ld')
        for bad in (marker + '\n' + output, output + 'late output\n'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='rpt-ld')

    def test_mapped_logic_scope(self):
        for profile in ('logic-map', 'logic-map-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            mutations = (('restore=1152', 'restore=0'), ('reads=0', 'reads=1'),
                         ('writes=0', 'writes=1'), ('pointer_changes=0', 'pointer_changes=1')) if profile.endswith('edge') else (
                         ('restore=110592', 'restore=0'), ('zero_extend=1', 'zero_extend=0'),
                         ('source_sample=Read', 'source_sample=Execute'),
                         ('accumulator_sample=Execute', 'accumulator_sample=Read'),
                         ('flags_preserved=1', 'flags_preserved=0'))
            for old, new in mutations + (('reset_before_restore=1', 'reset_before_restore=0'),):
                with self.subTest(profile=profile, mutation=old), self.assertRaises(ValueError):
                    check_result(output.replace(old, new), 3, profile=profile)
            for bad in (marker + '\n' + output, output + 'late output\n'):
                with self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_mapped_shift16_scope(self):
        for profile in ('shift16-map', 'shift16-map-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            mutations = (('restore=7680', 'restore=0'), ('reads=0', 'reads=1'),
                         ('writes=0', 'writes=1'), ('pointer_changes=0', 'pointer_changes=1')) if profile.endswith('edge') else (
                         ('restore=147456', 'restore=0'), ('mapped_sources=16', 'mapped_sources=0'),
                         ('source_sample=Read', 'source_sample=Execute'),
                         ('accumulator_sample=Execute', 'accumulator_sample=Read'),
                         ('partial_carry=1', 'partial_carry=0'), ('sticky_overflow=1', 'sticky_overflow=0'))
            for old, new in mutations + (('reset_before_restore=1', 'reset_before_restore=0'),):
                with self.subTest(profile=profile, mutation=old), self.assertRaises(ValueError):
                    check_result(output.replace(old, new), 3, profile=profile)
            for bad in (marker + '\n' + output, output + 'late output\n'):
                with self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_staged_conditional_store_scope(self):
        for profile in ('cond-store', 'cond-store-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            mutations = (('restore=1024', 'restore=0'), ('reads=0', 'reads=1'),
                         ('writes=0', 'writes=1'), ('pointer_changes=0', 'pointer_changes=1')) if profile.endswith('edge') else (
                         ('restore=442368', 'restore=0'), ('false_read_writeback=1', 'false_read_writeback=0'),
                         ('read_retained=1', 'read_retained=0'), ('saturation_ignored=1', 'saturation_ignored=0'),
                         ('predicate_source_sample=Execute', 'predicate_source_sample=Read'))
            for old, new in mutations + (('reset_before_restore=1', 'reset_before_restore=0'),):
                with self.subTest(profile=profile, mutation=old), self.assertRaises(ValueError):
                    check_result(output.replace(old, new), 3, profile=profile)
            for bad in (marker + '\n' + output, output + 'late output\n'):
                with self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_repeated_indexed_mvdk_scope(self):
        for profile in ('mvdk-index-rpt', 'mvdk-irpt-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            mutations = (('restore=480', 'restore=0'), ('reads=0', 'reads=1'),
                         ('writes=0', 'writes=1')) if profile.endswith('edge') else (
                         ('restore=8928', 'restore=0'), ('source_geometries_skipped=288', 'source_geometries_skipped=0'),
                         ('ordered_samples=1', 'ordered_samples=0'),
                         ('destination_literal_once=1', 'destination_literal_once=0'))
            for old, new in mutations + (('reset_before_restore=1', 'reset_before_restore=0'),):
                with self.subTest(profile=profile, mutation=old), self.assertRaises(ValueError):
                    check_result(output.replace(old, new), 3, profile=profile)
            for bad in (marker + '\n' + output, output + 'late output\n'):
                with self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_indexed_mvdk_scope(self):
        for profile in ('mvdk-index', 'mvdk-index-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            mutations = (('restore=19712', 'restore=0'), ('reads=0', 'reads=1'),
                         ('writes=0', 'writes=1')) if profile.endswith('edge') else (
                         ('restore=294336', 'restore=0'), ('raw_words=1', 'raw_words=0'),
                         ('alias_order=sample,modify,publish', 'alias_order=modify,sample,publish'),
                         ('mapped_sources=18', 'mapped_sources=0'))
            for old, new in mutations + (('reset_before_restore=1', 'reset_before_restore=0'),):
                with self.subTest(profile=profile, mutation=old), self.assertRaises(ValueError):
                    check_result(output.replace(old, new), 3, profile=profile)
            for bad in (marker + '\n' + output, output + 'late output\n'):
                with self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_parallel_arithmetic_scope(self):
        for profile in ('st-par-alu', 'st-par-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            mutations = (('restore=128', 'restore=0'), ('reads=0', 'reads=1'),
                         ('writes=0', 'writes=1'), ('pointer_changes=0', 'pointer_changes=1')) if profile.endswith('edge') else (
                         ('restore=540672', 'restore=0'), ('read_retained=1', 'read_retained=0'),
                         ('old_source_store=1', 'old_source_store=0'),
                         ('subtraction=memory_minus_other', 'subtraction=other_minus_memory'),
                         ('carry_bit=32', 'carry_bit=40'), ('sticky_overflow=1', 'sticky_overflow=0'))
            for old, new in mutations + (('reset_before_restore=1', 'reset_before_restore=0'),):
                with self.subTest(profile=profile, mutation=old), self.assertRaises(ValueError):
                    check_result(output.replace(old, new), 3, profile=profile)
            for bad in (marker + '\n' + output, output + 'late output\n'):
                with self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_circular_asm_store_scope(self):
        marker, ending = EXTRA_PROFILES['asm-circular']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='asm-circular')
        for old, new in (('restore=393216', 'restore=0'), ('bk_sample=Decode', 'bk_sample=Execute'),
                         ('address_asm_sample=Read', 'address_asm_sample=Execute'),
                         ('data_sample=Execute', 'data_sample=Read'),
                         ('destination_reads=0', 'destination_reads=1'),
                         ('low_page_rejected=1', 'low_page_rejected=0'),
                         ('reset_before_restore=1', 'reset_before_restore=0')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='asm-circular')
        for bad in (marker + '\n' + output, output + 'late output\n'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='asm-circular')

    def test_repeated_stld_scope(self):
        for profile in ('stld-repeat', 'stld-rpt-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            mutations = (('restore=96', 'restore=0'), ('reads=0', 'reads=1'),
                         ('writes=0', 'writes=1'), ('pointer_changes=0', 'pointer_changes=1')) if profile.endswith('edge') else (
                         ('restore=9216', 'restore=0'), ('source_retained=1', 'source_retained=0'),
                         ('accumulator_feedback=1', 'accumulator_feedback=0'),
                         ('t_publish=Read', 't_publish=Execute'), ('opcode_fetches=1', 'opcode_fetches=2'),
                         ('reset_before_restore=1', 'reset_before_restore=0'))
            for old, new in mutations:
                with self.subTest(profile=profile, mutation=old), self.assertRaises(ValueError):
                    check_result(output.replace(old, new), 3, profile=profile)
            for bad in (marker + '\n' + output, output + 'late output\n'):
                with self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_indirect_stm_scope(self):
        for profile in ('stm-ind', 'stm-ind-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for old, new in (('restore=1536', 'restore=0'), ('reads=0', 'reads=1'),
                             ('writes=0' if profile.endswith('edge') else 'writes=1',
                              'writes=1' if profile.endswith('edge') else 'writes=0'),
                             ('retained=1', 'retained=0'),
                             ('reset_before_restore=1', 'reset_before_restore=0'),
                             ('silicon_phase_claim=0', 'silicon_phase_claim=1')):
                with self.subTest(profile=profile, mutation=old), self.assertRaises(ValueError):
                    check_result(output.replace(old, new), 3, profile=profile)
            for bad in (marker + '\n' + output, output + 'late output\n'):
                with self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_bd_delay_repeat_scope(self):
        marker, ending = EXTRA_PROFILES['bd-rpt']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='bd-rpt')
        for old, new in (('restore=5760', 'restore=0'), ('ordered_copies=1', 'ordered_copies=0'),
                         ('target_retained=1', 'target_retained=0'), ('fresh_target_repeat=1', 'fresh_target_repeat=0'),
                         ('opcode_poison=staged_only', 'opcode_poison=none'),
                         ('reset_before_restore=1', 'reset_before_restore=0'),
                         ('physical_phase_claim=0', 'physical_phase_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='bd-rpt')
        for bad in (marker + '\n' + output, output + 'late output\n'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='bd-rpt')

    def test_bd_delay_repeat_exclusions(self):
        marker, ending = EXTRA_PROFILES['bd-rpt-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='bd-rpt-edge')
        for old, new in (('restore=32', 'restore=0'), ('reads=RETD_stack_only', 'reads=all'),
                         ('writes=0', 'writes=1'), ('ar_changes=0', 'ar_changes=1'),
                         ('counter_changes=0', 'counter_changes=1'), ('silicon_trap_claim=0', 'silicon_trap_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='bd-rpt-edge')

    def test_repeated_dual_mac_scope(self):
        marker, ending = EXTRA_PROFILES['mac-rpt']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mac-rpt')
        for old, new in (('restore=98304', 'restore=0'), ('per_clock_accumulator=1', 'per_clock_accumulator=0'),
                         ('paired_reads=1', 'paired_reads=0'), ('newer_t_preserved=1', 'newer_t_preserved=0'),
                         ('source_opcode_poison=1', 'source_opcode_poison=0'),
                         ('physical_banks=unvalidated', 'physical_banks=validated')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='mac-rpt')
        for bad in (marker + '\n' + output, output + 'late output\n'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='mac-rpt')

    def test_repeated_dual_mac_exclusions(self):
        marker, ending = EXTRA_PROFILES['mac-rpt-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mac-rpt-edge')
        for old, new in (('restore=512', 'restore=0'), ('reads=0', 'reads=1'),
                         ('writes=0', 'writes=1'), ('pointer_changes=0', 'pointer_changes=1'),
                         ('t_changes=0', 't_changes=1'), ('silicon_trap_claim=0', 'silicon_trap_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='mac-rpt-edge')

    def test_repeated_square_exclusions(self):
        marker, ending = EXTRA_PROFILES['sqacc-rpt-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='sqacc-rpt-edge')
        for old, new in (('restore=64', 'restore=0'), ('data_reads=0', 'data_reads=1'),
                         ('arithmetic_changes=0', 'arithmetic_changes=1'),
                         ('t_changes=0', 't_changes=1'), ('silicon_trap_claim=0', 'silicon_trap_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='sqacc-rpt-edge')

    def test_repeated_square_accumulate_scope(self):
        marker, ending = EXTRA_PROFILES['sqacc-repeat']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='sqacc-repeat')
        for old, new in (('restore=49152', 'restore=0'), ('per_clock_accumulator=1', 'per_clock_accumulator=0'),
                         ('newer_t_preserved=1', 'newer_t_preserved=0'),
                         ('source_opcode_poison=1', 'source_opcode_poison=0'),
                         ('data_writes=0', 'data_writes=1'),
                         ('physical_phase_claim=0', 'physical_phase_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='sqacc-repeat')
        for bad in (marker + '\n' + output, output + 'late output\n'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='sqacc-repeat')

    def test_banzd_delay_t_load_scope(self):
        marker, ending = EXTRA_PROFILES['banzd-t-load']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='banzd-t-load')
        for old, new in (('restore=9216', 'restore=0'), ('delay_reads=1', 'delay_reads=2'),
                         ('selected_path_multiply=1', 'selected_path_multiply=0'),
                         ('source_opcode_target_poison=1', 'source_opcode_target_poison=0'),
                         ('brc_preserved=1', 'brc_preserved=0'),
                         ('physical_phase_claim=0', 'physical_phase_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='banzd-t-load')
        for bad in (marker + '\n' + output, output + 'late output\n'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='banzd-t-load')

    def test_memory_repeat_in_block_scope(self):
        marker, ending = EXTRA_PROFILES['rptb-memory']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rptb-memory')
        for old, new in (('restore=3840', 'restore=0'), ('fresh_counts=1', 'fresh_counts=0'),
                         ('consumed_counts_poisoned=1', 'consumed_counts_poisoned=0'),
                         ('outer_counts_preserved=1', 'outer_counts_preserved=0'),
                         ('physical_phase_claim=0', 'physical_phase_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='rptb-memory')
        for bad in (marker + '\n' + output, output + 'late output\n'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='rptb-memory')

    def test_mapped_t_addm_scope(self):
        marker, ending = EXTRA_PROFILES['addm-t']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='addm-t')
        for old, new in (('restore=6144', 'restore=0'), ('retained_source=1', 'retained_source=0'),
                         ('newer_t_overwritten=1', 'newer_t_overwritten=0'),
                         ('duplicate_reads=0', 'duplicate_reads=1'),
                         ('physical_phase_claim=0', 'physical_phase_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='addm-t')
        for bad in (marker + '\n' + output, output + 'late output\n'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='addm-t')

    def test_repeated_arithmetic_shift_scope(self):
        marker, ending = EXTRA_PROFILES['rpt-sfta']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rpt-sfta')
        for old, new in (('restore=16384', 'restore=0'), ('carry_overflow=1', 'carry_overflow=0'),
                         ('each_iteration=1', 'each_iteration=0'),
                         ('retained_opcode=1', 'retained_opcode=0'),
                         ('physical_phase_claim=0', 'physical_phase_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='rpt-sfta')
        for bad in (marker + '\n' + output, output + 'late output\n'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='rpt-sfta')

    def test_repeated_accumulator_load_exclusion_scope(self):
        marker, ending = EXTRA_PROFILES['rpt-ld-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rpt-ld-edge')
        for old, new in (('restore=32', 'restore=0'), ('data_reads=0', 'data_reads=1'),
                         ('data_writes=0', 'data_writes=1'), ('arithmetic_changes=0', 'arithmetic_changes=1'),
                         ('flag_changes=0', 'flag_changes=1'), ('silicon_trap_claim=0', 'silicon_trap_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='rpt-ld-edge')

    def test_basic_memory_repeat_exclusion_scope(self):
        marker, ending = EXTRA_PROFILES['rpt-smem-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rpt-smem-edge')
        for old, new in (('restore=100', 'restore=0'), ('mod3_cases=64', 'mod3_cases=0'), ('data_reads=0', 'data_reads=1'),
                         ('data_writes=0', 'data_writes=1'), ('pointer_changes=0', 'pointer_changes=1'),
                         ('silicon_trap_claim=0', 'silicon_trap_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='rpt-smem-edge')

    def test_basic_memory_repeat_scope(self):
        marker, ending = EXTRA_PROFILES['rpt-smem']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rpt-smem')
        for old, new in (('restore=10376', 'restore=0'),
                         ('count_reads=1', 'count_reads=2'),
                         ('body_fetches=1', 'body_fetches=2'),
                         ('body_count=rc+1', 'body_count=rc'),
                         ('pointer_effect=once', 'pointer_effect=twice'),
                         ('retained_count=1', 'retained_count=0'),
                         ('balanced_frames=1', 'balanced_frames=0'),
                         ('physical_phase_claim=0', 'physical_phase_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='rpt-smem')
        for bad in (marker + '\n' + output, output + 'late output\n'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='rpt-smem')

    def test_rptbd_shift_store_scope(self):
        marker, ending = EXTRA_PROFILES['rptbd-shift-st']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rptbd-shift-st')
        for old, new in (('restore=115200', 'restore=0'),
                         ('delay_writes=1', 'delay_writes=2'),
                         ('pointer_effect=once', 'pointer_effect=twice'),
                         ('body_count=brc+1', 'body_count=brc'),
                         ('balanced_frames=1', 'balanced_frames=0'),
                         ('retained_words=1', 'retained_words=0'),
                         ('physical_phase_claim=0', 'physical_phase_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='rptbd-shift-st')
        for bad in (marker + '\n' + output, output + 'late output\n'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='rptbd-shift-st')

    def test_rptbd_shift_store_exclusion_scope(self):
        marker, ending = EXTRA_PROFILES['rptbd-shift-edge']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rptbd-shift-edge')
        for old, new in (('restore=16', 'restore=0'), ('data_reads=0', 'data_reads=1'),
                         ('data_writes=0', 'data_writes=1'), ('pointer_changes=0', 'pointer_changes=1'),
                         ('brc_changes=0', 'brc_changes=1'), ('silicon_trap_claim=0', 'silicon_trap_claim=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='rptbd-shift-edge')

    def test_mvdk_accumulator_destination_scope(self):
        for profile in ('mvdk-acc', 'mvdk-acc-prod'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for old, new in (('guard_width=8', 'guard_width=16'),
                             ('source_reads=1', 'source_reads=2'),
                             ('destination_writes=1', 'destination_writes=2'),
                             ('restore=', 'missing_restore='),
                             ('physical_phase_claim=0', 'physical_phase_claim=1')):
                with self.subTest(profile=profile, mutation=old), self.assertRaises(ValueError):
                    check_result(output.replace(old, new), 3, profile=profile)
            for bad in (marker + '\n' + output, output + 'late output\n'):
                with self.subTest(profile=profile), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_shifted_ar_store_scope(self):
        for profile in ('st-shift-ar', 'st-shift-ar-use', 'st-shift-ar-bank'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for bad in (output.replace('publish_clock=8', 'publish_clock=7'),
                        output.replace('retained_extensions=1', 'retained_extensions=0'),
                        output.replace('writes=1', 'writes=2'),
                        output.replace('restore=', 'missing_restore='),
                        output.replace('silicon_phase_claim=0', 'silicon_phase_claim=1'),
                        marker + '\n' + output, output + 'late output\n'):
                if bad == output:
                    continue
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_shifted_ar_bank_and_exclusion_scope(self):
        for profile, replacements in (
                ('st-shift-ar-bank', (('early_write=7,8', 'early_write=7,7'),
                                      ('source_reads=once', 'source_reads=twice'),
                                      ('restore=768', 'restore=0'))),
                ('st-shift-ar-edge', (('source_reads=0', 'source_reads=1'),
                                      ('destination_writes=0', 'destination_writes=1'),
                                      ('register_changes=0', 'register_changes=1'),
                                      ('restore=48', 'restore=0')))):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for old, new in replacements:
                with self.subTest(profile=profile, mutation=old), self.assertRaises(ValueError):
                    check_result(output.replace(old, new), 3, profile=profile)

    def test_repeat_page_comparator_scope(self):
        marker, ending = EXTRA_PROFILES['rptb-page']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rptb-page')
        for bad in (output.replace('rea_width=16', 'rea_width=23'),
                    output.replace('wrap_page=current', 'wrap_page=caller'),
                    output.replace('restore=384', 'restore=0'),
                    output.replace('delay_effects=once', 'delay_effects=twice'),
                    output.replace('physical_phase_claim=0', 'physical_phase_claim=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='rptb-page')

    def test_instruction_observer_scope(self):
        markers, ending = EXTRA_PROFILES['instr-observer']
        output = '\n'.join((*markers, ending)) + '\n'
        check_result(output, 3, profile='instr-observer')
        for bad in (output.replace('restore=32', 'restore=0'),
                    output.replace('ordinary_reads=0', 'ordinary_reads=1'),
                    output.replace('captured_opcode=1', 'captured_opcode=0'),
                    output.replace('rejected_slots=absent', 'rejected_slots=present'),
                    markers[0] + '\n' + output, output + 'late output\n',
                    *(output.replace(marker + '\n', '') for marker in markers)):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='instr-observer')

    def test_sp_sources_require_operation_contexts_and_exclusions(self):
        for profile in ('add-sp', 'mvkd-sp'):
            markers, ending = EXTRA_PROFILES[profile]
            output = '\n'.join((*markers, ending)) + '\n'
            check_result(output, 3, profile=profile)
            for marker in markers:
                for bad in (output.replace(marker + '\n', ''), marker + '\n' + output):
                    with self.subTest(profile=profile, marker=marker, bad=bad), self.assertRaises(ValueError):
                        check_result(bad, 3, profile=profile)
            for bad in (output.replace('restore=576', 'restore=0'),
                        output.replace('operand_reads=0', 'operand_reads=1'), output + 'late output\n'):
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)
            with self.assertRaises(ValueError):
                check_result(output, 0, profile=profile)

    def test_ram_forwarding_scopes(self):
        for profile in ('imm-forward', 'rmw-forward', 'mvkd-forward'):
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

    def test_mvkd_forwarding_retains_source_once(self):
        marker, ending = EXTRA_PROFILES['mvkd-forward']
        output = marker + '\n' + ending + '\n'
        for old, new in (('source_reads=1', 'source_reads=2'),
                         ('retained_source=1', 'retained_source=0'),
                         ('same_and_different=1', 'same_and_different=0')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='mvkd-forward')

    def test_block_xc_scopes(self):
        for profile in ('block-xc', 'block-xc-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for bad in (output.replace('restore=8704', 'restore=0').replace('restore=18', 'restore=0'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_endpoint_xc_protection_scope(self):
        marker, ending = EXTRA_PROFILES['block-xc-end-irq']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='block-xc-end-irq')
        for old, new in (('restore=64', 'restore=0'), ('wrapped_protection=1', 'wrapped_protection=0'),
                         ('reset_before_restore=1', 'reset_before_restore=0'),
                         ('final_fallthrough=1', 'final_fallthrough=0'),
                         ('all_stores_before_vector=1', 'all_stores_before_vector=0'),
                         ('vector_fetches=1', 'vector_fetches=2')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='block-xc-end-irq')
        for bad in (marker + '\n' + output, output + 'late output\n'):
            with self.assertRaises(ValueError):
                check_result(bad, 3, profile='block-xc-end-irq')

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

    def test_direct_irq_repeat_scope(self):
        marker, ending = EXTRA_PROFILES['repeat-body-isr']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='repeat-body-isr')
        for bad in (output.replace('restore=768', 'restore=0'),
                    output.replace('near_return=0', 'near_return=1'),
                    output.replace('balanced_frames=1', 'balanced_frames=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='repeat-body-isr')

    def test_active_repeat_context_scope(self):
        marker, ending = EXTRA_PROFILES['repeat-context']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='repeat-context')
        for bad in (output.replace('restore=384', 'restore=0'),
                    output.replace('source_reads=once', 'source_reads=twice'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='repeat-context')

    def test_delayed_accumulator_far_call_scope(self):
        marker, ending = EXTRA_PROFILES['fcala-delay']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='fcala-delay')
        for bad in (output.replace('restore=648', 'restore=0'),
                    output.replace('frames=balanced', 'frames=unvalidated'),
                    output.replace('delay_effects=once', 'delay_effects=twice'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='fcala-delay')

    def test_indexed_immediate_store_scope(self):
        marker, ending = EXTRA_PROFILES['imm-index']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='imm-index')
        for bad in (output.replace('restore=98304', 'restore=0'),
                    output.replace('excluded_pointer_updates=0', 'excluded_pointer_updates=1'),
                    output.replace('literal=retained', 'literal=reread'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='imm-index')

    def test_mapped_ar_arithmetic_scope(self):
        marker, ending = EXTRA_PROFILES['arith-ar']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='arith-ar')
        for bad in (output.replace('restore=1024', 'restore=0'),
                    output.replace('duplicate_reads=0', 'duplicate_reads=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='arith-ar')

    def test_mapped_bk_arithmetic_scope(self):
        marker, ending = EXTRA_PROFILES['arith-bk']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='arith-bk')
        for bad in (output.replace('restore=1024', 'restore=0'),
                    output.replace('post_read_change=1', 'post_read_change=0'),
                    output.replace('duplicate_reads=0', 'duplicate_reads=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='arith-bk')

    def test_delayed_absolute_add_scope(self):
        marker, ending = EXTRA_PROFILES['add-delay']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='add-delay')
        for bad in (output.replace('restore=18432', 'restore=0'),
                    output.replace('call_frames=owned', 'call_frames=unvalidated'),
                    output.replace('target_observes_result=1', 'target_observes_result=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='add-delay')

    def test_long_banz_block_scope(self):
        marker, ending = EXTRA_PROFILES['rptb-banz']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rptb-banz')
        for bad in (output.replace('restore=768', 'restore=0'),
                    output.replace('ar_updates_once=1', 'ar_updates_once=0'),
                    output.replace('actual_iterations=1', 'actual_iterations=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='rptb-banz')

    def test_unconditional_delayed_block_branch_scope(self):
        marker, ending = EXTRA_PROFILES['rptb-bd']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rptb-bd')
        for bad in (output.replace('restore=512', 'restore=0'),
                    output.replace('delay_words_once=1', 'delay_words_once=0'),
                    output.replace('actual_iterations=1', 'actual_iterations=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='rptb-bd')

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

    def test_delay_asm_scope(self):
        for name in ('asm-delay', 'asm-delay-edge'):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace('restore=', 'missing_restore='),
                        output.replace('t_preserved=1', 't_preserved=0') if name == 'asm-delay'
                        else output.replace('data_reads=0', 'data_reads=1'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(name=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)
            with self.assertRaises(ValueError):
                check_result(output, 3)

    def test_constant_dagen_store_scope(self):
        marker, ending = EXTRA_PROFILES['st-dagen']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='st-dagen')
        for bad in (output.replace('restore=3920', 'restore=0'),
                    output.replace('writes_each=1', 'writes_each=2'),
                    output.replace('repeated_publication=0', 'repeated_publication=1'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='st-dagen')
        with self.assertRaises(ValueError):
            check_result(output, 3)

    def test_delay_dld_scope(self):
        for name in ('dld-delay', 'dld-delay-edge'):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace('restore=', 'missing_restore='),
                        output.replace('ordered_reads=2', 'ordered_reads=1') if name == 'dld-delay'
                        else output.replace('data_reads=0', 'data_reads=1'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(name=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)
            with self.assertRaises(ValueError):
                check_result(output, 3)

    def test_delay_mac_scope(self):
        for name in ('mac-delay', 'mac-delay-edge'):
            marker, ending = EXTRA_PROFILES[name]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace('restore=', 'missing_restore='),
                        output.replace('t_retained=1', 't_retained=0') if name == 'mac-delay'
                        else output.replace('data_reads=0', 'data_reads=1'),
                        marker + '\n' + output, output + 'late output\n'):
                with self.subTest(name=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)
            with self.assertRaises(ValueError):
                check_result(output, 3)

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
        markers, ending = EXTRA_PROFILES['store-bypass']
        output = '\n'.join((*markers, ending)) + '\n'
        check_result(output, 3, profile='store-bypass')
        for bad in (output.replace('restore=288', 'restore=0'),
                    output.replace('alias_reads=0', 'alias_reads=1'),
                    output.replace('restore=128', 'restore=0'),
                    output.replace('count_reads=1', 'count_reads=2'),
                    output.replace(markers[1] + '\n', ''),
                    markers[0] + '\n' + output, output + 'late output\n'):
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
            markers = marker if isinstance(marker, tuple) else (marker,)
            output = '\n'.join((*markers, ending)) + '\n'
            check_result(output, 3, profile=name)
            for bad in (output.replace('restore=', 'missing_restore='),
                        markers[0] + '\n' + output, output + 'late output\n',
                        *(output.replace(item + '\n', '') for item in markers)):
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
            markers, ending = EXTRA_PROFILES[name]
            markers = (markers,) if isinstance(markers, str) else markers
            output = '\n'.join((*markers, ending)) + '\n'
            check_result(output, 3, profile=name)
            for marker in markers:
                with self.subTest(profile=name, marker=marker), self.assertRaises(ValueError):
                    check_result(output.replace(marker, ''), 3, profile=name)
            for bad in (output.replace('restore=', 'missing_restore='),
                        markers[0] + '\n' + output, output + 'late output\n'):
                with self.subTest(profile=name, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=name)

    def test_logic_transfer_mod3_exclusions(self):
        markers, ending = EXTRA_PROFILES['logic-short']
        output = '\n'.join((*markers, ending)) + '\n'
        for old, new in (('restore=2816', 'restore=0'), ('opcode_classes=11', 'opcode_classes=10'),
                         ('frontends=2', 'frontends=1'), ('initial_tc=2', 'initial_tc=1'),
                         ('program_writes=0', 'program_writes=1'), ('io_writes=0', 'io_writes=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='logic-short')

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
            markers, ending = EXTRA_PROFILES[name]
            output = '\n'.join((*markers, ending)) + '\n'
            check_result(output, 3, profile=name)
            for marker in markers:
                with self.subTest(profile=name, marker=marker), self.assertRaises(ValueError):
                    check_result(output.replace(marker, ''), 3, profile=name)
            with self.subTest(profile=name), self.assertRaises(ValueError):
                check_result(output.replace('restore=', 'missing_restore='), 3, profile=name)

    def test_extended_popd_scope(self):
        markers, ending = EXTRA_PROFILES['popd-ram']
        output = '\n'.join((*markers, ending)) + '\n'
        for old, new in (('restore=16128', 'restore=0'), ('forms=25', 'forms=3'),
                         ('stack_reads=1', 'stack_reads=2'), ('destination_writes=1', 'destination_writes=2'),
                         ('circular_steps_bounded=1', 'circular_steps_bounded=0'),
                         ('irq_acceptance=0', 'irq_acceptance=1'), ('mapped_acceptance=0', 'mapped_acceptance=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                check_result(output.replace(old, new), 3, profile='popd-ram')

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
        for bad in (output.replace('restore=52224', 'restore=0'),
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
        for profile in ('mac-dual', 'mac-dual-edge', 'mac-family', 'mac-family-edge', 'stmac', 'stmac-edge', 'sqdst', 'sqdst-edge', 'portr-bypass', 'mvdk-circular', 'mvdk-circ-edge', 'mvdk-repeat', 'mvdk-rpt-edge', 'rptb-call', 'rptb-call-edge', 'rpt-mem-copy', 'rpt-mem-wrap', 'rpt-mem-chain', 'addc', 'addc-edge', 'addc-chain'):
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

    def test_parallel_bsp_load_scope(self):
        marker, ending = EXTRA_PROFILES['stld-bsp']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='stld-bsp')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='stld-bsp')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='stld-bsp')

    def test_mvdd_follower_scope(self):
        marker, ending = EXTRA_PROFILES['mvdd-follow']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mvdd-follow')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='mvdd-follow')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mvdd-follow')

    def test_tc_delayed_call_scope(self):
        marker, ending = EXTRA_PROFILES['tc-delay-call']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='tc-delay-call')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='tc-delay-call')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='tc-delay-call')

    def test_repeated_t_mac_scope(self):
        for profile in ('tmac-repeat', 'tmac-rpt-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for field in marker.split()[4:]:
                with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                    check_result(output.replace(field, field + '_wrong', 1), 3,
                                 profile=profile)
            for bad in (ending + '\n', marker + '\n' + output,
                        output + 'late output\n'):
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_lms_scope(self):
        for profile in ('lms', 'lms-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for field in marker.split()[4:]:
                with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                    check_result(output.replace(field, field + '_wrong', 1), 3,
                                 profile=profile)
            for bad in (ending + '\n', marker + '\n' + output,
                        output + 'late output\n'):
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_short_mapped_ar_addm_scope(self):
        profile = 'addm-ar-short'
        marker, ending = EXTRA_PROFILES[profile]
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile=profile)
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile=profile)
        for bad in (ending + '\n', marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile=profile)

    def test_bd_long_accumulator_multiply_scope(self):
        for profile in ('bd-maca-long', 'bd-maca-edge', 'push-mvmm-sp', 'xorm-absolute', 'mvdm-t'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for field in marker.split()[4:]:
                with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                    check_result(output.replace(field, field + '_wrong', 1), 3,
                                 profile=profile)
            for bad in (ending + '\n', marker + '\n' + output, output + 'late output\n'):
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_mapped_memory_immediate_mac_scope(self):
        marker, ending = EXTRA_PROFILES['mac-map-imm']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mac-map-imm')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='mac-map-imm')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mac-map-imm')

    def test_bd_read_a_scope(self):
        marker, ending = EXTRA_PROFILES['bd-read-a']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='bd-read-a')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='bd-read-a')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='bd-read-a')

    def test_mapped_add_sub_age_scope(self):
        marker, ending = EXTRA_PROFILES['addsub-ar-age']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='addsub-ar-age')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='addsub-ar-age')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='addsub-ar-age')

    def test_shifted_mapped_load_age_scope(self):
        marker, ending = EXTRA_PROFILES['shift16-load-age']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='shift16-load-age')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='shift16-load-age')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='shift16-load-age')

    def test_accumulator_store_mvmm_scope(self):
        marker, ending = EXTRA_PROFILES['st-acc-mvmm']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='st-acc-mvmm')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='st-acc-mvmm')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='st-acc-mvmm')

    def test_short_control_delay_bitt_scope(self):
        marker, ending = EXTRA_PROFILES['bitt-ctrl']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='bitt-ctrl')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='bitt-ctrl')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='bitt-ctrl')

    def test_conditional_delay_repeat_scope(self):
        for profile in ('bcd-rpt', 'bcd-rpt-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for field in marker.split()[4:]:
                with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                    check_result(output.replace(field, field + '_wrong', 1), 3,
                                 profile=profile)
            for bad in (ending + '\n', marker + '\n' + output,
                        output + 'late output\n'):
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_delayed_memory_square_scope(self):
        for profile in ('squr-delay', 'rptbd-squr'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for field in marker.split()[4:]:
                with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                    check_result(output.replace(field, field + '_wrong', 1), 3,
                                 profile=profile)
            for bad in (ending + '\n', marker + '\n' + output,
                        output + 'late output\n'):
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_repeated_parallel_arithmetic_scope(self):
        for profile in ('st-alu-rpt', 'st-alu-rpt-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for field in marker.split()[4:]:
                with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                    check_result(output.replace(field, field + '_wrong', 1), 3,
                                 profile=profile)
            for bad in (ending + '\n', marker + '\n' + output,
                        output + 'late output\n'):
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_direct_low_ram_stm_scope(self):
        marker, ending = EXTRA_PROFILES['stm-low-ram']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='stm-low-ram')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='stm-low-ram')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='stm-low-ram')

    def test_control_delay_t_load_scope(self):
        marker, ending = EXTRA_PROFILES['ctrl-t-load']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='ctrl-t-load')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='ctrl-t-load')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='ctrl-t-load')

    def test_call_delay_repeat_scope(self):
        marker, ending = EXTRA_PROFILES['calld-rpt']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='calld-rpt')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='calld-rpt')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='calld-rpt')

    def test_conditional_ar_store_scope(self):
        marker, ending = EXTRA_PROFILES['cond-store-ar']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='cond-store-ar')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='cond-store-ar')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='cond-store-ar')

    def test_parallel_multiply_scope(self):
        for profile in ('st-par-mpy', 'st-mpy-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for field in marker.split()[4:]:
                with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                    check_result(output.replace(field, field + '_wrong', 1), 3,
                                 profile=profile)
            for bad in (ending + '\n', marker + '\n' + output,
                        output + 'late output\n'):
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_delayed_square_scope(self):
        marker, ending = EXTRA_PROFILES['square-delay']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='square-delay')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='square-delay')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='square-delay')

    def test_parallel_peripheral_load_scope(self):
        marker, ending = EXTRA_PROFILES['stld-periph']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='stld-periph')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='stld-periph')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='stld-periph')

    def test_mapped_parallel_load_scope(self):
        marker, ending = EXTRA_PROFILES['stld-map']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='stld-map')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='stld-map')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='stld-map')

    def test_delayed_rounded_multiply_scope(self):
        for profile in ('mpyr-delay', 'mpyr-delay-edge'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for field in marker.split()[4:]:
                with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                    check_result(output.replace(field, field + '_wrong', 1), 3,
                                 profile=profile)
            for bad in (ending + '\n', marker + '\n' + output,
                        output + 'late output\n'):
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_rptbd_square_scope(self):
        marker, ending = EXTRA_PROFILES['rptbd-square']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='rptbd-square')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='rptbd-square')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='rptbd-square')

    def test_delayed_brc_move_scope(self):
        marker, ending = EXTRA_PROFILES['move-brc-delay']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='move-brc-delay')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='move-brc-delay')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='move-brc-delay')

    def test_xc_return_scope(self):
        marker, ending = EXTRA_PROFILES['xc-ret']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='xc-ret')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='xc-ret')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='xc-ret')

    def test_delay_copy_scope(self):
        for profile in ('delay-copy', 'delay-copy-ctrl'):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            for field in marker.split()[4:]:
                with self.subTest(profile=profile, field=field), self.assertRaises(ValueError):
                    check_result(output.replace(field, field + '_wrong', 1), 3,
                                 profile=profile)
            for bad in (ending + '\n', marker + '\n' + output,
                        output + 'late output\n'):
                with self.subTest(profile=profile, bad=bad), self.assertRaises(ValueError):
                    check_result(bad, 3, profile=profile)

    def test_delayed_accumulator_multiply_scope(self):
        marker, ending = EXTRA_PROFILES['maca-delay']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='maca-delay')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='maca-delay')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='maca-delay')

    def test_shifted_delay_arithmetic_scope(self):
        marker, ending = EXTRA_PROFILES['shift-alu-delay']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='shift-alu-delay')
        for field in marker.split()[4:]:
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(output.replace(field, field + '_wrong', 1), 3,
                             profile='shift-alu-delay')
        for bad in (ending + '\n', marker + '\n' + output,
                    output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='shift-alu-delay')

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
        for bad in (output.replace('restore=208896', 'restore=0'),
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
                    output.replace('memory_cannot_set_intm=1', 'memory_cannot_set_intm=0'),
                    output.replace('clear_policy=legacy_unverified', 'clear_policy=proven'),
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
        for bad in (output.replace('restore=52224', 'restore=0'),
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
        markers, ending = EXTRA_PROFILES['banzd-long']
        self.assertEqual(len(markers), 2)
        output = '\n'.join((*markers, ending)) + '\n'
        check_result(output, 3, profile='banzd-long')
        for marker in markers:
            for changed in (output.replace(marker, ''), marker + '\n' + output):
                with self.subTest(marker=marker), self.assertRaises(ValueError):
                    check_result(changed, 3, profile='banzd-long')
        for old, new in (('restore=2304', 'restore=0'),
                         ('restore=4608', 'restore=0'),
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
        for profile, restores in (('drsub', 6144), ('long-pair', 18432)):
            markers, ending = EXTRA_PROFILES[profile]
            output = '\n'.join((*markers, ending)) + '\n'
            check_result(output, 3, profile=profile)
            for marker in markers:
                with self.subTest(profile=profile, marker=marker), self.assertRaises(ValueError):
                    check_result(output.replace(marker, ''), 3, profile=profile)
            for old, new in ((f'restore={restores}', 'restore=0'),
                             ('pair_reads=2', 'pair_reads=1'),
                             ('memory_writes=0', 'memory_writes=1'),
                             ('restore=3584', 'restore=0'), ('c16=2', 'c16=1'),
                             ('data_reads=0', 'data_reads=1'), ('opcode_classes=14', 'opcode_classes=8')):
                with self.subTest(profile=profile, old=old), self.assertRaises(ValueError):
                    check_result(output.replace(old, new), 3, profile=profile)

    def test_normalization_profile_scopes(self):
        for profile, restores in (('norm', 98304), ('norm-edge', 16384), ('exp-norm', 6144)):
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
        for profile, count in (('rptb-fcall', 768), ('rptb-fcalld', 2304)):
            marker, ending = EXTRA_PROFILES[profile]
            output = marker + '\n' + ending + '\n'
            check_result(output, 3, profile=profile)
            mutations = [(f'restore={count}', 'restore=0'),
                         ('sources=none,IRQ,NMI', 'sources=none'),
                         ('caller_loop_preserved=1', 'caller_loop_preserved=0')]
            if profile == 'rptb-fcalld':
                mutations.extend((('return_pc=call+4', 'return_pc=call+2'),
                                  ('delay_effects=once', 'delay_effects=twice')))
            for old, new in mutations:
                with self.subTest(profile=profile, old=old), self.assertRaises(ValueError):
                    check_result(output.replace(old, new), 3, profile=profile)

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
        for profile, restores in (('ts-alu', 153984), ('ts-alu-producer', 18432), ('ts-alu-edge', 320)):
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
        for old, new in (('restore=52992', 'restore=0'),
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

    def test_mac_bk_profile_is_separate(self):
        marker, ending = EXTRA_PROFILES['mac-bk']
        output = marker + '\n' + ending + '\n'
        check_result(output, 3, profile='mac-bk')
        for bad in (output.replace('restore=98304', 'restore=0'),
                    output.replace('bk_retained=1', 'bk_retained=0'),
                    output.replace('t_retained=1', 't_retained=0'),
                    marker + '\n' + output, output + 'late output\n'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                check_result(bad, 3, profile='mac-bk')
        with self.assertRaises(ValueError):
            check_result(output, 3)

    def test_mac_profile_is_separate(self):
        (marker, square, poly, register, dependencies, remaining), ending = EXTRA_PROFILES['mac-smem']
        output = '\n'.join((marker, square, poly, register, dependencies, remaining, ending)) + '\n'
        check_result(output, 3, profile='mac-smem')
        for bad in (output.replace('restore=73728', 'restore=0'),
                    output.replace('flags=16', 'flags=8'),
                    output.replace('t_retained=1', 't_retained=0'),
                    output.replace(poly + '\n', ''),
                    output.replace(square + '\n', ''),
                    output.replace(register + '\n', ''),
                    output.replace(dependencies + '\n', ''),
                    output.replace('restore=107520','restore=0'),
                    output.replace('sources=RAM,mapped_T','sources=RAM'),
                    output.replace(remaining + '\n', ''),
                    output.replace('restore=129792','restore=0'),
                    output.replace('poly_smul=excluded','poly_smul=validated'),
                    output.replace('literal_fetches=0,1','literal_fetches=0'),
                    output.replace('restore=69120','restore=0'),
                    output.replace('accumulator_sample=Execute','accumulator_sample=Read'),
                    output.replace('overflow_initial=clear,sticky','overflow_initial=sticky'),
                    output.replace('restore=36864', 'restore=0'),
                    output.replace('t_republish=0', 't_republish=1'),
                    output.replace('restore=4608', 'restore=0'),
                    output.replace('irq_acceptance=0', 'irq_acceptance=1'),
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

    def test_verbose_teardown_is_exact_and_opt_in(self):
        suffix = 'Enter sdlwindow_exit\nLeave sdlwindow_exit\n'
        check_result(self.output + suffix, 3, verbose_teardown=True)
        check_result(self.output, 3, verbose_teardown=True)
        with self.assertRaises(ValueError):
            check_result(self.output + suffix, 3)
        for output in (self.output + suffix + 'crash\n',
                       self.output + 'unexpected\n' + suffix,
                       self.output + 'Enter sdlwindow_exit\n',
                       self.output.replace(COMPLETION, '') + suffix,
                       self.output.replace(REQUIRED[0] + '\n', '') + suffix):
            with self.subTest(output=output[-100:]), self.assertRaises(ValueError):
                check_result(output, 3, verbose_teardown=True)
        with self.assertRaises(ValueError):
            check_result(self.output + suffix, 0, verbose_teardown=True)

    def test_other_fatal_error_rejected(self):
        with self.assertRaises(ValueError):
            check_result('Fatal error: unexpected failure\n' + self.output, 3)


if __name__ == '__main__':
    unittest.main()
