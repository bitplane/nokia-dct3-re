import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class DspCodeBlockLifecycleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "driver/nokia_dsp_hle.cpp").read_text()
        cls.header = (ROOT / "driver/nokia_dsp_hle.h").read_text()

    def test_code_block_selector_is_one_shot_and_saved(self):
        self.assertIn("bool m_service_code_block_published = false;", self.header)
        self.assertIn(
            "save_item(NAME(m_service_code_block_published));", self.source)
        self.assertIn(
            "!m_service_code_block_published &&", self.source)
        self.assertIn("m_service_code_block_published = true;", self.source)

    def test_reset_starts_a_new_finite_transfer(self):
        self.assertIn("m_service_code_block_published = false;", self.source)


if __name__ == "__main__":
    unittest.main()
