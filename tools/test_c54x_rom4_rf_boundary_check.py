import unittest

from tools.c54x_rom4_rf_boundary_check import check


def summary(*, frames=6497, reads=207232, pairs=0, port38=0, port39=0,
            ifr="0000", imr="035f"):
    return (
        "rom4_interface_summary: completion_strobes=2 mailbox_writes=3 "
        f"slot_expiries=0 frame_expiries={frames} rf_reads={reads} "
        f"rf_port32_writes={pairs} rf_port38_reads={port38} "
        f"rf_port39_reads={port39} pc=31a5 pmst=0020 ifr={ifr} imr={imr} "
        "mode_aa=0000 mode_ac=0000\n"
    )


class C54xRom4RfBoundaryCheckTest(unittest.TestCase):
    def test_accepts_quantified_int0_receiver_activation(self):
        self.assertEqual(check(summary())["frame_expiries"], 6497)

    def test_rejects_short_run(self):
        with self.assertRaisesRegex(ValueError, "only 100"):
            check(summary(frames=100))

    def test_rejects_inactive_receiver(self):
        with self.assertRaisesRegex(ValueError, "receiver did not become active"):
            check(summary(reads=0))

    def test_rejects_changed_read_cadence(self):
        with self.assertRaisesRegex(ValueError, "RF read cadence changed"):
            check(summary(reads=207231))

    def test_rejects_unexpected_burst_port_activity(self):
        with self.assertRaisesRegex(ValueError, "parallel burst path"):
            check(summary(port38=1))
        with self.assertRaisesRegex(ValueError, "parallel burst path"):
            check(summary(port39=1))

    def test_rejects_unexpected_interrupt_state(self):
        with self.assertRaisesRegex(ValueError, "unexpected terminal IMR"):
            check(summary(imr="53ff"))
        with self.assertRaisesRegex(ValueError, "INT0 remains pending"):
            check(summary(ifr="0001"))


if __name__ == "__main__":
    unittest.main()
