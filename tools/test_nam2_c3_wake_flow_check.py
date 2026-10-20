import unittest

from tools.nam2_c3_wake_flow_check import check_flow,check_accounting


class WakeFlowTest(unittest.TestCase):
    def accounting_trace(self):
        contract={'base':'0x10f9f4','increment':50,'divisor':79,'stores':[
            {'address':'0x2fadac','target':'0x10f9fa','is_mad2_mmio':False},
            {'address':'0x2fadce','target':'0x10f9fa','is_mad2_mmio':False}]}
        state=0;reduce=False;trace=''
        for index in range(16):
            pc=0x2fadce if reduce else 0x2fadac
            state=state%79 if reduce else (state+50)&0xffff
            trace+=f'nam2_sleep_accounting: index={index+1} pc={pc:08x} base=0010f9f4 target=0010f9fa value={state:04x} mask=0000ffff t={8.918+index*.0001:.9f} observer_writes=0\n'
            reduce=not reduce and state>=79
        return trace,contract

    def test_ram_accounting_follows_add_and_remainder(self):
        trace,contract=self.accounting_trace()
        self.assertFalse(check_accounting(trace,contract)['hardware_timer_write'])

    def test_accounting_rejects_misattribution_or_changed_arithmetic(self):
        trace,contract=self.accounting_trace()
        for bad in (trace.replace('base=0010f9f4','base=00020000',1),
                    trace.replace('target=0010f9fa','target=00020006',1),
                    trace.replace('value=0032','value=0033',1),
                    trace.replace('mask=0000ffff','mask=ffff0000',1),
                    trace.replace('index=16','index=15')):
            with self.assertRaises(ValueError):check_accounting(bad,contract)
        contract['stores'][0]['is_mad2_mmio']=True
        with self.assertRaises(ValueError):check_accounting(trace,contract)

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
