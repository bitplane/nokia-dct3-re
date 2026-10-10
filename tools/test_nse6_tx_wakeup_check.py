import unittest

from tools.nse6_tx_wakeup_check import check


VALID = """nse6_native_completion_bits: value=0002 pc=395e t=0.4
nse6_tx_mailbox_object: object=0010d758 prefix=0002 type=1a t=6.5
nse6_tx_space_check: object=0010d758 type=1a length=44 available=162 t=6.6
nse6_tx_ring_request: source=0010d75b type=1a length=68 lr=002d3701 t=6.7
nse6_stage_count: pc=002b5e78 count=13
"""


class NativeTxWakeupTests(unittest.TestCase):
    def test_publication(self):
        self.assertEqual(check(VALID), {"wakeups": 13, "published": 6.7})

    def test_requires_wakeup(self):
        with self.assertRaisesRegex(ValueError, "FIQ1"):
            check(VALID.replace("count=13", "count=0"))

    def test_rejects_wrong_object(self):
        with self.assertRaisesRegex(ValueError, "same-object"):
            check(VALID.replace("source=0010d75b", "source=0010e75b"))

    def test_requires_sufficient_space(self):
        with self.assertRaisesRegex(ValueError, "same-object"):
            check(VALID.replace("available=162", "available=69"))

    def test_rejects_reversed_order(self):
        with self.assertRaisesRegex(ValueError, "ordered"):
            check(VALID.replace("t=6.7", "t=6.4"))

    def test_rejects_unsupported_execution(self):
        with self.assertRaisesRegex(ValueError, "unsupported"):
            check(VALID + "unimplemented C54x opcode")
