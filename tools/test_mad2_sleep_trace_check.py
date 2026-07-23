import unittest

from tools.mad2_sleep_trace_check import check


TIMER = """mad2_clock: event=W off=0d data=0e old=0c counter=0001 pc=1 t=0.1
mad2_sleep: event=request clocks=0c timer0=0001 timer1=1000 t=0.1
mad2_sleep: event=wake domain=FIQ fiq=020 irq=000 timer0=0002 timer1=7fff t=0.2
mad2_clock: event=W off=0d data=0c old=0c counter=0002 pc=1 t=0.3
"""


class Mad2SleepTraceCheckTest(unittest.TestCase):
	def test_timer1_sleep_wake_and_state_roundtrip(self):
		self.assertEqual(check(TIMER, "timer1", "state_roundtrip=pass\n"), [])

	def test_rejects_wrong_timer_wake_bit(self):
		errors = check(TIMER.replace("fiq=020", "fiq=010"), "timer1")
		self.assertIn("Timer-1 wake did not carry FIQ5/status bit 0x020", errors)

	def test_keypad_requires_irq0(self):
		keypad = TIMER.replace("domain=FIQ fiq=020 irq=000", "domain=IRQ fiq=000 irq=001")
		self.assertEqual(check(keypad, "keypad"), [])

	def test_fiq8_requires_extended_pending_bit(self):
		fiq8 = TIMER.replace("fiq=020", "fiq=100")
		self.assertEqual(check(fiq8, "fiq8"), [])
		errors = check(fiq8.replace("fiq=100", "fiq=020"), "fiq8")
		self.assertIn("FIQ8 wake did not carry extended FIQ/status bit 0x100", errors)

	def test_rejects_stopped_sleep_domain_timer(self):
		errors = check(TIMER.replace("timer0=0002 timer1=7fff", "timer0=0001 timer1=7fff"), "timer1")
		self.assertIn("Timer 0 did not advance while the ARM clock was stopped", errors)


if __name__ == "__main__":
	unittest.main()
