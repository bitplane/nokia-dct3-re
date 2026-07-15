import unittest

from tools.mad2_interrupt_trace_check import (
    check_fiq8,
    check_mask,
    check_overlap,
    parse,
)


class Mad2InterruptTraceCheckTest(unittest.TestCase):
    def test_overlap_and_independent_ack(self):
        events = parse("""
mad2_interrupt: event=levels domain=IRQ keypad=1 ccont=1 pending_before=040 pending_after=041 t=1
mad2_interrupt: event=route domain=IRQ active=1 pending=041 mask=8e ctrl=05 t=1
mad2_interrupt: event=ack domain=IRQ mask=001 pending_before=041 pending_after=040 t=1
""")
        errors, counts = check_overlap(events, "final_irq_status=00\n")
        self.assertEqual([], errors)
        self.assertEqual(1, counts["independent_acks"])

    def test_overlap_requires_both_sources(self):
        events = parse("mad2_interrupt: event=levels domain=IRQ keypad=1 ccont=0 pending_before=000 pending_after=001 t=1")
        errors, _ = check_overlap(events, "final_irq_status=00\n")
        self.assertIn("keypad IRQ0 and CCONT IRQ6 were never pending simultaneously", errors)

    def test_masked_pending_delivery(self):
        events = parse("""
mad2_interrupt: event=reg_W off=0b data=8f fiq=000 irq=000 fiqmask=ee irqmask=8f ctrl=00 extctrl=00 t=1
mad2_interrupt: event=levels domain=IRQ keypad=1 ccont=0 pending_before=000 pending_after=001 t=2
mad2_interrupt: event=reg_W off=0b data=8e fiq=000 irq=001 fiqmask=ee irqmask=8e ctrl=00 extctrl=00 t=3
mad2_interrupt: event=route domain=IRQ active=1 pending=001 mask=8e ctrl=04 t=3
mad2_interrupt: event=ack domain=IRQ mask=001 pending_before=001 pending_after=000 t=3
""")
        errors, counts = check_mask(events)
        self.assertEqual([], errors)
        self.assertEqual(1, counts["clearing_acks"])

    def test_fiq8_extended_route(self):
        events = parse("""
mad2_interrupt: event=assert domain=FIQ line=8 pending_before=000 pending_after=100 t=1
mad2_interrupt: event=reg_R off=16 data=07 fiq=100 irq=000 fiqmask=ee irqmask=8e ctrl=00 extctrl=05 t=2
mad2_interrupt: event=route domain=FIQ active=1 pending=100 mask=ee ctrl=01 extctrl=01 t=3
mad2_interrupt: event=route domain=FIQ active=0 pending=100 mask=ee ctrl=00 extctrl=01 t=3
mad2_interrupt: event=ack domain=FIQ mask=100 pending_before=100 pending_after=000 t=3
""")
        errors, counts = check_fiq8(events)
        self.assertEqual([], errors)
        self.assertEqual(1, counts["projected_reads"])


if __name__ == "__main__":
    unittest.main()
