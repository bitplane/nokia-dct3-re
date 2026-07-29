import unittest

from tools.radio_a5_two_calls_trace_check import check


CALL = """
data=801200000000000000000000063501
gsm_cipher: event=activated algorithm=1
GSM outgoing request id={request} digits=5551234
gsm_cipher: event=cleared algorithm=1
"""
TRACE = CALL.format(request=1) + "PCH no-identity fill\n" + CALL.format(request=2)


class A5TwoCallsTraceCheckTests(unittest.TestCase):
    def test_accepts_two_independent_calls(self):
        self.assertEqual([], check(TRACE))

    def test_rejects_missing_second_activation(self):
        broken = TRACE.rsplit("gsm_cipher: event=activated algorithm=1", 1)
        self.assertIn(
            "each call must activate and clear A5/1 exactly once",
            check("".join(broken)),
        )

    def test_requires_intermediate_pch(self):
        self.assertIn(
            "first call did not return to PCH before the second",
            check(TRACE.replace("PCH no-identity fill\n", "")),
        )


if __name__ == "__main__":
    unittest.main()
