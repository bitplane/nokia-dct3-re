import unittest

from tools.radio_incoming_smart_message_trace_check import (
    FREE_SMS_RECORD_PREFIX,
    SMS_NVRAM_OFFSET,
    expected_first_part,
    verify,
)


def good_trace() -> str:
    lines = [
        "dsp_hle: LAPDm Channel Release acknowledged nr=2",
        "dsp_hle: PCH IMSI page transmitted channel=60 fn=3759",
        "dsp_hle: TX packet type=1b data=0080013f410627",
        "dspif_transport: RX enqueue type=80 payload=34 data="
        "801200001a25000100000f3f01" + "2b" * 21,
        "dsp_hle: TX packet type=1b data=00800f7301",
    ]
    information = expected_first_part()
    for index in range(9):
        chunk = information[index * 20:(index + 1) * 20]
        more = index < 8
        frame = bytes([
            0x0f,
            (index & 7) << 1,
            (len(chunk) << 2) | (0x03 if more else 0x01),
        ]) + chunk
        frame += bytes([0x2b]) * (24 - len(frame))
        lines.append(
            "dspif_transport: RX enqueue type=80 payload=34 data="
            + "801200001a2a00010000" + frame.hex())
        lines.append(
            "dsp_hle: TX packet type=1b data=00800f"
            + f"{((((index + 1) & 7) << 5) | 1):02x}01")
    return "\n".join(lines)


def free_sms_nvram() -> bytes:
    result = bytearray(SMS_NVRAM_OFFSET + 176 + 88)
    result[SMS_NVRAM_OFFSET:
           SMS_NVRAM_OFFSET + len(FREE_SMS_RECORD_PREFIX)] = (
               FREE_SMS_RECORD_PREFIX)
    return bytes(result)


class IncomingSmartMessageTraceCheckTest(unittest.TestCase):
    def test_complete_first_multipart_part(self):
        verify(good_trace(), free_sms_nvram())

    def test_rejects_missing_stop_and_wait_ack(self):
        trace = good_trace()
        trace = trace.replace(
            "dsp_hle: TX packet type=1b data=00800f6101", "", 1)
        with self.assertRaisesRegex(ValueError, "stop-and-wait"):
            verify(trace, free_sms_nvram())

    def test_rejects_changed_payload(self):
        trace = good_trace().replace("5195cdd0", "5195cdd1", 1)
        with self.assertRaisesRegex(ValueError, "does not match"):
            verify(trace, free_sms_nvram())

    def test_rejects_text_sms_storage(self):
        with self.assertRaisesRegex(ValueError, "incorrectly filed"):
            verify(
                good_trace()
                + "\nsim_device: update fid=6f3c record=1 length=176\n",
                free_sms_nvram())

    def test_rejects_changed_sms_record(self):
        changed = bytearray(free_sms_nvram())
        changed[SMS_NVRAM_OFFSET] = 0x03
        with self.assertRaisesRegex(ValueError, "record 1 changed"):
            verify(good_trace(), bytes(changed))


if __name__ == "__main__":
    unittest.main()
