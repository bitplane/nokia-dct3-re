import unittest

from tools.dct3_msid_codec import encode_msid
from tools.nsm3d_runtime_hle_check import check_identity_query


class IdentityQueryTests(unittest.TestCase):
    def setUp(self):
        self.image = bytes.fromhex("e46902d5")
        self.msid = encode_msid(bytes.fromhex("e46902d500160010acadab00"), 0x83).hex()
        self.text = (
            "dsp_hle: identity_query family=83 chip=00160010 revision=00 verdict=not_evaluated\n"
            f"RX enqueue type=74 payload=16 producer=095 data=340e00{self.msid}\n"
            f"nsm3d_identity_boundary: ready=01 record={self.msid}\n")

    def test_forward_contract(self):
        check_identity_query(self.text, self.image)

    def test_different_checksum_or_chip_is_not_accepted(self):
        with self.assertRaises(ValueError):
            check_identity_query(self.text, bytes.fromhex("e46902d4"))
        with self.assertRaises(ValueError):
            check_identity_query(self.text.replace("chip=00160010", "chip=00160011"), self.image)

    def test_missing_retention_or_duplicate_reply(self):
        for text in (self.text.replace("ready=01", "ready=00"), self.text * 2):
            with self.assertRaises(ValueError):
                check_identity_query(text, self.image)

    def test_final_success_is_not_part_of_identity_contract(self):
        with self.assertRaises(ValueError):
            check_identity_query(self.text + "RX enqueue type=74 payload=2 producer=096 data=0d00\n",
                                 self.image)


if __name__ == "__main__":
    unittest.main()
