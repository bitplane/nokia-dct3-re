"""Check the native 6250 upload boundary, not graphical boot or silicon identity."""

import argparse
from pathlib import Path
import re


def check(text):
    stages = re.findall(r"release entry=0f00 words=(\d+) prom_input=0006 clock=13000000 stage=(\w+)", text)
    if stages != [("223", "verifier"), ("126", "loader")]:
        raise ValueError("missing ordered native verifier/loader launches")
    publications = re.findall(r"publication word0=(\w+) word1=(\w+) word2=(\w+) word3=(\w+) pc=(\w+)", text)
    if publications != [("0000", "0006", "0006", "0006", "0f65")]:
        raise ValueError("unexpected native result under the declared peripheral inputs")
    if re.findall(r"request selector=([0-9a-f]+)", text) != ["0014"] + ["0001"] * 124:
        raise ValueError("unexpected native loader request sequence")
    verified = text.find("loader2_verified words=613 entry=0a00")
    boundary = text.find("unavailable_program address=2c75 pc=2c76 stage=loader")
    if verified < 0 or boundary <= verified:
        raise ValueError("missing product-local loader verification or fail-closed mask boundary")
    if "unimplemented C54x opcode" in text or "[LUA ERROR]" in text:
        raise ValueError("execution or observer failed before the reviewed boundary")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        check(args.log.read_text())
    except (OSError, ValueError) as error:
        parser.exit(1, f"6250 staged boundary failed: {error}\n")
    print("6250 native uploads PASS; missing mask code at 2c75 remains unexecuted")


if __name__ == "__main__":
    main()
