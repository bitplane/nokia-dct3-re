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

Primary reference: [ST M28W320ECT/ECB preliminary specification, November 2001](https://www.mouser.com/datasheet/2/389/stmicroelectronics_SGSTS06829-1-1206398.pdf),
command/status sections and tables 3--10. Intel C3 remains a separate part and
does not provide ST OTP/CFI data.

- 71 individually protected blocks: 63 main blocks and eight top parameter blocks.
- Power-up/reset locking, lock/unlock/lock-down and WP-dependent protection.
- Identity and block signatures with documented address don't-care bits.
- Global status while busy/in status mode; sticky protection/sequence errors.
- Deferred word/double-word programming and block erase; no immediate array commit.
- Program/erase suspend/resume, including other-block programming during erase suspend.

Program data is not a command, even when its low byte is an opcode. Errors
remain until Clear Status or reset. The model uses typical program/main-erase/
parameter-erase intervals of 10 us/1 s/0.8 s, not measured board timings.

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
The runner requires a fresh directory and rejects incomplete/duplicate results.
The separate `c3test` preserves its Intel-only conformance contract.

## Remaining Limits

- ST-specific CFI, unique ID, OTP/security-block storage and permanent protection
  remain unimplemented. Unavailable signature/query words return erased data;
  this is not an authentic identifier or a valid CFI table.
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
