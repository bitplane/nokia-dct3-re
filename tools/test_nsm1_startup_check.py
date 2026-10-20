import unittest
from tools.nsm1_startup_check import check


class StartupTest(unittest.TestCase):
    def test_full_horizon_and_real_entry_counts(self):
        trace='\n'.join(f'nsm1_startup_sample: t={time:.6f} entries=280ac0:0,280b0c:0,2813a0:0,2bf2e6:0,2239f2:0,2bc4d4:0,2af080:2 reports=0e charger_state=00 notification=01 sim_ready=00 selector=ff observer_writes=0' for time in range(10,61,10))
        self.assertEqual(check(trace)['observed_seconds'],60)
        for bad in (trace.replace('2bc4d4:0','2bc4d4:1'),trace.replace('reports=0e','reports=0f'),trace.replace('t=60.000000','t=59.000000'),trace.replace('observer_writes=0','observer_writes=1',1)):
            with self.assertRaises(ValueError):check(bad)


if __name__=='__main__':unittest.main()
