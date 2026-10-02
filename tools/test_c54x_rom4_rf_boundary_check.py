import unittest

from tools.c54x_rom4_rf_boundary_check import check


def summary(*, frames=6499, reads=207040, first_frame=30, pairs=3,
            port38=0, port39=0, ifr="0000", imr="035f"):
    return (
        f"rom4_rf_read: sample=1 pc=324b frame={first_frame} t=0.160944\n"
        "rom4_rf_port32: sequence=1 port31=2a04 port32=0006 pc=a240\n"
        "rom4_rf_port32: sequence=2 port31=2a04 port32=0006 pc=a240\n"
        "rom4_rf_port32: sequence=3 port31=2813 port32=0030 pc=4028\n"
        "rom4_interface_summary: completion_strobes=2 mailbox_writes=3 "
        f"slot_expiries=0 frame_expiries={frames} rf_reads={reads} "
        f"rf_port32_writes={pairs} rf_port38_reads={port38} "
        f"rf_port39_reads={port39} pc=31a5 pmst=0020 ifr={ifr} imr={imr} "
        "mode_aa=0000 mode_ac=0000\n"
    )


class C54xRom4RfBoundaryCheckTest(unittest.TestCase):
    def test_accepts_quantified_int0_receiver_activation(self):
        self.assertEqual(check(summary())["frame_expiries"], 6499)

    def test_accepts_one_in_flight_frame_at_time_cutoff(self):
        self.assertEqual(check(summary(frames=6500))["rf_reads"], 207040)

    def test_accepts_two_in_flight_frames_at_time_cutoff(self):
        self.assertEqual(check(summary(frames=6500, reads=207008))["rf_reads"], 207008)

    def test_rejects_wrong_first_rf_frame(self):
        with self.assertRaisesRegex(ValueError, "started on frame 23"):
            check(summary(first_frame=23))
        with self.assertRaisesRegex(ValueError, "missing first rom4_rf_read"):
            check(summary().split("\n", 1)[1])

    def test_rejects_short_run(self):
        with self.assertRaisesRegex(ValueError, "only 100"):
            check(summary(frames=100))

    def test_rejects_inactive_receiver(self):
        with self.assertRaisesRegex(ValueError, "receiver did not become active"):
            check(summary(reads=0))

    def test_rejects_changed_read_cadence(self):
        with self.assertRaisesRegex(ValueError, "RF read cadence changed"):
            check(summary(reads=207007))
        with self.assertRaisesRegex(ValueError, "RF read cadence changed"):
            check(summary(frames=6502))
        with self.assertRaisesRegex(ValueError, "RF read cadence changed"):
            check(summary(frames=6500, reads=206944))

    def test_rejects_unexpected_burst_port_activity(self):
        with self.assertRaisesRegex(ValueError, "parallel burst path"):
            check(summary(port38=1))
        with self.assertRaisesRegex(ValueError, "parallel burst path"):
            check(summary(port39=1))

    def test_rejects_changed_rf_sequence(self):
        for text in (summary(pairs=0), summary().replace("port32=0006", "port32=0007"),
                     summary().replace("pc=4028", "pc=3712"),
                     summary().replace("sequence=2", "sequence=1")):
            with self.assertRaisesRegex(ValueError, "RF port sequence changed"):
                check(text)

    def test_rejects_unexpected_interrupt_state(self):
        with self.assertRaisesRegex(ValueError, "unexpected terminal IMR"):
            check(summary(imr="53ff"))
        with self.assertRaisesRegex(ValueError, "INT0 remains pending"):
            check(summary(ifr="0001"))


if __name__ == "__main__":
    unittest.main()
