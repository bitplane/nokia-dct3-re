import unittest

from tools.nsm3_verifier_check import check


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


if __name__ == "__main__":
    unittest.main()
