#!/usr/bin/env python3
"""Check stable pixels in a binary PGM LCD capture."""

import argparse
import hashlib
from pathlib import Path


def read_pgm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    position = 0

    def token() -> bytes:
        nonlocal position
        while position < len(data):
            if data[position] == ord("#"):
                position = data.find(b"\n", position)
                if position < 0:
                    raise ValueError("unterminated PGM comment")
            elif chr(data[position]).isspace():
                position += 1
            else:
                break
        start = position
        while position < len(data) and not chr(data[position]).isspace():
            position += 1
        return data[start:position]

    if token() != b"P5":
        raise ValueError("expected binary PGM (P5)")
    width, height, maximum = (int(token()) for _ in range(3))
    if maximum != 255:
        raise ValueError(f"unsupported PGM maximum {maximum}")
    if position >= len(data) or not chr(data[position]).isspace():
        raise ValueError("missing PGM data separator")
    position += 1
    pixels = data[position:]
    if len(pixels) != width * height:
        raise ValueError(f"expected {width * height} pixels, found {len(pixels)}")
    return width, height, pixels


def stable_digest(width: int, height: int, pixels: bytes, mask: tuple[int, int, int, int]) -> str:
    x, y, mask_width, mask_height = mask
    if x < 0 or y < 0 or x + mask_width > width or y + mask_height > height:
        raise ValueError("mask lies outside the frame")
    stable = bytearray()
    for row in range(height):
        for column in range(width):
            if not (x <= column < x + mask_width and y <= row < y + mask_height):
                stable.append(pixels[row * width + column])
    return hashlib.sha256(stable).hexdigest()


def crop_digest(width: int, height: int, pixels: bytes, crop: tuple[int, int, int, int]) -> str:
    x, y, crop_width, crop_height = crop
    if crop_width <= 0 or crop_height <= 0:
        raise ValueError("crop must have positive dimensions")
    if x < 0 or y < 0 or x + crop_width > width or y + crop_height > height:
        raise ValueError("crop lies outside the frame")
    selected = bytearray()
    for row in range(y, y + crop_height):
        selected.extend(pixels[row * width + x:row * width + x + crop_width])
    return hashlib.sha256(selected).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("frame", type=Path)
    parser.add_argument("--mask", default="0,0,0,0", help="ignored x,y,width,height rectangle")
    parser.add_argument("--crop", help="hash only this x,y,width,height rectangle")
    parser.add_argument("--sha256", required=True)
    args = parser.parse_args()
    mask = tuple(int(value, 0) for value in args.mask.split(","))
    if len(mask) != 4:
        parser.error("--mask requires x,y,width,height")
    width, height, pixels = read_pgm(args.frame)
    crop = tuple(int(value, 0) for value in args.crop.split(",")) if args.crop else None
    if crop is not None and len(crop) != 4:
        parser.error("--crop requires x,y,width,height")
    digest = crop_digest(width, height, pixels, crop) if crop else stable_digest(width, height, pixels, mask)
    print(f"frame       : {args.frame}")
    print(f"stable sha256: {digest}")
    print(f"oracle       : {args.sha256}")
    if digest != args.sha256:
        print("MISMATCH - stable LCD regions diverged")
        return 1
    print("OK - stable LCD regions reproduced")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
