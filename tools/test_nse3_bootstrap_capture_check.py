import unittest

from tools.nse3_bootstrap_capture_check import (
    ROM3_V548_SHA1,
    ROM4_V548_SHA1,
    verify,
)


def good_capture():
    return {
        "schema_version": 1,
        "product": "Nokia 6110 NSE-3",
        "firmware": {
            "variant": "v5.48-rom3-ppmb",
            "sha1": ROM3_V548_SHA1,
        },
        "source": {
            "kind": "real_handset",
            "dsp_backend": "physical",
            "hle_completion": False,
            "firmware_or_ram_patching": False,
            "capture_sha256": "12" * 32,
        },
        "events": [
            {"owner": "mcu", "action": "write", "offset": 0x002, "value": 0xFFFF},
            {"owner": "mcu", "action": "write", "offset": 0x004, "value": 0xFFFF},
            {"owner": "mcu", "action": "write", "offset": 0x006, "value": 0xFFFF},
            {"owner": "dsp", "action": "write", "offset": 0x004, "value": 3},
            {"owner": "dsp", "action": "write", "offset": 0x006, "value": 3},
            *[
                {"owner": "dsp", "action": "exchange_ack", "index": index}
                for index in range(1, 65)
            ],
            {"owner": "dsp", "action": "write", "offset": 0x000, "value": 0x0B06},
            {"owner": "dsp", "action": "write", "offset": 0x002, "value": 0x1234},
        ],
    }


class Nse3BootstrapCaptureCheckTest(unittest.TestCase):
    def test_accepts_complete_physical_capture(self):
        result = verify(good_capture())
        self.assertEqual(0x1234, result["verification_verdict"])
        self.assertEqual(64, result["exchange_count"])

    def test_rejects_hle_completion(self):
        capture = good_capture()
        capture["source"]["hle_completion"] = True
        with self.assertRaisesRegex(ValueError, "exclude HLE"):
            verify(capture)

    def test_rejects_wrong_external_firmware(self):
        capture = good_capture()
        capture["firmware"]["sha1"] = "00" * 20
        with self.assertRaisesRegex(ValueError, "pinned NSE-3"):
            verify(capture)

    def test_accepts_observed_rom4_pair_without_presuming_its_value(self):
        capture = good_capture()
        capture["firmware"] = {
            "variant": "v5.48-rom4-ppmb",
            "sha1": ROM4_V548_SHA1,
        }
        capture["events"][3]["value"] = 4
        capture["events"][4]["value"] = 4
        result = verify(capture)
        self.assertEqual([4, 4], result["preupload_pair"])
        self.assertEqual("v5.48-rom4-ppmb", result["firmware_variant"])

    def test_rejects_presumed_unequal_rom4_pair(self):
        capture = good_capture()
        capture["firmware"] = {
            "variant": "v5.48-rom4-ppmb",
            "sha1": ROM4_V548_SHA1,
        }
        capture["events"][3]["value"] = 4
        capture["events"][4]["value"] = 3
        with self.assertRaisesRegex(ValueError, "equal single-digit"):
            verify(capture)

    def test_rejects_missing_exchange(self):
        capture = good_capture()
        del capture["events"][20]
        with self.assertRaisesRegex(ValueError, "64 ordered"):
            verify(capture)

    def test_rejects_parked_verdict(self):
        capture = good_capture()
        capture["events"][-1]["value"] = 0xFFFF
        with self.assertRaisesRegex(ValueError, "sentinel"):
            verify(capture)

    def test_rejects_publication_before_upload(self):
        capture = good_capture()
        publications = capture["events"][-2:]
        del capture["events"][-2:]
        capture["events"][5:5] = publications
        with self.assertRaisesRegex(ValueError, "precede"):
            verify(capture)


if __name__ == "__main__":
    unittest.main()
