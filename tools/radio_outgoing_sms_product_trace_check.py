#!/usr/bin/env python3
"""Verify a sibling ROM completes physical mobile-originated SMS."""

import hashlib
import pathlib
import re
import sys

try:
    from tools.radio_outgoing_sms_trace_check import MESSAGE_SENT_HASHES
except ModuleNotFoundError:  # Direct execution from tools/.
    from radio_outgoing_sms_trace_check import MESSAGE_SENT_HASHES


CHECKPOINTS = (
    ("SMS CM Service Request", re.compile(
        r"GSM service establish sapi=0 pd=05 message=24 length=16")),
    ("SAPI 3 SABM", re.compile(r"TX packet type=1b .*data=00800d3f01")),
    ("decoded SMS-SUBMIT", re.compile(
        r"gsm_sms_submit: cp=[0-9a-f]{2} rp=01 smsc=1234567890 "
        r"destination=5551234 alphabet=[0-2] user_length=[1-9][0-9]* "
        r"outcome=0 status_report=[01]")),
    ("network CP-ACK", re.compile(
        r"GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2")),
    ("network RP-ACK", re.compile(
        r"GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5")),
    ("handset final CP-ACK", re.compile(
        r"GSM service uplink sapi=3 pd=09 message=04 length=2 "
        r"data=[0-9a-f]{2}04")),
    ("RR Channel Release", re.compile(
        r"LAPDm service Channel Release acknowledged nr=2")),
)


NHM5_MESSAGE_SENT_HASHES = {
    "44dbccdc9aebdb60998a7d7491d1ed2974a0ea37ff24f51e743a87c3e2286a75",
    "48afd7a5f3103a99e20f54f0a8066ece3683d0a478bf8074b0f30a84ade98e72",
    "3ee877904724bc41cd0b9133ce53e4797a029cae31e01b9a43a57845771273f0",
    "c2268ce0d9affb0b05c74c4db85220ea87e783652fdedf87b3a3608ccf0919d4",
    "bfe89d2440646302ccd54cce39bffbbc3aede4d57746e34eab48c0f540392c68",
    "046e6ae27d660146a7ac144356252df4dae1b01d8f9980ded87478646e7aa927",
}
NHM6_MESSAGE_SENT_HASHES = {
    "71641c2f9548e5425eb827bf5a59cd2e7a2fc4fd402c4c4e958f5579ac04a498",
}
NHM2_MESSAGE_SENT_HASHES = {
    "4c413fdb7b8e49a48f02f6e1ef3365c0182de793d1de7de420867f4dba8fa43c",
    "9ea23fd0a123dd22f2c140aaeb6b87722c95737be1547eb2838cc49631250adf",
    "f4589d7ee3234a7d78ce2ac97f9d92263580b73379bbc118f342250545f85984",
    "057226cf79db8001bf36edcd010e52dff1ad1e90c3bc775bbb9b78012022da4f",
    "8382b1f43d27a2717dae8f3d997affa98de04b5eda3a03c0ae16ab9a7440046c",
    "674e4d939668ad9689b11ef6617d88faa1bf6d400e4b3f627cdea98ee278f2d3",
    "7a95df684452191db95aeab6bc291a225280d9e7013315b0ec770b5b365c78f0",
}


def verify(text: str, frame_directory: pathlib.Path,
           product: str = "nse8") -> None:
    cursor = 0
    for label, pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing or out-of-order product SMS checkpoint: {label}")
        cursor = match.end()
    hashes = {
        hashlib.sha256(frame.read_bytes()).hexdigest()
        for frame in frame_directory.glob("nokia_dct3_lcdmirror_*.pgm")
    }
    expected_hashes = {
        "nhm5": NHM5_MESSAGE_SENT_HASHES,
        "nhm6": NHM6_MESSAGE_SENT_HASHES,
        "nhm2": NHM2_MESSAGE_SENT_HASHES,
    }.get(product, MESSAGE_SENT_HASHES)
    if not hashes.intersection(expected_hashes):
        raise ValueError("firmware Message sent frame was not observed")


def main() -> int:
    if len(sys.argv) not in (3, 4):
        raise SystemExit(
            "usage: radio_outgoing_sms_product_trace_check.py LOG FRAME_DIR")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text(), pathlib.Path(sys.argv[2]),
               sys.argv[3] if len(sys.argv) == 4 else "nse8")
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - sibling ROM completed physical mobile-originated SMS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
