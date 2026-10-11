#!/usr/bin/env python3
"""Compare archival word-array uploads with explicitly supplied images."""

import argparse
import hashlib
import json
from pathlib import Path
import re

from dsp_upload_extract import encode_words


def read_reference(path: Path) -> tuple[list[int], list[int]]:
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
    return header, words


def read_payload(path: Path) -> list[int]:
    return read_reference(path)[1]


def descriptor_match(header: list[int], image: bytes, hit: dict) -> dict | None:
    offset = hit["file_offset"] - 12
    if offset < 0:
        return None
    observed = [int.from_bytes(image[index:index + 2], hit["byteorder"])
                for index in range(offset, offset + 12, 2)]
    return {"file_offset": offset, "words": observed,
            "archival_header_equal": observed[:5] == header}


def verify_catalogue(payloads, image: bytes, offset: int, base: int) -> list[dict]:
    entries = []
    for source, header, words in payloads:
        index = int(source.stem[5:], 16)
        slot = offset + index * 4
        if slot < 0 or slot + 4 > len(image):
            raise ValueError("catalogue pointer outside image")
        address = int.from_bytes(image[slot:slot + 4], "big")
        start = address - base
        end = start + 12 + len(words) * 2
        if start < 0 or end > len(image):
            raise ValueError(f"{source.stem}: catalogue descriptor outside image")
        expected = encode_words(header + [0] + words, "big")
        if image[start:end] != expected:
            raise ValueError(f"{source.stem}: catalogue descriptor/payload mismatch")
        entries.append({"block": source.stem, "pointer_file_offset": slot,
                        "descriptor_address": address, "descriptor_file_offset": start})
    return entries


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
    parser.add_argument("--catalogue-offset", type=lambda value: int(value, 0))
    parser.add_argument("--image-base", type=lambda value: int(value, 0))
    args = parser.parse_args()
    if (args.catalogue_offset is None) != (args.image_base is None):
        parser.error("catalogue verification requires both --catalogue-offset and --image-base")
    try:
        sources = sorted(args.reference_dir.glob("block*.h"))
        if not sources:
            raise ValueError("no block headers found")
        payloads = [(path, *read_reference(path)) for path in sources]
        report = {"headers": len(payloads), "nonempty": sum(bool(w) for _, _, w in payloads),
                  "offset_namespace": "file bytes, not MCU/DSP addresses", "images": []}
        for path in args.images:
            image = path.read_bytes()
            entries = []
            for source, header, words in payloads:
                hits = matches(words, image)
                if hits:
                    for hit in hits:
                        hit["preceding_descriptor_candidate"] = descriptor_match(header, image, hit)
                    entries.append({"block": source.stem, "words": len(words),
                                    "archival_header": header,
                                    "payload_sha256_be": hashlib.sha256(encode_words(words, "big")).hexdigest(),
                                    "matches": hits})
            report["images"].append({"path": str(path), "sha256": hashlib.sha256(image).hexdigest(),
                                     "size": len(image), "blocks": entries})
            if args.catalogue_offset is not None:
                report["images"][-1]["verified_catalogue"] = verify_catalogue(
                    payloads, image, args.catalogue_offset, args.image_base)
    except (OSError, ValueError) as error:
        parser.exit(1, f"error: {error}\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
