#!/usr/bin/env python3
"""Check 8850 native upload ownership and optional scoped SIM/UI acceptance."""

import argparse
import hashlib
from pathlib import Path
import re
import sys


STAGES = (
    ("verifier release", r"staged_dsp: release entry=0f00 words=223 prom_input=0006 .*stage=verifier"),
    ("native verifier publication", r"staged_dsp: publication word0=0000 word1=0006 word2=0006 word3=0006"),
    ("loader release", r"staged_dsp: release entry=0f00 words=126 prom_input=0006 .*stage=loader"),
    ("product-local loader verification", r"staged_dsp: loader2_verified words=613 entry=0a00"),
    ("loader control RMW", r"staged_dsp: control_write port=001c data=0200"),
    ("missing mask boundary", r"staged_dsp: outside_uploaded_code pc=2c75"),
    ("fail-closed suspension", r"staged_dsp: observation_halt pc=2c75 ownership_retained=1"),
    ("firmware readiness", r"8850_ready: state=01 shared_e4=0000"),
)


def check_physical_inputs(text: str) -> list[str]:
    return check_correlated_inputs(text, (
        ("security_physical: key=Keypad 1", "01"),
        ("security_physical: key=Keypad 2", "02"),
        ("security_physical: key=Keypad 3", "03"),
        ("security_physical: key=Keypad 4", "04"),
        ("security_physical: key=Keypad 5", "05"),
        ("security_physical: key=Menu", "19"),
        ("navigation_physical: action=messages_menu", "19"),
        ("navigation_physical: action=inbox", "19"),
        ("navigation_physical: action=names", "1a"),
    ))


def check_correlated_inputs(text: str, events: tuple[tuple[str, str], ...]) -> list[str]:
    errors = []
    cursor = 0
    for label, key in events:
        marker = text.find("8850_" + label, cursor)
        if marker < 0:
            errors.append(f"missing or out-of-order physical input {label}")
            continue
        next_marker = re.search(r"8850_[a-z_]+_physical:", text[marker + 1:])
        end = marker + 1 + next_marker.start() if next_marker else len(text)
        if not re.search(rf"8850_keypad_decoded key={key}\b", text[marker:end]):
            errors.append(f"physical input {label} did not decode as {key}")
        cursor = end
    return errors


def check_calculator(text: str, path: Path) -> list[str]:
    from PIL import Image
    errors = check_correlated_inputs(text, tuple(
        ("application_physical: action=" + action, key) for action, key in (
            ("input_1", "01"), ("input_12", "02"),
            ("operation_options", "19"), ("add", "18"), ("plus", "19"),
            ("input_3", "03"), ("options", "19"), ("result", "19"),
        )))
    try:
        with Image.open(path) as frame:
            # Arithmetic output only; exclude cursor/softkeys and the status row.
            digest = hashlib.sha256(frame.convert("L").crop((0, 8, 84, 36)).tobytes()).hexdigest()
            if frame.size != (84, 48) or digest != "afed4f977aaabaad65fb2d0d0090d0056cb6cda2df297146ae7b90c302503f68":
                errors.append("calculator result differs from reviewed 12+3=15 frame")
    except OSError as error:
        errors.append(f"calculator frame: {error}")
    return errors


def check_navigation_frames(directory: Path) -> list[str]:
    from PIL import Image
    errors = []
    # Top-left title/list rows only: exclude the animated menu icon and scrollbar.
    for name, expected in (
        ("8850_messages_menu.png", "1e5c11fcea9aac5331e18c0070697e8795003d6de9a0250284b3d9605209e654"),
        ("8850_inbox.png", "8655c31363c0632c945343fa325d5487b9cc9cecc236fc46d12b97fd85506798"),
        ("8850_names.png", "8805936b7afa6dc245d2df6a22a387e8080290c1db117f5c5ee3e7aa234f72d0"),
    ):
        try:
            with Image.open(directory / name) as frame:
                if frame.size != (84, 48):
                    errors.append(f"{name}: unexpected geometry {frame.size}")
                    continue
                digest = hashlib.sha256(frame.convert("L").crop((0, 0, 72, 16)).tobytes()).hexdigest()
                if digest != expected:
                    errors.append(f"{name}: stable title/list pixels differ")
        except OSError as error:
            errors.append(f"{name}: {error}")
    return errors


def check_trace(text: str, runtime_hle: bool = False, startup_readiness: bool = False,
                display_transfer: bool = False, sim_reads: bool = False) -> list[str]:
    errors = []
    cursor = 0
    stages = STAGES
    if runtime_hle:
        stages = STAGES[:6] + (
            ("exclusive HLE handoff", r"staged_dsp: runtime_hle_handoff pc=2c75 native_suspended=1"),
            ("own self-test request", r"dspif_transport: TX pending type=70 payload=2 data=0d00"),
            ("compact peer response", r"dspif_transport: RX enqueue type=74 payload=2 .*data=0d00"),
            ("firmware fault clearance", r"8850_faults: bytes=[0-9a-f]{30}000000[0-9a-f]{12}"),
            STAGES[-1],
        )
    for name, pattern in stages:
        match = re.compile(pattern).search(text, cursor)
        if match is None:
            errors.append(f"missing or out-of-order {name}")
        else:
            cursor = match.end()
    if "Fatal error:" in text or (not runtime_hle and "runtime_hle_handoff" in text):
        errors.append("native-only observation contains a fatal error or HLE handoff")
    if startup_readiness:
        cursor = 0
        for name, pattern in (
            ("organic report 14", r"8850_report14_stub r14=00244cbb"),
            ("report 14 consumption", r"8850_startup_dispatch report=00000014 state=000d"),
            ("complete startup predicate", r"8850_startup_check power=06 reports=0f"),
            ("startup continuation", r"8850_startup_dispatch report=[0-9a-f]{8} state=0004"),
            ("physical matrix press", r"8850_matrix_press: column=1 host_bit=02"),
            ("physical keypad IRQ", r"kbgpio: irq=1"),
            ("firmware keypad acknowledgement", r"kbgpio: ack latched=1"),
        ):
            match = re.search(pattern, text[cursor:])
            if match is None:
                errors.append(f"missing or out-of-order {name}")
            else:
                cursor += match.end()
    if display_transfer:
        # This establishes nonzero framebuffer delivery, not semantic UI input.
        glyph = re.search(r"8850_glyph_framebuffer_done pixels=80800000/007f7e0c mask=00000000/00000000", text)
        transfer = None if glyph is None else re.search(
            r"8850_lcd_framebuffer_transfer start=00 count=54 flags=00 pixels=80800000/007f7e0c mask=00000000/00000000",
            text[glyph.end():])
        if glyph is None or transfer is None:
            errors.append("missing or out-of-order nonzero glyph framebuffer transfer")
    if sim_reads:
        cursor = 0
        for name, pattern in (
            ("ICCID read", r"sim_device: read-binary fid=2fe2 offset=0 length=10"),
            ("service table read", r"sim_device: read-binary fid=6f38 offset=0 length=12"),
            ("IMSI read", r"sim_device: read-binary fid=6f07 offset=0 length=9"),
            ("last ADN record read", r"sim_device: header cla=a0 ins=b2 p1=32 p2=04 p3=20 selected=6f3a"),
            ("post-ADN card status", r"sim_device: header cla=a0 ins=f2 p1=00 p2=00 p3=16 selected=6f3a"),
        ):
            match = re.search(pattern, text[cursor:])
            if match is None:
                errors.append(f"missing or out-of-order {name}")
            else:
                cursor += match.end()
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--runtime-hle", action="store_true",
                        help="check native uploads followed by compact request-correlated HLE")
    parser.add_argument("--startup-readiness", action="store_true",
                        help="also require organic readiness and physical IRQ delivery, not a rendered UI")
    parser.add_argument("--display-transfer", action="store_true",
                        help="require nonzero glyph pixels followed by framebuffer transfer; not interactive acceptance")
    parser.add_argument("--sim-reads", action="store_true",
                        help="require the organic identity/service/ADN read conversation, not persistence or registration")
    parser.add_argument("--physical-navigation", type=Path, metavar="SNAPSHOT_DIR",
                        help="require physical security digits/softkeys and Messages/Inbox/Names frame crops")
    parser.add_argument("--calculator-frame", type=Path,
                        help="require correlated calculator inputs and the reviewed 12+3=15 result")
    args = parser.parse_args()
    try:
        text = args.log.read_text(encoding="utf-8", errors="replace")
        errors = check_trace(text,
                             args.runtime_hle, args.startup_readiness, args.display_transfer, args.sim_reads)
        if args.physical_navigation:
            errors.extend(check_physical_inputs(text))
            errors.extend(check_navigation_frames(args.physical_navigation))
        if args.calculator_frame:
            errors.extend(check_calculator(text, args.calculator_frame))
    except OSError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    for error in errors:
        print(f"FAIL: {error}", file=sys.stderr)
    if errors:
        return 1
    print("OK - 8850 native uploads" +
          (" and compact runtime self-test" if args.runtime_hle else " reach the missing-mask boundary") +
          ("; nonzero LCD framebuffer transfer verified" if args.display_transfer else "") +
          ("; calculator 12+3=15 verified" if args.calculator_frame else "") +
          ("; physical Messages/Inbox/Names navigation verified" if args.physical_navigation
           else "; interactive phone acceptance not proven"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
