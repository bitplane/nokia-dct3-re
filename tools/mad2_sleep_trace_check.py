#!/usr/bin/env python3
"""Validate MAD2 clock-stop requests and interrupt-driven wake-up."""

import argparse
import re
from pathlib import Path


REQUEST = re.compile(
	r"mad2_sleep: event=request clocks=(?P<clocks>[0-9a-f]{2}) "
	r"timer0=(?P<timer0>[0-9a-f]{4}) timer1=(?P<timer1>[0-9a-f]{4}) "
	r"t=(?P<time>[0-9.]+)")
WAKE = re.compile(
	r"mad2_sleep: event=wake domain=(?P<domain>FIQ|IRQ) "
	r"fiq=(?P<fiq>[0-9a-f]{3}) irq=(?P<irq>[0-9a-f]{3}) "
	r"timer0=(?P<timer0>[0-9a-f]{4}) timer1=(?P<timer1>[0-9a-f]{4}) "
	r"t=(?P<time>[0-9.]+)")


def check(log_text, source, summary_text=None):
	errors = []
	requests = list(REQUEST.finditer(log_text))
	wakes = list(WAKE.finditer(log_text))
	if len(requests) != 1:
		errors.append(f"expected one clock-stop request, observed {len(requests)}")
	expected_domain = "FIQ" if source == "timer1" else "IRQ"
	matching = [wake for wake in wakes if wake.group("domain") == expected_domain]
	if not matching:
		errors.append(f"no {expected_domain} wake followed the clock-stop request")
	elif requests and float(matching[0].group("time")) <= float(requests[0].group("time")):
		errors.append("wake did not occur after the clock-stop request")
	if matching and source == "timer1":
		if not (int(matching[0].group("fiq"), 16) & 0x020):
			errors.append("Timer-1 wake did not carry FIQ5/status bit 0x020")
		if int(matching[0].group("timer1"), 16) == int(requests[0].group("timer1"), 16):
			errors.append("Timer-1 did not advance to the programmed wake destination")
	if matching and requests:
		request_timer0 = int(requests[0].group("timer0"), 16)
		wake_timer0 = int(matching[0].group("timer0"), 16)
		if request_timer0 == wake_timer0:
			errors.append("Timer 0 did not advance while the ARM clock was stopped")
	if matching and source == "keypad" and not (int(matching[0].group("irq"), 16) & 0x001):
		errors.append("keypad wake did not carry IRQ0/status bit 0x001")
	if "mad2_clock: event=W off=0d data=0e old=0c" not in log_text:
		errors.append("fixture did not issue the evidenced 0x0e clock-stop command")
	if "mad2_clock: event=W off=0d data=0c old=0c" not in log_text:
		errors.append("clock-stop command bit did not read back auto-cleared")
	if summary_text is not None:
		if "state_roundtrip=pass" not in summary_text:
			errors.append("save/load round trip did not pass while the MCU clock was stopped")
		# The save/load occurs while the fixed 0x7fff destination is still
		# pending. A subsequent wake plus the round-trip predicate proves that
		# the device timer and sleeping state survived restoration; unlike the
		# former accelerated-clock fixture, there is no pre-load wake to duplicate.
		if not matching:
			errors.append("restored pending sleep did not reach its Timer-1 wake")
	return errors


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("log", type=Path)
	parser.add_argument("--source", choices=("timer1", "keypad"), required=True)
	parser.add_argument("--summary", type=Path)
	args = parser.parse_args()
	errors = check(args.log.read_text(errors="replace"), args.source,
		args.summary.read_text(errors="replace") if args.summary else None)
	if errors:
		raise SystemExit("MAD2 sleep contract failed: " + "; ".join(errors))
	print(f"MAD2 sleep contract: clock-stop request woke through {args.source}")


if __name__ == "__main__":
	main()
