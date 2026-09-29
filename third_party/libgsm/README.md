# libgsm 1.0.24

Unmodified codec library sources from
[Jutta Degener and Carsten Bormann's GSM 06.10 implementation](https://www.quut.com/gsm/).
The command-line `toast` program and upstream test utilities are excluded.
The complete library source set and its five required headers are retained.

Source archive: `https://www.quut.com/gsm/gsm-1.0.24.tar.gz`

SHA-256: `a3c40c6471928383f4abfcb2e8f24012a1f562be2f17b8d672145d5986681a92`

The original permissive license and authorship notices are in `COPYRIGHT` and
each source file. Keep those notices with any redistributed source subset.

MAME's `gsm` static-library project builds these C sources with arithmetic
signed right shifts (`SASR`). Floating-point acceleration, LTP-cut and WAV49
extensions are disabled. `lib/util/gsmfr.cpp` is the C++ adaptation layer; it
compiles against this same release's private header for field-wise predictor
snapshots. Drivers include only the generic utility header. Updating this
library requires reviewing that adapter and rerunning codec vectors and
active-call save/load gates.
