# Firmware images (bring your own)

This repository contains **no firmware**. The Nokia 3210 flash image is Nokia's
copyrighted code and is **not redistributable**, so it is git-ignored and must be
supplied by you. Everything in this repo is original tooling, analysis, and
annotations that operate *on* such an image — it is useless to anyone who does not
already have a legitimately-obtained dump.

## Reference 3210 image

The **NSE-8/9 v06.00 3210** flash file. It is distributed (by third parties) inside
a `.rar` of flash files, e.g.:

- `https://firmware.center/firmware/Nokia/3210%20(NSE-8-9)/Flash%20Files/NSE-8%20v.06.00%203210%20NSE-9.rar`

Extract the `.fls` from that archive.

## Verify what you have

Match one of these SHA-256 sums so analysis/addresses line up with the docs:

| file | sha256 | notes |
|---|---|---|
| `3210f600a.fls` | `7bf29b96e544b682c4d6d01c7a6eaef89909c4191a52d829115d37b31c0c0d8a` | raw flash dump as extracted |
| `3210f600a_swap16.bin` | `66d2ec57385099d6dca8d93b75d72fcde496f3f8a3246331351d8ebce6fac8c1` | halfword-swapped image used by the tools/Ghidra (32-bit literals are halfword-swapped in the raw `.fls`; the tools expect the swapped form) |
| `nokia_3210_nse-8_v05_01_full_hu.fls` | `62de70cd5451444cfcd4ed6c6d8a9a84c0e783a557f8489f2dc5faa283b66272` | local v5.01 same-product runtime/static control; installed into the MAME set as BIOS `501` under `3210f501.fls` |

If your sums differ you have a different firmware version; the absolute addresses
in `docs/` and `ghidra/symbols/3210.csv` are specific to `3210f600a`.

## Where to put it

Place the files here (git-ignored):

```
roms/
  3210f600a.fls
  3210f600a_swap16.bin      # produce via the swap step (see Makefile / docs)
  noki3210/                 # MAME ROM set layout, for `make run`
```

The EEPROM is a separate 24C128 used for product, calibration, identity,
security, and user NV data. An all-`0xFF` image represents an erased chip, not a
factory-provisioned handset. The project normally generates an explicit test
profile; see `docs/eeprom_analysis.md`.

## MAD2 mask-ROM caveat

The MAD2 ARM boot ROM is on-chip and is not part of a `.fls` image. Local files
named `boot_rom` are 64 KiB of uniform `0xff` placeholder fill, not executable
dumps. The driver declares the mask ROM `NO_DUMP` and HLEs only its established
exit contract: one ARM branch from reset vector zero to flash entry `0x200040`.
It does not claim to model the real ROM's clock, power or flashing-mode setup.

## DSP region caveat

The currently declared `dsp_prom`, `dsp_drom`, and `dsp_pdrom` files are
checksum-valid MAME set members, but the local files are uniform `0xFF` fill:

| file | size | SHA-256 | classification |
|---|---:|---|---|
| `dsp_prom` | 49,152 | `abd8e2a70d51a53287941838446e6ed141005d401faf597fd7c6de0bbc8c329d` | placeholder fill |
| `dsp_drom` | 16,384 | `0fbba07a833d4dcfc7024eaf313661a0ba8f80a05c6d29b8801c612e10e60dee` | placeholder fill |
| `dsp_pdrom` | 4,096 | `f47a8ec3e9aff2318d896942282ad4fe37d6391c82914f54a5da8a37de1300c6` | placeholder fill |

They contain no executable C54x program and cannot support DSP disassembly or
cycle-accurate execution. MAME ROM-audit success proves only that these files
match the current declaration. Run `make audit-dsp-roms PHONE=noki3210` to
classify the actual local regions before using them as evidence. A future real
DSP dump must be identified by product and hash and must not silently replace
these placeholders in a test baseline.

### Nokia 5110 DSP ROM4 research input

A complete 5110 ROM4 DSP mask image was historically published by AlexD in
2003 and is now used by an independent native C54x co-simulation. It is not
part of the handset `.fls`, is absent from that project's public Git tree, and
is not redistributable here. The surviving discussions are:

- `https://nokiafree.org/forums/archive/index.php/t-39175.html`
- `https://forum.gsmhosting.com/vbb/f83/alexd-made-nokia-5110-rom4-dump-288554/`

The GSMHosting mirror describes its archive as AlexD's Nokia 5110 DSP-ROM
memory dump and says it includes an annotated IDA project. The recovered
four-volume `sss.part01.rar` through `sss.part04.rar` package matches that
description exactly: it contains `dsp_full.bin`, a complete `dsp_drom.txt`,
`dsp_full.idb`, two disassembly listings, DSP-block notes, and task notes. This
is substantially stronger provenance than the historical filename alone,
although no author-published canonical digest is available.

Keep a lawfully obtained candidate under an ignored research path:

```text
roms/research/nse1-rom4/original/<original filename>
roms/research/nse1-rom4/working/dsp_full.bin
roms/research/nse1-rom4/working/dsp_drom.txt
roms/research/nse1-rom4/SHA256SUMS
```

Hash the untouched volumes before extraction. Do not rename a 3210 MCU-uploaded
DSP block, the uniform-fill `dsp_*` placeholders, or an unrelated Calypso ROM
as this image. The locally recovered package is inventoried as follows:

| source volume | bytes | SHA-256 |
|---|---:|---|
| `sss.part01.rar` | 286,720 | `850bca9c0a78b5ad7989356c44beb8911ed02dc0dc82b2106d2f654826359549` |
| `sss.part02.rar` | 286,720 | `fc97d944c5412ca9b5fdf0a64ae5e58e82b4f122965c912848df314ba692e770` |
| `sss.part03.rar` | 286,720 | `3af1031f97ccbb638d1b39a827fb1f468429b9728fe3e2dd3a157bb5bc7dfc2c` |
| `sss.part04.rar` | 202,812 | `4a37e8cf5baad0bdb4fd9bc0d93b233d3d362325c38a1970d13b1ce4776e49ae` |

The working `dsp_full.bin` is 131,070 bytes (65,535 big-endian words), SHA-256
`4a7a9ba9b3b90c732dce8b2f36522ea49249a7b0d1ecbb4538f7fdbb9c51504f`.
It omits DSP program word `0xffff`; the archive's disassembly also supplies no
word at that address. Keep this historical export unchanged. A backend may
leave the final word at its initialized value, as the independent loader does,
but must not present an invented padding word as dumped data. The complete
16,384-word `dsp_drom.txt` has SHA-256
`998f006dd06c3d73adb1ac7401e50dc42496a95dc3e3a396cf8f0bfad38f5055`.

After placing the private working pair at the paths above, run
`make audit-dsp-rom4`. The gate recognizes the untouched 131,070-byte historical
program export as well as an explicitly represented 128 KiB address space, and
requires a complete, unique, nonuniform 16,384-word `B000:EFFF` DROM map. It
validates transport shape, not authorship or correctness.

The input matters because the native 5110 backend can log DSP-to-COBBA serial,
parallel-control and PCM traffic without physical probing. It remains a 5110
input: applying values to a 3210 needs cross-product corroboration, and a known
missing acquisition overlay limits what a completed boot proves. See
`docs/djr_dsp_integration.md` and `docs/cobba_control_boundary.md`.

## Portability ROMs

Additional firmware is used only as local validation material. Put each complete MAME set in its
own ignored directory (`roms/noki3330/`, and so on), run `make audit-roms PHONE=<set>`, and never
commit an image or extracted archive.

### DCT3 archive coverage

The firmware.center Nokia index contains directories for many DCT3 products,
but most of those directories are empty placeholders.  As checked on
2026-09-08, the two populated sets not already represented locally were:

#### Nokia 2100 NAM-2 v5.84

Source:
`https://firmware.center/firmware/Nokia/2100%20%28NAM-2%29/Flash%20Files/NAM-2%20v.05.84%202100.rar`

Place the untouched archive and extracted members in
`roms/2100-nam2-v584/`:

| file | bytes | SHA-256 |
| --- | ---: | --- |
| `NAM-2 v.05.84 2100.rar` | 2,329,827 | `7305eaa8453c2e98eea63d169ee02ca16e3c349a271dd738ffe6eff0523c6f0d` |
| `NAM205.840` | 1,443,376 | `6382beb1f8ba2d768f53ccc3111586657f26eff961900249c69061cb710db283` |
| `NAM205.84E` | 590,472 | `0e2f2d90d297946522934cfbefe180ce2b7727e331b04f972e48f5a1231502be` |
| `2100sharp.pmm` | 2,099,456 | `4e30a95c44b2c3b7ca79a3e020ab38c24571e0de18a9572ce730c7f7173a3ece` |

`NAM205.840` and `NAM205.84E` are contiguous Wintesla MCU and PPM-E streams:

| member | target range | extracted bytes |
| --- | --- | ---: |
| `NAM205.840` | `0x200000..0x35ffff` | `0x160000` |
| `NAM205.84E` | `0x360000..0x3effff` | `0x090000` |

`make normalize-2100` validates both source hashes and reconstructs
`roms/noki2100/2100f584e.fls`. The resulting `0x1f0000`-byte stock
candidate has SHA-1 `10795e5b5a8186df5571e661833ed5887061c21f` and SHA-256
`5e489a47db0a5529711f6dca8a822f920830363085dc0101ca62da64eaedf69f`.

Despite its filename, `2100sharp.pmm` is not PMM or EEPROM data. It is a complete
Wintesla flash stream covering `0x200000..0x3fffff`. Its extracted 2 MiB image
has SHA-1 `b7e30deb4393a76d509f843d25ecf20881bc25bb` and SHA-256
`c774f3e100663307a66a11c811c5b98f3101004a73ff4ae74b40403873d8e742`,
and differs from the stock MCU+PPM composition from image offset `0x22`. Keep it
as a separately labelled alternative/custom image; do not use it as persistent
storage or silently combine it with the stock members.

A static audit of the stock candidate recovered 652 direct MAD2 accesses from
326 literal seeds and the familiar sparse PUP, keypad GPIO and UIF regions. It
also recovered the exact 18-entry CCONT descriptor vocabulary at `0x0033e4f8`
(literal references at `0x002ffec8` and `0x0030704c`). This establishes useful
later-MAD2 family evidence, but not the fitted display, memory map, interrupt
routing, DSP contract or nonvolatile-storage policy. The `noki2100` declaration
therefore uses the conservative later-MAD2 composition only. It is a bounded
bring-up instrument, not a supported product profile.

#### Nokia 3610 NAM-1 v5.11

Source:
`https://firmware.center/firmware/Nokia/3610%20%28NAM-1%29/Flash%20Files/NAM-01%20v.05.11%203610.rar`

Place the untouched archive and extracted members in
`roms/3610-nam1-v511/`:

| file | bytes | SHA-256 |
| --- | ---: | --- |
| `NAM-01 v.05.11 3610.rar` | 3,600,924 | `6d6b479153cd61f6bc448c79d2ce62546477446d60320ac7c98098c099ae2ab2` |
| `NAM105.110` | 2,689,928 | `016b279283464a9f46d2790a292f84e420c9062d8025cdaca7321cfe5ada3caf` |
| `NAM105.11A` | 787,296 | `c7624ed644d31e3346f06d7f1ff82d0d6c0b1fdf8e2e3b1bf89f9b57a61b7d84` |
| `NAM105.11B` | 787,296 | `d4b81873b06452d2cf2f2d0794b4f33183247dee03689ec42185709bdf52f4e2` |
| `NAM105.11C` | 787,296 | `c4350f0bec54ed589371acc747b21846fdfac82f94a878e9cff4ee7f256b5ac2` |
| `NAM105.11D` | 787,296 | `55b2dbaf75a46792519ada9eb7cc0c6a7331362c927ca052c98875d36f1bfd7f` |
| `NAM105.11E` | 787,296 | `7304283589a7493f544279c3c78911679b3c21f7a97e7408926dd1911740d170` |
| `NAM105.11F` | 787,296 | `08d5c75f4bb5f712148ead898993661ff0ee19bb9bba23c0f3d4ea7fa1a832cd` |

`NAM105.110` is the common component and `A` through `F` are alternative PPM
members. All seven streams validate cleanly. The common MCU covers
`0x200000..0x48ffff` (`0x290000` bytes), and every PPM variant covers
`0x490000..0x54ffff` (`0x0c0000` bytes). No EEPROM/PMM member is present.

`make normalize-3610` selects PPM E as the representative comparison image,
validates both source hashes, and reconstructs
`roms/noki3610-candidate/3610f511e.fls`. The `0x350000`-byte result has SHA-1
`429bc32afe0a554887ba7539cf9ba3c9a67e7043` and SHA-256
`bcb22f093746b14cd55503abff72f7e3c043fc226e0bcc0b6972b03d9d635c89`.
The other PPM members remain equally valid source material; E is not asserted
to have a privileged hardware role.

The representative image yields 670 direct MAD2 accesses from 274 literal
seeds. Of those, 78 address PUP, 30 keypad GPIO and 29 UIF; the conservative
scan recovers no direct GENSIO SELECT access. The byte-lane-correct CCONT scan
finds the same 18-entry descriptor vocabulary at `0x00489b74` (literal
references at `0x003d7a70` and `0x003ec8f8`). These results establish a strong
MAD2-family relationship while also exposing an unresolved product-specific
serial/control path. No executable machine profile is declared from them.

The nominal firmware.center directories for 3390, 5510, 6130, 6150, 6250,
7110, 8210, 8250, 8290, 8810, 8850, 8855 and 8890 were empty at that date.
Two misleading directories were rejected: `5130 (NSK-1)` contains RM-495
XpressMusic firmware, and `6210 (NPE-3)` contains RM-367 Navigator firmware.
Neither is a DCT3 input.  This list records archive availability, not an
exhaustive assertion about every regional DCT3 product.

### Nokia 3330 NHM-6 v4.50

The first portability target is the 3330. Firmware.center provides service-format archive
`NHM-6 v.04.50 3330.rar` containing:

| archive member | size | sha256 |
|---|---:|---|
| `NHM6NX04.500` | 2,689,928 | `fac546ceaf7f56e536383730ec6cadc6519f9a11a08a3a9688f2439e77f3eca8` |
| `NHM6NX04.50E` | 787,296 | `318fcc3fa9d2a926e574fea89af9d8cda81796acb904f196636b886ef4e6ce9e` |
| `3330 virgin eeprom.pmm` | 65,608 | `83e58fe48153a7c1d60247ba7b80b72e05ddb8fd5443aa0d9ed976ec3ad88808` |

These are Wintesla record streams. `make normalize-3330` validates and removes
their nine-byte per-`0x2000` record headers, then combines the contiguous MCU
and PPM E regions. The generated files remain git-ignored.

The existing MAME declaration expects:

| file | size | CRC32 | SHA-1 |
|---|---:|---|---|
| `3330f450c.fls` | `0x350000` | `259313e7` | `88bcc39d9358fd8a8562fe3a0280f0ce82f5897f` |
| `3330 virgin eeprom 005f0000.fls` | `0x10000` | `23459c10` | `68481effb39d90a1639e8f261009c66e97d3e668` |

Only an exact MAME audit match establishes the canonical 3330 baseline. Differently sourced
service images remain useful RE inputs, but their results must be labelled with their own hashes.
The local PPM E result is declared separately as BIOS `450e` with SHA-1
`7e88caa4963c57ebbd4d919023e38103ff8b528a`; run it with `make smoke-3330e`.

### Nokia 3410 NHM-2 v5.46

Place `NHM2NX05.460`, `NHM2NX05.46E`, and `3410 virgin eeprom.pmm` in
`roms/3410-nhm2-v546/`. `make normalize-3410` reconstructs the MCU+PPM and PMM
regions, verifies their pinned hashes, and prepares the `noki3410` ROM set.
The v5.46 set is the default 3410 BIOS; the older local v5.06 MCU/PPM image is
retained as a separate, unprovisioned comparison BIOS.

### Nokia 5210 NSM-5 v5.40

Firmware.center's `NSM-5 v.05.40 5210.rar` contains Wintesla members
`nsm5_5.400`, `nsm5_5.40e`, and `5210 virgin eeprom.pmm`. The untouched archive
has SHA-256 `65fee7acd30bdb37f48d7e1be71a9126d9fbe6e91d478c08fce163e1c373d03e`.
The member SHA-256 values are respectively
`f4dd4cbaf5df59d0205fdda58fdba6296c96924b90a711bb99b06604c67635f5`,
`9927b72cc020e219d2e316f98060f50dfc803b89d8d281d9ce23f5d4bd7457b8`,
and `bbdee216e92011966681dc148daa144d0a8939bb518df5f2bb40d60459aceb84`.

Place them in `roms/5210-nsm5-v540/` and run `make normalize-5210`. The resulting
PPM E flash and PMM are exposed as BIOS `540e`; this is a labelled portability
input, not evidence that the existing compatibility profile matches NSM-5.

### Nokia 3310 NHM-5 v6.39 local spike

The bounded 3310 portability spike uses a local, already-combined 2 MiB flash
and its 192 KiB PMM tail as BIOS `639`:

| file | size | SHA-256 |
|---|---:|---|
| `3310f639e.fls` | `0x200000` | `975ec791205f026d647254ee772d7fa32691fa50c72a68eecdaff7c8a5921442` |
| `3310 v2 pmm.bin` | `0x30000` | `dcb2212579f2a2a7059ed85ef81174d337003566ce2f83f284f20bc70aef8bf4` |

This pair is a labelled portability input rather than the canonical 3310 MAME
set. Its PMM is BIOS-specific and must not be combined with the older declared
3310 images.

### Nokia 6110 NSE-3

The primary service manual specifies a 1 MiB Intel TE28F800 program flash and
an independent 8 KiB serial EEPROM. Internet Archive item
`Nokia_DCT3_firmwares` preserves original self-extracting package
`Nse-3_v4.06.exe`:

| source | Bytes | SHA-256 |
| --- | ---: | --- |
| `Nse-3_v4.06.exe` | 673,764 | `851c5c3df031055a5023665bb8ead3e3c69fa38fd40a0c04443c1511730f4cbb` |
| `NSE32514.060` MCU records | 782,176 | `efc34ed1b4420de6f466ab435f24db3b27631dbb5f7f482f4da5ac37abe8dda1` |
| `NSE32514.06B` PPM B records | 262,432 | `cac545fe9fbf737a93fade3ecddbdeb4eb0b1d0c8df52d6da79a6f13e51463b3` |

The archive's SHA-1 and MD5 agree with its public metadata. The two members are
Wintesla record streams. MCU covers `0x200000..0x2bebff`; PPM B covers
`0x2c0000..0x2fffff`. `make normalize-6110` validates every record and gap,
then writes the ignored 1 MiB image
`roms/noki6110/6110_nse3_v406_rom3_candidate.fls`:

| Bytes | CRC32 | SHA-1 | SHA-256 |
| ---: | --- | --- | --- |
| 1,048,576 | `78f6dce9` | `5025a6ac3b4a13714211fde903f27f92cbb7c9b6` | `aace812405bca224689ae707ea1a6174dbcf413bf62c88a944d96d298880ba60` |

The image identifies `V 4.06`, dated 16-01-98, NSE-3, PPM B. Contemporary
version tables distinguish `v5.48 ROM3` from `05.48 ROM4`; the unpadded v4.06
spelling is therefore a ROM3 candidate, not yet proof. The driver declares it
under that label but cannot promote booting until the matching F711604 internal
boot/DSP ROMs are acquired or independently dumped.

The same Internet Archive item also preserves three differently wrapped copies
of the final v5.48 Wintesla payload:

| source | Bytes | SHA-256 |
| --- | ---: | --- |
| `nse3_548.exe` | 2,371,584 | `01631387b7f587903d28053ef9dc1a9fb59ad6342e29cc766e1b06127dbc9f25` |
| `nse3_v5.48.exe` | 2,376,521 | `39b7304d2bde3da01073aaa9b631db04b98b91117ba90b3f5eb5b985560be20b` |
| `nse3_v548.exe` | 2,329,558 | `806270f50857039b5bcf49f7261a46e5424fb6c1bca1802b45aaf1c6bfc55218` |

Their extracted 36-file payload trees are byte-identical. The package's own
`nse-3.ini` names `nse3nx_5.480` as `ImageFile` and `nse3nx05.480` as
`Rom4ImageFile`; each market entry likewise pairs `nse3nx_5.48?` with an
explicit `Rom4PpmFile=NSE3NX05.48?`. This is direct package evidence for the
ROM3/ROM4 split rather than an inference from filename punctuation.

For language pack B, the source members and normalized images are:

| identity | Source members (bytes, SHA-256) | Normalized 1 MiB image (CRC32, SHA-1, SHA-256) |
| --- | --- | --- |
| v5.48 ROM3 | `nse3nx_5.480` (782,176, `bff2b8418bdb726acb1450e570bdb00ac3860f5d1375f4f080edb68ea4dfa822`); `nse3nx_5.48b` (262,432, `7d5ba47f879b9f57df3c6fe1847d128b329eed4a565d4273db3ddce1f3cd0395`) | `451cde56`, `5768841c9eb39c744f4fa04f0485e4f9ad4553b3`, `3ad47781485cb776910d30fa20d440a963eae90e847cfe24748b5c4ac2f8e6e3` |
| v5.48 ROM4 | `nse3nx05.480` (787,040, `5126588116fa441c0fe0586b4dd4866941e9512783dde69581c136a8a19b272a`); `nse3nx05.48b` (262,432, `e07eb7521711c001514eb6ae3868ed28f011b1fd98f72fb6648bdd8ce47bee7c`) | `83f67ad4`, `3bcc5c93ec247c63490e134196aab98a4e60c184`, `2adca0d661af2d8e7bed3e04d2941b6db9572a1eb10b2b1ebc545e33fbdd7c7f` |

`make normalize-6110-v548` validates every Wintesla record and produces both
ignored images. ROM3 embeds `V  5.48`, `08-09-99`, DSP software `40.3.617`
dated `14-Dec-98`, matching the independent ROM3 handset log. ROM4 embeds
`V 05.48`, `03-09-99`; it is kept separate and is not assigned ROM3's
bootstrap contract.

Do not pad a later image, reuse a 3210 EEPROM, or declare a 2 MiB flash device.
The hardware and staged acceptance requirements are in
`docs/6110_bringup.md`.
