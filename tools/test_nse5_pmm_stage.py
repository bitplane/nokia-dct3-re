import unittest

from tools.nse5_pmm_stage import inspect_pmm, stage_block


class PacketPreparationTests(unittest.TestCase):
    def test_observed_packet(self):
        storage = bytes.fromhex("ad8ec52a42050032c6d521f4")
        block = bytes.fromhex("86556b13eac825ca9b58eb0d1a08843a01949091e882840e")
        self.assertEqual(stage_block(storage, block).hex(),
                         "8184b19115375a6760edee9d1dd95eb8fe6bef3c1337819e")

    def test_zero_products_complement_block(self):
        self.assertEqual(stage_block(bytes(12), bytes(24)), bytes([255]) * 24)

    def test_lengths(self):
        for storage, block in ((bytes(11), bytes(24)), (bytes(12), bytes(25))):
            with self.assertRaises(ValueError):
                stage_block(storage, block)

    def test_header(self):
        with self.assertRaises(ValueError):
            inspect_pmm(bytes(0x5e))

    def test_startup_request_offsets(self):
        image = bytearray(range(0x5e))
        image[6:12] = b"EEPROM"
        result = inspect_pmm(bytes(image))
        self.assertEqual(result["startup_requests"]["14"], bytes(range(0x3a, 0x46)).hex())
        self.assertEqual(result["startup_requests"]["15"], bytes(range(0x26, 0x3a)).hex())
        self.assertEqual(result["startup_requests"]["16"], result["staged"])


if __name__ == "__main__":
    unittest.main()
