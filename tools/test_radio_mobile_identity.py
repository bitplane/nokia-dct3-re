import unittest

from tools.radio_mobile_identity import (
    paging_group,
    registered_mobile_identity,
)


class RadioMobileIdentityTest(unittest.TestCase):
    def test_extracts_organic_location_update_identity(self):
        text = (
            "dsp_hle: TX packet type=1b payload=25 "
            "data=0080013f49050800000000000030080910101032547698")
        self.assertEqual(
            registered_mobile_identity(text), "0910101032547698")

    def test_derives_advertised_paging_group(self):
        self.assertEqual(paging_group("0910101032547698"), (1, 36))

    def test_combined_and_reserved_paging_blocks(self):
        identity="0910101032547698"
        self.assertEqual(paging_group(identity,combined=True), (1,6))
        self.assertEqual(paging_group(identity,combined=True,reserved=1), (0,16))
        self.assertEqual(paging_group(identity,combined=True,reserved=2,multiframes=9), (6,16))
        for options in (dict(combined=True,reserved=3),dict(reserved=8),dict(reserved=-1),
                        dict(multiframes=1),dict(multiframes=10)):
            with self.subTest(options=options),self.assertRaises(ValueError):paging_group(identity,**options)
        for bad in ('0110101032547698','09101010325476a8'):
            with self.assertRaises(ValueError):paging_group(bad)

    def test_rejects_non_imsi_identity(self):
        with self.assertRaisesRegex(ValueError, "not an IMSI"):
            paging_group("0800000000000000")


if __name__ == "__main__":
    unittest.main()
