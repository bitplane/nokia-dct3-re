import gzip
import unittest

from tools.nse5_installer_audit import compare, member, records


def record(address, payload):
    return (b"\x0b" + address.to_bytes(3, "big") + b"\x00" +
            len(payload).to_bytes(3, "big") + b"\x00" + payload)


class InstallerAuditTests(unittest.TestCase):
    def test_gzip_member_ignores_following_container_data(self):
        self.assertEqual(member(b"prefix" + gzip.compress(b"data") + b"tail", 6), b"data")

    def test_bounded_and_truncated_members(self):
        for source, limit in ((gzip.compress(b"1234"), 3), (gzip.compress(b"data")[:-3], 100)):
            with self.assertRaises(ValueError):
                member(source, 0, limit)

    def test_sparse_records(self):
        self.assertEqual(records(record(0x200000, b"a") + record(0x590000, b"b")),
                         [(0x200000, b"a"), (0x590000, b"b")])

    def test_overlap_and_truncation(self):
        for source in (record(1, b"ab") + record(2, b"c"), record(1, b"ab")[:-1], b"", b"x"):
            with self.assertRaises(ValueError):
                records(source)

    def test_comparison_does_not_fill_sparse_gaps(self):
        result = compare([(0x200000, b"ab"), (0x590000, b"z"),
                          (0x5FA000, b"xy")], b"ac", b"xy")
        self.assertEqual([row["different_bytes"] for row in result], [1, None, 0])


if __name__ == "__main__":
    unittest.main()
