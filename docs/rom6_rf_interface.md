# Native ROM6 radio interface

## Current boundary

The acquired-ROM6 **8210 NSM-3 v5.31** composition `nsm3r6` already issues
native radio-facing work. Its next missing external contract is **timed serial
output delivery and an attached RF/sample path**, not another MCU startup
report. No receiver, tuning decision, carrier or successful decode is supplied
by this census. [Native boot acceptance](rom6_dsp_bringup.md) remains separate.

## Reproduction and coverage

```sh
.venv/bin/python tools/run_rom6_native.py --scenario radio
```

This repeats the qualified 60-second physical unlock/menu scenario with passive
I/O taps. The runner checks native composition, own uploads, verifier/self-test,
program coverage and reviewed early/late menu frames before accepting capture.
`radio-census.json` summarizes `primary/rom6_radio_writes.tsv`; the latter
contains every write, timestamp, mask and bus-time CPU PC. Per-port read
summaries retain counts and first/last times, with explicit caps on distinct
values (16) and CPU contexts (32). These contexts are not issuing pipeline-slot
identities. Missing/overflowed streams and mismatched per-port counts fail.
This is a manual research instrument, not an RF acceptance gate. Retire its
broad overlay/port capture when focused serial-device and receiver tests cover
the recovered boundary; keep only the normalized contract and useful controls.

Two fresh processes produce byte-identical **342,473-write** streams. There
are 13 observed read addresses and 38 write addresses, not an exhaustive map
of all reachable firmware paths or physical MAD2 registers.

| Observed operation | Count | Boundary |
| --- | ---: | --- |
| P27 sample reads | 1,245,280 | Every value is zero; first 0.535343582s, last 60.014754923s. COBBA's unattached callback supplies deterministic silence, not measured receiver noise. |
| P31 / P32 writes | 30,354 each | Backend retains words but does not decode an RF chip or apply tuning. |
| P04 writes | 35,855 | Existing comparator raises transfer pending; no serial payload is delivered to a modeled RF device. |
| P30 writes | 30,789 | Raw values 0000/00bb/0100/0103; their physical meanings are not established here. |
| P0e frame-length writes | 11 | Values 0546/1387; inherited quarter-symbol/frame notification policy still needs ROM6-specific validation. |

The report has 546 distinct *issued* P30/P31/P32/P04 tuples. Only 30,348 P04
writes follow fresh writes to all three payload registers; the others lack a
complete fresh payload and their meanings are not established by this capture.
Do not treat every P04 write as a new RF
command. Delivery, bit order, latch timing, IRQ ownership and samples remain
separate obligations.

## Own-code attribution

The first-P31 snapshot preserves the live low program overlay before later
reuse. Its words `0b20..0b6f` exactly match 160 bytes at MCU offset `117070`,
SHA256 `206cc4a2a0352fea4ff5dcfdf037bea65c7eedc15faacdc243accde5035ebb46`.
The uploaded routine contains `PORTW *AR2+,P31` at `0b54` and
`PORTW *AR2,P32` at `0b58`, selecting data words `3f04` or `3f06` upstream.
Its observed bus-time PCs are `0b55/0b59`. This is one writer, not the sole
source: eight P32 CPU contexts occur in this run. A terminal overlay dump is
not evidence of bytes executed earlier at the same low address.

| P32 bus-time CPU PC | Writes |
| --- | ---: |
| `00ae3b` | 9 |
| `001a95` | 19,357 |
| `0048ac` | 3 |
| `003d2b` | 2,749 |
| `001614` | 2,747 |
| `0038c1` | 2,743 |
| `000b59` | 3 |
| `0023e0` | 2,743 |

The acquired resident routine `388c..38c8` has a separate AR2==31 destination
branch: `38bb` sends mapped AL to P31 and `38c0` sends AH to P32. Other branches
build words in RAM for later serializers, including the observed `1a8c..1a98`
P30/P31/P32/P04 path. A pair can therefore be halves of one computed word,
not two independently addressed RF registers.

## Cross-reference limits

Dan's snapshot `dd356ff7c5fc62529ab5843ce737593ebff51f73`, especially
`rom6-rf-acquisition-current.md`, `rom6-pll-unmatched-o3x.md` and
`rom6-frame-int0-ownership.md`, supplies hypotheses, not an 8210 specification.
Our own run reproduces setup pair `81c0/4000` 17 times, `9044/4400` 7 times and
`48ce/4400` 11 times. His 3310 trace's `48c8/4400` channel interpretation must
not be transplanted onto our different word. Zero P30, unmatched tuning and
frame-versus-block notification remain distinct questions; none justifies
forcing the laboratory carrier or synthesizing a received report.

The retained Nokia NSM-3 System Module manual (issue 1, 12/1999, printed 41)
identifies HAGAR, a serially programmed PLL, 64/65 prescaling, 200 kHz reference
and `M=N*64+A`. Printed 25 names MAD2's SCU and MFI separately. These primary
constraints do not decode the DSP port packet or prove which block owns each
edge. Source/hash: [hardware atlas](hardware_atlas.md#cobba-part-identity-does-not-select-pcm-timing).

## Next implementation boundary

First recover the queued-output contract: when P04 snapshots P30/P31/P32,
which deadline retires it, how replacement/cancellation/reset work, and whether
completion is independent of sample readiness. Implement a tested, saved
device-boundary queue only once those semantics are supported. Keep raw words
intact and unknown modes explicit; do not infer delivery from issued tuples.
Then decode the HAGAR serial fields against own firmware producers and the
documented frequency plan, followed by a clocked sample source with provenance.
Native FCCH/SCH/BCCH, camp and registration come after those boundaries.
