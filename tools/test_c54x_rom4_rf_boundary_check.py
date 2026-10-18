import unittest

from tools.c54x_rom4_rf_boundary_check import check


def summary(*, frames=865, reads=26752, first_frame=30, pairs=13,
            port38=0, port39=0, ifr="0000", imr="035f"):
    burst = [(0x2a04, 6, 0xa240), (0x41, 0x40, 0x3712),
             (0x27a2, 0x30, 0x3712), (0x41, 0x20, 0x3712),
             (0x488, 0x10, 0x3712), (0, 0x50, 0x3712)]
    writes = "".join(f"rom4_rf_port32: sequence={i} port31={value:04x} port32={command:04x} pc={pc:04x}\n"
                     for i, (value, command, pc) in enumerate(burst * 2 + [(0x2813, 0x30, 0x4028)], 1))
    return (
        f"rom4_rf_read: sample=1 pc=324b frame={first_frame} t=0.160944\n"
        + writes +
        "rom4_speech_state: address=1922 value=0041 mask=ffff pc=a1bc t=1.5\n"
        "rom4_speech_state: address=1923 value=0040 mask=ffff pc=a1bc t=1.5\n"
        "rom4_interface_summary: completion_strobes=2 mailbox_writes=3 "
        f"slot_expiries=0 frame_expiries={frames} rf_reads={reads} "
        f"rf_port32_writes={pairs} rf_port38_reads={port38} "
        f"rf_port39_reads={port39} pc=31a5 pmst=0020 ifr={ifr} imr={imr} "
        "mode_aa=0000 mode_ac=0000\n"
    )


class C54xRom4RfBoundaryCheckTest(unittest.TestCase):
    def scan_summary(self):
        text = '\n'.join(line for line in summary(frames=20280, reads=648032, pairs=1537).splitlines()
                         if not line.startswith('rom4_rf_port32:')) + '\n'
        burst = [(0x2a04, 6, 0xa240), (0x41, 0x40, 0x3712),
                 (0x27a2, 0x30, 0x3712), (0x41, 0x20, 0x3712),
                 (0x488, 0x10, 0x3712), (0, 0x50, 0x3712)]
        extra = "".join(
            f"rom4_rf_port32: sequence={n} port31={value:04x} port32={command:04x} pc={pc:04x}\n"
            for n, (value, command, pc) in enumerate(
                burst * 2 + [(0x2813, 0x30, 0x4028)] + burst * 2 +
                [(value, 0x30, 0x4028) for value in range(0x2727, 0x272e)], 1))
        return text + extra + (
            "rom4_serial_audit: space=io direction=write address=000e value=0546 mask=ffff pc=0959 t=6.454\n"
            "rom4_speech_state: address=0284 value=007c mask=ffff pc=3d8f t=6.426\n")

    def test_accepts_exact_scan_snapshot(self):
        self.assertEqual(check(self.scan_summary(), scan_30s=True)["rf_port32_writes"], 1537)

    def test_requires_full_capped_scan_prefix(self):
        text = self.scan_summary()
        with self.assertRaises(ValueError):
            check(text.replace("port31=272d", "port31=272e"), scan_30s=True)
        truncated = "\n".join(line for line in text.splitlines()
                              if not line.startswith("rom4_rf_port32:") or
                              int(line.split("sequence=")[1].split()[0]) <= 16)
        with self.assertRaises(ValueError):
            check(truncated, scan_30s=True)

    def test_rejects_previous_private_latch_snapshot(self):
        with self.assertRaisesRegex(ValueError, "snapshot changed"):
            check(self.scan_summary().replace("frame_expiries=20280", "frame_expiries=20288")
                  .replace("rf_reads=648032", "rf_reads=648288"), scan_30s=True)

    def test_rejects_payload_alias_and_missing_transfer_snapshots(self):
        for count in (1520, 1518):
            with self.assertRaises(ValueError):
                check(self.scan_summary().replace('frame_expiries=20280', 'frame_expiries=20285')
                      .replace('rf_reads=648032', 'rf_reads=648192')
                      .replace('rf_port32_writes=1537', f'rf_port32_writes={count}'), scan_30s=True)

    def test_snapshot_failure_reports_current_drift_without_accepting_it(self):
        text = (self.scan_summary().replace("frame_expiries=20280", "frame_expiries=20282")
                .replace("rf_reads=648032", "rf_reads=648096")
                .replace("rf_port32_writes=1537", "rf_port32_writes=1519"))
        with self.assertRaises(ValueError) as caught:
            check(text, scan_30s=True)
        for detail in ("frames=20282 expected=20280 delta=+2",
                       "rf_reads=648096 expected=648032 delta=+64",
                       "rf_port32_writes=1519 expected=1537 delta=-18"):
            self.assertIn(detail, str(caught.exception))

    def test_sequence_failure_distinguishes_count_from_prefix(self):
        with self.assertRaisesRegex(ValueError, "count=3 expected=13; first_difference=None"):
            check(summary(pairs=3))
        with self.assertRaisesRegex(ValueError, "first_difference=0; observed=\\[\\(1, 10756, 7, 41536\\)\\]"):
            check(summary().replace("port32=0006", "port32=0007"))

    def test_sequence_failure_reports_missing_record(self):
        text = summary().replace("rom4_rf_port32: sequence=13 port31=2813 port32=0030 pc=4028\n", "")
        with self.assertRaisesRegex(ValueError, "first_difference=12; observed=\\[\\]; expected=\\[\\(13,"):
            check(text)

    def test_scan_requires_reload_selector_and_exact_sequence(self):
        for old, new in (("value=0546", "value=0547"), ("value=007c", "value=0001"),
                         ("port31=27a2", "port31=27a3"), ("rf_port32_writes=1537", "rf_port32_writes=1538")):
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
        self.assertEqual(check(summary())["frame_expiries"], 865)

    def test_accepts_one_in_flight_frame_at_time_cutoff(self):
        self.assertEqual(check(summary(frames=866))["rf_reads"], 26752)

    def test_accepts_two_in_flight_frames_at_time_cutoff(self):
        self.assertEqual(check(summary(frames=866, reads=26720))["rf_reads"], 26720)

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
            check(summary(reads=26719))
        with self.assertRaisesRegex(ValueError, "RF read cadence changed"):
            check(summary(frames=868))
        with self.assertRaisesRegex(ValueError, "RF read cadence changed"):
            check(summary(frames=866, reads=26656))

    def test_rejects_reset_or_fault_with_otherwise_valid_evidence(self):
        for fault in ("rom4_reset_request: reason=00000004", "TMS320C54x illegal instruction"):
            with self.subTest(fault=fault), self.assertRaises(ValueError):
                check(summary() + fault)

    def test_rejects_old_five_write_snapshot(self):
        with self.assertRaisesRegex(ValueError, "expected=13"):
            check(summary(pairs=5))

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
