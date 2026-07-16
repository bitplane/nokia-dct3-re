import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from tools.dsp_shared_transition_census import parse, transition_role


class DspSharedTransitionCensusTest(unittest.TestCase):
	def test_roles(self):
		self.assertEqual(("shared_control_busy", "DSPIF command-4 doorbell"), transition_role(0x0E0, 1.0))
		self.assertEqual(("tx_consumer", "peer consumes committed TX packet"), transition_role(0x0A6, 1.0))

	def test_correlates_doorbell_and_consumer(self):
		with TemporaryDirectory() as directory:
			path = Path(directory) / "error.log"
			path.write_text(
				"dspif_transport: doorbell command=0004 pending=0000 t=1.000000\n"
				"dspif_transport: peer RAM W off=0e0 old=0001 data=0000 t=1.000001\n"
				"dsp_shared_read: off=0e0 data=0000 pc=00290000 t=1.000010\n"
			)
			items = parse("v600", path)
		self.assertEqual(1, len(items))
		self.assertEqual("doorbell", items[0]["trigger"]["kind"])
		self.assertEqual(0x290000, items[0]["consumer"]["pc"])


if __name__ == "__main__":
	unittest.main()
