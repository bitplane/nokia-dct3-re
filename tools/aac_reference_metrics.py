"""Independent per-channel RMS/LSB metrics; no alignment or tolerance fitting."""

import math
import struct
import wave
from itertools import chain
from pathlib import Path


def read_reference(path):
    """Read the original signed 24-bit PCM reference without requantizing."""
    with wave.open(str(Path(path)), 'rb') as source:
        if source.getsampwidth() != 3 or source.getcomptype() != 'NONE':
            raise ValueError('reference must be uncompressed 24-bit PCM')
        channels, rate, frames = source.getnchannels(), source.getframerate(), source.getnframes()
        raw = source.readframes(frames)
    if len(raw) != frames * channels * 3:
        raise ValueError('truncated reference PCM')
    samples = [int.from_bytes(raw[i:i+3], 'little', signed=True) / (1 << 23)
               for i in range(0, len(raw), 3)]
    return samples, channels, rate


def read_native_s32(path, fractional_bits):
    """Explicit fixed-point scaling only; caller must establish its provenance."""
    if not isinstance(fractional_bits, int) or not 1 <= fractional_bits <= 31:
        raise ValueError('fractional bits must be explicitly established in 1..31')
    raw = Path(path).read_bytes()
    if len(raw) % 4:
        raise ValueError('truncated native signed-32 PCM')
    return [value[0] / (1 << fractional_bits) for value in struct.iter_unpack('<i', raw)]


def compare(native, reference, channels, accuracy_bits=16):
    """Evaluate supplied intervals exactly; this alone makes no conformance claim."""
    if not isinstance(channels, int) or channels < 1:
        raise ValueError('positive channel count required')
    if not isinstance(accuracy_bits, int) or not 2 <= accuracy_bits <= 32:
        raise ValueError('invalid accuracy bits')
    if not native or len(native) != len(reference) or len(native) % channels:
        raise ValueError('nonempty, equal, complete interleaved intervals required; no automatic trimming')
    if any(not math.isfinite(x) or not -1 <= x <= 1 for x in chain(native, reference)):
        raise ValueError('samples must be finite and normalized to [-1, 1]')
    rms_limit = 2 ** (-(accuracy_bits - 1)) / math.sqrt(12)
    max_limit = 2 ** (-(accuracy_bits - 2))
    rows = []
    for channel in range(channels):
        errors = [a-b for a, b in zip(native[channel::channels], reference[channel::channels])]
        rms = math.sqrt(math.fsum(error * error for error in errors) / len(errors))
        maximum = max(map(abs, errors))
        rows.append(dict(channel=channel, samples=len(errors), rms=rms, maximum=maximum,
                         rms_limit=rms_limit, maximum_limit=max_limit,
                         criterion_met=rms < rms_limit and maximum <= max_limit))
    return dict(accuracy_bits=accuracy_bits, channels=rows,
                scope='Interval metrics only; applicability, alignment and measurement-point ownership are caller obligations')
