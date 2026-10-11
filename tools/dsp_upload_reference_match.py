#!/usr/bin/env python3
"""Compare archival word-array uploads with explicitly supplied images."""

import argparse
import hashlib
import json
from pathlib import Path
import re

from dsp_upload_extract import encode_words


def read_payload(path: Path) -> list[int]:
    text = path.read_text()
    declaration = re.search(
        r"static\s+struct\s+dsp_codeblock\s+\w+\s*=\s*\{"
        r"\s*\{([^{}]*)\}\s*,\s*\{([^{}]*)\}\s*\};", text
    )
    if declaration is None:
        raise ValueError(f"{path}: unsupported upload declaration")
    arrays = []
    for body in declaration.groups():
        body = re.sub(r"/\*.*?\*/|//[^\n]*", "", body, flags=re.S)
        tokens = [token.strip() for token in body.split(",") if token.strip()]
        if any(not re.fullmatch(r"0x[0-9a-fA-F]{1,4}", token) for token in tokens):
            raise ValueError(f"{path}: unsupported word literal")
        arrays.append([int(token, 16) for token in tokens])
    header, words = arrays
    if len(header) != 5 or len(words) != header[2]:
        raise ValueError(f"{path}: descriptor/payload size mismatch")
    return words


def matches(words: list[int], image: bytes) -> list[dict]:
    if not words:
        return []
    result = []
    for order in ("big", "little"):
        needle = encode_words(words, order)
        offset = image.find(needle)
        while offset >= 0:
            result.append({"byteorder": order, "file_offset": offset})
            offset = image.find(needle, offset + 1)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference_dir", type=Path)
    parser.add_argument("images", type=Path, nargs="+")
    args = parser.parse_args()
    try:
        sources = sorted(args.reference_dir.glob("block*.h"))
        if not sources:
            raise ValueError("no block headers found")
        payloads = [(path, read_payload(path)) for path in sources]
        report = {"headers": len(payloads), "nonempty": sum(bool(w) for _, w in payloads),
                  "offset_namespace": "file bytes, not MCU/DSP addresses", "images": []}
        for path in args.images:
            image = path.read_bytes()
            entries = []
            for source, words in payloads:
                hits = matches(words, image)
                if hits:
                    entries.append({"block": source.stem, "words": len(words),
                                    "payload_sha256_be": hashlib.sha256(encode_words(words, "big")).hexdigest(),
                                    "matches": hits})
            report["images"].append({"path": str(path), "sha256": hashlib.sha256(image).hexdigest(),
                                     "size": len(image), "blocks": entries})
    except (OSError, ValueError) as error:
        parser.exit(1, f"error: {error}\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
