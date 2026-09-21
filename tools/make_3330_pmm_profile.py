#!/usr/bin/env python3
"""Build the evidenced NHM-6 v4.50 settled-security PMM fixture."""

import argparse
import hashlib
from pathlib import Path


FLASH_SIZE = 0x400000
PMM_OFFSET = 0x3F0000
PMM_SIZE = 0x10000
SECURITY_VERIFIER_OFFSET = 0x069A
VIRGIN_SECURITY_VERIFIER = bytes.fromhex("d43537dc")
SETTLED_SECURITY_VERIFIER = bytes.fromhex("d83b30d8")
VIRGIN_PMM_SHA256 = "a9f478e03d8e281a52e4b7b70f0c7084e48bec2c8da8f568eb03d71ed7a99fcf"


def make_profile(base: bytes, pmm: bytes, expected_pmm_sha256: str | None = None) -> bytes:
    if len(base) != 0x350000:
        raise ValueError("expected a 0x350000-byte NHM-6 MCU/PPM image")
    if len(pmm) != PMM_SIZE:
        raise ValueError(f"expected a {PMM_SIZE:#x}-byte NHM-6 PMM image")
    if expected_pmm_sha256 is not None:
        digest = hashlib.sha256(pmm).hexdigest()
        if digest != expected_pmm_sha256:
            raise ValueError(
                f"NHM-6 PMM SHA-256 mismatch: expected {expected_pmm_sha256}, got {digest}")
    actual = pmm[
        SECURITY_VERIFIER_OFFSET:SECURITY_VERIFIER_OFFSET + len(VIRGIN_SECURITY_VERIFIER)]
    if actual != VIRGIN_SECURITY_VERIFIER:
        raise ValueError(
            "unexpected virgin security verifier at PMM offset "
            f"{SECURITY_VERIFIER_OFFSET:#x}: {actual.hex()}")

    result = bytearray(b"\xff" * FLASH_SIZE)
    result[:len(base)] = base
    result[PMM_OFFSET:PMM_OFFSET + PMM_SIZE] = pmm
    start = PMM_OFFSET + SECURITY_VERIFIER_OFFSET
    result[start:start + len(SETTLED_SECURITY_VERIFIER)] = SETTLED_SECURITY_VERIFIER
    return bytes(result)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("base", type=Path)
    parser.add_argument("pmm", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    profile = make_profile(
        args.base.read_bytes(), args.pmm.read_bytes(), VIRGIN_PMM_SHA256)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(profile)
    print(f"wrote {args.output} ({len(profile)} bytes)")


if __name__ == "__main__":
    main()
