# Acquired 8xxx software frontiers

## Current boundary

The stock 8250 v5.02 PPM K, 8850 v5.31 PPM C and 8890 v12.20 PPM C
execute with their own acquired flash/PMM inputs. None is promoted to graphical
boot, interactive UI, registration or calls. Input hashes and acquisition
provenance remain in `roms/README.md`.

The normal 8250 remains at its fail-closed verifier boundary. The separate
`nsm3dr6` research composition executes the MCU-uploaded verifier and advances
to the next DSP loader. Its declared PROM6 input is not an acquired mask ROM;
graphical boot and phone functionality remain unproved.

## 8250 staged verifier

The stock descriptor at flash `0x3188c0` is
`0f00 0000 00df 0f00 00dc 0000`. Its 223-word program at `0x3188cc`
has SHA-1 `6646da3c5be9c70deda7e0b5b9f257d5d2ace815`, identical to
the independently extracted 8210/6210 program. A read-only live capture at
0.5 seconds reproduces every word at MCU `0x11e00` and these supplied fields:
DSP `087b..0881 = 0100 0300 0000 e800 0001 0001 0200`.
The final observation records 58 ownership transitions on each buffer,
strictly alternating, with shared results `0000 ffff` still unpublished.

`tools/nsm3d_verifier_observe.lua` and
`tools/nsm3d_bootstrap_trace_check.py` protect that boundary; the latter also
pins the stock flash before comparing the runtime program. Execute the Lua
observer with the same private-directory options as the scout, without
`-verbose`, and check its log against `roms/noki8250/8250-502mcuppmk.fls`.

The `nsm3dverify` core-only instrument executes those stock bytes with the
observed geometry and sparse input stream. Reproduce with:

```sh
.venv/bin/python tools/nsm3_verifier_check.py mame/mame \
  roms/noki8250/8250-502mcuppmk.fls run_8250_staged_core --product 8250
```

The boundary case consumes 116 blocks before requiring port `002d` at PC
`0f9f`. Existing COBBA-model comparisons select F/status/F and publish the
supplied register-F value in word 0, the selected PROM version in words 1/2,
and 6 in word 3. Stock-input fingerprint `f3a3625c` differs from NSM-3's
`c2e06006`. The ROM4 and alternate-F BIOSes are sensitivity fixtures only;
none identifies fitted NSM-3D silicon, and no fixture publication is injected
into `noki8250`.

Once shared offset 2 changes, `0x2cb316..0x2cb31e` copies shared word 1
to RAM `0x12f028` and word 0 to `0x12f026`, then returns without validating
them locally. A halfword-aligned literal Thumb-BL scan of the complete stock
flash finds one candidate call to `0x2cb226`, at `0x2f33d0`; its decoded
caller continues initialization after return. The direct literal reader of
`0x12f026`, at `0x2de1ca`, formats a three-character COBBA identifier; it is
not a verdict check. Indirect consumers through aliased structure pointers
are not closed by the scan. The next target is the descriptor-driven loader
and its mask-entry/publication contract, not an assumed result-validation gate.

### Live uploaded-code composition

`nsm3dr6` combines the existing MCU/HLE devices with a bounded C54x staged
executor. MCU-uploaded shared memory is its executable program; native code
owns buffer acknowledgements and result publication while active. HLE
bootstrap acknowledgements are suppressed during that ownership. No result
words, transfer-count completion or MCU state are manufactured.

The authentic service package `nsm3d_604.exe`, member `nsm-3d.ini`, names
`Rom6ImageFile=nsm3dx_6.040`. This supports a ROM6-family experiment, not a
fitted-mask identification for v5.02. The composition supplies immutable
PROM version 6, existing nominal COBBA register-F/status inputs, and a
declared 13 MHz execution clock. It retains the staged code's CTSI writes
to ports 0/0c/0e without fabricating interrupts; other ports and execution
outside the recovered program fail closed.

On a product-local cold run, native execution publishes `0000/0006/0006/0006`
at 0.996196 seconds. MCU `0x2cb328` has retained `0000/0006` after exactly
58 ordered ownership pairs per buffer. The next initialization clears shared
memory, loads descriptor `0x311d14` with fields
`fd00/ff80/027e/0500/0078/0000`, and releases DSP reset at `0x2cb4c0`
at 1.014471 seconds. The research composition executes the independently
recovered loader prefix and stops at its first missing program-ROM read,
`ff80` from instruction `0f1e` (reported next PC `0f20`), at 1.015569 seconds.
It does not pretend to execute the missing mask.

The descriptor's 638-word upload has SHA-1
`1250a9e17ce44ec8cc373f222a817f99f505bcdf`. A read-only capture reproduces
every word at MCU `0x10a00` (DSP DARAM `0d00..0f7d`); the final 126 words
are executable loader code at `0f00..0f7d`, SHA-1
`5bcd6f091b23730b2484eb844841361ef7a12889`. The earlier words contain a
branch table and padding, not a complete DSP ROM.

GNU tic54x disassembly independently decodes the prefix: preserve fingerprint
words `04f7/04f8`, clear work memory, then repeat `MVPD ff80,*AR2+` for
104 words into data `0780..07e7`. The needed source range is `ff80..ffe7`;
the declared single version input at `ff87` does not supply the other words.
Later code installs 422 uploaded branch-table words into program `0590..0735`
using `MVDP`, posts selectors `14` then `01` at `0871`, strobes bit 3 at
MMR `29`, and waits on `0872`. It copies input chunks from `087e+0800` to
destination `087b`, decrementing the remaining `087d` count, then branches
to `0a00` when done. These later paths are statically decoded, not executed
or validated against ROM6. The next software question is the MCU consumer of
those selectors and whether its observable loader contract permits an honest
HLE implementation without importing ROM4 mask instructions.

Run `nsm3dr6` with the private-directory options below and
`tools/nsm3d_verifier_observe.lua`. The expected bounded run exits nonzero at
the loader's missing-mask read. Validate the captured native publication,
MCU retention, complete loader upload and precise stop boundary:

```sh
.venv/bin/python tools/nsm3d_live_verifier_check.py RUN/error.log \
  roms/noki8250/8250-502mcuppmk.fls
```

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

## Verification

The initial product-only GENSIO correction preserved the 3210 semantic
baseline and coherent frontier. The SELECT gate's missing records were an
ownership-filtered logging omission: retained board latches are not GENSIO
endpoint registers. Separate passive `gensio_select` records restore coverage
without changing register ownership or behavior; `verify-gensio` now passes
both 3210 firmware revisions. After adding the separate live staged-code
composition, the normal 8250 boundary, 3210 baseline and coherent frontier
still reproduce. The expanded tool suite passes 1,250 tests and all 11 MAME
overlay patches apply to the pinned upstream commit.
