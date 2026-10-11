import unittest

from tools.c54x_rom4_rf_boundary_check import check


def summary(*, frames=6499, reads=207040, first_frame=30, pairs=5,
            port38=0, port39=0, ifr="0000", imr="035f"):
    return (
        f"rom4_rf_read: sample=1 pc=324b frame={first_frame} t=0.160944\n"
        "rom4_rf_port32: sequence=1 port31=2a04 port32=0006 pc=a240\n"
        "rom4_rf_port32: sequence=2 port31=0041 port32=0040 pc=3712\n"
        "rom4_speech_state: address=1922 value=0041 mask=ffff pc=a1bc t=1.5\n"
        "rom4_speech_state: address=1923 value=0040 mask=ffff pc=a1bc t=1.5\n"
        "rom4_rf_port32: sequence=3 port31=2a04 port32=0006 pc=a240\n"
        "rom4_rf_port32: sequence=4 port31=2813 port32=0030 pc=4028\n"
        "rom4_rf_port32: sequence=5 port31=0041 port32=0040 pc=3712\n"
        "rom4_interface_summary: completion_strobes=2 mailbox_writes=3 "
        f"slot_expiries=0 frame_expiries={frames} rf_reads={reads} "
        f"rf_port32_writes={pairs} rf_port38_reads={port38} "
        f"rf_port39_reads={port39} pc=31a5 pmst=0020 ifr={ifr} imr={imr} "
        "mode_aa=0000 mode_ac=0000\n"
    )


class C54xRom4RfBoundaryCheckTest(unittest.TestCase):
    def scan_summary(self):
        text = summary(frames=20282, reads=648096, pairs=1519)
        extra = "".join(
            f"rom4_rf_port32: sequence={n} port31={value:04x} port32={command:04x} pc={pc:04x}\n"
            for n, value, command, pc in
            [(6, 0x2a04, 6, 0xa240), (7, 0x2a04, 6, 0xa240)] +
            [(n, 0x2727 + n - 8, 0x30, 0x4028) for n in range(8, 17)])
        return text + extra + (
            "rom4_serial_audit: space=io direction=write address=000e value=0546 mask=ffff pc=0959 t=6.454\n"
            "rom4_speech_state: address=0284 value=007c mask=ffff pc=3d8f t=6.426\n")

    def test_accepts_exact_scan_snapshot(self):
        self.assertEqual(check(self.scan_summary(), scan_30s=True)["rf_port32_writes"], 1519)

    def test_rejects_previous_private_latch_snapshot(self):
        with self.assertRaisesRegex(ValueError, "snapshot changed"):
            check(self.scan_summary().replace("frame_expiries=20282", "frame_expiries=20288")
                  .replace("rf_reads=648096", "rf_reads=648288"), scan_30s=True)

    def test_snapshot_failure_reports_current_drift_without_accepting_it(self):
        text = (self.scan_summary().replace("frame_expiries=20282", "frame_expiries=20285")
                .replace("rf_reads=648096", "rf_reads=648192")
                .replace("rf_port32_writes=1519", "rf_port32_writes=1520"))
        with self.assertRaises(ValueError) as caught:
            check(text, scan_30s=True)
        for detail in ("frames=20285 expected=20282 delta=+3",
                       "rf_reads=648192 expected=648096 delta=+96",
                       "rf_port32_writes=1520 expected=1519 delta=+1"):
            self.assertIn(detail, str(caught.exception))

    def test_sequence_failure_distinguishes_count_from_prefix(self):
        with self.assertRaisesRegex(ValueError, "count=3 expected=5; first_difference=None"):
            check(summary(pairs=3))
        with self.assertRaisesRegex(ValueError, "first_difference=0; observed=\\[\\(1, 10756, 7, 41536\\)\\]"):
            check(summary().replace("port32=0006", "port32=0007"))

    def test_sequence_failure_reports_missing_record(self):
        text = summary().replace("rom4_rf_port32: sequence=5 port31=0041 port32=0040 pc=3712\n", "")
        with self.assertRaisesRegex(ValueError, "first_difference=4; observed=\\[\\]; expected=\\[\\(5,"):
            check(text)

    def test_scan_requires_reload_selector_and_exact_sequence(self):
        for old, new in (("value=0546", "value=0547"), ("value=007c", "value=0001"),
                         ("port31=2727", "port31=2728"), ("rf_port32_writes=1519", "rf_port32_writes=1520")):
            with self.assertRaises(ValueError):
                check(self.scan_summary().replace(old, new), scan_30s=True)

    def test_requires_firmware_queue_producer(self):
        for text in (summary().replace("pc=a1bc", "pc=0000"),
                     summary().replace("address=1923 value=0040", "address=1923 value=0041")):
            with self.assertRaisesRegex(ValueError, "queue producer"):
                check(text)

    def test_rejects_pre_fiq1_sequence(self):
        with self.assertRaisesRegex(ValueError, "RF port sequence changed"):
            check(summary(pairs=3))

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
