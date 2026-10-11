import pathlib
import tempfile
import unittest

from dsp_upload_reference_match import descriptor_match, matches, read_payload, verify_catalogue


class ReferenceMatchTest(unittest.TestCase):
    def test_catalogue_including_empty_entry(self):
        header = [1, 2, 0, 4, 5]
        data = (0x200004).to_bytes(4, "big") + b"".join(v.to_bytes(2, "big") for v in header + [0])
        payloads = [(pathlib.Path("block00.h"), header, [])]
        self.assertEqual(len(verify_catalogue(payloads, data, 0, 0x200000)), 1)
        with self.assertRaises(ValueError):
            verify_catalogue(payloads, data[:-1] + b"\x01", 0, 0x200000)
        with self.assertRaises(ValueError):
            verify_catalogue(payloads, data, 0, 0x300000)

    def payload(self, text):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "block00.h"
            path.write_text(text)
            return read_payload(path)

    def test_size_and_literals(self):
        self.assertEqual(self.payload("static struct dsp_codeblock x = {{0x1,0x2,0x2,0x0,0x0},{0x1234,0xabcd}};"), [0x1234, 0xabcd])
        with self.assertRaises(ValueError):
            self.payload("static struct dsp_codeblock x = {{0x1,0x2,0x3,0x0,0x0},{0x1234,0xabcd}};")
        with self.assertRaises(ValueError):
            self.payload("static struct dsp_codeblock x = {{0x1,0x2,0x1,0x0,0x0},{GUESS}};")

    def test_full_matches_and_both_orders(self):
        self.assertEqual(matches([0x1234, 0xabcd], bytes.fromhex("001234abcd3412cdab")),
                         [{"byteorder": "big", "file_offset": 1}, {"byteorder": "little", "file_offset": 5}])
        self.assertEqual(matches([0x1234, 0xabcd], bytes.fromhex("1234ab00")), [])

    def test_empty_and_repeated(self):
        self.assertEqual(matches([], b"anything"), [])
        self.assertEqual(matches([0x1234], bytes.fromhex("12341234")),
                         [{"byteorder": "big", "file_offset": 0}, {"byteorder": "big", "file_offset": 2},
                          {"byteorder": "little", "file_offset": 1}])

    def test_descriptor_candidate(self):
        header = [1, 2, 3, 4, 5]
        for order in ("big", "little"):
            image = b"".join(v.to_bytes(2, order) for v in header + [0]) + b"payload"
            result = descriptor_match(header, image, {"byteorder": order, "file_offset": 12})
            self.assertTrue(result["archival_header_equal"])
            self.assertEqual(result["words"], header + [0])
            self.assertFalse(descriptor_match([9] * 5, image,
                                             {"byteorder": order, "file_offset": 12})["archival_header_equal"])
        self.assertIsNone(descriptor_match(header, b"short", {"byteorder": "big", "file_offset": 0}))


if __name__ == "__main__":
    unittest.main()
