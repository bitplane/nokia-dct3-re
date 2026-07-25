import unittest

from tools.radio_incoming_sms_trace_check import (
    SMS_NVRAM_OFFSET,
    STORED_RECORD_PREFIX,
    verify,
)


GOOD = """
dsp_hle: LAPDm Channel Release acknowledged nr=2
dsp_hle: PCH IMSI page transmitted channel=60 fn=3759
dsp_hle: TX packet type=1b payload=25 words=14 radio_phase=contention_resolution data=0080013f4106270703331881080910101032547698
dspif_transport: RX enqueue type=80 payload=34 producer=0a8 data=8012000012c40001000001734106270703331881080910101032547698
dspif_transport: RX enqueue type=80 payload=34 producer=0ba data=8012000012c500010000030029053247627042210000002b2b2b2b
dspif_transport: RX enqueue type=80 payload=34 producer=084 data=801200001a25000100000f3f012b2b
dsp_hle: TX packet type=1b payload=25 words=14 radio_phase=service_downlink data=00800f73012b2b
dspif_transport: RX enqueue type=80 payload=34 producer=0b2 data=801200001a2a000100000f005309012101400691214365870900160407815515322b
dsp_hle: TX packet type=1b payload=25 words=14 radio_phase=service_downlink data=00800f21012b2b
dspif_transport: RX enqueue type=80 payload=34 producer=0c4 data=801200001a2c000100000f0241f400006270422100000005e8329bfd062b2b
dsp_hle: TX packet type=1b payload=25 words=14 radio_phase=service_uplink_request data=00800f41012b2b
sim_device: update fid=6f3c record=1 length=176
"""


def nvram_with_message() -> bytes:
    result = bytearray(SMS_NVRAM_OFFSET + 176 + 88)
    result[SMS_NVRAM_OFFSET:
           SMS_NVRAM_OFFSET + len(STORED_RECORD_PREFIX)] = STORED_RECORD_PREFIX
    return bytes(result)


class IncomingSmsTraceCheckTest(unittest.TestCase):
    def test_complete_persistent_delivery(self):
        verify(GOOD, nvram_with_message())

    def test_rejects_duplicate_sapi3_establishment(self):
        sabm = next(line for line in GOOD.splitlines() if "0f3f01" in line)
        with self.assertRaisesRegex(ValueError, "exactly one SAPI 3 SABM"):
            verify(GOOD + sabm + "\n", nvram_with_message())

    def test_rejects_missing_second_segment(self):
        without = "\n".join(
            line for line in GOOD.splitlines() if "0f0241" not in line)
        with self.assertRaisesRegex(ValueError, "segment 2"):
            verify(without, nvram_with_message())

    def test_rejects_unstored_message(self):
        with self.assertRaisesRegex(ValueError, "EF_SMS record 1"):
            verify(GOOD, bytes(SMS_NVRAM_OFFSET + 176 + 88))


if __name__ == "__main__":
    unittest.main()
