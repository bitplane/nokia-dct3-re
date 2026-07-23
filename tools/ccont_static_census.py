#!/usr/bin/env python3
"""Census the firmware-visible CCONT descriptor surface across DCT3 ROMs.

The MCU images store instructions in swap16 order while byte tables are read
through the opposite byte lane.  This tool keeps those operations explicit and
does not assign semantics to registers merely because firmware can address
them.
"""

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FLASH_BASE = 0x200000
DESCRIPTOR_COUNT = 18
DESCRIPTOR_SIGNATURE = bytes.fromhex(
    "ff ff 10 20 18 ff ff ff ff 38 40 48 50 58 60 68 70 78"
)
DEFAULT_ROMS = (
    ("3210-v6.00", ROOT / "roms/noki3210/3210f600a.fls"),
    ("3210-v5.01", ROOT / "roms/noki3210/3210f501.fls"),
    ("3310-v6.39", ROOT / "roms/noki3310/3310f639e.fls"),
    ("3330-v4.50", ROOT / "roms/noki3330/3330f450e.fls"),
    ("3410-v5.46", ROOT / "roms/noki3410/3410f546e.fls"),
)


def swap16(data: bytes) -> bytes:
    result = bytearray(data)
    result[0::2], result[1::2] = data[1::2], data[0::2]
    return bytes(result)


def cpu_byte(image: bytes, offset: int) -> int:
    return image[offset ^ 1]


def logical_bytes(image: bytes) -> bytes:
    return bytes(cpu_byte(image, offset) for offset in range(len(image)))


def effective_u32(image: bytes, offset: int) -> int:
    raw = int.from_bytes(image[offset:offset + 4], "little")
    return ((raw << 16) | (raw >> 16)) & 0xffffffff


def locate_descriptor_table(image: bytes) -> int:
    logical = logical_bytes(image)
    hits = []
    cursor = 0
    while True:
        cursor = logical.find(DESCRIPTOR_SIGNATURE, cursor)
        if cursor < 0:
            break
        hits.append(FLASH_BASE + cursor)
        cursor += 1
    if len(hits) != 1:
        raise ValueError(f"expected one CCONT descriptor table, found {len(hits)}")
    return hits[0]


def table_entries(image: bytes, address: int) -> list[int]:
    offset = address - FLASH_BASE
    return [cpu_byte(image, offset + index) for index in range(DESCRIPTOR_COUNT)]


def table_defaults(image: bytes, address: int) -> list[int]:
    offset = address - FLASH_BASE + DESCRIPTOR_COUNT
    return [cpu_byte(image, offset + index) for index in range(DESCRIPTOR_COUNT)]


def table_literal_references(image: bytes, address: int) -> list[int]:
    return [
        FLASH_BASE + offset
        for offset in range(0, len(image) - 3, 4)
        if effective_u32(image, offset) == address
    ]


def physical_register(command: int):
    return None if command == 0xff else (command >> 3) & 0x0f


def analyze(label: str, path: Path) -> dict:
    image = swap16(path.read_bytes())
    table = locate_descriptor_table(image)
    entries = table_entries(image, table)
    defaults = table_defaults(image, table)
    return {
        "label": label,
        "rom": str(path.relative_to(ROOT) if path.is_relative_to(ROOT) else path),
        "image_bytes": len(image),
        "descriptor_table": f"0x{table:08x}",
        "literal_references": [f"0x{value:08x}" for value in table_literal_references(image, table)],
        "descriptors": [
            {
                "logical": index,
                "command": f"0x{command:02x}",
                "register": physical_register(command),
                "default": None if defaults[index] == 0xff else f"0x{defaults[index]:02x}",
            }
            for index, command in enumerate(entries)
        ],
        "mapped_registers": sorted({
            register for command in entries
            if (register := physical_register(command)) is not None
        }),
        "unmapped_logical_descriptors": [
            index for index, command in enumerate(entries) if command == 0xff
        ],
    }


def build_payload(roms) -> dict:
    reports = [analyze(label, path) for label, path in roms]
    common = reports[0]["descriptors"]
    return {
        "schema_version": 1,
        "method": "unique byte-lane-correct descriptor signature and literal-reference census",
        "coverage": {
            "roms_requested": len(roms),
            "roms_decoded": len(reports),
            "descriptor_entries_per_rom": DESCRIPTOR_COUNT,
            "total_descriptor_entries": len(reports) * DESCRIPTOR_COUNT,
        },
        "cross_rom_descriptor_identity": all(report["descriptors"] == common for report in reports[1:]),
        "special_direct_paths": {
            "register_0": "ADC request helper, outside descriptor table",
            "register_5": "watchdog/power helper, outside descriptor table",
            "register_1": "outside descriptor table; direct boot writes observed",
            "register_6": "outside descriptor table; direct boot writes observed",
        },
        "roms": reports,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rom", action="append", nargs=2, metavar=("LABEL", "PATH"))
    parser.add_argument("--json", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    roms = [(label, Path(path)) for label, path in args.rom] if args.rom else list(DEFAULT_ROMS)
    payload = build_payload(roms)
    if args.check:
        if payload["coverage"]["roms_decoded"] != 5:
            raise SystemExit("checked census requires all five supported ROM controls")
        if not payload["cross_rom_descriptor_identity"]:
            raise SystemExit("CCONT descriptor tables differ across supported ROMs")
        expected = [2, 3, 4, 7, 8, 9, 10, 11, 12, 13, 14, 15]
        for report in payload["roms"]:
            if report["mapped_registers"] != expected:
                raise SystemExit(f"{report['label']} mapped register surface changed")
            if len(report["literal_references"]) != 2:
                raise SystemExit(f"{report['label']} descriptor reference coverage changed")
    output = json.dumps(payload, indent=2) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(output)
    else:
        print(output, end="")


if __name__ == "__main__":
    main()
