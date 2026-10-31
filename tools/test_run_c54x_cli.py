"""Firmware runner selection and process accounting, without launching MAME."""
import contextlib
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools.run_c54x_core import main, EXTRA_PROFILES


class FirmwareCliTests(unittest.TestCase):
    def test_profiles_select_owner_and_preserve_process_status(self):
        for profile, machine in (('rom4-context', 'tms54test'), ('cold-pipeline', 'tms54rom4')):
            for verbose in (False, True):
                with self.subTest(profile=profile, verbose=verbose), tempfile.TemporaryDirectory() as directory:
                    command = []
                    def execute(args, **kwargs):
                        command.extend(args)
                        text = '\n'.join(EXTRA_PROFILES[profile]) + '\n'
                        if verbose:
                            text += 'Enter sdlwindow_exit\nLeave sdlwindow_exit\n'
                        kwargs['stdout'].write(text)
                        return SimpleNamespace(returncode=3, wait=lambda timeout=None: 3)
                    argv = ['runner', '--mame', 'mame/mame', '--rompath', 'mame/roms',
                            '--output-root', directory, '--profile', profile]
                    if verbose:
                        argv.append('--verbose')
                    with patch('sys.argv', argv), patch('tools.process_runner.subprocess.Popen', execute), \
                            contextlib.redirect_stdout(io.StringIO()):
                        self.assertEqual(main(), 0)
                    self.assertEqual(command[1], machine)
                    self.assertEqual(command[command.index('-bios') + 1], profile)
                    self.assertEqual('-verbose' in command, verbose)
                    status = json.loads(next(Path(directory).glob('*/exit_status.json')).read_text())
                    self.assertEqual(status, {'returncode': 3, 'command': command, 'interrupted': False})

    def test_invalid_owner_does_not_launch_or_create_run(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'runs'
            with patch('sys.argv', ['runner', '--mame', 'mame/mame', '--rompath', 'mame/roms',
                                    '--output-root', str(output)]), \
                    patch('tools.run_c54x_core.machine_for_profile', side_effect=ValueError('missing owner')), \
                    patch('tools.run_c54x_core.run_emulator') as execute, \
                    contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                main()
            execute.assert_not_called()
            self.assertFalse(output.exists())

    def test_completion_marker_does_not_override_failed_process(self):
        with tempfile.TemporaryDirectory() as directory:
            def execute(args, **kwargs):
                kwargs['stdout'].write('\n'.join(EXTRA_PROFILES['rom4-context']) + '\n')
                return SimpleNamespace(returncode=0, wait=lambda timeout=None: 0)
            with patch('sys.argv', ['runner', '--mame', 'mame/mame', '--rompath', 'mame/roms',
                                    '--output-root', directory]), \
                    patch('tools.process_runner.subprocess.Popen', execute), \
                    contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(main(), 1)
            self.assertEqual(json.loads(next(Path(directory).glob('*/exit_status.json')).read_text())['returncode'], 0)


if __name__ == '__main__':
    unittest.main()
