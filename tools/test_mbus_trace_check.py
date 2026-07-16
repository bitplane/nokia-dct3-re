import unittest

from tools import mbus_trace_check


class MbusTraceCheckTest(unittest.TestCase):
    def test_parse(self):
        text = "\n".join((
            "mbus: event=W off=18 data=4c old=0c pc=002ab064",
            "mbus_device: event=rx_ready data=a5 ctrl=4c status=e7",
            "mbus_fixture: data=a5 accepted=1",
            "mbus_device: event=rx_read data=a5 ctrl=4c status=c7",
            "mbus: event=W off=08 data=04 old=04 pc=002aae20",
        ))
        result = mbus_trace_check.parse(text)
        self.assertEqual(result["accesses"][0][:3], ("W", 0x18, 0x4C))
        self.assertEqual(result["rx_ready"], 1)
        self.assertTrue(result["fixture_accepted"])
        self.assertTrue(result["fiq2_ack"])


if __name__ == "__main__":
    unittest.main()
