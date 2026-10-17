import unittest

from tools.mu4_cleanup_control_check import check


def fixture():
    return '\n'.join([
        'mu4_cleanup_control_scope: address=aa1a boundary=data_bus context_pc=not_owner '
        'observer_writes=0 audio_acceptance=0',
        'mu4_cleanup_control_write: ordinal=1 value=0001 mask=ffff t=0.2 '
        'context_pc=02788b observer_writes=0',
        'mu4_cleanup_control_read: ordinal=1 value=0001 t=9.7 '
        'context_pc=029500 observer_writes=0',
        'mu4_cleanup_control_terminal: writes=1 reads=1 capped=0 illegal=0 t=10 '
        'observer_writes=0 audio_acceptance=0'])


class CleanupControlTest(unittest.TestCase):
    def test_binds_read_to_preceding_bus_write_without_claiming_ownership(self):
        result = check(fixture())
        self.assertEqual(result['read_observations'][0]['preceding_write_time'], 0.2)
        self.assertEqual(result['read_observations'][0]['value'], '0001')

    def test_rejects_incomplete_or_ambiguous_observations(self):
        for before, after in [('capped=0', 'capped=1'), ('illegal=0', 'illegal=1'),
                              ('writes=1', 'writes=2'), ('reads=1', 'reads=0'),
                              ('mask=ffff', 'mask=00ff'), ('t=9.7', 't=0.1'),
                              ('t=9.7', 't=0.2'), ('t=9.7', 't=11'),
                              ('value=0001 t=9.7', 'value=0000 t=9.7'),
                              ('ordinal=1 value=0001 t=9.7', 'ordinal=2 value=0001 t=9.7'),
                              ('context_pc=not_owner', 'context_pc=owner'),
                              ('observer_writes=0', 'observer_writes=1')]:
            with self.subTest(before=before, after=after), self.assertRaises(ValueError):
                check(fixture().replace(before, after))


if __name__ == '__main__':
    unittest.main()
