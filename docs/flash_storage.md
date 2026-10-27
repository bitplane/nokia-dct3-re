# Flash storage contracts

## Ownership

Native MAME `intelfsh` devices own identity, command mode, array data and
persistent storage. The ST M28W320ECT subclass owns its part-specific command
engine. The Nokia driver only routes reads/writes; it has no flash command
state, fixed-status address, block-lock acknowledgement shim or erase timer.

The 3410 uses this native engine for fresh PMM compaction and cold reload,
retaining its exact idle-frame oracle. This establishes handset compatibility
alongside, not instead of, independent device conformance.

## Implemented ST Contract

Primary references: ST's full M28W320ECT/ECB specifications,
[February 2003 revision 3.2](https://datasheet4u.com/pdf-down/M/2/8/M28W320ECT-STMicroelectronics.pdf)
and [April 2003 edition](https://www.manuallib.com/download/pdf1/ST-M28W320ECT-M28W320ECB-32-MBIT-%282MB-X16-BOOT-BLOCK%29-3V-SUPPLY-FLASH-MEMORY-HANDBOOK.PDF),
command/status sections and Tables 3--11, 27--30. Intel C3 remains a separate
part; its CFI/OTP layout is not reused.

- 71 individually protected blocks: 63 main blocks and eight top parameter blocks.
- Power-up/reset locking, lock/unlock/lock-down and WP-dependent protection.
- Identity and block signatures with documented address don't-care bits.
- Global status while busy/in status mode; sticky protection/sequence errors.
- Deferred word/double-word/quadruple-word programming and block erase; no immediate array commit.
- Program/erase suspend/resume, including other-block programming during erase suspend.
- Literal ST CFI query data at word offsets 0x10--0x47, with zero upper data
  bytes, x16 interface, 4 MiB capacity, ordered top-boot erase regions and
  eight-byte programming pages. Identity words remain 0x0020/0x88ba.

Program data is not a command, even when its low byte is an opcode. Errors
remain until Clear Status or reset. The model uses typical program/main-erase/
parameter-erase intervals of 10 us/1 s/0.4 s, not measured board timings.
Quadruple programming accepts four distinct addresses within one aligned
four-word page, in any order; each data cycle remains data even if it resembles
a command. Malformed pages are rejected without inventing a silicon error code.

The November 2001 preliminary specification is not interchangeable with the
full revision: it reports four-byte pages and 0.8 s parameter erase, and
describes a security block removed in the later revision history. Do not add
that preliminary-only protection mechanism to this part. The full CFI table
still literally reports 0x03 at offset 0x47, despite the explicit protection
map listing eight user words; query emulation preserves the published byte,
not an inferred correction. Actual OTP storage remains a separate contract.

## Independent Acceptance

```sh
make build
python3 tools/run_st_flash.py tmp/st-flash-check --mame mame/mame
make verify-flash-pmm RUN_DIR=tmp/st-flash-pmm
```

The ROM-independent `sttest` has 120 checks, enumerates all blocks and
checks deferred commits, sticky errors, command-like data, busy-command
admission, neighboring main/parameter isolation, WP/lock-down transitions,
double-word programming, four pending/suspended save-state replays and two
reset-abort paths. It claims neither a handset boot nor physical cell fidelity.
Additional terminal results check all 56 CFI bytes, independently recompute
capacity/region geometry, restore query mode, restore a half-collected page and
a pending page program, reject locked/malformed pages, and bracket the nominal
parameter erase between 350 and 450 ms. No flash array or factory ID is
fabricated to satisfy query reads.
The runner requires a fresh directory and rejects incomplete/duplicate results.
The separate `c3test` preserves its Intel-only conformance contract.

## Remaining Limits

- Unique ID, user OTP storage and its permanent lock remain unimplemented.
  Unavailable signature/query words return erased data; this is not an
  authentic identifier. CFI describes the physical part, not a claim that every
  advertised electrical/protection feature is implemented.
- No VPP/voltage-margin, wear or program-pulse failure model exists.
- Suspend completes immediately in the command abstraction. Electrical latency
  and board-specific timing are unmeasured.
- Reset cancels pending timers without committing their data. Retaining prior
  cells is an emulator policy: physical interrupted cells are unspecified and
  may require erasure before reuse.
- Device-reset conformance does not establish the board's RP#/VPP wiring or
  behavior during every rail transition; warm baseband reset remains distinct.
- Reads of the suspended operation's own block, nonadjacent double-word writes
  and partial CPU byte-lane programming are not validated contracts.

The removed fixed-address status behavior was a compatibility assumption,
not partitioned read-while-write silicon. Do not restore it to compensate for
a future command-ordering or timing discrepancy.
