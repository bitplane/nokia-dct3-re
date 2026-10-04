"""Check the declared hybrid boundary, not complete ROM6 execution or boot."""

import argparse
from pathlib import Path
import re

try:
    from tools.extract_nsm3_verifier import extract, extract_loader, extract_program_fragment
    from tools.nsm3d_live_verifier_check import check, check_boundary
except ModuleNotFoundError:
    from extract_nsm3_verifier import extract, extract_loader, extract_program_fragment
    from nsm3d_live_verifier_check import check, check_boundary


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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("flash", type=Path)
    args = parser.parse_args()
    try:
        text, image = args.log.read_text(), args.flash.read_bytes()
        check(text, extract(image, "8250"), extract_loader(image), extract_program_fragment(image))
        check_boundary(text)
        check_handoff(text)
    except (OSError, ValueError) as error:
        parser.exit(1, f"8250 runtime HLE failed: {error}\n")
    print("8250 native uploads and exclusive runtime HLE parameter acceptance verified; phone boot unproved")


if __name__ == "__main__":
    main()
