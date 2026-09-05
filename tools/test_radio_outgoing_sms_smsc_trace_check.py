import unittest

from tools.radio_outgoing_sms_smsc_trace_check import (
    EXPECTED_FIRST_RECORD,
    SMSP_OFFSET,
    verify,
)


GOOD = """
sim_device: update fid=6f42 record=1 length=44 t=37.3
GSM service establish sapi=0 pd=05 message=24 length=16 data=0524740333
gsm_sms_submit: cp=29 rp=01 smsc=9876543210 destination=5551234 alphabet=0 user_length=2 outcome=0
GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2
GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5
GSM service uplink sapi=3 pd=09 message=04 length=2 data=2904
LAPDm service Channel Release acknowledged nr=2
"""


def card_with(record: bytes = EXPECTED_FIRST_RECORD) -> bytes:
    return bytes(SMSP_OFFSET) + record


class OutgoingSmsSmscTraceCheckTest(unittest.TestCase):
    def test_complete_transaction(self):
        verify(GOOD, card_with())

    def test_rejects_default_service_centre(self):
        changed = GOOD.replace("smsc=9876543210", "smsc=1234567890")
        with self.assertRaisesRegex(ValueError, "decoded SMS-SUBMIT"):
            verify(changed, card_with())

    def test_rejects_missing_card_update(self):
        without = GOOD.replace(
            "sim_device: update fid=6f42 record=1 length=44 t=37.3\n", "")
        with self.assertRaisesRegex(ValueError, "EF_SMSP update"):
            verify(without, card_with())

    def test_rejects_wrong_persisted_record(self):
        wrong = bytearray(EXPECTED_FIRST_RECORD)
        wrong[30] ^= 1
        with self.assertRaisesRegex(ValueError, "record 1 mismatch"):
            verify(GOOD, card_with(bytes(wrong)))


if __name__ == "__main__":
    unittest.main()
