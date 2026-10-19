import unittest

from tools.mu4_cleanup_ingress_check import check, check_lifecycle_restart, reference_input_extent


def fixture(sites=None):
    events = []
    for ordinal, (address, opcode) in enumerate(sites or ((0x29438, 0x1004), (0x29500, 0xf830), (0x2956f, 0x60f8)), 1):
        events.append(f'mu4_cleanup_ingress: ordinal={ordinal} address={address:06x} opcode={opcode:04x} '
                      f'cycles={100+ordinal} sp=d6f4 f4=0011 f5=162e f6=0000 f7=0000 f8=0000 f9=0000 '
                      'control=0001 ar1=0000 ar2=0000 identity=debugger_Execute_hook operands=context_only observer_writes=0')
    return '\n'.join([
        'mu4_cleanup_ingress_scope: identity=debugger_Execute_hook operands=context_only '
        'annulment=unclaimed observer_writes=0 audio_acceptance=0',
        'mu4_playback_execute_entry: address=026d62 opcode=fd00 t=0.2',
        'mu4_native_playback_overlay: PASS marker=aa99 records=3127 words=49843 checked_ram_words=49839 excluded_register_words=4 before_execution=1',
        *events,
        'mu4_playback_execute_entry: address=026d62 opcode=fd00 t=9.86',
        f'mu4_cleanup_ingress_terminal: events={len(events)} cap=4096 observer_writes=0 audio_acceptance=0',
        'mu4_cleanup_ingress_stop: t=10 illegal=0 observer_writes=0 audio_acceptance=0',
        'mu4_playback_parent_terminal: first_session_complete=1 overlay_verified=1 illegal=0 '
        'opcode_mismatch=0 selection=first_verified_aa99_session observer_writes=0'])


class CleanupIngressTest(unittest.TestCase):
    def test_explicit_first_session_excludes_later_capture_cap(self):
        text = fixture()
        later = ('mu4_cleanup_ingress: ordinal=4 address=029438 opcode=1004 cycles=104 '
                 'sp=d6f4 f4=0011 f5=162e f6=0000 f7=0000 f8=0000 f9=0000 '
                 'control=0001 ar1=0000 ar2=0000 identity=debugger_Execute_hook '
                 'operands=context_only observer_writes=0\n')
        text = text.replace('mu4_cleanup_ingress_terminal:', later+'mu4_cleanup_ingress_terminal:')
        text = text.replace('events=3 cap=4096', 'events=4 cap=4')
        with self.assertRaises(ValueError):
            check(text)
        result = check(text, first_resident_only=True)
        self.assertEqual(result['events'], 3)
        self.assertEqual(result['capture_selection']['excluded_later_events'], 1)
        self.assertTrue(result['capture_selection']['global_cap_reached'])
        with self.assertRaises(ValueError):
            check(fixture().replace('cap=4096', 'cap=3'), first_resident_only=True)
        with self.assertRaises(ValueError):
            check(text.replace('address=026d62 opcode=fd00 t=9.86',
                               'address=026d63 opcode=fd00 t=9.86'), first_resident_only=True)

    def test_media_extent_must_agree_with_executed_length_successors(self):
        text = fixture(((0x29438, 0x1004), (0x294c3, 0xf84a), (0x294fd, 0x60f8),
                        (0x29500, 0xf830), (0x2956f, 0x60f8))).replace('f7=0000', 'f7=0006')
        result = check(text, bytes(12))['input_extent_model']
        self.assertEqual(result['limit_words'], 6)
        self.assertEqual(result['final']['request_words'], 6)
        with self.assertRaises(ValueError):
            check(text, bytes(14))

    def test_even_file_extent_distinguishes_equality_and_overshoot(self):
        equal = reference_input_extent(12, [4, 2])
        self.assertEqual([row['crosses'] for row in equal], [False, True])
        self.assertEqual(equal[-1]['remaining_words'], 2)
        over = reference_input_extent(12, [4, 3])
        self.assertEqual(over[-1]['remaining_words'], 2)
        self.assertEqual(over[-1]['prior_words'], 4)
        self.assertEqual(over[-1]['request_words'], 3)

    def test_extent_model_refuses_uncovered_regimes_and_missing_end(self):
        for size, requests in ((11, [6]), (0, [1]), (12, []), (12, [4]),
                               (12, [0x8000]), (12, [0]), (12, [6, 1]), (1<<32, [1])):
            with self.subTest(size=size, requests=requests), self.assertRaises(ValueError):
                reference_input_extent(size, requests)

    def test_successor_defines_cleanup_without_operand_claim(self):
        result = check(fixture())
        self.assertEqual(result['cleanup_successors'], {'02956f': 1})
        self.assertEqual(result['input_counts'], {'0011': 1})
        # A snapshot alone is not a retained operand or branch outcome.
        self.assertEqual(check(fixture().replace('control=0001', 'control=0000'))['cleanup_successors'],
                         {'02956f': 1})

    def test_rejects_incomplete_capped_or_wrong_identity(self):
        for before, after in [('events=3', 'events=4'), ('cap=4096', 'cap=3'),
                              ('ordinal=2', 'ordinal=3'), ('cycles=102', 'cycles=101'),
                              ('opcode=f830', 'opcode=0000'), ('illegal=0', 'illegal=1'),
                              ('operands=context_only', 'operands=retired'),
                              ('f7=0000', 'f7=10000'), ('words=49843', 'words=1'),
                              ('address=02956f opcode=60f8', 'address=0294fd opcode=60f8'),
                              ('observer_writes=0', 'observer_writes=1')]:
            with self.subTest(before=before, after=after), self.assertRaises(ValueError):
                check(fixture().replace(before, after))

    def test_rejects_missing_terminal_and_censored_branch(self):
        for marker in ('mu4_cleanup_ingress_terminal:', 'ordinal=3'):
            text = '\n'.join(line for line in fixture().splitlines() if marker not in line)
            if marker == 'ordinal=3':
                text = text.replace('events=3', 'events=2')
            with self.assertRaises(ValueError):
                check(text)

    def test_length_route_uses_executed_successor(self):
        text = fixture().replace('address=029438 opcode=1004', 'address=0294c3 opcode=f84a')
        text = text.replace('address=029500 opcode=f830', 'address=0294fd opcode=60f8')
        with self.assertRaisesRegex(ValueError, 'no observed cleanup'):
            check(text)
        # An unrelated selected site cannot stand in for the branch outcome.
        text = text.replace('address=0294fd opcode=60f8', 'address=029471 opcode=1005')
        with self.assertRaisesRegex(ValueError, 'unresolved length'):
            check(text)


class LifecycleRestartTests(unittest.TestCase):
    def fixture(self):
        lines = ['mu4_playback_execute_entry: address=026d62 opcode=fd00 t=0.2']
        for ordinal, (time, value) in enumerate(((1, '0004'), (9, '0005'),
                                                (10, '0007'), (10.1, '0003'),
                                                (10.2, '0004')), 1):
            if ordinal == 2:
                lines.append('mu4_cleanup_ingress: ordinal=42 address=02956f observer_writes=0')
            if ordinal == 3:
                lines.append('mu4_playback_execute_entry: address=026d62 opcode=fd00 t=9.5')
            lines.append(f'mu4_playback_lifecycle_write: address=373e value={value} '
                         f'mask=ffff ordinal={ordinal} t={time} observer_writes=0')
        lines.append('mu4_playback_lifecycle_terminal: illegal=0 lifecycle_writes=5 '
                     'observer_writes=0 audio_acceptance=0')
        return '\n'.join(lines)

    def test_ordered_restart(self):
        result = check_lifecycle_restart(self.fixture())
        self.assertEqual(result['resident_return_time'], 9.5)
        self.assertEqual(result['lifecycle_values'], ['0004', '0005', '0007', '0003', '0004'])

    def test_reject_missing_or_misordered_evidence(self):
        for before, after in [('lifecycle_writes=5', 'lifecycle_writes=6'),
                              ('value=0007', 'value=0005'), ('t=9.5', 't=10.5'),
                              ('t=10.1', 't=nan'), ('mask=ffff', 'mask=00ff'),
                              ('observer_writes=0', 'observer_writes=1'),
                              ('address=02956f', 'address=029500')]:
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_lifecycle_restart(self.fixture().replace(before, after))


if __name__ == '__main__':
    unittest.main()
