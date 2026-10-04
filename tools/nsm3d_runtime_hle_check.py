"""Check the declared hybrid boundary, not complete ROM6 execution or boot."""

import argparse
from pathlib import Path
import re

try:
    from tools.extract_nsm3_verifier import extract, extract_loader, extract_program_fragment
    from tools.nsm3d_live_verifier_check import check, check_boundary
    from tools.dct3_msid_codec import decode_msid
except ModuleNotFoundError:
    from extract_nsm3_verifier import extract, extract_loader, extract_program_fragment
    from nsm3d_live_verifier_check import check, check_boundary
    from dct3_msid_codec import decode_msid


def check_handoff(text):
    handoff = "staged_dsp: runtime_hle_handoff pc=2c75 native_suspended=1"
    if text.count(handoff) != 1 or "staged_dsp: observation_halt" in text:
        raise ValueError("missing exclusive native-to-HLE handoff")
    accepts = list(re.finditer(
        r"dsp_hle: parameter_accept coefficient=([0-9a-f]+) pending=([0-9a-f]+)", text))
    if len(accepts) != 7 or any(
            match.groups() != ("3fff", "0001") or match.start() < text.index(handoff)
            for match in accepts):
        raise ValueError("parameters were not accepted exclusively after handoff")
    requests = re.findall(
        r"nsm3d_control_request: command=([0-9a-f]+) argument=([0-9a-f]+)", text)
    expected = [("0032", "3fff"), ("0031", "ff00"), ("0033", "e000"),
                ("0008", "0002"), ("0009", "000f"), ("002f", "0000"),
                ("002f", "0000")]
    if requests != expected:
        raise ValueError("hybrid control sequence changed")
    if not re.search(r"nsm3d_loader_boundary: pc=2c75 selector=0000 ack=0000 "
                     r"pending=0000 fields=[^\n]+ t=8\.000000", text):
        raise ValueError("native resumed or parameter busy remained set")


def check_discovery(text):
    events = [
        "TX pending type=05 payload=10 data=1eff00d000030101e000",
        "RX enqueue type=8e payload=10 producer=086 data=1e0002d000030101e000",
        "RX enqueue type=8e payload=10 producer=08c data=1e0002d000030401c100",
        "TX pending type=05 payload=10 data=1e0200d0000305014100",
    ]
    cursor = 0
    for event in events:
        found = text.find(event, cursor)
        if found < 0:
            raise ValueError("missing ordered request-derived discovery exchange")
        cursor = found + len(event)
    words = dict(re.findall(
        r"nsm3d_shared_boundary: address=([0-9a-f]+) value=([0-9a-f]+)", text))
    if any(address not in words for address in ("000100a4", "000100a6", "000101c8", "000101ca")):
        raise ValueError("missing ring boundary observations")
    if words["000100a4"] != words["000100a6"] or words["000101c8"] != words["000101ca"]:
        raise ValueError("firmware did not drain the discovery rings")
    if "external_service: response command=" in text:
        raise ValueError("unsolicited application profile was enabled")


def check_service_inputs(text, image, pmm):
    # 28cb68 constructs these requests with logical PMM reads at 14, 00,
    # 0c and 20. The acquired PMM's low-record body starts at file 10026.
    base = 0x10026
    if len(image) != 0x1d0000 or len(pmm) != 0x30000:
        raise ValueError("unexpected 8250 flash or PMM extent")
    expected = [
        bytes.fromhex("1304") + image[-4:],
        bytes.fromhex("140c") + pmm[base + 0x14:base + 0x20],
        bytes.fromhex("1514") + pmm[base:base + 0x14],
        bytes.fromhex("1618") + pmm[base + 0x20:base + 0x38],
    ]
    packets = []
    for match in re.finditer(
            r"TX pending type=70 payload=(\d+) data=([0-9a-f]+)", text):
        payload = bytes.fromhex(match[2])
        if payload and payload[0] in (0x13, 0x14, 0x15, 0x16):
            if len(payload) != int(match[1]):
                raise ValueError("malformed service TX trace extent")
            packets.append(payload)
    if packets != expected:
        raise ValueError("service requests do not match ordered product-local inputs")


def check_identity_query(text, image):
    identities = re.findall(r"identity_query family=83 chip=([0-9a-f]{8}) "
                            r"revision=([0-9a-f]{2}) verdict=not_evaluated", text)
    replies = re.findall(r"RX enqueue type=74 payload=16 producer=[0-9a-f]+ "
                         r"data=(340e0083[0-9a-f]{24})", text)
    if len(identities) != 1 or len(replies) != 1:
        raise ValueError("missing unique computed identity query/reply")
    chip, revision = identities[0]
    msid = bytes.fromhex(replies[0])[3:]
    if decode_msid(msid) != image[-4:] + bytes.fromhex(chip + "acadab" + revision):
        raise ValueError("identity reply does not encode request checksum and modeled chip inputs")
    if f"nsm3d_identity_boundary: ready=01 record={msid.hex()}" not in text:
        raise ValueError("MCU did not retain the computed identity reply")
    if re.search(r"RX enqueue type=74[^\n]*data=0d00", text):
        raise ValueError("unrecovered self-test success was synthesized")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("flash", type=Path)
    parser.add_argument("--discovery", action="store_true", help="require verbose discovery and ring observations")
    parser.add_argument("--pmm", type=Path,
                        help="check verbose service requests against acquired product-local PMM")
    parser.add_argument("--identity", action="store_true",
                        help="require computed identity response and firmware retention, not final verdict")
    args = parser.parse_args()
    try:
        text, image = args.log.read_text(), args.flash.read_bytes()
        check(text, extract(image, "8250"), extract_loader(image), extract_program_fragment(image))
        check_boundary(text)
        check_handoff(text)
        if args.discovery:
            check_discovery(text)
        if args.pmm:
            check_service_inputs(text, image, args.pmm.read_bytes())
        if args.identity:
            check_identity_query(text, image)
    except (OSError, ValueError) as error:
        parser.exit(1, f"8250 runtime HLE failed: {error}\n")
    print("8250 native uploads and exclusive runtime HLE parameter acceptance verified; phone boot unproved")


if __name__ == "__main__":
    main()
