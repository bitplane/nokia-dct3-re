import pathlib
import tempfile
import unittest

from tools import ccont_runtime_census


class CcontRuntimeCensusTest(unittest.TestCase):
    def test_transaction_summary(self):
        text = "\n".join((
            "gensio: W off=2d data=25",
            "gensio: W off=2c data=08",
            "gensio: W off=2c data=70",
            "gensio: W off=2d data=25",
            "gensio: W off=2c data=74",
            "gensio: R off=6d data=07",
            "gensio: R off=6c data=03",
        ))
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "trace.log"
            path.write_text(text)
            report = ccont_runtime_census.analyze("fixture", path)
        self.assertEqual(2, report["transactions"])
        self.assertEqual([], report["adc_selectors"])
        self.assertEqual(
            [
                {"direction": "W", "register": 1, "count": 1, "values": [0x70]},
                {"direction": "R", "register": 14, "count": 1, "values": [0x03]},
            ],
            report["registers"],
        )


if __name__ == "__main__":
    unittest.main()
