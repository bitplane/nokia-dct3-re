from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.run_noki8290_sip_media import main


class RunnerTests(unittest.TestCase):
    def run_fixture(self, mode):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'source/nsb7hle'
            source.mkdir(parents=True)
            (source / 'sim_card').write_bytes(b'own-card')
            (source / 'flash').write_bytes(b'own-journal')
            arguments = ['runner', str(root / 'run'), '--nvram-source', str(source.parent),
                         '--pjsua', 'unused', mode]
            with patch('sys.argv', arguments), \
                    patch('tools.run_noki8290_sip_media.check_source') as card, \
                    patch('tools.run_noki8290_sip_media.inspect',
                          return_value=(None, None, {'checksum_valid': True, 'prefix_valid': True})), \
                    patch('tools.run_noki8290_sip_media.subprocess.run') as execute:
                main()
            card.assert_called_once_with(b'own-card')
            self.assertEqual((root / 'run/initial_sim_card.bin').read_bytes(), b'own-card')
            self.assertEqual((source / 'flash').read_bytes(), b'own-journal')
            return execute.call_args_list[0].args[0], execute.call_args_list[1].args[0]

    def test_unanswered_phases_do_not_request_media_acceptance(self):
        for mode, flag, script in (
                ('--restore-outgoing-pending', '--restore-outgoing', 'nsb7_sip_outgoing_restore.lua'),
                ('--restore-incoming-alerting', '--restore-call', 'nsb7_sip_incoming_alerting_restore.lua')):
            with self.subTest(mode=mode):
                command, checker = self.run_fixture(mode)
                self.assertNotIn('--signaling-only', command)
                self.assertIn(flag, command)
                self.assertTrue(command[-1].endswith(script))
                self.assertNotIn('--media', checker)
                self.assertIn('--clocked-unanswered', checker)
                self.assertIn(mode, checker)
                if 'outgoing' in mode:
                    self.assertEqual(command[command.index('--sip-response') + 1], '180')
                else:
                    self.assertEqual(command[command.index('--restore-phase') + 1], 'alerting')
                    self.assertIn('--ready-file', command)

    def test_connected_restoration_requires_media_acceptance(self):
        for mode in ('--restore-outgoing-media', '--restore-incoming-media'):
            with self.subTest(mode=mode):
                command, checker = self.run_fixture(mode)
                self.assertNotIn('--signaling-only', command)
                self.assertIn('--media', checker)
                self.assertIn('-throttle', command)


if __name__ == '__main__':
    unittest.main()
