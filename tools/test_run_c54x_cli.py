import contextlib
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
from tools.run_c54x_core import main, REQUIRED, COMPLETION


class CoreCliTests(unittest.TestCase):
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
                with patch('sys.argv', argv), patch('tools.run_c54x_core.subprocess.Popen', side_effect=execute), contextlib.redirect_stdout(io.StringIO()):
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
            with patch('sys.argv', argv), patch('tools.run_c54x_core.subprocess.Popen', side_effect=execute), \
                 contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(main(), 1)
            self.assertEqual(json.loads(next(root.glob('*/exit_status.json')).read_text())['returncode'], 0)


if __name__ == '__main__':
    unittest.main()
