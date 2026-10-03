# Acquired 8xxx software frontiers

## Current boundary

The stock 8250 v5.02 PPM K, 8850 v5.31 PPM C and 8890 v12.20 PPM C
execute with their own acquired flash/PMM inputs. None is promoted to graphical
boot, interactive UI, registration or calls. Input hashes and acquisition
provenance remain in `roms/README.md`.

The next bounded target is the 8250 final DSP verification publication, not
a guessed verdict. Independently locate its staged verifier and compare its
program and peripheral inputs with the established NSM-3 instrument before
deciding whether that instrument applies.

## Recovered GENSIO contract

All three independently select CCONT with control `0x22`, write the command
at `0x2c`, poll status `0x6d` bit 2, and read the response at `0x6c`:

| Product | Command setup | Receive-ready loop before correction | PC at eight seconds after correction |
| --- | --- | --- | --- |
| 8250 v5.02 | `0x2feb1c` | `0x2feb2c..0x2feb32` | `0x2cb314` |
| 8850 v5.31 | `0x3030ac` | `0x3030bc..0x3030c2` | `0x2f6e44` |
| 8890 v12.20 | `0x2fd0d8` | `0x2fd0e8..0x2fd0ee` | `0x2f0d40` |

The command byte must initiate receive-ready without requiring control bit 2.
`PRODUCT_8XXX` now configures that existing device contract. No DSP version,
completion, identity or provisioning was added by this correction.

The 8250 writes each ownership word (`0xfe`, `0x100`) to zero 58 times in
alternating order. It then polls shared offset 2 against `0xffff` at
`0x2cb30e..0x2cb314`. The 8850 and 8890 instead reach software flag loops;
their ownership and prerequisites are not yet classified. Similar GENSIO
code does not establish identical DSP families or completion semantics.

## Reproduction and limits

Use `tools/dct3_model_scout.lua` with `-autoboot_delay 0`,
`-seconds_to_run 9`, `-log`, `-noreadconfig`, and private working, NVRAM,
configuration and snapshot directories. The script captures CPU PCs and six
screens without changing firmware state. Use fresh NVRAM seeded only from
the acquired product-local PMM. `-verbose` additionally records transport
writes, but the tight final-result poll can produce very large logs.

The declared legacy `dsp_prom/drom/pdrom` uniform-fill audit members are
needed to satisfy the current ROM declarations; they are not executed by this
HLE composition and are not fitted mask evidence. No missing boot ROM was
fabricated. A zero process exit and a PC sample are not phone acceptance.

The associated 6250 v5.03 cold scout, still using `PRODUCT_DEFAULT`, parks
at `0x4e7dca..0x4e7dce` polling CTSI reset-ready bit 4. Its release contract
requires independent recovery before inheriting any NPE-3 configuration.

## Verification caveat

The 3210 semantic baseline, coherent frontier and all 1,230 tool tests pass
after this product-only change. `verify-gensio` currently fails its SELECT-observation requirements:
CCONT transactions are present, but the checker finds no SELECT records.
This is an unresolved gate/instrumentation discrepancy, not a passing gate
or evidence of a changed 3210 SELECT contract.
