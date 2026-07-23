import unittest

from tools.ccont_mask_pending_check import check


class CcontMaskPendingCheckTest(unittest.TestCase):
    def test_pending_then_unmask(self):
        text = "\n".join((
            "ccont_rtc: event=mask_write data=f8 status=03 t=2.0",
            "ccont_rtc: event=read reg=0e data=b3 t=4.2",
            "ccont_rtc: event=mask_write data=78 status=b3 t=4.3",
            "ccont_route: state=1 irq_line=2 pending=004 mask=00 ctrl=00 t=4.3",
        ))
        self.assertEqual([], check(text))

    def test_rejects_early_delivery(self):
        text = "\n".join((
            "ccont_rtc: event=mask_write data=f8 status=03 t=2.0",
            "ccont_route: state=1 irq_line=2 pending=004 mask=00 ctrl=00 t=3.0",
            "ccont_rtc: event=read reg=0e data=83 t=4.2",
            "ccont_rtc: event=mask_write data=78 status=83 t=4.3",
        ))
        self.assertIn("CCONT IRQ asserted while alarm remained masked", check(text))


if __name__ == "__main__":
    unittest.main()
