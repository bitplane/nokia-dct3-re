import unittest

from tools.nsm3_verifier_check import check, check_cobba


class VerifierFrontierTest(unittest.TestCase):
    def setUp(self):
        self.output = "NSM3 verifier requires peripheral read: port=002d pc=0f9f blocks=116"
        self.trace = "".join(f"nsm3_verifier: block={i} flag=087f\n" for i in range(116))
        self.trace += "".join(f"nsm3_verifier: port_write={port} data={data} blocks=116\n"
                              for port, data in [("000e", "1387"), ("0000", "000d"), ("000c", "0010")])

    def test_frontier(self):
        check(self.output, self.trace, 1)

    def test_no_invented_completion(self):
        with self.assertRaises(ValueError):
            check("NSM3 verifier publication", self.trace, 0)

    def test_missing_block(self):
        with self.assertRaises(ValueError):
            check(self.output, self.trace.replace("block=42 flag=087f\n", ""), 1)

    def test_changed_peripheral_write(self):
        with self.assertRaises(ValueError):
            check(self.output, self.trace.replace("data=0010", "data=0011"), 1)

    def comparison(self, value, version=6):
        trace = self.trace + "nsm3_verifier: immutable_version_write=0006\n"
        for select in ("001f", "001d", "001f"):
            trace += f"nsm3_verifier: port_write=002c data={select} blocks=116\n"
        for read in [value] * 3 + [12, 12, value]:
            trace += f"nsm3_verifier: port_read=002d data={read:04x} blocks=116\n"
        output = f"publication: blocks=116 word0={value:04x} word1={version:04x} word2={version:04x} word3=0006 pc=0f64 fingerprint=c2e06006 pmst=ffa8"
        return output, trace

    def test_wrong_fingerprint(self):
        output, trace = self.comparison(0)
        with self.assertRaises(ValueError):
            check_cobba(output.replace("c2e06006", "c2e06007"), trace, 3, 0)

    def test_wrong_pmst(self):
        output, trace = self.comparison(0)
        with self.assertRaises(ValueError):
            check_cobba(output.replace("pmst=ffa8", "pmst=ffa0"), trace, 3, 0)

    def test_cobba_publication(self):
        for value in (0, 0x16):
            check_cobba(*self.comparison(value), 3, value)

    def test_wrong_peripheral_value(self):
        with self.assertRaises(ValueError):
            check_cobba(*self.comparison(0), 3, 0x16)

    def test_wrong_status_handshake(self):
        output, trace = self.comparison(0)
        with self.assertRaises(ValueError):
            check_cobba(output, trace.replace("data=001d", "data=001c"), 3, 0)

    def test_immutable_version_sensitivity(self):
        check_cobba(*self.comparison(0, 4), 3, 0, 4)

    def test_no_shadow_ram_version(self):
        with self.assertRaises(ValueError):
            check_cobba(*self.comparison(0, 6), 3, 0, 4)

    def test_npe3_count_and_fingerprint(self):
        output, trace = self.comparison(0)
        output = output.replace("116", "232").replace("c2e06006", "f65a0d46")
        trace = trace.replace("blocks=116", "blocks=232")
        trace += "".join(f"nsm3_verifier: block={i} flag=087f\n" for i in range(116, 232))
        check_cobba(output, trace, 3, 0, count=232, fingerprint="f65a0d46")
        with self.assertRaises(ValueError):
            check_cobba(output, trace, 3, 0)

    def test_6250_count_and_fingerprint(self):
        output, trace = self.comparison(0)
        output = output.replace("116", "232").replace("c2e06006", "c62d430c")
        trace = trace.replace("blocks=116", "blocks=232")
        trace += "".join(f"nsm3_verifier: block={i} flag=087f\n" for i in range(116, 232))
        check_cobba(output, trace, 3, 0, count=232, fingerprint="c62d430c")
        with self.assertRaises(ValueError):
            check_cobba(output, trace, 3, 0, count=232, fingerprint="f65a0d46")


if __name__ == "__main__":
    unittest.main()
