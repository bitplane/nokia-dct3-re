import unittest

from tools.mbus_2100_terminal_trace_check import check


GOOD = """
mbus_terminal: phone_frame type=d0 command=01 length=9 sequence=01 startup=0 t=0.204747
mbus_terminal: tx_complete type=7f length=6
mbus_terminal: tx_complete type=d0 length=9
mbus_terminal: phone_frame type=d0 command=05 length=9 sequence=02 startup=1 t=0.276244
mbus_terminal: tx_complete type=7f length=6
"""


class Mbus2100TerminalTraceCheckTest(unittest.TestCase):
    def test_complete_exchange(self):
        check(GOOD)

    def test_rejects_missing_application_reply(self):
        with self.assertRaises(SystemExit):
            check(GOOD.replace("command=05", "command=04"))

    def test_rejects_wrong_order(self):
        lines = GOOD.strip().splitlines()
        with self.assertRaises(SystemExit):
            check("\n".join(lines[:2] + lines[3:] + lines[2:3]))

    def test_rejects_late_registration_phase(self):
        with self.assertRaises(SystemExit):
            check(GOOD.replace("t=0.204747", "t=0.249605"))


if __name__ == "__main__":
    unittest.main()
