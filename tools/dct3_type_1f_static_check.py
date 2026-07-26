#!/usr/bin/env python3
"""Compare the independently constructed DCT3 type-0x1f task-3 profile."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import capstone

try:
    from tools.message_census import decode_image
except ModuleNotFoundError:
    from message_census import decode_image


FLASH_BASE = 0x200000
EXPECTED = {
    "nse8_3210_v600": {
        "sha256": "7bf29b96e544b682c4d6d01c7a6eaef89909c4191a52d829115d37b31c0c0d8a",
        "constructors": [
            [0x2178DC, 0x2178DE],
            [0x21937C, 0x21937E],
            [0x2197BA, 0x2197BC],
            [0x2848AE, 0x2848B0],
        ],
        "task_post_api": 0x26A204,
        "task_post_calls": [0x21792A, 0x219398, 0x219844, 0x2848BE],
        "forms": [
            {"site": 0x2178DC, "flags": [4], "value": "table_derived"},
            {"site": 0x21937C, "flags": [1], "value": 0},
            {"site": 0x2197BA, "flags": [0x0C, 0x0D], "value": "table_derived"},
            {"site": 0x2848AE, "flags": [1, 6, 7], "value": "derived_or_zero"},
        ],
        "anchors": {
            0x2178CE: ("movs", "r0, #2"),
            0x2178D2: ("strh", "r0, [r4]"),
            0x2178D6: ("strb", "r6, [r4, #4]"),
            0x2178D8: ("movs", "r0, #4"),
            0x2178DC: ("movs", "r0, #0x1f"),
            0x217924: ("strb", "r0, [r4, #5]"),
            0x217926: ("movs", "r0, #3"),
            0x21792A: ("bl", "#0x26a204"),
            0x219372: ("movs", "r0, #2"),
            0x219374: ("strh", "r0, [r1]"),
            0x219376: ("strb", "r5, [r1, #4]"),
            0x219378: ("movs", "r0, #4"),
            0x21937C: ("movs", "r0, #0x1f"),
            0x219394: ("strb", "r0, [r1, #5]"),
            0x219396: ("movs", "r0, #3"),
            0x219398: ("bl", "#0x26a204"),
            0x2197AC: ("movs", "r0, #2"),
            0x2197B0: ("strh", "r0, [r4]"),
            0x2197B4: ("strb", "r7, [r4, #4]"),
            0x2197B6: ("movs", "r0, #4"),
            0x2197BA: ("movs", "r0, #0x1f"),
            0x21983E: ("strb", "r0, [r4, #5]"),
            0x219840: ("movs", "r0, #3"),
            0x219844: ("bl", "#0x26a204"),
            0x2848A6: ("movs", "r0, #2"),
            0x2848A8: ("strh", "r0, [r4]"),
            0x2848AA: ("movs", "r0, #4"),
            0x2848AE: ("movs", "r0, #0x1f"),
            0x2848BA: ("movs", "r0, #3"),
            0x2848BE: ("bl", "#0x26a204"),
            0x216080: ("push", "{r4, r5, lr}"),
            0x21608E: ("movs", "r1, #0"),
            0x216092: ("bl", "#0x2b65e4"),
        },
    },
    "nhm5_3310_v639": {
        "sha256": "975ec791205f026d647254ee772d7fa32691fa50c72a68eecdaff7c8a5921442",
        "constructors": [[0x2A837C, 0x2A837E]],
        "task_post_api": 0x299E4C,
        "task_post_calls": [0x2A83E2],
        "forms": [
            {
                "site": 0x2A837C,
                "flags": "bits_0_to_3_from_four_runtime_inputs",
                "value": "optional_input_byte_0",
            },
        ],
        "anchors": {
            0x2A8364: ("movs", "r0, #8"),
            0x2A836C: ("movs", "r1, #0"),
            0x2A836E: ("movs", "r2, #8"),
            0x2A8370: ("bl", "#0x2f1c8c"),
            0x2A8374: ("movs", "r0, #4"),
            0x2A8378: ("movs", "r0, #2"),
            0x2A837A: ("strh", "r0, [r4]"),
            0x2A837C: ("movs", "r0, #0x1f"),
            0x2A837E: ("strb", "r0, [r4, #3]"),
            0x2A83A6: ("strb", "r1, [r4, #5]"),
            0x2A83C2: ("strb", "r0, [r4, #6]"),
            0x2A83DE: ("movs", "r0, #3"),
            0x2A83E2: ("bl", "#0x299e4c"),
        },
    },
    "nse3_6110_v406": {
        "sha256": "aace812405bca224689ae707ea1a6174dbcf413bf62c88a944d96d298880ba60",
        "constructors": [
            [0x20D7B6, 0x20D7B8],
            [0x20F0F8, 0x20F0FA],
            [0x20F168, 0x20F16A],
            [0x20F460, 0x20F462],
            [0x27FEBC, 0x27FEBE],
        ],
        "task_post_api": 0x25FB4C,
        "task_post_calls": [
            0x20D7E4, 0x20F114, 0x20F196, 0x20F4C2, 0x27FECC,
        ],
        "forms": [
            {"site": 0x20D7B6, "flags": [4], "value": "table_derived"},
            {"site": 0x20F0F8, "flags": [1], "value": 0},
            {"site": 0x20F168, "flags": [0x0D], "value": "state_derived"},
            {"site": 0x20F460, "flags": [0x0C, 0x0D], "value": "table_derived"},
            {"site": 0x27FEBC, "flags": [1, 6, 7], "value": "derived_or_zero"},
        ],
        "anchors": {
            0x20D7A8: ("strh", "r7, [r1]"),
            0x20D7B0: ("strb", "r0, [r1, #4]"),
            0x20D7B2: ("movs", "r2, #4"),
            0x20D7B6: ("movs", "r2, #0x1f"),
            0x20D7E0: ("strb", "r0, [r1, #5]"),
            0x20D7E2: ("movs", "r0, #3"),
            0x20D7E4: ("bl", "#0x25fb4c"),
            0x20F0EC: ("movs", "r0, #2"),
            0x20F0EE: ("strh", "r0, [r1]"),
            0x20F0F2: ("strb", "r0, [r1, #4]"),
            0x20F0F4: ("movs", "r2, #4"),
            0x20F0F8: ("movs", "r2, #0x1f"),
            0x20F110: ("strb", "r0, [r1, #5]"),
            0x20F112: ("movs", "r0, #3"),
            0x20F114: ("bl", "#0x25fb4c"),
            0x20F158: ("movs", "r7, #2"),
            0x20F15A: ("strh", "r7, [r5]"),
            0x20F162: ("strb", "r0, [r5, #4]"),
            0x20F164: ("movs", "r1, #4"),
            0x20F168: ("movs", "r1, #0x1f"),
            0x20F190: ("strb", "r0, [r5, #5]"),
            0x20F192: ("movs", "r0, #3"),
            0x20F196: ("bl", "#0x25fb4c"),
            0x20F454: ("movs", "r0, #2"),
            0x20F458: ("strh", "r0, [r7]"),
            0x20F45A: ("strb", "r4, [r7, #4]"),
            0x20F45C: ("movs", "r0, #4"),
            0x20F460: ("movs", "r0, #0x1f"),
            0x20F4BC: ("strb", "r0, [r7, #5]"),
            0x20F4BE: ("movs", "r0, #3"),
            0x20F4C2: ("bl", "#0x25fb4c"),
            0x27FEB4: ("movs", "r0, #2"),
            0x27FEB6: ("strh", "r0, [r4]"),
            0x27FEB8: ("movs", "r0, #4"),
            0x27FEBC: ("movs", "r0, #0x1f"),
            0x27FEC8: ("movs", "r0, #3"),
            0x27FECC: ("bl", "#0x25fb4c"),
            0x20C798: ("push", "{r4, r5, lr}"),
            0x20C7A6: ("movs", "r1, #0"),
            0x20C7AA: ("bl", "#0x2a4f44"),
        },
    },
}


def swap16(data: bytes) -> bytes:
    if len(data) % 2:
        raise ValueError("swap16 input must contain an even number of bytes")
    result = bytearray(data)
    result[0::2], result[1::2] = data[1::2], data[0::2]
    return bytes(result)


def direct_type_0x1f_constructors(instructions) -> list[list[int]]:
    result = []
    for index, insn in enumerate(instructions[:-1]):
        next_insn = instructions[index + 1]
        if (
            not insn
            or not next_insn
            or insn.mnemonic != "movs"
            or not insn.op_str.endswith(", #0x1f")
            or next_insn.mnemonic != "strb"
        ):
            continue
        source_register = insn.op_str.split(",", 1)[0]
        if (
            next_insn.op_str.startswith(f"{source_register}, [")
            and "#3]" in next_insn.op_str
        ):
            result.append([insn.address, next_insn.address])
    return result


def verify_anchors(physical: bytes, anchors: dict[int, tuple[str, str]]) -> None:
    decoder = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
    for address, expected in anchors.items():
        offset = address - FLASH_BASE
        decoded = list(
            decoder.disasm(physical[offset : offset + 4], address, count=1)
        )
        actual = (decoded[0].mnemonic, decoded[0].op_str) if decoded else None
        if actual != expected:
            raise ValueError(
                f"anchor {address:#x}: expected {expected}, got {actual}"
            )


def verify_model(name: str, data: bytes) -> dict:
    expected = EXPECTED[name]
    digest = hashlib.sha256(data).hexdigest()
    if digest != expected["sha256"]:
        raise ValueError(
            f"{name}: expected SHA-256 {expected['sha256']}, got {digest}"
        )
    physical = swap16(data)
    instructions = decode_image(physical, FLASH_BASE)
    constructors = direct_type_0x1f_constructors(instructions)
    if constructors != expected["constructors"]:
        raise ValueError(
            f"{name}: expected constructors {expected['constructors']}, "
            f"got {constructors}"
        )
    verify_anchors(physical, expected["anchors"])
    return {
        "sha256": digest,
        "direct_constructor_sites": constructors,
        "direct_constructor_count": len(constructors),
        "queue_object": {
            "status": 2,
            "declared_payload_bytes": 4,
            "destination_task": 3,
            "task_post_api": expected["task_post_api"],
            "task_post_calls": expected["task_post_calls"],
        },
        "wire_profile": {
            "bytes": [0x1F, 0, "product_policy_flags", "product_policy_value"],
            "local_metadata_offset": 7,
            "local_metadata_is_outside_declared_payload": True,
        },
        "forms": expected["forms"],
    }


def verify(images: dict[str, bytes]) -> dict:
    models = {
        name: verify_model(name, images[name])
        for name in EXPECTED
    }
    return {
        "models": models,
        "shared_contract": {
            "evidenced_models": list(EXPECTED),
            "queue_status": 2,
            "declared_payload_bytes": 4,
            "destination_task": 3,
            "common_wire_prefix": [0x1F, 0],
            "flags_and_value_policy": "product_specific",
            "local_metadata_offset": 7,
            "local_metadata_transmitted": False,
            "dsp_side_semantics": "not_established_by_static_comparison",
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--3210", dest="nse8_3210_v600", type=Path, required=True)
    parser.add_argument("--3310", dest="nhm5_3310_v639", type=Path, required=True)
    parser.add_argument("--6110", dest="nse3_6110_v406", type=Path, required=True)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    result = verify(
        {
            name: getattr(args, name).read_bytes()
            for name in EXPECTED
        }
    )
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(result, indent=2) + "\n")
    counts = ", ".join(
        f"{name}={model['direct_constructor_count']}"
        for name, model in result["models"].items()
    )
    print(f"verified DCT3 type-0x1f shared boundary: {counts}")


if __name__ == "__main__":
    main()
