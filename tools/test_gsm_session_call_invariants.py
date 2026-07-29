from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class GsmSessionCallInvariantTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = (ROOT / "driver/nokia_gsm_session.h").read_text()
        cls.source = (ROOT / "driver/nokia_gsm_session.cpp").read_text()

    def test_assignment_idempotence_is_generic_saved_session_state(self):
        self.assertIn("bool m_traffic_assignment_issued = false;", self.header)
        self.assertIn(
            "save_item(NAME(m_traffic_assignment_issued));", self.source)
        call_confirmed = self.source.split("if (message_type == 0x08)", 1)[1]
        call_confirmed = call_confirmed.split("if (message_type == 0x01)", 1)[0]
        self.assertIn("m_traffic_assignment_issued", call_confirmed)
        self.assertIn("m_traffic_assignment_issued = true;", call_confirmed)
        for product in ("noki", "NHM", "NSE"):
            self.assertNotIn(product, call_confirmed)

    def test_assignment_guard_resets_only_at_call_boundaries(self):
        self.assertIn(
            "m_incoming_service = u8(service);\n"
            "\tm_traffic_assignment_issued = false;",
            self.source,
        )
        release = self.source.split(
            "if (m_state == u8(state::awaiting_release_complete)", 1)[1]
        self.assertIn("m_traffic_assignment_issued = false;", release)


if __name__ == "__main__":
    unittest.main()
