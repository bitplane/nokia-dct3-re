import sys
from pathlib import Path
import unittest
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from noki8850_staged_trace_check import check_trace, check_physical_inputs, check_navigation_frames, check_calculator


TRACE = """
staged_dsp: release entry=0f00 words=223 prom_input=0006 clock=13000000 stage=verifier
staged_dsp: publication word0=0000 word1=0006 word2=0006 word3=0006
staged_dsp: release entry=0f00 words=126 prom_input=0006 clock=13000000 stage=loader
staged_dsp: loader2_verified words=613 entry=0a00
staged_dsp: control_write port=001c data=0200
staged_dsp: outside_uploaded_code pc=2c75
staged_dsp: observation_halt pc=2c75 ownership_retained=1
8850_ready: state=01 shared_e4=0000
"""


class StagedTraceTests(unittest.TestCase):
    def test_native_boundary(self):
        self.assertEqual(check_trace(TRACE), [])

    def test_missing_publication(self):
        self.assertTrue(check_trace(TRACE.replace("publication", "unpublished")))

    def test_wrong_loader(self):
        self.assertTrue(check_trace(TRACE.replace("words=613", "words=612")))

    def test_reordered_readiness(self):
        lines = TRACE.strip().splitlines()
        self.assertTrue(check_trace("\n".join([lines[-1], *lines[:-1]])))

    def test_fatal_error(self):
        self.assertTrue(check_trace(TRACE + "Fatal error: unexpected port"))

    def test_hle_not_native(self):
        self.assertTrue(check_trace(TRACE + "runtime_hle_handoff"))

    def test_runtime_selftest(self):
        trace = self.runtime_trace()
        self.assertEqual(check_trace(trace, runtime_hle=True), [])

    def test_runtime_wrong_response(self):
        self.assertTrue(check_trace(self.runtime_trace().replace(
            "RX enqueue type=74", "RX enqueue type=70"), runtime_hle=True))

    def test_runtime_fault_not_cleared(self):
        self.assertTrue(check_trace(self.runtime_trace().replace(
            "0000000000000000ffff", "0f10000000000000ffff"), runtime_hle=True))

    def test_startup_readiness(self):
        self.assertEqual(check_trace(self.startup_trace(), True, True), [])

    def test_incomplete_readiness(self):
        self.assertTrue(check_trace(self.startup_trace().replace("reports=0f", "reports=0e"), True, True))

    def test_no_physical_irq(self):
        self.assertTrue(check_trace(self.startup_trace().replace("irq=1", "irq=0"), True, True))

    def test_ack_before_press(self):
        trace = self.startup_trace().replace("kbgpio: ack latched=1", "")
        self.assertTrue(check_trace("kbgpio: ack latched=1\n" + trace, True, True))

    def test_display_transfer(self):
        trace = self.startup_trace() + """
8850_glyph_framebuffer_done pixels=80800000/007f7e0c mask=00000000/00000000
8850_lcd_framebuffer_transfer start=00 count=54 flags=00 pixels=80800000/007f7e0c mask=00000000/00000000
"""
        self.assertEqual(check_trace(trace, True, True, True), [])
        self.assertTrue(check_trace(trace.replace("count=54", "count=00"), True, True, True))
        self.assertTrue(check_trace(trace.replace("007f7e0c", "00000000"), True, True, True))

    def test_transfer_before_glyphs(self):
        trace = self.startup_trace() + """
8850_lcd_framebuffer_transfer start=00 count=54 flags=00 pixels=80800000/007f7e0c mask=00000000/00000000
8850_glyph_framebuffer_done pixels=80800000/007f7e0c mask=00000000/00000000
"""
        self.assertTrue(check_trace(trace, True, True, True))

    def test_sim_reads(self):
        trace = self.runtime_trace() + """
sim_device: read-binary fid=2fe2 offset=0 length=10
sim_device: read-binary fid=6f38 offset=0 length=12
sim_device: read-binary fid=6f07 offset=0 length=9
sim_device: header cla=a0 ins=b2 p1=32 p2=04 p3=20 selected=6f3a
sim_device: header cla=a0 ins=f2 p1=00 p2=00 p3=16 selected=6f3a
"""
        self.assertEqual(check_trace(trace, True, sim_reads=True), [])
        self.assertTrue(check_trace(trace.replace("p1=32", "p1=31"), True, sim_reads=True))
        self.assertTrue(check_trace(trace.replace("fid=6f07", "fid=6f08"), True, sim_reads=True))

    def test_physical_navigation_requires_correlated_decoding(self):
        lines = []
        for label, key in (
            ("key=Keypad 1", "01"), ("key=Keypad 2", "02"),
            ("key=Keypad 3", "03"), ("key=Keypad 4", "04"),
            ("key=Keypad 5", "05"), ("key=Menu", "19"),
            ("action=messages_menu", "19"), ("action=inbox", "19"), ("action=names", "1a"),
        ):
            kind = "security" if label.startswith("key=") else "navigation"
            lines.extend((f"8850_{kind}_physical: {label}", f"8850_keypad_decoded key={key}"))
        trace = "\n".join(lines)
        self.assertEqual(check_physical_inputs(trace), [])
        self.assertTrue(check_physical_inputs(trace.replace("key=03", "key=02")))
        # A late decode must not validate the preceding input marker.
        self.assertTrue(check_physical_inputs(trace.replace("8850_keypad_decoded key=04", "")))

    def test_missing_and_blank_navigation_frames_fail(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.assertEqual(len(check_navigation_frames(root)), 3)
            for name in ("8850_messages_menu.png", "8850_inbox.png", "8850_names.png"):
                Image.new("L", (84, 48), 255).save(root / name)
            self.assertEqual(len(check_navigation_frames(root)), 3)

    def test_calculator_rejects_wrong_result_and_uncorrelated_input(self):
        from PIL import Image
        trace = "\n".join(
            f"8850_application_physical: action={action}\n8850_keypad_decoded key={key}"
            for action, key in (
                ("input_1", "01"), ("input_12", "02"),
                ("operation_options", "19"), ("add", "18"), ("plus", "19"),
                ("input_3", "03"), ("options", "19"), ("result", "19"),
            ))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "result.png"
            Image.new("L", (84, 48), 255).save(path)
            self.assertEqual(len(check_calculator(trace, path)), 1)
            self.assertGreater(len(check_calculator(trace.replace("key=18", "key=17"), path)), 1)
            self.assertGreater(len(check_calculator("", path)), 1)

    @classmethod
    def startup_trace(cls):
        return cls.runtime_trace() + """
8850_report14_stub r14=00244cbb
8850_startup_dispatch report=00000014 state=000d base=00138070
8850_startup_check power=06 reports=0f
8850_startup_dispatch report=00000033 state=0004 base=00138070
8850_matrix_press: column=1 host_bit=02
kbgpio: irq=1
kbgpio: ack latched=1
"""

    @staticmethod
    def runtime_trace():
        return TRACE.replace(
            "staged_dsp: observation_halt pc=2c75 ownership_retained=1",
            """staged_dsp: runtime_hle_handoff pc=2c75 native_suspended=1
dspif_transport: TX pending type=70 payload=2 data=0d00
dspif_transport: RX enqueue type=74 payload=2 producer=08e data=0d00
8850_faults: bytes=ffff00ff00ffff00ffffff0000ff0000000000000000ffff""")


if __name__ == "__main__":
    unittest.main()
