# Flash storage contracts

## Ownership

Native MAME `intelfsh` devices own identity, command mode, array data and
persistent storage. The ST M28W320ECT subclass owns its part-specific command
engine. The Nokia driver only routes reads/writes; it has no flash command
state, fixed-status address, block-lock acknowledgement shim or erase timer.
Named setup states collect word/page data independently of command dispatch;
their saved byte values remain stable across the readability refactor.

The ordered source series separates `mame-intelfsh-geometry.patch` (shared
geometry/identity attributes, Intel B3 part declarations and NOR programming),
`mame-intelfsh-c3.patch` (opt-in Intel C3 protection/CFI), and
`mame-intelfsh-st.patch` (the ST subclass and virtual dispatch seam). The
combined series reproduces the same native source. Overlay application checks
the complete patched tree, since later patches can change earlier context.

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
- Separate protection plane: four immutable factory words and eight user OTP
  words, visible through signature/CFI modes. C0h programs user bits only from
  one to zero; clearing lock DQ1 permanently closes the user segment. WP,
  device reset and array erase cannot undo this lock or erase OTP.
- Qualified VPP lockout input: word/page programming, erase and OTP programming
  refuse to start when invalid, setting sticky status bit 3 without changing
  either storage plane. Array/signature/CFI reads remain available.

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
not an inferred correction. The explicit protection map, not that size byte,
defines the eight implemented user words.

## OTP Identity And Persistence

An optional `otp` region beneath the flash device supplies thirteen big-endian
halfwords: lock at 0x80, factory identity at 0x81--0x84 and user data at
0x85--0x8c. No handset configuration fabricates a factory identifier; without a
dump those factory words remain 0xffff. Synthetic identity bytes exist only in
the independent device fixture. Reserved lock bits retain their seed values;
only DQ1 is writable, and DQ2 must not be programmed to zero (full Table 7).
Malformed reserved-bit/address writes are refused without inventing a silicon
error code. Factory/locked-segment writes produce protection/program errors.

Protection programming is deferred and cannot be suspended. It can run during
array-erase suspension without replacing that erase's remaining time. Reset
cancels an unfinished OTP program under the same prior-cell-retention policy
as array programming. Its nominal 10 us duration reuses the word-program
interval; a separate OTP pulse duration is not established.

Unchanged OTP preserves legacy raw-array NVRAM files. Changed OTP appends a
part-specific 26-byte big-endian protection tail. Imports validate the complete
array and optional tail before publishing either: truncated/oversized tails,
reserved-lock changes, factory words differing from the configured seed and
attempts to set seeded zero bits are rejected atomically. A raw-array import
uses the configured OTP seed; it does not invent or import a factory identity.
Loading an older NVRAM/save-state image can rewind emulated state, as with any
other persistent MAME device; firmware commands cannot reverse the OTP lock.

## Supply Qualification

`vpp_valid_w` supplies the logical qualification used at operation start. Its
default is valid, preserving the handset profiles' existing supply assumption;
no board pin or numerical voltage is inferred. Device reset clears sticky
errors but does not change this external input. Save states preserve it.
Supply recovery alone does not clear the error. Successful later operations
can complete with the old error still set until 50h or reset clears it.

The full datasheet's low-voltage control-input description samples VPP only
at the start of ordinary word programming/erase. The model does not abort an
accepted operation when this qualified input changes. This is not a model of
high-VPP fast programming, unstable supplies, voltage margins or uncertain
multiword results; those require separate electrical evidence. Competing
simultaneous fault conditions have no validated status-priority contract.

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
The VPP result requires five refused operation families, three unaffected read
planes, sticky-error recovery, sampled-start behavior, reset/input separation
and pending-operation/input restoration.
The runner requires a fresh directory and rejects incomplete/duplicate results.
The separate `c3test` preserves its Intel-only conformance contract.

The same runner executes `stotptst` twice in separate processes: fresh OTP
conformance and cold reload of the files it wrote. It checks all eight user
words in both read planes, factory immutability, one-way programming, permanent
locking, WP/reset refusal, three partial/pending restore cases (including OTP
inside suspended erase), seven atomic malformed-import controls, two short
write failures, raw-array compatibility and binding to an explicit fixture
seed. The second process verifies persisted user data, lock and array data,
then attempts another protected write. This is a device-interface contract,
not evidence that a handset's authentic factory OTP has been recovered.

## Remaining Limits

- Authentic factory unique IDs remain undumped. Unavailable identity and
  reserved query words return erased data, not a valid recovered identifier.
  CFI describes the physical part, not a claim that every advertised electrical
  feature is implemented.
- VPP lockout qualification is implemented, not an analogue supply model.
  Numerical voltage margins, high-VPP behavior, wear and pulse failures remain
  unmodeled; the physical handset VPP/RP# connections remain unproved.
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
