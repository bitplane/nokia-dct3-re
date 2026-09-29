# GSM-FR codec integration

`util::gsm_fr_codec` owns GSM 06.10 encode/decode and predictor state for one
independent encoder and decoder. `util::gsm_fr_receiver` owns the existing
GSM 06.11 bad-frame substitution policy. Each codec block is 160 signed 16-bit
8 kHz PCM samples and one 33-byte serial frame with the `0xd` magic nibble.
Nokia DSP packet interpretation, radio scheduling, channel coding, audio clocks
and COBBA routes remain with their existing device owners.

## Build and provenance

`third_party/libgsm` contains the unmodified library source subset from the
authors' [libgsm 1.0.24 release](https://www.quut.com/gsm/), with its complete
permissive notice in `COPYRIGHT`. Its README records the archive hash and source
selection. No proprietary firmware or generated state is part of this library.

`patches/mame-libgsm-build.patch` adds MAME's `gsm` static-library project,
the utility sources to `utils`, the ordinary executable link dependency and
the third-party license index. `make overlay` copies the tracked library and
utility sources into their MAME directories. The build uses neither a download
step nor an external `--whole-archive` link override.

The bundled release is required: an arbitrary system libgsm would expose an
unreviewed private state layout. A library update must preserve or deliberately
version the utility's explicit snapshot contract. Upstream acceptance of this
new dependency and utility remains subject to MAME review.

## State contract

Drivers include the generic C++ header. Only `lib/util/gsmfr.cpp` includes the
bundled library's `gsm.h` and `private.h`; the driver no longer repeats its C
structure definition or function declarations. The private header and C
library are built from the same tracked release.

Snapshots retain the previous fixed-width predictor fields for both directions,
including long-term samples, short-term filters and preprocessing histories.
MAME devices save those fields individually. Pointers, allocator data and C
structure padding are excluded. Restore validates both channel states before
installing either. Decoder lag, history bank and preprocessing ranges are
checked; unsupported LTP-cut, fast, WAV49 and debug/framing flags are rejected.
The preprocessor's logical 32-bit value is stored in its existing 64-bit save
field and range-checked for hosts whose C `long` is narrower.

Encoding and decoding copy public buffers across the older mutable C interface.
A rejected frame leaves the caller's PCM buffer unchanged. Bad-frame
substitution retains the previous valid frame, attenuates repeated losses and
mutes at 320 ms; its history is saved independently of the codec predictors.

## Acceptance

- `make verify-gsm-fr-codec`: five deterministic input families, 64 frames
  each, reproduce the original pinned integration's encoded-byte and decoded-PCM
  digests. These are compatibility vectors, not ETSI conformance vectors.
  The same executable tests predictor continuation, atomic restore failure,
  malformed-frame handling and saved bad-frame concealment.
- `make check-mame-patches`: the complete overlay stack applies to pinned MAME
  independently of the already-overlaid working tree.
- `make verify-radio-call-state-roundtrip`: 3210 v6.00 and v5.01 active-call
  saves replay exact ordered handset/network media checkpoints.
- Product-specific physical-duplex gates check that the shared utility still
  carries microphone and receiver media through each handset's existing routes.
  Their per-run configuration copies preserve handset input settings but omit
  saved host mixer routes, allowing the temporary audio devices to be discovered
  afresh without writing host settings back into the shared fixture.

These checks validate the software codec integration and established HLE media
paths. The real-DSP backend must execute its own speech algorithm and satisfy
its separate COBBA/PCM gates before receiving full-phone promotion.
