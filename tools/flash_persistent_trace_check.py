#!/usr/bin/env python3
"""Check that a bounded boot reaches its frontier before persistent flash."""

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
	print("persistent-flash boundary: application reached; no product-state access")


if __name__ == "__main__":
	main()
