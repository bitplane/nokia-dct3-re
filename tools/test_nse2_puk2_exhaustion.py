import unittest
from unittest.mock import patch
import signal
import subprocess
from tools.run_nse2_puk2_exhaustion import check_transition,run_stage
from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import ATTEMPTS_OFFSET,AOC_NVRAM_SIZE


class ExhaustionTest(unittest.TestCase):
    def test_timeout_kills_the_owned_process_group(self):
        with patch('tools.process_runner.subprocess.Popen') as spawn, \
             patch('tools.process_runner.os.killpg') as kill:
            process=spawn.return_value
            process.pid=1234
            process.wait.side_effect=[subprocess.TimeoutExpired(['runner'],1),0]
            with self.assertRaises(subprocess.TimeoutExpired):
                run_stage(['runner'],'cwd',None,timeout=1)
            self.assertTrue(spawn.call_args.kwargs['start_new_session'])
            kill.assert_called_once_with(1234,signal.SIGKILL)
            self.assertEqual(process.wait.call_count,2)

    def test_failed_stage_does_not_continue(self):
        with patch('tools.process_runner.subprocess.Popen') as spawn:
            spawn.return_value.wait.return_value=2
            with self.assertRaises(subprocess.CalledProcessError):
                run_stage(['runner'],'cwd',None)

    def test_termination_cleans_children_and_restores_signal_handler(self):
        previous=signal.getsignal(signal.SIGTERM)
        def interrupted(*args,**kwargs):
            signal.getsignal(signal.SIGTERM)(signal.SIGTERM,None)
        with patch('tools.process_runner.subprocess.Popen') as spawn, \
             patch('tools.process_runner.os.killpg') as kill:
            process=spawn.return_value
            process.pid=1234
            # The second wait reaps the killed process rather than interrupting again.
            calls=0
            def wait(*args,**kwargs):
                nonlocal calls
                calls+=1
                return interrupted() if calls==1 else 0
            process.wait.side_effect=wait
            with self.assertRaises(SystemExit):
                run_stage(['runner'],'cwd',None)
            kill.assert_called_once_with(1234,signal.SIGKILL)
        self.assertIs(signal.getsignal(signal.SIGTERM),previous)

    def test_each_step_changes_only_one_retry(self):
        before=bytearray(make_profile(False).ljust(AOC_NVRAM_SIZE,b'\xff'))
        before[ATTEMPTS_OFFSET+1]=0
        for remaining in range(9,-1,-1):
            after=bytearray(before)
            after[ATTEMPTS_OFFSET+3]=remaining
            check_transition(before,after,remaining)
            bad=bytearray(after)
            bad[-1]^=1
            with self.assertRaises(ValueError):
                check_transition(before,bad,remaining)
            with self.assertRaises(ValueError):
                check_transition(before,after,remaining+1)
            before=after
        with self.assertRaises(ValueError):
            check_transition(before[:-1],before,0)


if __name__=='__main__':
    unittest.main()
