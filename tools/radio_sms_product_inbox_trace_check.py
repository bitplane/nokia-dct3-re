#!/usr/bin/env python3
"""Cross-product ordinary MT-SMS application and SIM-storage checks."""

import pathlib
import re
import sys

try:
    from tools.radio_sms_acceptance_common import (
        FIRST_SMS_DELIVER_BODY,
        TRANSPORT_CLOSURE,
        frame_hashes,
        require_ordered,
        sms_record,
    )
except ModuleNotFoundError:
    from radio_sms_acceptance_common import (
        FIRST_SMS_DELIVER_BODY,
        TRANSPORT_CLOSURE,
        frame_hashes,
        require_ordered,
        sms_record,
    )

PRODUCT_HASHES = {
    # NHM-6 currently proves transport and SIM ownership only. Its preserved-
    # PMM cold boot re-enters firmware time/date provisioning before ordinary
    # inbox navigation, so no application/UI promotion is implied here.
    "3330": {"transport": set()},
    "3310": {
        "received": {
            "5b59a304d1f24eef14559a0fa7ce17f1c40975f3dd105a7934114a6a69911af2"},
        "read": {
            "bc0a41ddb29d40586ba95e35afe6b1c74728a6d3793c32fc81c7b90f288c58ff"},
        "deleted": {
            "9bff187c806de70d4f99d04c69dc6e7fa7f9b1e0df2d592437ff2033bc36588a"},
    },
    "3410": {
        "received": {
            "2f360de4b72eaa262d4014b94a987be9ab033cc342018f0dd5bb407b6d5eb4a9"},
        "read": {
            "28e06c27bd5a9e54467d26fea8df99eea10b95da7a46bd2c733d53f5e27e121f"},
        "deleted": {
            "9ae777d3d57e365bee020add58a730a4b5520f951ba48abfcfcb804b1a1de80a"},
    },
}


def verify(
        product: str,
        outcome: str,
        log: str,
        snapshots: pathlib.Path,
        sim_nvram: bytes) -> None:
    try:
        expected_hashes = PRODUCT_HASHES[product][outcome]
    except KeyError:
        raise ValueError(f"unsupported product/outcome {product}/{outcome}") from None
    if expected_hashes and expected_hashes.isdisjoint(frame_hashes(snapshots)):
        raise ValueError(f"{product} did not reach semantic UI state {outcome}")

    record = sms_record(sim_nvram)
    expected_status = {
        "transport": 0x03, "received": 0x03,
        "read": 0x01, "deleted": 0x00}[outcome]
    if record[0] != expected_status:
        raise ValueError(
            f"{product} EF_SMS status is {record[0]:02x}, "
            f"expected {expected_status:02x}")
    if not record[1:].startswith(FIRST_SMS_DELIVER_BODY):
        raise ValueError(f"{product} unexpectedly rewrote the SMS payload")

    if outcome in ("transport", "received"):
        require_ordered(log, TRANSPORT_CLOSURE)
    elif not re.search(
            r"sim_device: update fid=6f3c record=1 length=176", log):
        raise ValueError(f"{product} did not persist the {outcome} transition")


def main() -> int:
    if len(sys.argv) != 6:
        raise SystemExit(
            "usage: radio_sms_product_inbox_trace_check.py "
            "PRODUCT transport|received|read|deleted LOG SNAPSHOTS SIM_NVRAM")
    try:
        verify(
            sys.argv[1], sys.argv[2],
            pathlib.Path(sys.argv[3]).read_text(),
            pathlib.Path(sys.argv[4]),
            pathlib.Path(sys.argv[5]).read_bytes())
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(f"OK - Nokia {sys.argv[1]} ordinary SMS outcome is {sys.argv[2]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
