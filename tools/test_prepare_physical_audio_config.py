import tempfile
import unittest
from pathlib import Path
import xml.etree.ElementTree as ET

from tools.prepare_physical_audio_config import prepare


class PhysicalAudioConfigTest(unittest.TestCase):
    def test_missing_fixture_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaises(ValueError):
                prepare(root / "missing", root / "run")

    def test_preserves_inputs_and_does_not_modify_fixture(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "fixture"
            source.mkdir()
            content = '<mameconfig><system name="noki3210"><input><port value="2"/></input><mixer><sound_map tag=":microphone"/></mixer></system></mameconfig>'
            path = source / "noki3210.cfg"
            path.write_text(content)
            prepare(source, root / "run")
            tree = ET.parse(root / "run/noki3210.cfg")
            self.assertEqual(tree.find("system/input/port").get("value"), "2")
            self.assertIsNone(tree.find("system/mixer"))
            self.assertEqual(path.read_text(), content)
            with self.assertRaises(ValueError):
                prepare(source, source)


if __name__ == "__main__":
    unittest.main()
