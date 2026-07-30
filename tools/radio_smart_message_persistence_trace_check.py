#!/usr/bin/env python3
"""Check firmware-owned received-ringtone persistence and discard evidence."""

import hashlib
import pathlib
import re
import sys


# NSE-8 v6.00's received-tone object occupies the second user-tone slot in
# the fitted 24C128.  These bounds are evidence about this product's EEPROM
# layout, not a generic Smart Messaging storage contract.
NSE8_RECEIVED_TONE_START = 0x0B24
NSE8_RECEIVED_TONE_END = 0x0C60
NSE8_RECEIVED_TONE_TITLE = b"Test for Dhiram\x00"
NSE8_RECEIVED_TONE_PREFIX = bytes.fromhex(
    "0002fc09000a018a1c40028a1c4002871c4002891c40020afe")

SAVED_SCREEN_SHA256 = (
    "5024682c5c1d9879925d32461ea13066589ffcae93acf768dba9d40cae3c2d02")
COLD_LISTING_SCREEN_SHA256 = (
    "87080555d70a3a052d780f2f612bcec8ad0f14c1d0359c13b6cac2fedab9d94f")
OPTIONS_SCREEN_SHA256 = (
    "64c4586b72410eb6093daf93c1986f998bef14706e3842180545cc261d007c1a")
NHM2_COLD_LISTING_SCREEN_SHA256 = frozenset({
    "193ac4deb94e6564f6f88392d696261d522d07c83eac36b3ebe75545a9f52633",
})
NHM5_COLD_LISTING_SCREEN_SHA256 = frozenset({
    # The same saved object under the two organically selected PPM languages.
    "9a3860263374ebe67d0a1e8af995e079538b224c6f21fc1ffead6655a7ea3362",
    "85b23d7e6553c188132bbae3243b512b5dbe32079b182e9bddb42277d133e940",
})

# NHM-2 and NHM-5 fit their product-management data into the flash device,
# unlike NSE-8's separate received-tone EEPROM object.  The lower bounds are
# product layout evidence; they are deliberately not a shared DCT3 offset.
NHM2_PMM_START = 0x360000
NHM5_PMM_START = 0x1E0000

DOWNLINK = re.compile(
    r"GSM service downlink kind=16 sapi=3 pd=09 message=01 .*t=([0-9.]+)")
PUP_NOTE = re.compile(
    r"buzzer: enabled=1 divider=(\d+) frequency=(\d+) volume=(\d+) "
    r"t=([0-9.]+)")
DSP_TONE_NOTE = re.compile(
    r"dsp_tone: oscillator=[0-9a-f]+/[0-9a-f]+ frequency=(\d+)/\d+ "
    r"amplitude=[0-9a-f]+ active=1/0 t=([0-9.]+)")


def _frame_hashes(snapshot_dir: pathlib.Path) -> set[str]:
    return {
        hashlib.sha256(path.read_bytes()).hexdigest()
        for path in snapshot_dir.glob("nokia_dct3_lcdmirror_*.pgm")
    }


def _verify_storage_owner(
        eeprom: bytes, flash: bytes, reference_flash: bytes) -> bytes:
    if len(eeprom) != 0x4000:
        raise ValueError(
            f"NSE-8 EEPROM has {len(eeprom)} bytes, expected 16384")
    if flash != reference_flash:
        raise ValueError("phone flash changed during received-tone lifecycle")
    return eeprom[NSE8_RECEIVED_TONE_START:NSE8_RECEIVED_TONE_END]


def verify(
        log: str,
        snapshot_dir: pathlib.Path,
        eeprom: bytes,
        flash: bytes,
        reference_flash: bytes,
        outcome: str) -> None:
    state_outcome = outcome.endswith("-state")
    base_outcome = outcome.removesuffix("-state")
    if state_outcome and "state_roundtrip: result=pass" not in log:
        raise ValueError("missing successful persistence-boundary state replay")

    slot = _verify_storage_owner(eeprom, flash, reference_flash)
    frame_hashes = _frame_hashes(snapshot_dir)

    if base_outcome == "saved":
        if len(DOWNLINK.findall(log)) != 2:
            raise ValueError("save run did not organically deliver both parts")
        if SAVED_SCREEN_SHA256 not in frame_hashes:
            raise ValueError("missing firmware 'Ringing tone saved' transition")
        if not slot.startswith(NSE8_RECEIVED_TONE_PREFIX):
            raise ValueError("received RTPL command stream is absent from EEPROM")
        if not slot.endswith(NSE8_RECEIVED_TONE_TITLE):
            raise ValueError("received RTPL title is absent from EEPROM")
        if eeprom.count(NSE8_RECEIVED_TONE_TITLE) != 1:
            raise ValueError("received RTPL title is duplicated in EEPROM")
    elif base_outcome == "cold":
        if COLD_LISTING_SCREEN_SHA256 not in frame_hashes:
            raise ValueError(
                "preserved cold boot did not list 'Test for Dhiram' as 9-2-39")
        if not slot.startswith(NSE8_RECEIVED_TONE_PREFIX):
            raise ValueError("cold-boot EEPROM lost the received RTPL stream")
        if not slot.endswith(NSE8_RECEIVED_TONE_TITLE):
            raise ValueError("cold-boot EEPROM lost the received RTPL title")
        frequencies = {
            int(frequency)
            for _, frequency, _, _ in PUP_NOTE.findall(log)
            if int(frequency) < 10000
        }
        if len(frequencies) < 5:
            raise ValueError(
                "ordinary cold-boot tone listing did not replay varied notes")
    elif base_outcome in ("discarded", "cancelled"):
        if len(DOWNLINK.findall(log)) != 2:
            raise ValueError(
                f"{base_outcome} run did not organically deliver both parts")
        if (base_outcome == "cancelled" and
                OPTIONS_SCREEN_SHA256 not in frame_hashes):
            raise ValueError("cancel run did not reach the physical Options UI")
        if slot != b"\xff" * len(slot):
            raise ValueError(
                f"{base_outcome.capitalize()} left a received-tone object "
                "in EEPROM")
        if NSE8_RECEIVED_TONE_TITLE in eeprom:
            raise ValueError(
                f"{base_outcome.capitalize()} retained the received-tone title")
    else:
        raise ValueError(f"unsupported persistence outcome {outcome!r}")


def verify_pmm_product(
        log: str,
        snapshot_dir: pathlib.Path,
        flash: bytes,
        reference_flash: bytes,
        eeprom: bytes,
        reference_eeprom: bytes,
        sim: bytes,
        reference_sim: bytes,
        product: str,
        outcome: str) -> None:
    """Verify independently evidenced PMM-backed NHM-2/NHM-5 persistence."""
    profiles = {
        "nhm2": (NHM2_PMM_START, NHM2_COLD_LISTING_SCREEN_SHA256, 5),
        "nhm5": (NHM5_PMM_START, NHM5_COLD_LISTING_SCREEN_SHA256, 0),
    }
    if product not in profiles:
        raise ValueError(f"unsupported PMM persistence product {product!r}")
    pmm_start, listing_digest, minimum_pup_notes = profiles[product]
    if len(flash) != len(reference_flash):
        raise ValueError("saved and reference flash sizes differ")
    if eeprom != reference_eeprom:
        raise ValueError(f"{product.upper()} EEPROM changed during PMM save")
    if sim != reference_sim:
        raise ValueError(f"{product.upper()} SIM changed during PMM save")

    changed = [
        offset
        for offset, (before, after) in enumerate(zip(reference_flash, flash))
        if before != after
    ]
    if not changed:
        raise ValueError(f"{product.upper()} PMM did not record the ringtone")
    if min(changed) < pmm_start:
        raise ValueError(
            f"{product.upper()} ringtone save changed executable flash")
    if len(changed) < 64:
        raise ValueError(
            f"{product.upper()} PMM change is too small for ringtone data")

    if outcome == "saved":
        if len(DOWNLINK.findall(log)) != 2:
            raise ValueError("save run did not organically deliver both parts")
    elif outcome == "cold":
        if listing_digest.isdisjoint(_frame_hashes(snapshot_dir)):
            raise ValueError(
                f"{product.upper()} cold boot did not list the saved title")
        if minimum_pup_notes:
            frequencies = {
                int(frequency)
                for _, frequency, _, _ in PUP_NOTE.findall(log)
                if int(frequency) < 10000
            }
            if len(frequencies) < minimum_pup_notes:
                raise ValueError(
                    f"{product.upper()} saved tone lacked varied PUP notes")
        if product == "nhm5" and len(DSP_TONE_NOTE.findall(log)) < 5:
            raise ValueError(
                "NHM5 saved-tone selection produced no sustained DSP audio")
    else:
        raise ValueError(f"unsupported PMM persistence outcome {outcome!r}")


def main() -> int:
    if len(sys.argv) == 11 and sys.argv[1] in ("nhm2", "nhm5"):
        try:
            verify_pmm_product(
                pathlib.Path(sys.argv[2]).read_text(),
                pathlib.Path(sys.argv[3]),
                pathlib.Path(sys.argv[4]).read_bytes(),
                pathlib.Path(sys.argv[5]).read_bytes(),
                pathlib.Path(sys.argv[6]).read_bytes(),
                pathlib.Path(sys.argv[7]).read_bytes(),
                pathlib.Path(sys.argv[8]).read_bytes(),
                pathlib.Path(sys.argv[9]).read_bytes(),
                sys.argv[1],
                sys.argv[10])
        except ValueError as error:
            raise SystemExit(str(error)) from None
        print(
            f"OK - {sys.argv[1].upper()} PMM-backed received-ringtone "
            f"persistence outcome is {sys.argv[10]}")
        return 0
    if len(sys.argv) != 7:
        raise SystemExit(
            "usage: radio_smart_message_persistence_trace_check.py "
            "MAME_ERROR_LOG SNAPSHOT_DIR EEPROM FLASH REFERENCE_FLASH "
            "saved|cold|discarded\n"
            "   or: radio_smart_message_persistence_trace_check.py "
            "nhm2|nhm5 MAME_ERROR_LOG SNAPSHOT_DIR FLASH REFERENCE_FLASH "
            "EEPROM REFERENCE_EEPROM SIM REFERENCE_SIM saved|cold")
    try:
        verify(
            pathlib.Path(sys.argv[1]).read_text(),
            pathlib.Path(sys.argv[2]),
            pathlib.Path(sys.argv[3]).read_bytes(),
            pathlib.Path(sys.argv[4]).read_bytes(),
            pathlib.Path(sys.argv[5]).read_bytes(),
            sys.argv[6])
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - NSE-8 firmware-owned received-ringtone persistence outcome is "
        f"{sys.argv[6]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
