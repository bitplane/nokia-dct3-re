#!/usr/bin/env python3
"""Verify decoded handset audio reached the isolated playback endpoint."""

import argparse
from array import array
import math
from pathlib import Path
import sys
import wave


SAMPLE_RATE = 8_000
WINDOW_SAMPLES = 160
MIN_CONSECUTIVE_TONE_WINDOWS = 25


def check(
    path: Path,
    allow_transient_clipping: bool = False,
    require_tone: bool = True,
) -> str:
    total_samples = 0
    nonzero_samples = 0
    peak = 0
    square_sum = 0
    consecutive_tone_windows = 0
    longest_tone_run = 0
    consecutive_non_silent_windows = 0
    longest_non_silent_run = 0

    try:
        source = wave.open(str(path), "rb")
    except (FileNotFoundError, wave.Error) as exc:
        raise ValueError(f"cannot read downlink capture: {exc}") from exc

    with source:
        if (
            source.getnchannels() != 1
            or source.getsampwidth() != 2
            or source.getframerate() != SAMPLE_RATE
        ):
            raise ValueError(
                "downlink capture is not mono 16-bit PCM at 8 kHz"
            )
        while True:
            encoded = source.readframes(WINDOW_SAMPLES)
            if not encoded:
                break
            samples = array("h")
            samples.frombytes(encoded)
            if sys.byteorder != "little":
                samples.byteswap()
            if len(samples) != WINDOW_SAMPLES:
                break

            total_samples += len(samples)
            window_square_sum = sum(sample * sample for sample in samples)
            square_sum += window_square_sum
            nonzero_samples += sum(sample != 0 for sample in samples)
            peak = max(peak, max(abs(sample) for sample in samples))

            # A 1 kHz service signal completes exactly four cycles per 20 ms
            # GSM speech block at 8 kHz. Measure its normalized projection
            # rather than accepting arbitrary ringing or key-click energy.
            cosine_sum = 0.0
            sine_sum = 0.0
            mean = sum(samples) / len(samples)
            for index, sample in enumerate(samples):
                phase = 2.0 * math.pi * 1_000 * index / SAMPLE_RATE
                centered = sample - mean
                cosine_sum += centered * math.cos(phase)
                sine_sum += centered * math.sin(phase)
            tone_rms = math.sqrt(2.0) * math.hypot(
                cosine_sum, sine_sum
            ) / len(samples)
            window_rms = math.sqrt(window_square_sum / len(samples))
            if window_rms > 32:
                consecutive_non_silent_windows += 1
                longest_non_silent_run = max(
                    longest_non_silent_run,
                    consecutive_non_silent_windows,
                )
            else:
                consecutive_non_silent_windows = 0
            if window_rms > 32 and tone_rms >= 0.35 * window_rms:
                consecutive_tone_windows += 1
                longest_tone_run = max(
                    longest_tone_run, consecutive_tone_windows
                )
            else:
                consecutive_tone_windows = 0

    if total_samples < SAMPLE_RATE:
        raise ValueError("downlink playback capture is shorter than one second")
    if nonzero_samples < SAMPLE_RATE // 2 or peak <= 64:
        raise ValueError("handset playback endpoint remained silent")
    if peak >= 32767 and not allow_transient_clipping:
        raise ValueError("handset playback endpoint clipped")
    if require_tone and longest_tone_run < MIN_CONSECUTIVE_TONE_WINDOWS:
        raise ValueError(
            "decoded 1 kHz downlink was not sustained at the playback endpoint"
        )
    if (
        not require_tone
        and longest_non_silent_run < MIN_CONSECUTIVE_TONE_WINDOWS
    ):
        raise ValueError(
            "decoded host media was not sustained at the playback endpoint"
        )

    rms = math.sqrt(square_sum / total_samples)
    evidence = (
        f"1 kHz run={longest_tone_run * 20} ms"
        if require_tone
        else f"non-silent run={longest_non_silent_run * 20} ms"
    )
    return (
        f"OK - physical downlink peak/rms={peak}/{rms:.1f}; "
        f"{evidence}"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("--allow-transient-clipping", action="store_true")
    parser.add_argument(
        "--non-silent", action="store_true",
        help="require sustained non-silent media instead of a pure 1 kHz tone",
    )
    args = parser.parse_args()
    try:
        result = check(
            args.capture,
            args.allow_transient_clipping,
            require_tone=not args.non_silent,
        )
    except ValueError as exc:
        print(exc, file=sys.stderr)
        return 1
    print(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
