# Firmware images (bring your own)

This repository contains **no firmware**. The Nokia 3210 flash image is Nokia's
copyrighted code and is **not redistributable**, so it is git-ignored and must be
supplied by you. Everything in this repo is original tooling, analysis, and
annotations that operate *on* such an image — it is useless to anyone who does not
already have a legitimately-obtained dump.

## Classic handset collection

The local, ignored collection now includes 131 product-coded service-package
archives (120 distinct SHA-1 payloads, 573,867,126 bytes) from
[Internet Archive's Nokia firmware collection](https://archive.org/details/Nokia_DCT3_firmwares).
They are stored under `roms/archive-dct3-packages/`. Each member's size and
SHA-1 match the source metadata; `manifest.json` records its source URL,
size, SHA-1 and independently computed SHA-256. `source_metadata.json`
preserves the source listing. Duplicate package aliases remain identifiable
by hash rather than being counted as distinct firmware revisions.

The selection covers NAM-2, NHM-2/3/5/6/9, NPE-3, NPM-5, NSE-1/2/3/5/6/8,
NSK-1/3, NSM-1/2/3/3D/4/5 and classic NSB/NSC regional package prefixes.
This includes multiple releases for comparison, not just the newest release.
Product-code selection is acquisition triage, not a verified hardware-family
assignment for every package. The source also contains CDMA and later-platform
software; it was not downloaded indiscriminately as DCT3. Installers have not
been executed. Some legacy self-extracting formats are not fully understood
by 7-Zip, so matching an archive hash does not establish successful extraction.

The separate ignored `roms/twister-dct3/` collection retains two selected
members of the [Twister service collection](https://archive.org/details/Twister_Mobile_Phone_Flashing_Box):
`nam1_511.exe` and `3350 virgin eeprom 005F0000.fls`. Its `manifest.json`
records exact ZIP member names, sizes, CRCs and independently computed SHA-256s.
Only bounded HTTP ranges were fetched; member CRCs were verified, not the
whole 22 GB archive hash. The source metadata and full firmware/support-DVD
member indexes are retained alongside them. Installers were not executed.
The NAM-1 installer matches the existing v5.11 MCU/PPM-E but supplies no
PMM record stream; see `tools/nam1_installer_audit.py`. The 3350 file's label
does not establish pristine factory data or matching-version provenance.
Neither acquisition is a new accepted handset input.

The same collection's `New.zip` contains a nested UFS suite installer. A
read-only InstallShield cabinet extraction retains five service-loader
binaries under `roms/research/mad2-flash-loaders/DCTxBB5_TIA/`, with source
chain in `provenance.json` and member hashes in `audit.json`.
`tools/mad2_flash_loader_audit.py` verifies those identities and the two
bootstrap files' self-checksum spans. They are service-tool inputs, not
resident MAD2 ROM dumps or newly accepted handset configuration. The Windows
installer was not executed; its complete 624-file cabinet listing is retained
under `roms/twister-dct3/` separately from firmware admission.

Six additional complete MAME firmware/PMM pairs are extracted in their
`roms/noki*/` directories. All twelve members' sizes and SHA-1s match the
current MAME ROM declarations, and all six ZIPs pass their internal CRC tests.
Source archives and per-member hashes are retained in
`roms/mame-dct3-archives/manifest.json`.

| Product | Firmware | Source archive SHA-256 |
| --- | --- | --- |
| [6210](https://www.planetemu.net/rom/mame-roms/noki6210) | v5.56 PPM C | `2768e54ceee1a2d39ddd51e8f0407af497fe084bd8c0c8665f4575c42910e3d7` |
| [6250](https://www.planetemu.net/rom/mame-roms/noki6250) | v5.03 PPM C | `50f88e6b9d31ea0f23dc338c7be0da29b3b9251bdd35de879ef5822870f87da2` |
| [7110](https://www.planetemu.net/rom/mame-roms/noki7110) | v5.01 PPM C | `41c27d8c46c60c71fd4d5007f89cf37fb99447bdc458b8b04382c8e0e5253363` |
| [8250](https://www.planetemu.net/rom/mame-roms/noki8250) | v5.02 PPM K | `85efd026a7f5c9f4e73a68d6c15952ecde9fcd228c6ac5ab777edae5680a7d0c` |
| [8850](https://www.planetemu.net/rom/mame-roms/noki8850) | v5.31 PPM C | `c05269b942ad821de82c6ddc5ce1f7e791d80b887ce8d7a42ecc3cfee49bdd47` |
| [8890](https://www.planetemu.net/rom/mame-roms/noki8890) | v12.20 PPM C | `ad2b5600a1193466a496942c15859a3d473438e7c5c69e9f01c90a0cc79d600b` |

The files labelled "virgin eeprom" in those sets are flash-backed PMM tails,
not external serial EEPROM or internal DSP ROM images. Their archival names
and matching MAME hashes do not independently establish factory authenticity.
No ROM5/ROM6 DSP mask dump was acquired, no missing MAD2 ROM is fabricated,
and acquisition alone does not promote any handset's runtime coverage.

### Nokia7110 intact public backups

The [Cosconor preserved full-backup directory](https://www.cosconor.fr/GSM/Nokia/Netmonitor/DCT-3/Nokia%207110/Flash%20Files/)
supplies additional inputs under `roms/research/nse5-cosconor/`:

| File | Bytes | SHA256 |
| --- | --- | --- |
| [FuBu7110-v501-PPM-D.fls](https://www.cosconor.fr/GSM/Nokia/Netmonitor/DCT-3/Nokia%207110/Flash%20Files/FuBu7110%20v5.01%20%28PPM%20D%29.fls) | 4194304 | `da259e776feea05d905399a644f3e4f9ed2862f591e8f3cd8dbc8d05884a5deb` |
| [My-7110-original-FuBu.zip](https://www.cosconor.fr/GSM/Nokia/Netmonitor/DCT-3/Nokia%207110/Flash%20Files/My%207110%20original%20FuBu.zip) | 2118257 | `4391f36226314808a6ffce4d38509e2879a51eff0839b75670b168b25e9bda77` |
| Archive member `My 7110 v5.00.fls` | 4194304 | `4e0660c14e8e335380f963ae200bf2404d50e826b15f50355a68c9da0a78aecd` |

The direct-download basename is normalized locally; file bytes are unchanged.
The ZIP has exactly one member, extracted without executing anything.
Research BIOS labels are separately normalized to `7110f501d_full.fls` and
`7110f500_full.fls`. The acceptance tools copy the byte-identical acquired
inputs under those labels into isolated run directories; collection files
and archive members retain their provenance names. New capture metadata
records the chosen label explicitly. Older retained captures remain readable
without rewriting their original files or manifests.
Both raw images include their own complete upper storage rather than a
transplanted PMM tail. Each has a write-journal EEPROM sector at CPU5fc000;
independent replay yields57/87 writes and distinct cache hashes. The entire
310000-byte v5.01 MCU prefix matches the existing PPM-C acquisition.
The source's version/full-backup labels do not prove original chip pairing
or integrity of every stored record. In particular, the v5.01 low record
fails its own decoded checksum predicate; no repair is supplied.
The source files are preserved intact and private identity bytes are not
copied into documentation or test fixtures.

`tools/nse5_fullbackup_check.py` pins all input/archive hashes, validates the
member, independently replays journals and supports isolated read-only native
trials. Research `nse5r4t` BIOSes `501d-full`/`500-full` load the whole
images; original default501 and normal `noki7110` remain unchanged.
Observed boundaries and reproduction commands are in `docs/7110_bringup.md`.

The acquired NSK-3 v5.61 package is additionally decoded into two own
MCU/PPM-A pairs under ignored `roms/noki6130/`. Reproduce and qualify with
`tools/nsk3_static_check.py`; `docs/6130_bringup.md` records member pairing,
normalized hashes and the bootstrap's distinct3/4 identity writes. No external
EEPROM or runtime support is implied by these extracted inputs.

## Nokia 5110i NSE-2 v5.53

`make verify-5110i-static` qualifies both acquired
`archive-dct3-packages/nse2_553.zip` and `nse2_553.exe` without executing
either installer. Both contain the exact same CPU-order raw members:

| Member | Bytes | SHA-256 |
| --- | --- | --- |
| `5110i MCU-05.53.fls` | 1,638,400 | `21084d1546648364ff9d5ee38176ddca31c7b0862e46d4e14dd15c1aca13d628` |
| `5110i PPM-05.53A.fls` | 458,752 | `a6502decf1a1e6dc3278530b745ce7088c865adcbf25c5a5a08df83d4e1020de` |

The unchanged combined `noki5110i/5110i_5.53A.fls` is 2,097,152 bytes,
SHA-256 `00ab7e062de9c89718eb81031528a646a79317ba0bad5a272a6d4d3b09c77781`.
`noki5110i/manifest.json` retains both archive hashes and static contracts.
No external EEPROM or DSP mask is supplied and no runtime support is implied.
See [the NSE-2 boundary](../docs/5110i_bringup.md).

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

### External EFR reference input

The optional `--encoder-homing-fixture` generates locally pinned
`homing_enc.inp`/`homing_enc.cod` from a 46-frame EHF/repeated-speech input.
The installation-verified reference encoder runs twice from fresh process
state; steady EHF and repeated post-reset speech must reproduce. Files remain
private research inputs. See
`docs/cobba_control_boundary.md#native-efr-encoder-homing`.

The optional `--homing-fixture` derives a canonical DHF from the unmodified
reference encoder's second EHF input frame. It generates a locally pinned
46-frame decoder conversation with repeated speech between DHF pairs;
two fresh interface/decoder sequences must agree. `homing.dec` and
`homing.out` remain private research fixtures. The native acceptance and
caller convention are documented in
`docs/cobba_control_boundary.md#native-efr-decoder-homing`.

The optional `--erasure-fixture` generates fixed active-speech BFI bursts
of 1, 3, 8 and 16 frames from the verified installation decoder input.
Payload and TAF are unchanged; two fresh decoder processes must reproduce
the locally pinned `erasure.dec` and `erasure.out`. These generated files
remain private research fixtures, not distributable MAME ROMs or published
conformance vectors. Native recovery currently fails at frame 9; see
`docs/cobba_control_boundary.md#native-efr-active-speech-erasures`.

The ETSI EFR ANSI-C reference and installation vectors are separate research
inputs, not MAME runtime code or handset ROMs. The locally acquired mirrored
archive is retained at
`roms/research/etsi-efr/original/ts_146053v080000p0.zip`, SHA-256
`54f4bcba0fbcc3fe1e42e0fb8de2c7f8bb3ac811cf08f1db9b6e40d38c91894d`.
Download provenance is [FreeCalypso's codec archive](https://www.freecalypso.org/pub/GSM/codecs/ts_146053v080000p0.zip);
[ETSI TS 46.053](https://www.etsi.org/deliver/etsi_ts/146000_146099/146053/09.00.00_60/ts_146053v090000p.pdf)
describes the reference distribution and installation checks. Direct ETSI
archive retrieval was unavailable; the mirror hash is a local content pin,
not a claimed publisher signature. Retain all original notices and keep source
and speech vectors out of Git and the MAME overlay.

`python3 tools/verify_etsi_efr_reference.py roms/research/etsi-efr/original/ts_146053v080000p0.zip`
builds an isolated, unmodified extraction and compares encoder, interface and
decoder output with the supplied installation vectors. It does not run the
DSP or establish native EFR speech. See `docs/cobba_control_boundary.md` for
the independent-reference acceptance boundary.

The optional `--dtx-fixture` generates a separate 518-frame quiet-background/speech
conversation after installation verification, with DTX enabled. Two fresh
reference process sequences must reproduce the locally pinned `dtx.*` outputs,
including nonzero comfort-noise output.
These are generated research vectors, not published conformance files or
handset ROMs. `--fixture-dir` exports them only into ignored private fixture
storage; the native acceptance scope is documented in
`docs/cobba_control_boundary.md#native-efr-dtx-conversation`.

### Original ISO MPEG-2 AAC research input

`roms/research/mpeg2-aac-conformance/` retains original `al09_44.adts` and
its 24-bit mono `al09_44.wav` reference from the
[ISO/IEC 13818-4:2004 AAC suite](https://standards.iso.org/ittf/PubliclyAvailableStandards/ISO_IEC_13818-4_2004_Conformance_Testing/AAC/).
Both files match the separately acquired publisher MD5 files; the local
`provenance.json` records URLs, byte sizes and SHA-256. The original README
classifies compressed ADTS and reference waveforms as normative inputs.
Keep these assets unmodified, ignored and subject to ISO's original terms;
they are not project-licensed MAME ROMs. Acquisition and native packet
acceptance do not establish audio conformance or handset playback. See
`docs/aac_validation.md#original-mpeg-2-al09-input` for exact identities and
the still-open alignment/end-buffer contract.

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
`roms/noki3610/3610f511e.fls`. The `0x350000`-byte result has SHA-1
`429bc32afe0a554887ba7539cf9ba3c9a67e7043` and SHA-256
`bcb22f093746b14cd55503abff72f7e3c043fc226e0bcc0b6972b03d9d635c89`.
The other PPM members remain equally valid source material; E is not asserted
to have a privileged hardware role.

The normalized image is declared as the fail-closed `noki3610` bring-up
machine using the established later-MAD2 address map, 32-Mbit flash extent,
product display/GENSIO contracts, the ordinary later-MAD2 SIMI controller and
nominal BLB-2 CCONT inputs. No sibling product storage image is enabled.

Public NAM-1 flash maps place the active PMM at `0x5f0000`, but it is absent
from this MCU/PPM archive. Firmware validates at least two product-calibration
records before resuming its second application-task batch. A diagnostic load
of NHM-6 PMM data made the common `0x120`-byte checksum pass but failed the
second product-specific calibration and reset; it is therefore not retained.
A matching NAM-1 PMM dump is required to advance this profile faithfully.

The representative image yields 670 direct MAD2 accesses from 274 literal
seeds. Of those, 78 address PUP, 30 keypad GPIO and 29 UIF; the conservative
scan recovers no direct GENSIO SELECT access. The byte-lane-correct CCONT scan
finds the same 18-entry descriptor vocabulary at `0x00489b74` (literal
references at `0x003d7a70` and `0x003ec8f8`). Subsequent runtime evidence
established the product serial/control and display paths; see
`docs/3610_bringup.md` for the current PMM boundary.

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

### Nokia 5190 NSB-1 v6.71

`archive-dct3-packages/5190e671.zip` supplies own MCU and PPM A/B service
streams, not a full handset dump. `tools/nsb1_package_check.py` creates
`noki5190/5190f671a.fls` from qualified own records, with the unsupplied
`2d2d00..2dffff` range explicitly erased. The normalized A SHA-1 is
`27b8f0cb8640e5b423960b206e9aee065f32d726`. No provisioning or resident mask
is supplied. Source hashes, exact spans and reproduction are authoritative
in `docs/5190_bringup.md`. The separate MCU research configuration validates
the own upload and supervisor boundary, not a graphical phone boot.

### Nokia 3350 NHM-9 v5.30

The retained `archive-dct3-packages/nhm9_05.30_install.EXE` (SHA-256
`e9686c1c5171157b4308a0bdf96008ac040e866e6f5c31ca1ed993bac4e34acd`)
contains one MCU and six matching PPM record streams, but no PMM stream.
`nhm9_530.exe` has the same hash. The original acquisition URL and hashes
are retained in that directory's `manifest.json`.

```sh
.venv/bin/python tools/nhm9_package_check.py \
  roms/archive-dct3-packages/nhm9_05.30_install.EXE \
  --ppm A --output roms/noki3350/3350f530a.fls
```

The normalized CPU-order file is `0x330000` bytes, SHA-1
`3d0dce9c098bdd619c6f3684f739796f64b66169`, covering `0x200000..0x52ffff`.
No erased or donor tail is appended. `make normalize-3350` prepares the same
input for the fail-closed `noki3350` MCU fixture; this is not a phone-boot
result. See `docs/3350_bringup.md` for its own-upload/first-wait acceptance.
Own 247-word verifier-format code, the 638-word loader and the 104-word
bootstrap fragment are additionally extracted into `roms/research/nhm9/` by
`tools/extract_nsm3_verifier.py --product 3350` (and `--loader`/`--fragment`).
Their source descriptors and hashes are in the bring-up contract. The fragment
matches the acquired 8250 fragment, but this does not supply a complete ROM6
mask or establish native execution. Nokia's combined product/system manuals
are retained under `reference-docs/nhm9/` with hashes in that same contract.

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

### Nokia 8210 NSM-3 v5.31

The original [Nokia service package](https://archive.org/download/Nokia_DCT3_firmwares/nsm3_531.exe)
contains MCU `nsm3_5.310` and twelve alternative PPM files. Its SHA-1
`6503a59d4b06ccfa949cbadf08b924ab662d668a` matches the archive metadata.
Extract it without executing the installer into
`roms/8210-nsm3-v531/service-package/`. The package's `nsm-3.ini` explicitly
assigns the same MCU file to both ROM5 and ROM6 hardware.

`make normalize-8210` reconstructs PPM C; its SHA-1
`c1a0fe95cedb89a92b19654208cc4855e1a4988e` exactly matches MAME's canonical
`8210_5.31ppm_c.fls` declaration. MCU and PPM-C source SHA-256 values are
`b6bb4a7f7650c89db4c6c9ae350c21567c105d6afd8eb6ccf048f99b289d4861` and
`b913766141d44b98f5dbdcbdde7ed259f3e253cb882ca370c7289f03e6264c40`.

The separately acquired [canonical MAME set](https://www.planetemu.net/rom/mame-roms/noki8210)
contains that same flash and `8210 virgin eeprom 003d0000.fls`.
The latter is a 196,608-byte flash-backed PMM tail, not a physical serial
EEPROM dump: SHA-1 `1c01ad3948ff9919890498a84f31052369d93e1d`, SHA-256
`31f51bcd69e183f23c39136574bd6a44864eb2ba1939417b9d848a3e0639ec59`.
Place it under `roms/noki8210/`; do not substitute another product's PMM.
The downloaded set archive has SHA-256
`047076cea1c19e3103a2dfd560b028ebe8fc8eb5a5d0193ca33081c5a6424eda`.

The alternative `nsm3_531.zip` service archive matches its published SHA-1
`142c91a4edf4aee524f11fcee78536d114f8b179`, but its PPM-H member fails ZIP CRC
validation. Use the clean self-extracting package above for normalization;
do not treat the ZIP's damaged member as firmware evidence.

### Nokia 6150 NSM-1 v5.23

The acquired original package `archive-dct3-packages/nsm1_523.exe` has
SHA-256 `69b882a55bbf8ed5fb81f7e52bd950ec631d619a0e141084d766a98860627904`.
Its own `nsm-1.ini` selects `nsm1ny_5.230` and, for EURO_C,
`nsm1ny_5.23c`; its declared erase range is `0x200000..0x3fffff`.
The structured Wintesla decoder validates every record: MCU covers
`0x200000..0x2e0eff`, PPM C covers `0x360000..0x3fffff`, and the
intervening gap is filled with erased bytes, not donor firmware.

Reproduce the ignored normalized input after extracting those members:

```sh
.venv/bin/python tools/extract_dct3_wintesla.py \
  --mcu roms/research/nsm1-v523/nsm1ny_5.230 \
  --ppm roms/research/nsm1-v523/nsm1ny_5.23c \
  --flash-output roms/research/nsm1-v523/6150-v523-ppm-c.fls \
  --expect-flash-sha1 ea877d2ec204d0771b77fba414575407ccbc99ad
```

The resulting 2,097,152-byte image has SHA-256
`8180cbf7ed53f33551ca49f308dd01b4dc48e4fed4d18aae3ee320ae107f922f`.
This establishes a usable flash input, not an executable product profile.
Matching EEPROM contents, resident DSP compatibility and product-specific
hardware configuration remain to be established; no sibling provisioning
is accepted by this normalization.

The [original NokiX scripts archive, 2011.07.24](https://sourceforge.net/projects/nokix/files/NokiX%20scripts/2011.07.24/NokiX-scripts.zip/download)
was acquired as ignored `research/nsm1-v523/NokiX-scripts-2011.07.24.zip`
(876,685 bytes; SHA-256
`412244a6e03f803bacff4096216355f7f88d13e20d36987158e54fe92e298154`).
Its data member `NokiX/scripts/scripts/misc/repair_external_eeprom/nsm-1.bin`
is 16,384 bytes, SHA-1 `1035ee33849563abbeafe0889d5b5c695a817fb5`,
SHA-256 `95d3326ab9bbcdb31838696c552f56295aac9b234cad53813aff7af9e0a73cbd`.
This original NSM-1 repair template is retained unchanged, not treated as a
factory handset dump. `verify-6150-static` reads the pinned ZIP member directly
and audits its own-image record layout; no NokiX script is executed and no
canonical EEPROM checksum or setting is modified. Boot compatibility remains unproved.
The separately generated `research/nsm1-v523/nsm1-security-checksum-valid.bin`
fixture corrects only bytes `0x03d2..0x03d3`, from `0x3124` to `0x034d`,
using the own firmware's decoded identity and checksum rule. It has SHA-1
`07335e492f72ba9da9c3436890c9eb4af67fd9a9`; original identity, settings and
calibration are preserved. This derived fixture is explicitly not a factory
handset dump. The static proof and remaining runtime boundary are in
`docs/6150_bringup.md`.

The acquired NSM-1 installers `nsm-1_v5.17.exe`, `nsm-1_v5.20.exe`,
`nsm1_522.exe`, `nsm-1_v5.23up.exe` and `nsm1_523.exe` were inspected
as ZIP archives, without execution. Their member inventories contain
firmware and service/package configuration, but no standalone handset
EEPROM dump. The `Nsm1.PDF` member of `nsm1_522.exe` is a one-page
language-package table, not a hardware manual or provisioning specification.
It is retained as ignored `research/nsm1-v523/reference/Nsm1.PDF`, SHA-256
`d0e6587b27febec941939537f9415aaa808d21d7c28f7c66083ddee6b31cde1e`.
The repair template remains the only acquired NSM-1 EEPROM artifact;
its native record-validation failure is documented separately and must
not be relabeled as a compatible factory dump.

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

## NSE-6 system manual reference

`reference-docs/nse6/03SYS-preview.html` is the downloaded eServiceInfo text
preview of Nokia's NSE-6 System Module manual, Original08/98:
<https://www.eserviceinfo.com/preview_html.php?fileid=5448&previewid=3000>.
SHA-256: `91350eb0b42b650be11c43cb6be82596faae46a901af4fe1b6f2f5db6fbb507d`.
This is an HTML/OCR reference, not a downloaded full PDF or schematic. Its
accessory-interface description is product-specific corroboration, not a DSP
port34/35/36 register specification. No ready-bit semantics are inferred.

## NSB-7 v5.22 package

The already acquired `archive-dct3-packages/nsb7_5.22.exe` contains its own
MCU and PPM-A record streams, but no PMM. `tools/nsb7_package_check.py`
qualifies the installer, restore manifest, record addresses and exact decoded
hashes before writing `noki8290/8290f522a.fls` (CPU order, `0x1d0000` bytes,
SHA1 `4dacc50f57fe295168a53d3e5855a81acf35ab96`). Full inputs and
reproduction are in `docs/8290_bringup.md`. This is not boot acceptance.

## NSK RF documentation references

Indexed primary documents used by `docs/5130_bringup.md`; the PDF binaries
were not acquired, so no local file identity or hash is asserted:

- [NSK-3 System Module, issue 3, 01/2001](https://electronicsandbooks.com/edt/manual/Hardware/N/Nokia/Phone/6130/03sys%20%5B106%5D.pdf)
- [NSK-3 UR9U block diagrams](https://electronicsandbooks.com/edt/manual/Hardware/N/Nokia/Phone/6130/sysa3u%20%5B26%5D.pdf)
- [NSE/K-1/3 Repair Hints, version 2.0, 13 June 2001](https://files.elektroda.pl/44719,nokia%2B5110_5130_6110_6130_v20.html)

## Acquired complete service manuals

The public Mobile Collectors download forms supplied these single-PDF ZIP
archives. Both original ZIP and extracted PDF are retained under the listed
`research/` directory; binaries remain outside source control. The POST uses
the ordinary page's session token and `legal_confirmation=1`, without an
account. Archive integrity and PDF page counts were checked locally.

| Product / source | Directory and basename | ZIP SHA256 | PDF SHA256 |
| --- | --- | --- | --- |
| [8890 NSB-6](https://mobilecollectors.net/documents/1546/download), 303 pages | `research/nsb6/nokia_8890_nsb-6_` | `447c049b702bbecf2a00291219ad64fc585dc17ef1f2cbbd7da5342923efcf4f` | `d725449db85e04d82f2f9e693428a8ea327b0228bbd6975cba40578841f41996` |
| [6210 NPE-3](https://mobilecollectors.net/documents/1552/download), 240 pages | `research/npe3/nokia_6210_npe-3_` | `70fb153cbc94eb0e5047933ce89f69c526365489acf685ac488bcc6da71ee0d1` | `34b5bee7b3d66ed8f5ddc56522750bf84c05753174396f0f1ffd8f6d899e905d` |
| [6250 NHM-3](https://mobilecollectors.net/documents/1551/download), 245 pages | `research/nhm3/nokia_6250_nhm-3_` | `ce33517f4f7d166e512657d38357ba401ff91fb71190be7c0c2682ca09246865` | `e1012c4dd46cf415db3ee7666880be29eb27c5ed8ae7c8de0c49d39c1a0819e7` |

NSB-6 System Module printed page25, PDF page39, contains the acquired and
visually reviewed PCM timing diagram; its implementation contract is in
`docs/8xxx_bringup.md`. The NPE-3/NHM-3 bundles are independently retained
references, not evidence for importing NSB-6 clocks or framing into those
products. Some PDF fonts have broken text mappings; visual review remains
necessary rather than treating a negative text search as document absence.

The independently acquired [8290 NSB-7 service archive](https://mobilecollectors.net/documents/22198/download)
contains two PDFs, retained outside source control in `research/nsb7/`:

| File | SHA256 |
| --- | --- |
| `nokia_8290_nsb7_service.zip` | `993dbbedbef06d8e4790972123471627fd569adca7182a022df70ea963eaa809` |
| `nsb7-schematics.pdf` (14 pages) | `c266fea3b7106a55a469eb0eccf0249d71aed2e4c69bc6013eb7de5f0daeba2e` |
| `nsb7-service-manual.pdf` (308 pages) | `eee14565ada962e5458cb8227622bdd6f64ffe89a5a15ff38fbccd95fe428e6d` |

The visually reviewed System Module page27 (PDF page41) supplies NSB-7's
own PCM waveform; see `docs/8290_bringup.md`. It is independent evidence,
not an imported NSB-6 timing assumption.

## NSM-4 service regions (Nokia 8855)

`tools/nsm4_archive.py` qualifies the already acquired
`archive-dct3-packages/nsm4_518.zip` (SHA256
`8ef5e4bf68b0b190ca31a51790a80fe13e3fc2aee0dd4fed45975149d9c5807d`).
The normalized regions and per-member hashes are retained outside source control
under `noki8855/v518/manifest.json`. Reproduce into a fresh directory with:

```sh
.venv/bin/python tools/nsm4_archive.py roms/archive-dct3-packages/nsm4_518.zip --output roms/noki8855/v518
```

The MCU occupies `0x200000..0x490000`; six alternative PPM packs P through U
each occupy `0x490000..0x580000` (exclusive ends). These are contiguous service
regions, **not a full-flash dump**: no PMM tail or resident DSP mask is supplied.
No pack language, ASIC configuration or handset support follows from extraction.
NSM-4's Nokia 8855 identity is independently listed in the
[official NTC approval catalogue](https://ntc.gov.ph/wp-content/uploads/2024/TYPE%20APPROVED/CPE-1999-2021-Updated.pdf).
The extractor pins the archive, checks its exact member set and decoded extents,
and uses the shared strict Wintesla record decoder; unknown record-header
checksum fields are not interpreted.

## NSB-3 service regions (Nokia 6190)

The already acquired `archive-dct3-packages/nsb3_613.exe` is a ZIP
self-extractor (SHA256
`d2e684dbef4dc74fd0233ed96c3a3c1f1bdbd9b0615f338aff28e18a20c63d85`).
`tools/nsb3_archive.py` reads its ZIP structure without running the executable.
It pins the source, exact member set, normalized hashes and record extents.
Normalized regions and per-member hashes are retained outside source control
in `noki6190/v613/manifest.json`. Reproduce into a fresh directory:

```sh
.venv/bin/python tools/nsb3_archive.py roms/archive-dct3-packages/nsb3_613.exe --output roms/noki6190/v613
```

MCU `nsb3___6.130` occupies `0x200000..0x2d6d00`; PPM A/B/C each occupy
`0x2e0000..0x300000` (exclusive ends). The intervening `0x9300` bytes are
not supplied and are not synthesized by region extraction. Optional `--image`
fills that gap with `ff` under the own INI's explicit whole-image erase policy;
it does not source those bytes from another product. No external EEPROM or
resident DSP mask is supplied. The own MCU marker says `V  6.13`,
`28-04-00`, `NSB-3`; PPM headers say `27-04-00`. The matched installer INI
names these exact files and explicitly lists ROM4 file selections. Neither
that configuration nor a product label proves mask compatibility or runtime
support. The source URL/hash remains in the collection acquisition manifest.

## Nokia 6190 owner manual

The original Nokia 6190 owner's manual (NSB-3NX, issue 5) is independently
retained as `reference-docs/nsb3/nokia-6190-owner-manual.pdf`, SHA256
`a0a5a8e039bc3cb6548553b4bcf546aee2ef685ee51a8f571b65a1e46c3ce144`.
Source: [manual PDF](https://www.instructionsmanuals.com/sites/default/files/2019-05/Nokia-6190-en.pdf).
Its Technical Information gives 3.6 V operating voltage; this is a nominal
input reference, not a DSP mask or fitted CCONT calibration measurement.

## External implementation reference archive

The already-catalogued reference revision
`2b4c10ffd6b22a2d021c7f515f27ceb2fb6c78cd` is retained as
`research/djr-reference/nokia-dct3-emulator-2b4c10ffd6b22a2d021c7f515f27ceb2fb6c78cd.tar.gz`,
SHA256 `6e8badb69d91480e7e9920ddf19159227eca1019c888910c98222bf3ff9cf731`.
Source: [pinned repository archive](https://codeload.github.com/djr-747/nokia-dct3-emulator/tar.gz/2b4c10ffd6b22a2d021c7f515f27ceb2fb6c78cd).
This is implementation reference material, not a newly acquired Nokia mask
ROM or matched PMM. Its admission/dependency limits live in
`docs/djr_dsp_integration.md`; sibling HLE success is not a native-ROM oracle.
