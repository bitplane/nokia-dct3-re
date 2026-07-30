import pathlib
import tempfile
import unittest
from unittest import mock

from tools.radio_smart_message_persistence_trace_check import (
    COLD_LISTING_SCREEN_SHA256,
    NSE8_RECEIVED_TONE_END,
    NSE8_RECEIVED_TONE_PREFIX,
    NSE8_RECEIVED_TONE_START,
    NSE8_RECEIVED_TONE_TITLE,
    OPTIONS_SCREEN_SHA256,
    SAVED_SCREEN_SHA256,
    verify,
    verify_pmm_product,
)


DELIVERIES = "\n".join(
    f"GSM service downlink kind=16 sapi=3 pd=09 message=01 x t={time}"
    for time in (20.0, 24.0))
NOTES = "\n".join(
    f"buzzer: enabled=1 divider=1 frequency={frequency} volume=10 t=40.0"
    for frequency in (440, 494, 523, 587, 659))


class SmartMessagePersistenceTraceCheckTest(unittest.TestCase):
    def _eeprom(self, stored: bool) -> bytes:
        result = bytearray(b"\xff" * 0x4000)
        if stored:
            slot = bytearray(
                b"\xff" * (NSE8_RECEIVED_TONE_END -
                           NSE8_RECEIVED_TONE_START))
            slot[:len(NSE8_RECEIVED_TONE_PREFIX)] = (
                NSE8_RECEIVED_TONE_PREFIX)
            slot[-len(NSE8_RECEIVED_TONE_TITLE):] = (
                NSE8_RECEIVED_TONE_TITLE)
            result[NSE8_RECEIVED_TONE_START:NSE8_RECEIVED_TONE_END] = slot
        return bytes(result)

    def _snapshots(self, digest: str) -> pathlib.Path:
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        directory = pathlib.Path(temporary.name)
        # The checker deliberately hashes real frames. Patch the helper input
        # with a tiny object whose bytes have the production digest would hide
        # that invariant, so unit tests override only the set construction.
        return directory

    def test_saved_storage_shape(self):
        with mock.patch(
                "tools.radio_smart_message_persistence_trace_check."
                "_frame_hashes", return_value={SAVED_SCREEN_SHA256}):
            verify(
                DELIVERIES, self._snapshots(SAVED_SCREEN_SHA256),
                self._eeprom(True), b"flash", b"flash", "saved")

    def test_cold_listing_and_playback(self):
        with mock.patch(
                "tools.radio_smart_message_persistence_trace_check."
                "_frame_hashes", return_value={COLD_LISTING_SCREEN_SHA256}):
            verify(
                NOTES, self._snapshots(COLD_LISTING_SCREEN_SHA256),
                self._eeprom(True), b"flash", b"flash", "cold")

    def test_state_outcome_requires_roundtrip(self):
        with mock.patch(
                "tools.radio_smart_message_persistence_trace_check."
                "_frame_hashes", return_value={SAVED_SCREEN_SHA256}):
            with self.assertRaisesRegex(ValueError, "state replay"):
                verify(
                    DELIVERIES, self._snapshots(SAVED_SCREEN_SHA256),
                    self._eeprom(True), b"flash", b"flash", "saved-state")
            verify(
                DELIVERIES + "\nstate_roundtrip: result=pass",
                self._snapshots(SAVED_SCREEN_SHA256), self._eeprom(True),
                b"flash", b"flash", "saved-state")

    def test_discard_requires_erased_slot(self):
        verify(
            DELIVERIES, self._snapshots("unused"), self._eeprom(False),
            b"flash", b"flash", "discarded")
        with self.assertRaisesRegex(ValueError, "Discarded left"):
            verify(
                DELIVERIES, self._snapshots("unused"), self._eeprom(True),
                b"flash", b"flash", "discarded")

    def test_cancel_requires_options_and_erased_slot(self):
        with mock.patch(
                "tools.radio_smart_message_persistence_trace_check."
                "_frame_hashes", return_value={OPTIONS_SCREEN_SHA256}):
            verify(
                DELIVERIES, self._snapshots(OPTIONS_SCREEN_SHA256),
                self._eeprom(False), b"flash", b"flash", "cancelled")

    def test_flash_is_independent_owner(self):
        with self.assertRaisesRegex(ValueError, "flash changed"):
            verify(
                DELIVERIES, self._snapshots("unused"), self._eeprom(False),
                b"changed", b"flash", "discarded")

    def test_pmm_product_requires_product_local_flash_change(self):
        reference = b"\xff" * 0x360100
        saved = bytearray(reference)
        saved[0x360000:0x360080] = b"\x55" * 0x80
        verify_pmm_product(
            DELIVERIES, self._snapshots("unused"), bytes(saved), reference,
            b"eeprom", b"eeprom", b"sim", b"sim", "nhm2", "saved")
        saved[0x100:0x180] = b"\xaa" * 0x80
        with self.assertRaisesRegex(ValueError, "executable flash"):
            verify_pmm_product(
                DELIVERIES, self._snapshots("unused"), bytes(saved), reference,
                b"eeprom", b"eeprom", b"sim", b"sim", "nhm2", "saved")

    def test_pmm_product_keeps_other_nonvolatile_stores_unchanged(self):
        reference = b"\xff" * 0x1E0100
        saved = bytearray(reference)
        saved[0x1E0000:0x1E0080] = b"\x55" * 0x80
        with self.assertRaisesRegex(ValueError, "EEPROM changed"):
            verify_pmm_product(
                DELIVERIES, self._snapshots("unused"), bytes(saved), reference,
                b"changed", b"reference", b"sim", b"sim", "nhm5", "saved")
