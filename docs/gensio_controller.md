# GENSIO controller

## Ownership

`nokia_gensio_device` owns the recovered MAD2 GENSIO register subset, endpoint
selection and status, CCONT byte transport, LCD pin serialization, and the
SELECT1/2/3 backing latches. The phone driver supplies board wiring only:
CCONT byte/select callbacks and PCD8544 `DC`, `SDIN`, and `SCLK` pins.

| MAD2 offset | Contract | Status |
| --- | --- | --- |
| `0x2c` / `0x6c` | CCONT serial write/read | Tested partial hardware |
| `0x2d` / `0x6d` | endpoint control/status; LCD `0x21`, CCONT `0x25` | Tested partial hardware |
| `0x2e` / `0x6e` | LCD data/command, serialized MSB-first | Tested partial hardware |
| `0x6f`, `0xad..0xaf`, `0xed..0xef` | SELECT1/2/3 register latches | Mapped; attached peers unknown |

Selection resets status to idle/TX-ready `0x03` and CCONT selection starts a
new command phase. A selected CCONT transfer makes status `0x07`; consuming
`0x6c` clears receive-ready and restores `0x03`. Completion is synchronous
because neither recovered 3210 ROM exposes a minimum delay or completion IRQ.
This is a firmware-visible contract, not proof of zero physical latency.

## SELECT investigation

The v6.00 and v5.01 ROMs perform instruction-equivalent initialization:

| register | startup value | later observed operation |
| --- | ---: | --- |
| `0xaf` | `0x00` | read-modify-write clearing bit 6 |
| `0x6f` | `0x00` | repeated read-modify-write setting bit 0 |
| `0xef` | `0x00` | none in the first boot second |
| `0xad` / `0xed` | `0xc4` / `0x21` | none in the first boot second |
| `0xae` / `0xee` | `0x20` / `0x80` | none in the first boot second |

The owning routines relocate from v6.00 `0x2afbf2`, `0x2a31fa`, and
`0x2a1450` to v5.01 `0x2ad01e`, `0x2a06ce`, and `0x29e974`. This proves latch
and bit-operation behavior but does not identify the board devices attached to
the SELECT lines. No RF, audio, or other peer is modeled from these values.

`ExportGensioAccesses.java` enumerates direct, literal-derived and scalar
candidate uses for another ROM. `make verify-gensio` checks the two-ROM startup,
read-back and bit-0 contract without committing raw traces.

## Remaining fidelity work

- measure physical busy/clock timing and recover remaining control-bit meaning;
- identify attached SELECT peers from board evidence or coherent traffic;
- validate reset values beyond the boot ROM's explicit zero writes; and
- add a peer only after firmware produces a transaction whose protocol is known.
