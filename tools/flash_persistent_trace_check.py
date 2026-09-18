#!/usr/bin/env python3
"""Check direct persistent-flash bus activity in a bounded NAM-1 boot."""

import sys
from pathlib import Path


def check(text: str) -> None:
	if "external_service: response command=64 result=01 sequence=42" not in text:
		raise ValueError("NAM-1 application frontier was not reached")
	if "flash_persistent_read:" in text:
		raise ValueError("firmware read the product-state partition")
	if "flash_persistent_write:" in text:
		raise ValueError("firmware wrote the product-state partition")


def main() -> None:
	if len(sys.argv) != 2:
		raise SystemExit(f"usage: {sys.argv[0]} LOG")
	try:
		check(Path(sys.argv[1]).read_text(errors="replace"))
	except ValueError as error:
		raise SystemExit(f"persistent-flash boundary failed: {error}") from error
	print("persistent-flash bus: application reached; no direct product-state access")


if __name__ == "__main__":
	main()
