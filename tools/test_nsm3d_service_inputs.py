import unittest

from tools.nsm3d_runtime_hle_check import check_service_inputs


class ServiceInputTests(unittest.TestCase):
    def setUp(self):
        self.flash = bytearray(0x1d0000)
        self.flash[-4:] = bytes.fromhex("e46902d5")
        self.pmm = bytearray(0x30000)
        self.pmm[0x10026:0x1005e] = bytes(range(56))
        self.packets = [bytes.fromhex("1304") + self.flash[-4:],
                        bytes.fromhex("140c") + bytes(range(20, 32)),
                        bytes.fromhex("1514") + bytes(range(20)),
                        bytes.fromhex("1618") + bytes(range(32, 56))]

    def log(self, packets=None):
        return "\n".join(
            f"dspif_transport: TX pending type=70 payload={len(p)} data={p.hex()}"
            for p in (self.packets if packets is None else packets))

    def test_own_forward_inputs(self):
        check_service_inputs(self.log(), self.flash, self.pmm)

    def test_record_corruption_rejected(self):
        self.pmm[0x1003a] ^= 1
        with self.assertRaises(ValueError):
            check_service_inputs(self.log(), self.flash, self.pmm)

    def test_checksum_source_corruption_rejected(self):
        self.flash[-1] ^= 1
        with self.assertRaises(ValueError):
            check_service_inputs(self.log(), self.flash, self.pmm)

    def test_missing_duplicate_or_reordered_input_rejected(self):
        for packets in (self.packets[:-1], self.packets * 2,
                        list(reversed(self.packets))):
            with self.subTest(packets=packets), self.assertRaises(ValueError):
                check_service_inputs(self.log(packets), self.flash, self.pmm)

    def test_bad_extent_rejected(self):
        with self.assertRaises(ValueError):
            check_service_inputs(self.log().replace("payload=6", "payload=7"),
                                 self.flash, self.pmm)


if __name__ == "__main__":
    unittest.main()
