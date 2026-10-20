import unittest

from tools.mad2_sleep_trace_check import check


TIMER = """mad2_clock: event=W off=0d data=0e old=0c counter=0001 pc=1 t=0.1
mad2_sleep: event=request clocks=0c timer0=0001 timer1=1000 t=0.1
mad2_sleep: event=wake domain=FIQ fiq=020 irq=000 timer0=0002 timer1=7fff t=0.2
mad2_clock: event=W off=0d data=0c old=0c counter=0002 pc=1 t=0.3
"""


class Mad2SleepTraceCheckTest(unittest.TestCase):
	def gated(self, masked=False):
		source = "keypad-masked-gated" if masked else "keypad-gated"
		time = "0.27" if masked else "0.25"
		text = TIMER.replace("domain=FIQ fiq=020 irq=000", "domain=IRQ fiq=000 irq=001")
		text = text.replace("t=0.2", "t=" + time)
		text += f"mad2_wake_fixture: phase=edge source={source} ctrl=0a mask={'ff' if masked else 'fe'} t=0.25\n"
		if masked:
			text += f"mad2_wake_fixture: phase=unmask source={source} ctrl=0a mask=ff t=0.27\n"
		text += f"mad2_wake_fixture: phase=qualified source={source} ctrl=0a mask=fe t={time}\n"
		text += f"mad2_wake_fixture: phase=delivery source={source} ctrl=0a mask=fe t=0.29\n"
		return source, text

	def test_local_keypad_wakes_with_cpu_delivery_disabled(self):
		source, text = self.gated()
		self.assertEqual([], check(text, source))

	def test_extended_fiq_wakes_without_enabling_delivery(self):
		text = TIMER.replace("fiq=020", "fiq=100").replace("timer1=7fff t=0.2", "timer1=7fff t=0.2 ctrl=0a")
		self.assertEqual([], check(text, "fiq8-gated"))
		self.assertTrue(check(text.replace("ctrl=0a", "ctrl=05"), "fiq8-gated"))
		text += "mad2_interrupt: event=route domain=FIQ active=1 pending=100 mask=ff ctrl=0a t=0.2\n"
		self.assertTrue(check(text, "fiq8-gated"))

	def test_masked_keypad_waits_for_local_unmask(self):
		source, text = self.gated(masked=True)
		self.assertEqual([], check(text, source))
		self.assertTrue(check(text.replace("timer1=7fff t=0.27", "timer1=7fff t=0.25"), source))

	def test_disabled_gate_is_not_repaired_by_the_fixture(self):
		source, text = self.gated()
		self.assertTrue(check(text.replace("ctrl=0a", "ctrl=05"), source))
		self.assertTrue(check(text.replace("phase=qualified", "phase=missing"), source))

	def test_wake_does_not_deliver_a_globally_disabled_irq(self):
		source, text = self.gated()
		text += "mad2_interrupt: event=route domain=IRQ active=1 pending=001 mask=fe ctrl=0a t=0.25\n"
		self.assertIn("IRQ delivery occurred before the disabled global gate was released", check(text, source))

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
