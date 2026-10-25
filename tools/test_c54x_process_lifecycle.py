import json
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.process_runner import run_emulator


class ProcessLifecycleTest(unittest.TestCase):
    def test_real_completion_retains_intentional_nonzero_exit(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            with (run / 'console.log').open('w') as output:
                status = run_emulator([sys.executable, '-c', 'raise SystemExit(3)'], run, output)
            self.assertEqual(status, 3)
            result = json.loads((run / 'exit_status.json').read_text())
            self.assertEqual(result['returncode'], 3)
            self.assertFalse(result['interrupted'])

    def test_real_timeout_kills_and_reaps_child(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            with (run / 'console.log').open('w') as output, self.assertRaises(subprocess.TimeoutExpired):
                run_emulator([sys.executable, '-c', 'import time; time.sleep(30)'], run, output, timeout=0.05)
            result = json.loads((run / 'exit_status.json').read_text())
            self.assertEqual(result['returncode'], -signal.SIGKILL)
            self.assertTrue(result['interrupted'])

    def test_sigterm_cleans_group_and_restores_handler(self):
        previous = signal.getsignal(signal.SIGTERM)
        with tempfile.TemporaryDirectory() as directory, \
                patch('tools.process_runner.subprocess.Popen') as spawn, \
                patch('tools.process_runner.os.killpg') as kill:
            child = spawn.return_value
            child.pid = 1234
            child.returncode = -signal.SIGKILL
            def interrupt(*args, **kwargs):
                signal.getsignal(signal.SIGTERM)(signal.SIGTERM, None)
            child.wait.side_effect = lambda *a, **k: interrupt() if child.wait.call_count == 1 else 0
            with self.assertRaises(SystemExit) as error:
                run_emulator(['mame'], Path(directory), None)
            self.assertEqual(error.exception.code, 143)
            kill.assert_called_once_with(1234, signal.SIGKILL)
            self.assertTrue(spawn.call_args.kwargs['start_new_session'])
            self.assertEqual(child.wait.call_count, 2)
            self.assertTrue(json.loads((Path(directory) / 'exit_status.json').read_text())['interrupted'])
        self.assertIs(signal.getsignal(signal.SIGTERM), previous)


if __name__ == '__main__':
    unittest.main()
