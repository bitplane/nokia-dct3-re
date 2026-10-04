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
    boundary = text.find("outside_uploaded_code pc=2c75")
    if verified < 0 or boundary <= verified:
        raise ValueError("missing product-local loader verification or fail-closed mask boundary")
    if "observation_halt pc=2c75 ownership_retained=1" not in text:
        raise ValueError("missing silent native isolation at the mask boundary")
    if "unimplemented C54x opcode" in text or "[LUA ERROR]" in text:
        raise ValueError("execution or observer failed before the reviewed boundary")


def check_silent_runtime(text):
    check(text)
    commands = re.findall(r"6250_runtime_doorbell:.*pc=00429e48 command=([0-9a-f]+) argument=3fff pending=0001", text)
    if commands != ["0000"] * 3 + ["8102", "900f", "8426", "920c", "920c", "920f", "920f"]:
        raise ValueError("unexpected MCU parameter sequence with the DSP held silent")
    if not re.search(r"6250_runtime_boundary: arm_pc=[0-9a-f]+ dsp_pc=2c75 pending=0000", text):
        raise ValueError("missing retained native boundary at the observation endpoint")
    if "runtime_hle_handoff" in text or "RX enqueue" in text:
        raise ValueError("runtime peer answered during the silent comparison")


def check_service_control(text):
    if "runtime_hle_handoff pc=2c75 native_suspended=1" not in text:
        raise ValueError("missing exclusive native-to-HLE boundary")
    consumers = re.findall(r"6250_service_control_consumer: class=(\w+) command=(\w+) status=(\w+) armed=(\w+)", text)
    if consumers != [("74", "0d", "00", "84")]:
        raise ValueError("missing unique armed compact service-control consumption")
    if "6250_service_control_endpoint: flags=00 fault0=00 fault1=00" not in text:
        raise ValueError("service-control faults or armed wait remain")
    if "[LUA ERROR]" in text:
        raise ValueError("observer failed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--silent-runtime", action="store_true")
    parser.add_argument("--service-control", action="store_true")
    args = parser.parse_args()
    try:
        if args.silent_runtime and args.service_control:
            raise ValueError("silent and responding comparisons are mutually exclusive")
        checker = check_service_control if args.service_control else check_silent_runtime if args.silent_runtime else check
        checker(args.log.read_text())
    except (OSError, ValueError) as error:
        parser.exit(1, f"6250 staged boundary failed: {error}\n")
    print("6250 research boundary PASS; graphical idle and physical DSP identity remain unproved")


if __name__ == "__main__":
    main()
