import pathlib
import tempfile
import unittest
from unittest import mock

from tools.radio_sms_inbox_trace_check import (
    EMPTY_INBOX_SHA256,
    ERASE_PROMPT_SHA256,
    NOTIFICATION_SHA256,
    SENDER_SHA256,
    TEXT_SHA256,
    verify,
)
from tools.radio_sms_acceptance_common import (
    FIRST_SMS_DELIVER_BODY as SMS_DELIVER_BODY,
    SMS_NVRAM_OFFSET,
)


TRANSPORT = "\n".join((
    "sim_device: update fid=6f3c record=1 length=176",
    "GSM service uplink sapi=3 pd=09 message=04 length=2 data=8904",
    "GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240",
    "GSM service downlink kind=17 sapi=3 pd=09 message=04",
    "LAPDm service Channel Release acknowledged nr=3",
    "PCH IMSI page transmitted channel=60",
))


class SmsInboxTraceCheckTest(unittest.TestCase):
    def _sim(self, status: int) -> bytes:
        result = bytearray(b"\xff" * 4096)
        result[SMS_NVRAM_OFFSET] = status
        result[
            SMS_NVRAM_OFFSET + 1:
            SMS_NVRAM_OFFSET + 1 + len(SMS_DELIVER_BODY)] = SMS_DELIVER_BODY
        return bytes(result)

    def _dir(self) -> pathlib.Path:
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        return pathlib.Path(temporary.name)

    def test_unread_delivery(self):
        with mock.patch(
                "tools.radio_sms_inbox_trace_check.frame_hashes",
                return_value={NOTIFICATION_SHA256}):
            verify(TRANSPORT, self._dir(), self._sim(3), "delivered")
            verify(TRANSPORT, self._dir(), self._sim(3), "dismissed")

    def test_read_requires_sender_text_and_status_update(self):
        log = TRANSPORT + (
            "\nsim_device: update fid=6f3c record=1 length=176")
        with mock.patch(
                "tools.radio_sms_inbox_trace_check.frame_hashes",
                return_value={next(iter(SENDER_SHA256)), TEXT_SHA256}):
            verify(log, self._dir(), self._sim(1), "read")
            with self.assertRaisesRegex(ValueError, "read status"):
                verify(TRANSPORT, self._dir(), self._sim(1), "read")

    def test_cold_read_needs_no_redelivery(self):
        with mock.patch(
                "tools.radio_sms_inbox_trace_check.frame_hashes",
                return_value={next(iter(SENDER_SHA256)), TEXT_SHA256}):
            verify("", self._dir(), self._sim(1), "cold")

    def test_confirmed_and_cancelled_delete(self):
        with mock.patch(
                "tools.radio_sms_inbox_trace_check.frame_hashes",
                return_value={ERASE_PROMPT_SHA256, EMPTY_INBOX_SHA256}):
            verify(TRANSPORT, self._dir(), self._sim(0), "deleted")
        with mock.patch(
                "tools.radio_sms_inbox_trace_check.frame_hashes",
                return_value={ERASE_PROMPT_SHA256}):
            verify(TRANSPORT, self._dir(), self._sim(1), "cancelled")

    def test_state_suffix_requires_replay(self):
        with mock.patch(
                "tools.radio_sms_inbox_trace_check.frame_hashes",
                return_value={NOTIFICATION_SHA256}):
            with self.assertRaisesRegex(ValueError, "state replay"):
                verify(TRANSPORT, self._dir(), self._sim(3),
                       "delivered-state")
        verify(
            TRANSPORT + "\nstate_roundtrip: result=pass",
            self._dir(), self._sim(3), "delivered-state")
        replay = "\n".join((
            TRANSPORT,
            "sim_device: update fid=6f3c record=1 length=176",
            "state_roundtrip: result=pass",
            "state_replay: phase=reference event=begin",
            "state_replay: phase=restored event=begin",
        ))
        verify(replay, self._dir(), self._sim(3), "delivered-state")


if __name__ == "__main__":
    unittest.main()
