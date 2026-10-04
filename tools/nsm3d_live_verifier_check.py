"""Validate native uploaded-verifier publication, not a graphical phone boot."""

import argparse
from pathlib import Path
import re

try:
    from tools.extract_nsm3_verifier import extract, extract_loader, extract_program_fragment
except ModuleNotFoundError:
    from extract_nsm3_verifier import extract, extract_loader, extract_program_fragment


def check(text, program, loader=None, fragment=None):
    captures = re.findall(r"nsm3d_verifier_program: words=([0-9a-f]+)", text)
    if len(captures) != 1 or bytes.fromhex(captures[0]) != program:
        raise ValueError("runtime program differs from the pinned handset upload")
    publications = re.findall(
        r"staged_dsp: publication word0=([0-9a-f]+) word1=([0-9a-f]+) "
        r"word2=([0-9a-f]+) word3=([0-9a-f]+) pc=([0-9a-f]+) t=([0-9.]+)", text)
    if len(publications) != 1:
        raise ValueError("missing unique native publication")
    *words, pc, published = publications[0]
    if tuple(int(word, 16) for word in words) != (0, 6, 6, 6):
        raise ValueError("unexpected result for declared PROM6/nominal COBBA inputs")
    if not 0x0f00 <= int(pc, 16) < 0x0fdf:
        raise ValueError("publication outside the uploaded code")
    retained = re.findall(
        r"nsm3d_release: pc=002cb328 control=10 result0=0000 result1=0006 "
        r"retained0=0000 retained1=0006 pairs0=58 pairs1=58 order_errors=0 t=([0-9.]+)", text)
    if len(retained) != 1 or float(retained[0]) < float(published):
        raise ValueError("MCU did not retain the result after 58 ordered pairs")
    if "nsm3d_loader_descriptor: address=00311d14 fields=fd00/ff80/027e/0500/0078/0000" not in text:
        raise ValueError("next loader descriptor was not observed")
    if loader is not None:
        uploads = re.findall(r"nsm3d_loader_upload: words=([0-9a-f]+)", text)
        if len(uploads) != 1 or bytes.fromhex(uploads[0]) != loader:
            raise ValueError("runtime loader upload differs from pinned flash")
    if fragment is not None:
        fragments = re.findall(r"nsm3d_program_fragment: words=([0-9a-f]+)", text)
        if len(fragments) != 1 or bytes.fromhex(fragments[0]) != fragment:
            raise ValueError("program fragment differs from product-local flash")
        requests = re.findall(r"staged_dsp: request selector=([0-9a-f]+)", text)
        if requests != ["0014"] + ["0001"] * 133:
            raise ValueError("native loader request order differs")
        if "staged_dsp: loader2_verified words=613 entry=0a00" not in text:
            raise ValueError("MCU-supplied loader2 was not verified before execution")
    if "unimplemented C54x opcode" in text:
        raise ValueError("native execution stopped on an unimplemented instruction")
    if "[LUA ERROR]" in text:
        raise ValueError("observer failed")


def check_boundary(text):
    installs = re.findall(
        r"staged_dsp: installed_program words=(\d+) first=([0-9a-f]+) "
        r"last=([0-9a-f]+) target_data=([0-9a-f]+) pmst=([0-9a-f]+)", text)
    if installs != [("422", "0590", "0735", "0000", "07ac")]:
        raise ValueError("unexpected executed-loader program installation")
    probes = re.findall(
        r"staged_dsp: readonly_program_write address=([0-9a-f]+) "
        r"data=([0-9a-f]+) pc=([0-9a-f]+)", text)
    if probes != [("ff87", "0006", "0f12")]:
        raise ValueError("unexpected resident-ROM write probe")
    stops = re.findall(r"staged_dsp: outside_uploaded_code pc=([0-9a-f]+)", text)
    if stops != ["2c75"]:
        raise ValueError("native boundary is not the recovered CALL 2c75")
    if "unmodelled program write" in text:
        raise ValueError("native loader attempted an unsupported program write")


def check_silent_observation(text):
    if "staged_dsp: observation_halt pc=2c75 ownership_retained=1" not in text:
        raise ValueError("missing native observation halt")
    requests = re.findall(
        r"nsm3d_control_request: command=([0-9a-f]+) argument=([0-9a-f]+) "
        r"commit=([0-9a-f]+) wire=([0-9a-f]+) pending=([0-9a-f]+)", text)
    if requests != [("0032", "3fff", "0001", "900f", "0001")]:
        raise ValueError("unexpected requests while the DSP is explicitly silent")
    boundary = re.findall(
        r"nsm3d_loader_boundary: pc=([0-9a-f]+) selector=[0-9a-f]+ "
        r"ack=[0-9a-f]+ pending=([0-9a-f]+) fields=[^\n]+ t=([0-9.]+)", text)
    if boundary != [("2c75", "0001", "8.000000")]:
        raise ValueError("native PC or pending ownership changed during observation")


def check_parameter_origin(text):
    parameters = re.findall(
        r"nsm3d_control_parameter: caller=([0-9a-f]+) "
        r"wrapper_caller=([0-9a-f]+) address=([0-9a-f]+) value=([0-9a-f]+)", text)
    if parameters != [("002b6175", "002b61f1", "000100b8", "3fff")]:
        raise ValueError("initial DSP parameter did not originate in the recovered setter")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("flash", type=Path)
    args = parser.parse_args()
    try:
        image = args.flash.read_bytes()
        text = args.log.read_text()
        check(text, extract(image, "8250"), extract_loader(image), extract_program_fragment(image))
        check_boundary(text)
        check_silent_observation(text)
        check_parameter_origin(text)
    except (OSError, ValueError) as error:
        parser.exit(1, f"8250 live verifier failed: {error}\n")
    print("8250 native verifier and loaders verified; silent-DSP command-32 boundary preserved; phone boot remains unproved")


if __name__ == "__main__":
    main()
