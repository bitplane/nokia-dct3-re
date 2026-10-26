"""Declared machines and explicitly tracked pre-machine research products."""

from functools import lru_cache
from pathlib import Path
import argparse
import re


DRIVER = Path(__file__).resolve().parents[1] / "driver/nokia_dct3.cpp"
SYSTEM_RE = re.compile(r"^SYST\(\s*\d+\s*,\s*noki(\d+[a-z]*)\s*,", re.MULTILINE)

# Identity for tooling classification only, not a claim of runtime support.
RESEARCH_ONLY_PRODUCTS = frozenset({'5510', '6150'})


def composition_source() -> str:
    """Board source for convention checks, not preprocessed or executable code.

    The main file still owns the single ROM/system catalogue. Configuration
    and state declarations can live in sibling modules without disappearing
    from checks that previously inspected that whole file.
    """
    return '\n'.join(path.read_text() for path in sorted(DRIVER.parent.glob('nokia_dct3*'))
                     if path.suffix in ('.cpp', '.h'))


@lru_cache(maxsize=1)
def product_ids() -> frozenset[str]:
    return frozenset(SYSTEM_RE.findall(DRIVER.read_text())) | RESEARCH_ONLY_PRODUCTS


def nvram_system(machine: str, bios: str = "", *, source: str | None = None) -> str:
    """Resolve MAME's basename[_bios_index] from this driver's declarations.

    ROM_SYSTEM_BIOS indices are zero-based; MAME omits the suffix for the
    default BIOS. Research aliases retain their own machine basename. Reject
    unknown inputs rather than cleaning a guessed storage directory.
    """
    if source is None:
        source = DRIVER.read_text()
    # Preserve quoted strings while ignoring commented-out declarations.
    source = re.sub(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*.*?\*/',
                    lambda m: m[0] if m[0].startswith('"') else " ", source,
                    flags=re.S)
    systems = set(re.findall(r"^SYST\(\s*\d+\s*,\s*(\w+)\s*,", source, re.M))
    if machine not in systems:
        raise ValueError(f"unknown DCT3 machine: {machine}")
    # A config-time BIOS override or included ROM table needs explicit support
    # before this resolver can safely determine its default/storage namespace.
    if re.search(r"\bset_default_bios_tag\s*\(|\bROM_INCLUDE\s*\(", source):
        raise ValueError("unsupported indirect BIOS declaration")
    aliases = dict(re.findall(r"^#define\s+rom_(\w+)\s+rom_(\w+)\s*$", source, re.M))
    rom = machine
    seen = set()
    while rom in aliases:
        if rom in seen:
            raise ValueError(f"cyclic ROM alias: {machine}")
        seen.add(rom)
        rom = aliases[rom]
    match = re.search(r"ROM_START\(\s*" + re.escape(rom) + r"\s*\)(.*?)\bROM_END\b",
                      source, re.S)
    if match is None:
        raise ValueError(f"missing ROM declaration: {rom}")
    body = match[1]
    entries = [(int(index, 16 if index.startswith("0x") else 10), name)
               for index, name in re.findall(
                   r'ROM_SYSTEM_BIOS\(\s*(0x[0-9a-fA-F]+|\d+)\s*,\s*"([^"\n]+)"', body)]
    if len(entries) != len(re.findall(r"\bROM_SYSTEM_BIOS\s*\(", body)):
        raise ValueError(f"unsupported BIOS index expression: {rom}")
    if len({i for i, _ in entries}) != len(entries) or len({n for _, n in entries}) != len(entries):
        raise ValueError(f"duplicate BIOS declaration: {rom}")
    requested = bios.lower()
    if not entries:
        if requested not in ("", "default"):
            raise ValueError(f"{machine} has no selectable BIOS")
        return machine
    default = entries[0][0]
    defaults = re.findall(r'ROM_DEFAULT_BIOS\(\s*"([^"\n]+)"\s*\)', body)
    if defaults:
        found = [i for i, name in entries if name == defaults[0]]
        if len(defaults) != 1 or not found:
            raise ValueError(f"invalid default BIOS: {rom}")
        default = found[0]
    selected = default
    if requested not in ("", "default"):
        found = [i for i, name in entries if requested in (str(i), name.lower())]
        if not found:
            raise ValueError(f"unknown BIOS {bios!r} for {machine}")
        selected = found[0]
    return machine if selected == default else f"{machine}_{selected}"


def main() -> None:
    parser = argparse.ArgumentParser(description="Resolve a DCT3 NVRAM namespace")
    parser.add_argument("--nvram-system", required=True)
    parser.add_argument("--bios", default="")
    args = parser.parse_args()
    try:
        print(nvram_system(args.nvram_system, args.bios))
    except ValueError as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
