# Acquired 8xxx software frontiers

## Current boundary

The stock 8250 v5.02 PPM K, 8850 v5.31 PPM C and 8890 v12.20 PPM C
execute with their own acquired flash/PMM inputs. None is promoted to graphical
boot, interactive UI, registration or calls. Input hashes and acquisition
provenance remain in `roms/README.md`.

The next bounded target is the MCU validation of the 8250 staged verifier's
published fields. The program and supplied geometry match NSM-3, but a
sensitivity fixture's chosen PROM/COBBA values are not fitted-chip evidence
and are not supplied to the handset.

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
caller continues initialization after return. Indirect calls and consumers
through aliased structure pointers are not closed by that scan. Validation
of the retained fields, rather than the upload loop's mere return, remains
the next MCU-side question.

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
both 3210 firmware revisions. The expanded tool suite passes 1,238 tests.
