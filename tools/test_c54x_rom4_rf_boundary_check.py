import unittest

from tools.c54x_rom4_rf_boundary_check import check


def summary(*, frames=6497, reads=0, pairs=0, ifr="0001", imr="035e"):
    return (
        "rom4_interface_summary: completion_strobes=2 mailbox_writes=3 "
        f"slot_expiries=0 frame_expiries={frames} rf_reads={reads} "
        f"rf_tune_pairs={pairs} pc=31a5 pmst=0020 ifr={ifr} imr={imr} "
        "mode_aa=0000 mode_ac=0000\n"
    )


class C54xRom4RfBoundaryCheckTest(unittest.TestCase):
    def test_accepts_quantified_masked_int0_boundary(self):
        self.assertEqual(check(summary())["frame_expiries"], 6497)

    def test_rejects_short_run(self):
        with self.assertRaisesRegex(ValueError, "only 100"):
            check(summary(frames=100))

    def test_rejects_receiver_activity(self):
        with self.assertRaisesRegex(ValueError, "boundary has changed"):
            check(summary(reads=1))

    def test_rejects_unexpected_interrupt_state(self):
        with self.assertRaisesRegex(ValueError, "unexpected terminal IMR"):
            check(summary(imr="53ff"))
        with self.assertRaisesRegex(ValueError, "INT0 is not pending"):
            check(summary(ifr="0000"))


if __name__ == "__main__":
    unittest.main()
