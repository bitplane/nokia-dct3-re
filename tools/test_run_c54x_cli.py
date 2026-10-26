import contextlib
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
from tools.run_c54x_core import main, REQUIRED, COMPLETION, NAMED_SCOPES
from tools.c54x_harness_inventory import BASELINE


class CoreCliTests(unittest.TestCase):
    def test_named_scopes_keep_their_bios_and_banked_result_contracts(self):
        cases = [('--bus-registers', 'bus-registers', 'BUS_REGISTER'),
                 ('--bus-io', 'bus-io', 'BUS_IO'),
                 ('--atomic-io', 'bus-io-atomic', 'ATOMIC_IO'),
                 ('--compose', 'bus-io-compose', 'COMPOSE'),
                 ('--bus-stm', 'bus-stm', 'BUS_STM'),
                 ('--mapped-read-a', 'read-a-mapped', 'READ_A_MAPPED'),
                 ('--dp-absolute', 'dp-absolute', 'DP_ABSOLUTE')]
        self.assertEqual(set(NAMED_SCOPES), {flag[2:].replace('-', '_') for flag, _, _ in cases})
        bank = json.loads(BASELINE.read_text())
        with tempfile.TemporaryDirectory() as directory:
            for flag, bios, scope in cases:
                with self.subTest(flag=flag):
                    commands = []

                    def execute(command, run, output):
                        commands.append(command)
                        contract = bank['scopes'][scope]
                        output.write('\n'.join(contract['required'] + [contract['completion']]) + '\n')
                        return 3

                    argv = ['run_c54x_core', '--mame', 'mame/mame', '--rompath', 'mame/roms',
                            '--output-root', directory, flag]
                    with patch('sys.argv', argv), patch('tools.run_c54x_core.run_emulator', execute), \
                         contextlib.redirect_stdout(io.StringIO()):
                        self.assertEqual(main(), 0)
                    self.assertEqual(commands[0][1], 'tms54test')
                    self.assertEqual(commands[0][-2:], ['-bios', bios])

    def test_missing_bios_owner_does_not_launch_or_create_a_run(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'runs'
            argv = ['run_c54x_core', '--mame', 'mame/mame', '--rompath', 'mame/roms',
                    '--output-root', str(output), '--profile', 'negate']
            with patch('sys.argv', argv), \
                 patch('tools.run_c54x_core.machine_for_profile', side_effect=ValueError('missing owner')), \
                 patch('tools.run_c54x_core.run_emulator') as execute, \
                 contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                main()
            self.assertEqual(error.exception.code, 2)
            execute.assert_not_called()
            self.assertFalse(output.exists())

    def test_verbose_inventory_is_opt_in_and_retains_actual_exit(self):
        for verbose in (False, True):
            with self.subTest(verbose=verbose), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                command = []
                def execute(args, **kwargs):
                    command.extend(args)
                    text = '\n'.join(REQUIRED + [COMPLETION]) + '\n'
                    if verbose:
                        text += 'Enter sdlwindow_exit\nLeave sdlwindow_exit\n'
                    kwargs['stdout'].write(text)
                    return SimpleNamespace(returncode=3, wait=lambda timeout=None: 3)
                argv = ['run_c54x_core', '--mame', 'mame/mame', '--rompath', 'mame/roms', '--output-root', str(root)]
                if verbose:
                    argv.append('--verbose')
                with patch('sys.argv', argv), patch('tools.process_runner.subprocess.Popen', side_effect=execute), contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(main(), 0)
                self.assertEqual('-verbose' in command, verbose)
                status = json.loads(next(root.glob('*/exit_status.json')).read_text())
                self.assertEqual(status, {'returncode': 3, 'command': command, 'interrupted': False})

    def test_failed_completion_cannot_be_credited_from_markers_alone(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def execute(args, **kwargs):
                kwargs['stdout'].write('\n'.join(REQUIRED + [COMPLETION]) + '\n')
                return SimpleNamespace(returncode=0, wait=lambda timeout=None: 0)
            argv = ['run_c54x_core', '--mame', 'mame/mame', '--rompath', 'mame/roms', '--output-root', str(root)]
            with patch('sys.argv', argv), patch('tools.process_runner.subprocess.Popen', side_effect=execute), \
                 contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(main(), 1)
            self.assertEqual(json.loads(next(root.glob('*/exit_status.json')).read_text())['returncode'], 0)


if __name__ == '__main__':
    unittest.main()
