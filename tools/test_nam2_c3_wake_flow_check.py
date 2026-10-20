import unittest

from tools.nam2_c3_wake_flow_check import check_flow


class WakeFlowTest(unittest.TestCase):
    def setUp(self):
        self.trace='nam2_clock_early_key: down=1 t=8.95 physical_only=1\nnam2_clock_early_key: down=0 t=9.15 physical_only=1\n'
        self.trace+='mad2_sleep: event=wake domain=IRQ fiq=008 irq=001 timer0=0484 timer1=24f4 t=8.950000000 ctrl=0a\n'
        for i in range(64):
            pc=(0x2f85fa,0x2f860e,0x2f866a,0x2f8688)[i%4]
            self.trace+=f'nam2_wake_flow: index={i+1} pc={pc:08x} lr=00304c4d sp=0010fef4 cpsr=2000003f r0=00000001 ctrl=0a t={8.950001+i*.000001:.9f} observer_writes=0\n'
        self.trace+='nam2_idle_sample: t=19.000000 pc=002fac6a ctrl=0a fmask=08 imask=a2 fiq=19 irq=01 nest=00\n'

    def test_bounded_controller_not_cpu_mask_failure(self):
        result=check_flow(self.trace)
        self.assertFalse(result['cpu_irq_fiq_masked'])
        self.assertFalse(result['working_idle_or_late_input'])
        self.assertEqual(result['sampled_supervisor_iterations'],16)

    def test_rejects_masked_cpu_or_changed_continuation(self):
        for trace in (self.trace.replace('cpsr=2000003f','cpsr=200000bf',1),
                      self.trace.replace('cpsr=2000003f','cpsr=2000007f',1),
                      self.trace.replace('pc=002f866a','pc=002f864a',1),
                      self.trace.replace('ctrl=0a','ctrl=05',1)):
            with self.assertRaises(ValueError):check_flow(trace)

    def test_rejects_missing_physical_or_late_evidence(self):
        for trace in (self.trace.replace('down=1 t=8.95','down=1 t=9.00'),
                      self.trace.replace('domain=IRQ','domain=FIQ'),
                      self.trace.replace('index=64','index=63'),
                      self.trace.replace('irq=01 nest=00','irq=00 nest=00'),
                      self.trace+'[LUA ERROR] failed'):
            with self.assertRaises(ValueError):check_flow(trace)


if __name__=='__main__':unittest.main()
