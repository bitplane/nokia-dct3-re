import unittest

from tools.radio_authentication_boundary_trace_check import REQUEST, verify


GOOD = f"""
dspif_transport: RX enqueue type=80 payload=34 data=80000000000000000000{REQUEST}2b2b
sim_device: header cla=a0 ins=88 p1=00 p2=00 p3=10 selected=7f20
sim_device: body ins=88 length=16 selected=7f20
sim_device: header cla=a0 ins=c0 p1=00 p2=00 p3=0c selected=7f20
sim_device: pending response ins=c0 length=12 status=9000
sim_authentication_consumer: pc=00206914 get_status=0066 accepted=01 task=14
radio_pending_primitive: address=0010d12c old=0000 data=1000 pc=0026eb44
"""


class AuthenticationBoundaryTraceCheckTest(unittest.TestCase):
    def test_accepts_complete_organic_sim_frontier(self):
        result = verify(GOOD)
        self.assertEqual(1, result["authentication_requests"])
        self.assertEqual(12, result["result_bytes_fetched"])
        self.assertEqual(1, result["firmware_results_accepted"])
        self.assertEqual(1, result["sres_primitives_queued"])
        self.assertEqual(0, result["mm_authentication_responses"])
        self.assertFalse(result["registration_promotion"])

    def test_records_response_without_promoting_registration(self):
        result = verify(
            GOOD + "\ndsp_hle: GSM service uplink sapi=0 pd=05 "
            "message=14 length=6\n"
        )
        self.assertEqual(1, result["mm_authentication_responses"])
        self.assertFalse(result["registration_promotion"])

    def test_rejects_missing_card_result(self):
        with self.assertRaisesRegex(ValueError, "twelve-byte GET RESPONSE"):
            verify(GOOD.replace(
                "sim_device: pending response ins=c0 length=12 status=9000", ""
            ))

    def test_rejects_card_result_not_accepted_by_firmware(self):
        with self.assertRaisesRegex(ValueError, "firmware-accepted"):
            verify(GOOD.replace("get_status=0066 accepted=01", "get_status=0066 accepted=00"))

    def test_rejects_missing_sres_radio_primitive(self):
        with self.assertRaisesRegex(ValueError, "queued SRES"):
            verify(GOOD.replace(
                "radio_pending_primitive: address=0010d12c old=0000 "
                "data=1000 pc=0026eb44", ""
            ))


if __name__ == "__main__":
    unittest.main()
