import unittest

from tools.nsb3_boot_check import check_samples, check_startup_trace


class Nsb3BootTest(unittest.TestCase):
    def test_idle_return_is_not_promoted_to_boot_success(self):
        trace = ('nsb3_startup: site=0028e016 r0=00000000\n'
                 'nsb3_idle_state: latch=01 companion=00 ctrl=05 fiq=00 irq=00 fmask=e3 imask=de selector=0004 program0=f7bb')
        check_startup_trace(trace)
        for changed in (trace.replace('r0=00000000', 'r0=00000001'),
                        trace.replace('selector=0004', 'selector=0003'), ''):
            with self.assertRaises(ValueError):
                check_startup_trace(changed)

    def test_exact_bounded_sequence(self):
        trace = '\n'.join(f'nsb3_boot: second={n} pc=002a8ac8 lr=002a8a89 '
                          'selector=0004 catalogue3=002b7168 catalogue4=002bd0a8 '
                          'dsp_control=02 shared0=0000 shared2=ffff observer_writes=0'
                          for n in range(1, 6))
        check_samples(trace)
        for changed in (trace.replace('selector=0004', 'selector=0003'),
                        trace.replace('observer_writes=0', 'observer_writes=1'),
                        trace.replace('second=5', 'second=6'), trace + '\n' + trace):
            with self.assertRaises(ValueError):
                check_samples(changed)


if __name__ == '__main__':
    unittest.main()
