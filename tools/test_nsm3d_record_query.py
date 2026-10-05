import struct
import unittest

from tools.nse5_transform_trace_check import inverse_transform_words, transform_words
from tools.nsm3d_runtime_hle_check import check_record_query


class RecordQueryTests(unittest.TestCase):
    def setUp(self):
        self.encoded = bytes(range(24))
        chip = bytes.fromhex("00160010")
        key = bytes(value ^ (chip[index] if index < 4 else 0) for index, value in
                    enumerate(bytes.fromhex("7bb4d0ef9eb20abe73dad335")))
        schedule = tuple(v * 0x101 for v in bytes.fromhex("b173e65aab478e0d1a34680b"))
        decoded, markers = bytearray(), []
        for start in (0, 12):
            words = inverse_transform_words(struct.unpack(">6H", self.encoded[start:start + 12]),
                                            struct.unpack(">6H", key), schedule)
            data = struct.pack(">6H", *words)
            markers.append(data[-2:].hex())
            decoded.extend(data[:-2] + bytes(2))
        self.reply = (bytes.fromhex("35320000") + decoded + self.encoded).hex()
        self.text = (
            f"TX pending type=70 payload=26 data=1618{self.encoded.hex()}\n"
            f"RX enqueue type=74 payload=52 producer=0b0 data={self.reply}\n"
            f"record_decode family=83 chip=00160010 format=00 markers={markers[0]}/{markers[1]} verdict=not_evaluated\n"
            f"nsm3d_record_received: message=001039a8 bytes=0000007400360100{self.reply}\n")

    def test_actual_inverse_not_record_validity(self):
        check_record_query(self.text)

    def test_reply_corruption_and_donor_payload_rejected(self):
        for text in (self.text.replace(self.reply, "35320000" + "00" * 48),
                     self.text.replace("markers=", "markers=ffff"),
                     self.text.replace("chip=00160010", "chip=00160011")):
            with self.assertRaises(ValueError):
                check_record_query(text)

    def test_consumer_must_receive_same_bytes(self):
        with self.assertRaises(ValueError):
            check_record_query(self.text.replace("bytes=0000007400360100" + self.reply,
                                                "bytes=0000007400360100" + "00" * 52))

    def test_missing_or_duplicate_exchange_rejected(self):
        for text in ("", self.text * 2):
            with self.assertRaises(ValueError):
                check_record_query(text)

    def test_acquired_record_direction_comparison(self):
        # Both directions use the declared candidate key, not an inferred
        # physical identity. Neither passes the own-ROM ordinary +21 gate.
        encoded = bytes.fromhex('f26aa2e90d880230eb1ee358448a6b92c36710945239c864')
        key = struct.unpack('>6H', bytes.fromhex('7ba2d0ff9eb20abe73dad335'))
        schedule = tuple(v * 0x101 for v in bytes.fromhex('b173e65aab478e0d1a34680b'))
        results = {}
        for name, operation in (('forward', transform_words), ('inverse', inverse_transform_words)):
            results[name] = b''.join(struct.pack('>6H', *operation(
                struct.unpack('>6H', encoded[start:start + 12]), key, schedule))
                for start in (0, 12))
        self.assertEqual(results['forward'].hex(),
                         '253fe7330c12f635031edc2946d796a5f418539ebaa6ac9d')
        self.assertEqual(results['inverse'].hex(),
                         '65c48f02ab73bb006fee5146bd0df7c181e6c5956d06f343')
        for decoded in results.values():
            self.assertFalse(0x78 <= decoded[21] < 0x80)


if __name__ == "__main__":
    unittest.main()
