# Flash storage contracts

## Current Boundary

Native `intelfsh` owns identity, array data, geometry and persistent storage.
The transitional `nokia_b3_flash_device` adds the observed 3410 status/suspend
sequence. Handset acceptance is not complete flash conformance.

The independent `b3test` executable proves command-like program data remains
data, one-way programming, neighboring-block isolation and suspend/resume
state restoration. Its fixed-status check is explicitly compatibility coverage,
not a silicon claim. Programming `12ff` in another block during erase suspend used to
incorrectly change read mode; data now bypasses command decoding entirely.

```sh
python3 tools/run_mame_isolated.py --mame-dir mame --run-dir tmp/b3-conformance -- b3test -video none -sound none -nothrottle -seconds_to_run 1
```

## Datasheet Audit

Primary specifications: [ST M28W320ECT/ECB, November 2001](https://www.mouser.com/datasheet/2/389/stmicroelectronics_SGSTS06829-1-1206398.pdf),
pp. 11--19 and command/flowchart tables; [Intel B3 specification](https://pccomponents.com/datasheets/INTEL-TE28F160B3BD70.pdf),
command descriptions. Do not substitute C3 OTP/CFI tables for ST data.

| Contract | Present model | Remaining work |
| --- | --- | --- |
| ST `20:88ba`, 4 MiB top-boot geometry | Native part attributes | Further fitted-part identities remain product-specific. |
| Program setup followed by arbitrary data | Native one-way program; adapter preserves command-like values | Timed programming and voltage/error behavior. |
| Erase suspend/resume, read/program another block while suspended | Adapter timer and saved state | Native engine with deferred array commit, not immediate erasure. |
| ST status reads at any address in status mode | Adapter overrides only one configured address | Remove this compatibility assumption after classifying the handset's complete command ordering. No datasheet-backed partitioned-RWW claim. |
| ST all blocks locked after power-up; per-block lock/lock-down | Adapter acknowledges lock commands but does not enforce them | Generic ST protection implementation and independent locked-write/erase tests. C3 protection tests do not validate ST. |
| CFI, OTP and sticky error status | C3 subset separately tested | ST-specific tables, security storage and failures; do not borrow Intel layout. |

The one-second erase delay is unmeasured. Passing `verify-flash-pmm` proves
3410 compaction and cold reload, not interrupted-erase cell contents, physical
completion time or protection. Datasheet-known semantics above are software
backlog; only electrical duration/voltage margins require measurements.
