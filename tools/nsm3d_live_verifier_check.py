"""Validate native uploaded-verifier publication, not a graphical phone boot."""

import argparse
from pathlib import Path
import re

try:
    from tools.extract_nsm3_verifier import extract, extract_loader
except ModuleNotFoundError:
    from extract_nsm3_verifier import extract, extract_loader


def check(text, program, loader=None):
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
        if not re.search(r"staged_dsp: unavailable_program address=ff80 pc=0f20 stage=loader t=[0-9.]+", text):
            raise ValueError("loader did not reach the exact missing-mask read")
    if "[LUA ERROR]" in text:
        raise ValueError("observer failed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("flash", type=Path)
    args = parser.parse_args()
    try:
        image = args.flash.read_bytes()
        check(args.log.read_text(), extract(image, "8250"), extract_loader(image))
    except (OSError, ValueError) as error:
        parser.exit(1, f"8250 live verifier failed: {error}\n")
    print("8250 native publication, MCU retention and exact loader upload verified; mask read ff80 remains unavailable")


if __name__ == "__main__":
    main()
