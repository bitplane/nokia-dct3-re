from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

from tools.verify_etsi_efr_reference import observe_erasures


class EfrReferenceObserverTests(unittest.TestCase):
    def run_observer(self, *, corrupt_pcm=False, unstable_state=False, corrupt_binary=False):
        with TemporaryDirectory() as directory:
            source = Path(directory)
            (source / 'erasure.out').write_bytes(b'unchanged PCM')
            (source / 'decoder').write_bytes(b'original verified executable')
            runs = []

            def execute(command, **kwargs):
                if command[0] == 'make':
                    (source / 'decoder_observed').write_bytes(b'observer executable')
                    if corrupt_binary:
                        (source / 'decoder').write_bytes(b'replaced verified executable')
                else:
                    (source / command[2]).write_bytes(b'changed' if corrupt_pcm else b'unchanged PCM')
                    kwargs['stderr'].write('different output filename banner\n' +
                        ('EFRSTATE altered\n' if unstable_state and runs else 'EFRSTATE stable\n'))
                    runs.append(command)

            commands = []
            with patch('tools.verify_etsi_efr_reference.subprocess.run', side_effect=execute), \
                    patch('tools.c54x_rom4_efr_state_check.parse') as parse, \
                    patch('tools.c54x_rom4_efr_state_check.parse_policy') as policy:
                result = observe_erasures(source, commands, None)
            self.assertEqual(parse.call_count, 2)
            self.assertEqual(policy.call_count, 2)
            self.assertEqual(len(commands), 3)
            self.assertEqual((source / 'decoder').read_bytes(), b'original verified executable')
            self.assertEqual((source / 'erasure_state.records').read_bytes(), b'EFRSTATE stable\n')
            return result

    def test_reference_observer_preserves_pcm_and_original_binary(self):
        result = self.run_observer()
        self.assertTrue(result['reference_pcm_unchanged'])
        self.assertTrue(result['fresh_process_replay'])
        for key in ('state_records_sha256', 'wrapper_sha256', 'rules_sha256', 'executable_sha256'):
            self.assertEqual(len(result[key]), 64)

    def test_reference_observer_rejects_changed_pcm(self):
        with self.assertRaisesRegex(ValueError, 'changed reference PCM'):
            self.run_observer(corrupt_pcm=True)

    def test_reference_observer_rejects_unstable_records(self):
        with self.assertRaisesRegex(ValueError, 'differ across fresh processes'):
            self.run_observer(unstable_state=True)

    def test_reference_observer_rejects_replacing_verified_binary(self):
        with self.assertRaisesRegex(ValueError, 'replaced the verified decoder'):
            self.run_observer(corrupt_binary=True)


if __name__ == '__main__':
    unittest.main()
