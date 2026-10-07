# Nokia 5510 NPM-5 bring-up

## Current boundary

The acquired v3.53 PPM C service package is normalized and statically
validated. The `nmp5stage` research fixture executes its own verifier and
loaders to absent mask address `2c75`. A separate `nmp5hle` fixture reaches
the graphical `CONTACT SERVICE` diagnostic frame. No idle boot, input, SIM, registration,
call or SMS acceptance is claimed. The package supplies MCU and PPM record
streams, not a matching handset PMM or an internal DSP mask image.

The independent MU4 `mu4nand` fixture executes original erased-media
provisioning, receives all seven original R060 segments, reads back six
complete files, and verifies all DMA-loaded `MCUSI16` destinations. Original
`3538` then reaches the loaded entry and its page-2 common-window branch.
The partial McBSP1 transmitter carries six original firmware control words.
The default acceptance profile stops at serial setup. The separate `stream`
profile uses those controls to activate a partial AIC23 master-clock source,
then observes one 128-word McBSP0 TX block and DMA channel-3 completion.
The codec's digital DIN decoder verifies those transmitted words. Explicit
already-converted stereo fixture samples also complete the original 64-frame
RX DMA block. The `sustain` profile checks eight reloaded blocks in each
direction; analog conversion and longer application operation are not validated.
DMA/McBSP and codec digital interfaces have isolated conformance
and save/replay tests.
The `startup` profile passes its 20-second window with zero illegal
instructions and an active original streaming consumer. It observes 1,739
consumer entries/notification reads and 1,740 clears, but mode zero skips
buffer processing. A separate framed
command fixture reaches the original serial parser and queued acknowledgement.
The `wireack` fixture completes a native status-query transaction using the
shared McBSP2 TX model, a bench clock and register-level RX input; `pins`
completes the same transaction with device-owned pin-level RX too. The
`replay` profile also verifies native continuation across a mid-byte checkpoint.
The `measure` profile sends original selector `40` through serial pins and
verifies six stereo sample-energy blocks against independent arithmetic from
the live input reads, followed by an acknowledged response and mode-zero
cleanup. The same profile independently verifies generated tone buffers,
including phase wraparound; analog audio and music decoding remain unvalidated.
The `reset` diagnostic verifies mid-byte controller reset and retained-file
reload, but the resident command service does not resume in its bounded
window. It is not a successful reboot acceptance profile.
The paired `check-mu4-retained-original` test saves the reset diagnostic's
stalled-endpoint NAND, then a fresh `retained` process mounts the identical
image without erasure or a runtime snapshot and completes native status
service. Persistent NAND contents alone do not explain the reset stall.
The `bootstrap` profile loads the complete original `aa55` upload and
executes its uploaded `ff80` reset-vector prelude, original context
initializer, directory-selected resident loader and resident startup.
The `bootstatus` variant completes the same pin-level status transaction
after this full startup; `bootreplay` verifies its mid-byte save-state
continuation across all 1,039 registered emulation-state items. The
`bootmeasure` variant verifies the original sample-energy and tone-buffer
arithmetic after the same complete startup. The `bootreset` variant repeats original startup
after a mid-byte soft reset and completes that transaction again without
clearing firmware RAM from the supervisor. Its stream volume is not
validated music output.
The open boundary is physical reset/board attachment, accepted media and
remaining processing command/data semantics, and independently verified output,
not a missing worker activation. No full native boot or music decoding is
claimed. Routine streaming profiles compare the initial 1,024 DIN words;
`bootmeasure` continuously compares transmitter words with codec DIN
throughout its complete observation window, including nonzero output.
This is isolated music-DSP execution, not a baseband unlock or full MU4 boot.

Nokia's [NPM-5 service manual](https://www.manualslib.com/manual/1166046/Nokia-5510-Npm-5.html)
identifies separate MA4 and MU4 assemblies, including UI-module keypad,
display, USB and music functions. These are product differences to recover,
not permission to borrow another handset's keypad or UI completion events.

## Reproducible input normalization

`make verify-5510-package` reads the ignored acquired
`roms/archive-dct3-packages/NPM5_353_mcu.exe` as an embedded ZIP; it never
executes the installer. Package SHA-256:
`4f13d4aab02e970a86e83c15b903aed18590584dc5c383ed126cf632763d8d60`.
The source manifest records its acquisition URL and independent hashes.

Only the pinned MCU, PPM C and product INI members are read. ZIP CRCs,
member extents, complete record grammar and derived image hash are checked.
Outputs are written under ignored `roms/5510-npm5-v353/`; different existing
artifacts are not overwritten.

| Region | Address | Decoded bytes |
| --- | --- | ---: |
| MCU `npm5nx03.530` | `200000` | `24da00` |
| Unprogrammed gap | `44da00..48ffff` | `42600`, filled `ff` |
| PPM C `npm5nx03.53c` | `490000` | `c0000` |

The normalized `5510f353c.fls` is `350000` bytes, SHA-1
`1c0217fb2b2fe350c455b3b1d71b3f920eeed8ec`, SHA-256
`a320d808229adf1e471446547d3ec4643d597a1f67f4b0975cc9beada7c3d15a`.
The gap fill represents bytes absent from the programming streams; it is not
a recovered factory flash/PMM image. PPM identifies `V 03.53`, `15-04-02`,
`NPM-5`, language pack C. No identity, calibration or security record is
borrowed or generated by this step.

## Static upload contract

The gate pins four complete descriptors and their following payloads:

| Descriptor | Halfword fields | Payload SHA-1 |
| --- | --- | --- |
| `3e3304` | `fd00 ff80 027e 0500 0078 0000` | `dc3c2a37913e07a2962d09f6a17cde676a04dd76` |
| `3e9240` | `ff80 ff80 0068 0200 008c 0000` | `440bf49f1eba4cadb12f7f7581c992b0025807d6` |
| `3e936c` | `0a00 1000 0275 0200 03e8 0000` | `ab334f30a6c564ba0d60a1adb64e95f22620111a` |
| `3ea428` | `0f00 0000 00df 0f00 00dc 0000` | `6646da3c5be9c70deda7e0b5b9f257d5d2ace815` |

These are statically selected uploads, not executed ownership proofs.
The 104-word fragment matches another acquired ROM6 fragment byte-for-byte;
the 629-word loader and 223-word verifier have their own hashes. Fragment
equality does not identify the fitted mask ROM or a final DSP verdict.

### Startup selection and verifier consumer

The startup interpreter at `20018a` reads the copy table at `3b0430`:
big-endian 32-bit size and destination, payload, then four-byte alignment.
A zero size terminates it. The tested decoder covers all 2,090 records,
26,487 payload bytes, through terminator end `3bb8d8`. Destinations include
MMIO as well as RAM; this is a static analysis, not an execution of writes.

The table initializes descriptor pointers at `1237c0`, `1237d4` and
`1237f0` to `3e9240`, `3e936c` and `3ea428`, respectively. The MCU verifier
consumer at `319032` loads the last pointer at `31903e/319040`, copies its
payload to DSP program address `0f00`, and initializes shared control words:
`087b=0100`, `087c=0300`, `087d=0001`, `087e=a800`, `087f=0001`,
`0880=0001`, `0881=0200`.

It releases CTSI register `02` bit 0 at `319096`. Starting at flash
`200040`, it copies one halfword every 32 bytes into shared buffer `10200`:
211 blocks of 512 samples, then 510 samples and two `ffff` terminators.
Ownership alternates through shared addresses `100fe/10100`, waiting for
the DSP to return each buffer. The final wait at `31911a..319120` requires
shared word 1 to change from `ffff`; result words 0/1 are saved at
`12376a/12376c` before CTSI bit
0 is cleared again at `319136`. This is 212 ownership transfers, not an
inherited sibling-product count. No success value is supplied by the tool.

Startup calls the verifier directly at `37be7a`. The subsequent initializer
at `31916c` clears the DSP window and ring pointers, then loads the initial
loader pointer at `319284` through literal `31931c` and RAM `123784`.
The copy table initializes that pointer to `3e3304`: a 638-word upload to
MCU shared address `10a00` (DSP data `0d00`). Its 638 words span 512
data words followed by the 126-word loader at `0f00`. Its first five descriptor fields are published
to the shared control cells; CTSI bit 0 is released at `3192ce`.
This is not the separate 629-word descriptor at `3e936c`; its later
selection and loader completion remain to be mapped.

The package gate verifies initializer coverage, all four initialized
pointers, both consumer-code hashes, dependent literal pools and the direct
startup call. `noki5510_bootstrap_check.py` separately checks the executed
own verifier publication, MCU consumption and loader boundary.

## Executed bootstrap boundary

`nmp5stage` uses only normalized NPM-5 flash, erased nonvolatile state and
explicitly unavailable NPM-5 mask regions. It does not import the ROM4 mask
dump. Own ADC routine `3acc98` establishes GENSIO data/control/status at
`2c/2d/6d`, receiving at `6c`; the nominal full-scale selector-2 input passes
the startup threshold. Electrical calibration remains unvalidated.

CTSI bit 0 controls staged execution. The fixture's `0d` running readback
preserves the written setup bits without asserting ready bit 4; it is an
instrument setting, not the general NPM-5 silicon status contract.
No MU4 keys are mapped and no runtime radio/service peer is enabled.

The fresh eight-second run observes verifier result `0000/0006` consumed
by the MCU, initial loader selection `3e3304`, and exact 629-word second
loader verification at `0a00`. At `2c75` the DSP is suspended with transport
ownership retained: no missing opcode, return or runtime completion is
fabricated. Version 6 comes from the own firmware fragment, not a fitted
mask identity measurement. The framebuffer remains blank.

Reproduce after `make build` by placing the normalized image in
`roms/nmp5stage/5510f353c.fls`, launching `nmp5stage` from a fresh working
directory with `-noreadconfig -debug -debugger none -autoboot_delay 0
-autoboot_script <absolute path>/tools/noki5510_bootstrap_observe.lua
-seconds_to_run 8 -video none -sound none -nothrottle -log`, and supplying
an absolute `-rompath`. Validate the resulting `error.log` with
`python3 tools/noki5510_bootstrap_check.py <log>`.

## Hybrid service frontier

`nmp5hle` explicitly substitutes transport HLE at missing mask `2c75` after
both own native loaders. It enables only request-derived service discovery
and the compact self-test completion. Application registration, radio,
identity/record replies and SIM remain disabled; no donor PMM is supplied.

Own TX type `05` carries `1eff00d000030101e000`. Common discovery responds
through RX type `8e`; firmware acknowledges with
`1e0200d0000305014100`. It then publishes identity/record-shaped type-`70`
requests followed by compact `0d00`. Own selector `24c9d2..24c9e6`
dispatches command `0d` to `24ca36`, gated by `13fe5d` bit 2; fault bits
0/1 are consumed at `24ca6a..24ca96`. Runtime observes `0d/00`, flag `8c`
at that consumer and a later `0a09` request. No reply to the latter is
guessed. The eight-second frame is `CONTACT SERVICE`, not idle.

Run the same fresh-directory command using `nmp5hle` and `-verbose`;
validate with `noki5510_bootstrap_check.py --runtime <error.log>`.
The inherited 84x48 display geometry is still a research projection; the
observed frame does not validate MU4 wiring or all display commands.

### Decoded product-state failures

The read-only self-test audit identifies two failed logical-NV checks, not
an unhandled DSP self-test fault. Reader `3ac638` reaches `2fd578`, which
copies from logical cache `100044+offset`, bounded to `3800` bytes.
The fresh runtime snapshot is erased in the critical regions.

| Self-test record | Own check | Observed erased result |
| --- | --- | --- |
| `13fbf2`, code `12` at `24c7dc` | `3510ba -> 2a058c`: sum16 of `0000..011b` compared with big-endian u32 at `011c` | `1ae4 != ffffffff` |
| `13fbec`, code `0c` at `24c858` | `24c612`: sum16 of `0120..0255`, excluding `0154..0155`, compared with u16 at `0256`; sum OR u16 at `0170` must also be nonzero | `32cc != ffff` |

`36a1b0` is an additive byte sum, not CRC32. `33689c` supplies the excluded
`0154` word. The latter failure explicitly clears status flag bit 6 at
`24c862`. After compact DSP completion, fault records `13fbef..13fbf1`
are zero while the two NV failures remain. This does not prove every other
self-test or the absent PMM's identity/provisioning contract.

`noki5510_selftest_observe.lua` records bounded writes and dumps the logical
cache after four seconds, without changing firmware or MMIO. Assess with
`noki5510_nv_check.py <npm5_nv_cache.bin> --erased-frontier-log <error.log>`;
successful audit means the negative control reproduced, not valid NV.

The decoded type-`74` command-`0a` handler at `24cab2` formats message byte
11 as an ASCII digit and passes it to `33629e`. It is not a self-test-ready
completion path. Therefore the unanswered `0a09` request alone is not
evidence of the graphical boot dependency. No guessed response is added.

The four acquired NPM-5 installers contain MCU/PPM and service assets, but
no matching handset PMM is established.

### Flash-to-cache replay

Own initializer `2fd8d0` examines two descriptors at `11f41c`, selecting
flash sectors `5e0000/5f0000`. Their first six bytes must match
`f0f0fff80001`, and the version word at header offset `18` must equal 3.
Replay `2fcc5c` starts at offset `20`: the high six header bits encode a
short length (zero selects the following extended u16 length), then a u16
logical destination and even-padded bytes. `ffff` or header bit 9 stops
the stream. Deletion effects remain outside the write-only decoder.

The fresh firmware creates a version-3 journal, selects index 0 and returns
accepted `01`, flags `00`, status `e111`. Independent replay of the saved
64 KiB sector yields ten writes, stop offset `38ce`, and exactly matches
all `3800` live cache bytes. The critical NV records remain erased despite
successful storage initialization. This excludes a flash/cache mapping
failure for this observed boot; it does not establish every flash failure,
sector rollover, deletion or persistent-product-state contract.

Run `noki5510_storage_observe.lua` from a fresh working directory, then
extend the NV audit command with `--flash <nvram/nmp5hle/flash>`. The shared
journal decoder retains NSE-5 version-1/8 KiB defaults; NPM-5 explicitly
selects version 3, a 64 KiB sector and a `3800`-byte cache. No records are
generated or borrowed by the checker.

## Input consumer boundary

### Physical MU4 ownership

Nokia's [MU4 technical description](https://www.eserviceinfo.com/preview_html.php?fileid=26142&previewid=12780)
assigns keyboard scanning to a separate controller and identifies FBUS as
the control-message link to the phone. The LCD connects directly to the
phone; it is not rendered by the keyboard controller. MU4 also manages
accessory detection and its music subsystem. This rules out wiring the
QWERTY keys directly into the ordinary MAD2 matrix model.

The [Nokia MU4 schematic, version 1.0](https://www.s-manuals.com/manuals/phone/nokia/nokia_5510_npm-5_schematics.pdf),
MCU sheet 3/11, identifies U301 as an MSP430F135. Its keyboard nets connect
to U301; FBUS RX/TX, MBUS and PURX cross the 36-pin J301 connector. The
acquired schematic is retained locally, ignored, at
`roms/reference-docs/npm5/nokia_5510_npm-5_schematics.pdf`, SHA-256
`e642943045b2f1ae8dfd6cf0ebee3a0c0d7cb7026de4543c7b48bd8da97f3d58`.
This source does not provide the controller's firmware or packet grammar.
The primary User Interface Module manual, pages 10/11 (connector table 2),
and MU4 MCU sheet 3 identify these MA4/MU4 interface nets:

| J301 pin | Net | MU4 endpoint / documented role |
| --- | --- | --- |
| 1 | PURX | MA4-origin active-low reset |
| 12 | ROW4 | MSP430 P2.5; general-purpose bidirectional interface |
| 13 | ROW3 | MSP430 P2.4; general-purpose output toward MA4 |
| 15 | ROW2 | MSP430 P2.3; general-purpose input at MA4 |
| 21 | FBUSRX | MU4 transmit, MSP430 P3.4/UTXD0 |
| 22 | MBUS | MSP430 P3.0; programmer detection |
| 23 | FBUSTX | MU4 receive, MSP430 P3.5/URXD0 |

MA4 block diagram A3-3 connects the row nets to its CPU row bus; they are
not QWERTY switch contacts. Their electrical specification does not supply
startup levels, edge timing or a mapping to firmware's `2002b` status bits.
The MU4 schematic's pull resistors are not a recovered firmware-driven
handshake. Keep that distinction before introducing GPIO transitions.
The current MAME source tree has no MSP430 CPU core. A faithful native MU4
backend therefore needs both a core and the matching program; an explicitly
declared serial-boundary HLE still requires recovered message semantics.

### Software-accessible MU4 references

The [original EXT_UI flashing-tool guide](https://files.elektroda.pl/12624,5510%2Bui%2Bdsp%2Bsoftware%2Bflashing%2Btool.html)
describes an FBUS-connected UI MCU forwarding music-DSP updates into NAND;
it names a separate Maverick FBUS specification, not yet acquired.
The [RetroHack repair package](https://retrohack.eu/gsm/nokia-5510-dsp-repair-tool/)
provides original R060/R061 music images alongside modified variants and
an analysis of captured flashing traffic. Its hardware claims remain
external-oracle evidence until independently matched to the NPM-5 image.

Retained, ignored, under `roms/5510-mu4-reference/`:

| Artifact | SHA256 |
| --- | --- |
| `5510-dsp-repair-tool_1.00_all.deb` | `1ac1261929fdf48829669ad00597f5d01a8f872d02ee6262acad9dcb9488776e` |
| `InitData_R060.a00` | `65118e00dfd8d78726a45e781e2c7c96714228ec025bc5ccb141d445d85db094` |
| `InitData_R061.a00` | `fad9a4d8c340c91fc06355ba18b42cdff9f2682ed48abb88903344d281b89d99` |
| `initdisk_R060.a00` | `8240fc14b3683dee1018426359e74fc9cb1bb5d0ed438685312015b14e79fe4c` |

The package was extracted, not installed or executed; its hash matches
the publisher's value. Only the unmodified image names above were selected
as emulation inputs. Its application source has no declared reuse license;
keep it as a reference, not copied driver code. The packaged protocol
analysis identifies node `28` and the `42/d2` transaction wrapper, agreeing
with outgoing constructor `335a50`. It also describes unsolicited opcode
`06` and self-test/product-info exchanges. These are leads for the MCU
consumer census, not sufficient evidence for normal key/startup replies.
Music-DSP images are separate from MAD2's missing resident baseband mask
code and are not MSP430 program dumps.

The primary MU4 manual page 8 identifies the music processor as
TMS320DA150. TI's [Audio Solutions Guide](https://www.ti.com/download/vf/audio/audiosolutionsguide1.pdf)
identifies its C54x core, program ROM, RAM, HPI and serial peripherals.
Shared ISA does not make its memory/peripheral map interchangeable with
MAD2's resident DSP. The keyboard remains owned by the separate MSP430.

Read-only `tools/noki5510_a00_inventory.py` inventories the original
InitData containers independently: two-byte marker, big-endian four-byte
payload length, payload, and four-byte trailer. Seven segments
(`aa55/aa22/aa44/aabb/aa88/aa99/aadd`) cover all 741,916 decoded R060 bytes
and 742,146 R061 bytes. Payload hashes, bounds and payload-only XOR integrity
are reported; the recovered receiver contract is described below.

The first segment and standalone InitDisk match the serial-boot grammar in
[TI SPRA602F, figure 11](https://www.ti.com/lit/an/spra602/spra602.pdf):
signature, four compatibility words, entry XPC/PC, then word-count/XPC/PC
sections and a zero-count terminator. The four compatibility words are
`0018/0003/0800/0010`; they are not assigned register semantics here.
The parallel-boot grammar with only two configuration words does not fit.
The strict parser accounts for every byte, rejecting truncation, trailing
data and unsupported XPC bits. All recovered sections use page zero.

| Stream | Entry (word address) | Section destinations (hex), word counts (decimal) | Covered bytes |
| --- | --- | --- | ---: |
| InitDisk R060 | `492b` | `08ea`:22, `0900`:768, `2080`:1261, `256d`:9532, `4aa9`:22, `0080`:120 | 23,502 |
| InitData R060/R061 first segment | `0e41` | `2080`:46, `20ae`:5580, `36b0`:1457, `ff80`:12, `0200`:3379, `0f33`:273, `184b`:13 | 21,578 |

Both entries lie inside their respective loaded sections. The InitData
first segment is byte-identical across R060/R061. These are static format
results, not validation of DA150 memory mapping, boot-ROM behavior or native
execution.

The six later payloads match the same count/XPC/PC section grammar without
the signature, compatibility words or entry. Every record is bounded and
each zero-count terminator consumes the payload exactly:

| Marker | R060 records | R061 records | Destination pages |
| --- | ---: | ---: | --- |
| `aa22` | 3,254 | 3,256 | 0, 2, 3 |
| `aa44` | 3,365 | 3,367 | 0, 2, 3 |
| `aabb` | 2,744 | 2,745 | 0, 2 |
| `aa88` | 4,622 | 4,622 | 0, 2, 3 |
| `aa99` | 3,127 | 3,129 | 0, 2, 3 |
| `aadd` | 1,895 | 1,895 | 0, 2 |

This validates static stream structure, not which payload is selected for
which operation, NAND placement or address-space aliasing. Preserve the
extended word addresses. The project C54x core now has an explicit
extended-program capability, separate from existing 16-bit handset profiles.
DA150 memory mapping and overlay selection remain unvalidated; silently
truncating these stream addresses is forbidden.

The inventory also measures linear destination coverage without exporting a
flattened image. Every individual segment, and standalone InitDisk, has zero
internal destination overlaps and zero records crossing a page boundary.
Across all InitData segments the destinations conflict extensively:

| Container | Word writes | Unique destinations | Same-value rewrites | Changed-value rewrites |
| --- | ---: | ---: | ---: | ---: |
| R060 | 313,867 | 126,235 | 5,016 | 182,616 |
| R061 | 313,961 | 126,241 | 5,029 | 182,691 |

Rewrite counts compare each record value with the preceding write at that
destination in container order; they do not infer the device's loading order.
This is evidence against treating the entire container as a single resident
program image. It supports separate-overlay interpretation, but does not
establish overlay selection, shared resident regions or physical page aliases.
The report retains per-page extents and rejects ranges beyond the 23-bit
address space. Unit fixtures distinguish same-value/conflicting rewrites,
cross-page ranges and cross-segment conflicts.

Do not load the complete container as flat program ROM or substitute its
music DSP for MAD2's baseband DSP.

#### Music-DSP entry dependencies

Independent GNU Binutils 2.43.1 (`--target=tic54x-coff`) disassembly of the
original section payloads identifies these bounded startup contracts:

| Image/path | Observed code contract |
| --- | --- |
| InitDisk `492b` | Initializes SP to `0900`, aligns it after adding `02ff`, establishes status bits, reads initialization data at `2080`, then calls `3079` and `4a6c`. |
| InitData `0e41` | Initializes SP to `1200`, aligns it after adding `03ff`, establishes status bits, reads initialization data at `0f33`, then far-calls `090f` and `0db8`. |
| InitData `090f` | Direct writes to data addresses `54/28/2b/29/58/3c`; writes I/O port `0080`; far-calls `2082`. These addresses are not all ordinary RAM. |
| InitData `2080..20ad` | 23 two-word far-branch trampolines; `2082` branches to `308e`. This is a function-entry table, not proof of interrupt-vector ownership. |
| InitData `0db8` | Dispatches far function pointers through data `1742/1702/1744`, then calls `0df4`, which loops. Do not interpret that terminal loop as a missing peer reply. |
| InitData `2fbf/302f` | Writes/reads NAND I/O port `4000`; neighboring `2fca` waits on BIO, and `2fe6` performs masked read-modify-write of data `003d`. Physical HD0/HD1/HD2 and BIO wiring is recovered below; board execution remains unvalidated. |

The core's explicit extended-program mode implements the observed far
control-flow families and XPC, with instruction-level conformance fixtures.
The library contains a program-copy helper at `20ae..20b2`: it loads AR0
from an argument, repeats `WRITA *AR0+`, and far-returns. `2ebf` provides a
single-word WRITA entry through trampoline `20aa`; `2ec1` is an
accumulator-indirect far branch through `20ac`. These establish available
program-write/dispatch primitives, not an identified overlay selector.
An executable core fixture repeats WRITA and READA across `02:ffff` into
`03:0000`, checking wrong-page sentinels, unchanged A and executing XPC,
and the advancing data pointer. This tests the extended PAR contract in
TI SPRU172C's WRITA/READA definitions independently of DA150 mapping.
This is not DA150 execution or peripheral validation. Core MMR accesses (for example SP at data
`18`) must remain distinct from board/peripheral accesses. No success value
or busy-line transition is inferred from these static routines.

Startup's C initialization loop at `0e5b..0e6b` reads count/destination/data
records from program `0f33`, terminating on a zero count. Read-only
`noki5510_a00_inventory.py --segment aa55 --cinit-section 0xf33` accounts
for all 546 bytes and recovers these five data-space initializations:

| Destination | Words | Initial content |
| --- | ---: | --- |
| `1700` | 2 | `ffff ffff` |
| `1849` | 1 | `0000` |
| `1746` | 256 | Sector template, SHA256 `29427de21e77b3d8d39e56a4f40583108f87c298c45bbeb660668666aeee11c6` |
| `1742` | 1 | `0000` |
| `1744` | 2 | `0000 0000` |

The dispatcher at `0db8` therefore starts with an empty function registry
and null final function pointer. Routine `0dd7` appends far pointers into
`1702` while the count at `1742` is below 32. Neither initialization nor this
registry alone identifies an overlay selector or a missing external reply.

Routine `0880` copies all 256 words from `1746` into its working frame before
calling `069b` and `0725`. In original big-endian payload-byte order, the
template has signature `55 aa`, 512 bytes/sector, 32 sectors/cluster, two
reserved sectors, two FATs, 512 root entries, 127 sectors/FAT and 124,896
total sectors. Its textual label is `FAT16`, but those BPB fields yield only
3,894 data clusters (a FAT12-sized count). Do not promote the label or this
unmodified template into a claim about the real medium's format: the copy's
consumer and any subsequent field changes still need decoding. No disk image
is synthesized from it.

The template is not installed unconditionally. At `0943` startup mounts
through `2082`; zero result branches directly to `095e`. A nonzero result
runs a second mount call with argument zero, invokes template consumer `0880`,
then `051d`, and retries the original mount. A nonzero retry enters the
error tail at `09bb`. This is a storage-initialization/recovery path; forcing
mount success would bypass genuine storage operations.

At `095e` the code prepares a directory context, scans entries through
`09fa/0a0b`, and compares name/extension strings against the word strings
at `184b` (`MCUSI16 `) and `1854` (`BIN`). That is a concrete named-file
consumer, not yet proof that its contents are a selected DSP overlay.

#### File-backed program loader

The startup tail `09b5..09b7` explicitly selects index zero and calls
trampoline `2080 -> 3538`. That entry masks interrupts, selects PMST `2028`,
sets SP `3aea`, calls `2ed4`, then far-branches to program `2000`. It is a
storage-backed load followed by a program transfer, not the callback registry
at `0db8` or a selector for the outer A00 markers.

| Boundary | Derived contract |
| --- | --- |
| `2ed4..2ee3` | Index selects a 44-word descriptor at data `36b0 + 44*index`; its `+12/+13` pair supplies remaining length. |
| `2092 -> 33ea` | Resets/seeks descriptor state against the filesystem context at `3aea`. |
| `2086 -> 32d3` | Reads through `330e` and byte-swaps each returned word. This is a file-reader boundary, not raw NAND port access. |
| `2f12..2f39` | Reads three single-word fields: count, destination high word, destination low word. Reconstructs a 32-bit destination. A nonpositive signed count ends loading. |
| `2ef5..2f10` | Reads count payload words into data `3c21`, then calls `2f56` with the destination, count and a destination-space selector. |
| `2f56..2fa8` | Clears `54/55`, fills the auto-incrementing bank through `56`, enables bit 0 at `54` and polls it for completion. |
| `3538..3546` | Resets the stack/processor mapping, loads descriptor zero, then transfers to program `2000`. |

The [TI VC5410A family register map](https://www.ti.com/lit/ds/symlink/tms320vc5410a.pdf)
independently identifies `54` as DMPREC, `55` as DMSA and `56` as
auto-incrementing DMSDI. The five bank-zero writes match source `3c21`,
destination low word, count minus one, zero sync/frame control and mode
`0144 + selector`. Bank `1e` receives source page zero and destination high
word. This is strong DMA-family evidence, not proof that DA150 has every
VC5410A DMA feature or its timing.

Do not conflate the file stream with the serial-boot table: this consumer
reads all three header words before testing the count, and its destination
space/page handling has additional rules in `2f00..2f0e` and `2f60..2f70`.
Those rules are now covered by original-loader execution and complete
destination checks below; physical DA150 memory mapping remains separate.
The resident `36b0` record contains placeholder `beef` words; startup fills
its file descriptors by directory traversal. Original InitDisk now populates
the medium and original InitData directory/read routines validate every
stored file, as described below. DMA-backed loading is also validated in the
isolated fixture; attaching it to a full MU4 profile still requires the
program-entry and physical-memory contracts. Completion is never forced.

For reproduction, `noki5510_a00_inventory.py --segment aa55
--extract-section 0x200 --output <new-file>` exports only the exact original
InitData section, rejecting ambiguous addresses and existing outputs. Use
`--extract-section 0x256d` without a segment for InitDisk. The exporter
preserves big-endian word bytes; Binutils' C54x binary backend reads
little-endian words even with `-EB`, so swap each byte pair with `dd
conv=swab` before analysis. Confirm the first decoded InitData entry word
is `7718`, not `1877`. With that converted file:

```sh
objdump -D -b binary -m tic54x --adjust-vma=0x200 \
  --start-address=0xe41 --stop-address=0xe8c <little-endian-section>
```

Addresses in this output are word addresses. The external tool was built
from the [GNU release](https://sourceware.org/pub/binutils/releases/binutils-2.43.1.tar.xz),
SHA256 `13f74202a3c4c51118b797a39ea4200d3f6cfbe224da6d1d95bb938480132dfd`;
no tool implementation is incorporated into this project. The disassembly
does not establish whole-image code/data boundaries or runtime reachability.
#### Music-DSP storage consumers

The bounded InitData routines identify a filesystem mount, not an overlay
loader. Trampoline `2082` reaches `308e`, which reads a partition-like table
at byte offsets `01c2 + 16*n` and `01c6 + 16*n`, tests the signature at
`01fe`, then interprets the selected volume's boot sector. These offsets
support an MBR interpretation, but the underlying media layout is not yet
recovered from the flashing containers.

| Trampoline | Routine | Static contract |
| --- | --- | --- |
| `2088` | `329d` | Calls the word reader and masks the result to eight bits. |
| `208a` | `3234` | Reads a word at a byte offset, using a cached row/word position; odd offsets combine bytes from adjacent words. |
| `208c` | `32ad` | Combines reads at offset `n` and `n+2` into a 32-bit result. |
| `2082` | `308e` | Mounts/interprets a volume through these readers; it is not a code-download entry. |

The mount compares the word at byte `01fe` with raw `aa55`, and reads BPB
offsets `0b/0d/0e/10/11/13/16/20/24`: bytes per sector, sectors per cluster,
reserved sectors, FAT count, root entry count, total sectors, sectors per
FAT, and the larger total-sector/FAT-size alternatives. The field widths,
offsets and signature match Microsoft's
[FAT specification, sections 3.1 and 3.5](https://www.scs.stanford.edu/~zyedidia/docs/_other/fat.pdf).
The later comparisons contain raw cluster thresholds `0ff5` and `fff5`.
This identifies a FAT-family consumer; it does not prove successful mounting
or which FAT variant the original 64 MB medium uses.

The lower read chain is `3234 -> 3035 -> 2fca/306c -> 302e`:

- `3035` waits on BIO, clears control mask `4`, clears mask `2`, sets mask
  `1`, outputs `00`, clears mask `1`, sets mask `2`, then outputs `00`
  followed by three address components derived from its argument plus one.
  It clears mask `2` and waits on BIO again.
- Those control changes call `2fe6`, a masked update of data register
  `003d`. Command/address sequencing identifies masks `1/2/4` as CLE/ALE/CE;
  the schematic and HPI GPIO register contract independently corroborate it.
- `2fb5` saves interrupt state, writes I/O port `4000`, executes a bounded
  NOP repeat, and restores the prior interrupt-mask condition.
- `2fca` loops while BIO is low, which the schematic wires directly to the
  NAND's active-low ready/busy output. Latency comes from the NAND operation,
  not a fabricated firmware-ready event.
- `306c` performs two port reads through `302e/302f` and assembles the low
  and high bytes of a word. The row reader caches words in its context and
  uses `0100` as its word-count boundary; do not equate that alone with a
  complete physical NAND page including spare/OOB bytes.

The MU4 schematic sheet 7 identifies the components and separates strobe
decode from NAND semantics:

| Component / net | Primary schematic contract |
| --- | --- |
| U201 | Samsung `K9K1208U0A`, 64M x 8-bit NAND, TSOP1-48, Nokia part `4341191`. Do not substitute a similarly named K9F part without comparing its contract. |
| U202 | TI `SN74LV138APWR`, Nokia part `4341175`: combinational 3-to-8 decoder, not a programmable storage controller. |
| Decoder select inputs A/B/C | DSP `R/W`, `A14`, `A15`, respectively. |
| Decoder enables | `IOSTRB` at active-low G2A; G2B grounded; `ADD_H` at active-high G1. |
| Decoder outputs | Y2 -> `NF_WR`, Y3 -> `NF_RD`; Y6 -> `USB_WR`, Y7 -> `USB_RD`. |
| NAND bus / controls | DSP D0..D7 directly reach U201; separate `NF_CLE`, `NF_ALE`, `NF_CE1`, `NF_R/B` connect DSP and NAND. WP is tied to the supply. |

The enable/select equations from the
[TI SN74LV138A datasheet](https://www.ti.com/lit/ds/symlink/sn74lv138a.pdf)
place NAND strobes in the DSP I/O quadrant `4000..7fff` and USB strobes in
`c000..ffff`, when the enables are active. The lower 14 address bits do not
enter this decoder. That independently agrees with the observed NAND port
`4000`; the rest of DA150 address decoding and `ADD_H` ownership still need
mapping. This removes the need to invent an opaque R/W-controller protocol.
The DSP sheet 4 completes the control wiring:

| U101 DA150 pin | Board net / U201 input |
| --- | --- |
| HD0, pin 60 | `NF_CLE` -> CLE |
| HD1, pin 59 | `NF_ALE` -> ALE |
| HD2, pin 81 | `NF_CE1` -> active-low CE |
| BIO, pin 31 | `NF_R/B` <- NAND ready/busy |
| HPIENA, pin 92 | Ground; HPI interface disabled. |

TI's [VC5410A data manual, section 3.9.2](https://www.ti.com/lit/ds/symlink/tms320vc5410a.pdf)
documents HD0..HD7 GPIO via direction register `003c` and status/output
register `003d` when HPI is disabled. This is corroborating family evidence,
not a claim that DA150 is a VC5410A or shares its full memory/peripheral map.
The original MU4 startup at `0933..0939` ORs `7` into `003c`, and storage commands toggle `003d`
bits 0/1/2 in exactly the matching command/address/select phases. Together,
these establish a board-level attachment contract without importing an
unrelated DSP profile. The family PDF is retained outside SCM as
`roms/reference-docs/npm5/ti_tms320vc5410a_sprs139i.pdf`, SHA-256
`6c3a0258b64817faf46bd7ac41c034da8b17ca4e4e1849a954778eb81ca50e4f`.

[SPRU172C's condition-code definition](https://www.ti.com/lit/ug/spru172c/spru172c.pdf)
specifies BIO as low and NBIO as high. The core exposes a board-supplied
`bio_in_cb` with an unconnected high default; no pin identity or NAND behavior
belongs inside the CPU. Spare-area/ECC handling, address enables and
container-to-media placement remain open.

`make check-c54x-core` checks 28 BIO/NBIO variants at both pin levels
(branch, delayed branch, conditional call/return, delayed return and one-/two-word
conditional execution), plus a running busy loop released by a live pin change.
This proves instruction/input behavior, not native MU4 firmware execution.
The `aa55` outer firmware-container marker must not be confused with the
FAT boot-sector signature: their consumers and address spaces differ.

The dirty-cache flush at `0725` strengthens the NAND-protocol identification:

| Code | Observed operation |
| --- | --- |
| `0745..0778` | Outputs `60`, three row-address components, `d0`, waits on BIO, outputs `70` and tests status bit 0. |
| `0789..07e9` | Outputs `80`, one column plus three row components, 256 low/high word pairs, eight pairs of `ff` spare bytes, then `10`; waits and requests status with `70`. |
| `069b..0724` | Maintains a 32-row cache at data `4000`, with 256 words per row and a dirty flag at `3b14`; the write-back path tests that flag before flushing. |

These commands and 512+16-byte organization match Samsung's exact
[K9K1208U0A datasheet](https://datasheet4u.com/pdf/265061/K9K1208U0A-YIB0.pdf),
revision 0.2, January 17, 2001. The original PDF is retained outside SCM as
`roms/reference-docs/npm5/samsung_k9k1208u0a.pdf`, SHA-256
`c20df5ad534f2b6dc50ed1c1aeb48b70108dece3c8e3f9406a40c59d17c7ce59`.

| Contract | Manufacturer evidence |
| --- | --- |
| ID | `90`, address `00`, then two reads return `ec 76` (Read ID, page 22). |
| Geometry | 131,072 pages, 512 data + 16 spare bytes; 32 pages per erase block, 4,096 blocks (page 4). |
| Address cycles | One column and three row bytes for read/program; three row bytes for erase (page 4). |
| Read/program/erase latency | Read transfer maximum 10 us; program typical 200 us, maximum 500 us; erase typical 2 ms, maximum 3 ms (pages 7-8). These are rated values, not measured Nokia timings. |
| Sequential row read | Advances through pages within a block; the host must terminate at the block boundary by raising CE (page 18). CE is a protocol input, not merely a host-side access filter. |
| Ready/busy | Open-drain R/B reports internal operations; status bit 6 is ready and bit 0 is failure (pages 5, 22). |

The spare-byte loop writes erased fill on this path;
it does not prove that all media operations omit ECC or bad-block handling.
The observed control masks are consistent with command/address/chip selection,
and the corresponding GPIO mapping now has independent schematic support.

The overlay patch `patches/mame-nandflash-mu4.patch` extends MAME's generic
`machine/nandflash`, rather than introducing a Nokia NAND implementation.
`SAMSUNG_K9K1208U0A` supplies the exact ID, geometry and address cycles.
Its read transfers, program, erase and reset use timed ready/busy state;
program/erase mutate media only on completion. CE terminates its sequential
read cycle and block-end reads do not enter the next erase block. Existing
parts retain zero-duration operations and their previous sequential-read
policy unless CE is explicitly connected. Media, page register, pending
operation and address state are saved, and post-load republishes R/B.

`make check-mu4-nand` exercises this device without Nokia firmware: exact
ID, upper row address, data/spare access, AND-programming, erase,
pre-completion busy/status, CE termination on non-erased data, block-boundary
termination, reset cancellation and pending-program save/replay. It also
checks one existing K9F5608 part's synchronous program/read behavior. The
gate disables host NVRAM-file emission; it does not prove filesystem
persistence, analog pin timing, copy-back, factory bad-block contents or
all existing NAND users. Typical program/erase and maximum read/reset times
are deterministic datasheet selections, not measured MU4 board latencies.

`make check-mu4-storage-original` executes the unchanged InitData R060
library routines `3035` (read setup) and `306c` (word read) on the C54x core.
The isolated `mu4nand` fixture connects GPIO `003c/003d`, NAND strobes and
live BIO. Original code emits the five command/address bytes, waits while
R/B is low, and returns `ffff` from erased row 1. After external test-pattern
programming and a soft reset, it addresses row 65536 and returns `3412` from
bytes `12 34`. Both calls preserve SP. The core gate additionally checks
512 `BIT Xmem,BITC` variants, including the saved-INTM test used by the
original command writer.

The same gate calls unchanged dirty-cache flush `0725` with its documented
routine inputs: dirty flag `3b14`, physical row base `3b12/3b13` and 8,192
test words at data `4000`. Original code erases block 2, programs all 32
pages, fills 16 spare bytes per page with `ff`, and compares each page against
its cache. An independent NAND read checks all 8,192 words and 512 spare
bytes. A pre-existing zero byte makes erase necessary; successful return is
zero, SP is balanced, the dirty flag clears and compiler mode is preserved.

The original mount `308e` also runs with startup's filesystem-context
argument `3aea` and partition-aware flag one. Against an erased boot sector,
it performs NAND reads and busy waits, returns failure `1`, and preserves SP.
Trampolines `2080..20ad` are taken from the original container, not rewritten
calls. This validates rejection of missing media; it does not supply a BPB,
directory, file or a successful mount. Long-offset `ADDM` and immediate `ST`
use displacement-before-immediate encoding, independently exercised by these
original context updates and their core conformance fixtures.

The gate also supplies the unchanged `0f33` C-initialization records as
routine-global inputs, then executes recovery writer `0880`. Its call to
`069b` uses logical sector 31; the cache path adds one, so the 512-byte
template reaches physical NAND row 32. Independent byte reads match the
original template exactly. The fixture does not write a disk image directly.

Subsequent original format entry `051d` remounts with flag zero before
calling initializer `0454`. On this medium it returns `1` without erase or
program commands: the missing boot record is still a prerequisite even after
the row-32 template is written. Thus the startup recovery sequence alone
does not provision erased NAND. A valid initial media/partition contract must
come from InitDisk or a genuine medium image. The original InitDisk path is
now executed below, including original segment transfer; independent file
readback and DMA-backed execution remain open.

### InitDisk storage scan and receive boundary

The complete scan and block-reader predicate execute from unchanged InitDisk
R060's `256d` and `4aa9` records in the isolated storage gate. Startup
`3079` configures GPIO direction bits
0..2 and calls `30de` with data pointer `057e`. That routine initializes
the NAND GPIO through `377a -> 3781`, then calls `36b9` with context `068c`
and two output pointers. The scan covers 256 groups of 16 blocks: 4,096
blocks in total. Each `2889` call multiplies the block index by context
field +2 and reads the first two pages through `2832`.

`2832` emits NAND command `50`, column zero and three row bytes, reads all
16 spare bytes, then waits for status bit 6 using command `70`. `2889`
tests spare byte 5 on each page against `ff`; it returns `ff` only when
both match, otherwise zero. This matches the fitted NAND's factory
bad-block-marker convention. `36b9` records zero results in a 16-bit
per-group bitmap and separately examines blocks `0ff9..0fff`. The later
selection at `30fe..315b` tests masks `3/6/12/24/48/96` against that final
bitmap before choosing its data `01fa` and `04e0` values. Their allocation
meaning is not yet established; neither a disk geometry nor successful
provisioning should be inferred from these constants.

After the scan, `30de` enters a receive-driven state machine. Its byte ring
has producer `4bbf`, consumer `4bc0`, and storage indexed from `4abf`, with
8-bit wrapping and both indices initialized to `ff`. Handler `2b03`
increments the producer, reads peripheral register `0031`, writes the ring
entry and returns with `RETE`. Consumer `34a5` increments the read index;
`34c0` combines two received bytes high-first. Startup configures indexed
peripheral registers `0034/0035` through `35c7`. This is an on-chip serial
receive boundary, not evidence that InitDisk consumes the board's USB I/O
quadrant directly. The exact DA150 serial instance, interrupt wiring,
clock and external sender still need corroboration.

`check-mu4-storage-original` loads unchanged InitDisk code and its original
`2080` C globals after the InitData checks. Original `377a/2889/2832` returns
`ff` for erased block-zero markers. External NAND programming changes only
page-zero spare byte 5 to zero; the same original code then returns zero
after the first page, with balanced SP and timed status handling. The ABI
uses near `CALL`, matching these routines' `RET`, rather than `FCALL`.

The full `36b9` call uses context `068c` and stack output pointers
`2000/2100`. It scans all 4,096 blocks, records the externally programmed
block-zero marker, reports no bad reserved blocks, and returns zero with
balanced SP. Metadata writer `256d` byte-swaps the 256-word source bitmap
in place before its cache writeback; the final bitmap therefore begins
`0100`, not `0001`. The complete gate observes 156,412 NAND reads and
37,778 busy-pin reads. This checks routine execution, not a valid filesystem
or preservation of factory markers through every provisioning path.

The routine requires the separate original `4aa9` record: its metadata
tail calls helper `4aaf`. Omitting that record causes execution into blank
test program RAM and eventual wrapper re-entry, despite correct scan
counters. All required code records must be loaded before attributing such
noncompletion to the CPU or hardware. Temporary instruction/stack probes
are removed. Next establish the serial ingress contract; no received ring
entries, successful scan return or filesystem records should be injected
into RAM.

### InitDisk serial ingress and file selection

The original `0080` vector record branches from `00d8` to handler `2b03`.
Together with its read of `0031` and indexed configuration at `0034/0035`,
this matches TI VC5410A's McBSP2 DRR1, SPSA/SPSD and RINT2 vector (IFR/IMR
bit 6). This corroborates that peripheral subset; it does not establish
complete DA150 equivalence or physical serial-clock timing.

The isolated gate loads that original vector record and executes port
initializer `346a -> 35c7`. A word-level fixture delivers bytes `12/34`
at receive register `0031` with RRDY and receive interrupts. Original code
alone increments producer `4bbf` and writes ring storage. After two ISR
returns to idle, original consumer `34c0` returns `1234`, advances consumer
`4bc0` to one and preserves SP. The fixture neither posts ring entries nor
models serial framing, overrun, DMA or the MCU-to-MU4 sender.

`32dc` creates nine 18-word segment descriptors. Their marker fields are
at `010d + index * 18`; eight names come from the original `2080` C globals
and are copied into descriptor name fields by `491e`:

| Marker | Original name | Name source |
| --- | --- | --- |
| `aa55` | Raw boot stream; no name copy | None |
| `aa22` | `MCUSI16 ` | `01fd` |
| `aa44` | `MP3SI16 ` | `020f` |
| `aa88` | `RERSI16 ` | `0221` |
| `aabb` | `AACSI16 ` | `0218` |
| `aadd` | `USBSI16 ` | `0206` |
| `aa99` | `RELSI16 ` | `022a` |
| `bb77` | `PRODINFO` | `0233` |
| `bbcc` | `TESTINFO` | `023c` |

The first copy destination is `0112`, not `0100`: names start at descriptor
index one, after the raw `aa55` record. Original directory enumeration and
file reads independently corroborate `aa22 -> MCUSI16 .BIN`.

Receiver state `01f6` selects marker acquisition (0), 32-bit length (1),
payload words (2), and checksum (3). `3348` matches the marker against the
nine descriptors. `339d` combines two high-first words into byte length
`01f2` and rejects odd lengths. Payload buffering uses data `c000` and
512-byte chunks. `33fd` distinguishes `aa55`'s raw-storage path from the
file-write path `342e -> 2cb1`; the latter uses the selected descriptor.
Checksum state `32a7` disables accumulation, reads a word and compares it
with the accumulated byte XOR at `01f9`. The accumulator is reset and enabled
at `3229..322e`, after the length has been consumed: only payload bytes enter
the XOR. All seven segments in each original R060/R061 container match this
rule. The first two trailer bytes are the high-first checksum word; the
following `8888` word is outside checksum coverage and is consumed by marker
acquisition before the next segment. `mu4nand` transfers the complete original
R060 container through receive register `0031` and IRQ6, at a test-only
one-millisecond cadence, pausing on the firmware's external XF busy output.
Original ISR, ring consumer, marker selection, payload writer and checksum
comparison remain firmware-owned. The gate observes seven selected segments,
741,916 consumed wire bytes, final checksum `0040`, state zero and a drained
ring, then successfully mounts the retained medium with original InitData.
This covers the raw first-segment and subsequent file-write receiver paths.
Independent original-consumer file readback and DMA-backed loading are
validated below; native program-entry execution remains unverified.

The acquired repair-tool analysis describes a different, upstream layer:
PC-to-phone blocks `80 12 length payload XOR-even XOR-odd` and byte `90`
acknowledgements. Do not replay that envelope into McBSP2: the MCU forwarding
path has not yet been reconciled with this DSP segment receiver. The next
boundary is native program-entry execution after original InitData loading
through modeled DMA.

### Original erased-media provisioning

The gate gives initialization an independent erased NAND medium and clean
test RAM, loads only original InitDisk C globals, and near-calls original
`346a` and `30de(057e)`. Firmware owns the block scan, metadata writes,
filesystem setup and receive-descriptor initialization. It reaches the
empty-ring receiver loop at `31f0..3203`, with state `01f6` and completion
`01f5` both zero and no received words. The run observes 1,772,974 NAND
write strobes and 10,177,113 reads. These are operation counts, not physical
DA150 timing measurements; the 60-second fixture deadline is a test bound.

After a soft reset, the fixture restores InitData's original program library
and C globals, retains only the medium written by InitDisk, and calls the
same unchanged partition-aware mount `308e(3aea)` used in the erased-medium
negative control. It returns zero with balanced SP and live NAND busy waits.
Thus original firmware provisions media that the original consumer mounts;
no BPB, partition table, directory or successful return is supplied by the
fixture. The same gate then transfers the original segments as described
above; full MU4 boot remains unverified.

### Original file-consumer readback

After original InitDisk transfers the container, original InitData mount
`308e(3aea,1)` succeeds. The fixture calls original `09c6/09fa` with the same
directory ABI as startup `0960..096b`, then advances via `0a0b`. Original code
constructs each descriptor at `36b0`; no file metadata or contents are
supplied by the fixture. Six entries and the final end-of-directory return
are observed with balanced SP and idle return.

| Original file | Source marker | Verified bytes |
| --- | --- | ---: |
| `MCUSI16 .BIN` | `aa22` | 123,368 |
| `MP3SI16 .BIN` | `aa44` | 127,528 |
| `AACSI16 .BIN` | `aabb` | 103,902 |
| `RERSI16 .BIN` | `aa88` | 175,266 |
| `RELSI16 .BIN` | `aa99` | 118,450 |
| `USBSI16 .BIN` | `aadd` | 71,754 |

For each file, original seek `33ea` selects offset zero and original reader
`32d3` returns chunks of at most 256 words through its filesystem/cache/NAND
path. Every returned word is compared with the unchanged source payload:
720,268 bytes total, not a prefix or checksum-only assertion. Names,
extensions, lengths, unique source markers and complete directory count are
also checked. The first header in `MCUSI16 .BIN` is `0010 0002 2000`, matching
the file-backed loader's count/high/low grammar, not the raw serial-boot
header `08aa` of `aa55`.

The fixture uses routine ABI wrappers and retained in-memory NAND across
soft resets. This does not establish host NVRAM persistence, physical DA150
DMA mapping/timing or native music-DSP boot. The original `2ed4 -> 2f56`
load is executed below; program-entry execution remains the next boundary.

### Original DMA-backed loader

The fixture attaches a separate `tms320c54x_dma_device` at data `54..57`.
[TI SPRU302B](https://www.ti.com/lit/ug/spru302b/spru302b.pdf), sections 3.2/3.3,
documents the subbank, count-minus-one, space/index fields and fixed seven-bit
program-page registers. The ignored reference copy is
`roms/reference-docs/npm5/ti_tms320c54x_spru302b.pdf`, SHA-256
`daf74902629c8d3f54b12e0af57e95679fe172ddd581f5817aeac32b5503ec22`.

The loader acceptance covers polled, nonsynchronized, single-frame/single-word
blocks, with constant/increment/decrement addressing across program, data
and I/O spaces. `0144` increments data source and program destination;
`0145` selects data destination instead. Each timer event actually reads and
writes a word, updates addresses/count and clears DE only after the last
word. Low-address wrap does not modify the program-page register. Registers
and pending timers are saved. Subsequent serial synchronization, interrupt,
autoinitialization and multiframe extensions are documented under the
streaming boundary below. ABU, indexed addressing and overlapping arbitration
remain outside the validated subset; unsupported active modes fail explicitly. The two-clock cadence
at the fixture's 13 MHz is an assumption, not measured DA150 latency.

MMIO conformance checks exercise deferred completion, program-page wrap,
zero-count one-word transfer, cancellation and pending save/restore replay.
The original consumer then mounts and enumerates `MCUSI16 .BIN`, uses a
loader-safe stack at `3aea` matching `3538`, and calls unchanged `2ed4` before
the final program branch. Original file reads and `2f56` drive the device;
no DMA destination, enable completion or loader return is synthesized.
Every loaded destination from all 3,254 records is checked against the source:
51,921 words across data and program spaces, with DE clear and balanced SP.
InitDisk-only ROM mappings above `3679` are removed when restoring InitData,
so they cannot silently discard DMA writes to program `3d00`.

Original `2f00..2f0e` selects data space for destinations at most `ffff`;
larger destinations select program space. `2f60..2f70` clears the page for
low addresses below `8000`. These are executed firmware rules, not a flat
replay of outer container records.

### Loaded program entry

The isolated fixture invokes unchanged `3538` with descriptor index zero.
It reloads `MCUSI16`, selects PMST `2028` and transfers to program `2000`;
the loaded entry branches to logical `02:6d62`. Read-only program-bus taps
observe both entries after the complete loader comparison. TI SPRU131G
section 3.2.5 defines the required common lower 32K window: with OVLY set,
CPU accesses to `02:6d62` use physical program `00:6d62`, which original
`2f56` populated. The core preserves logical XPC and applies the translation
to CPU fetches and program operands, not DMA. Independent overlay-on/off
core fixtures protect both address modes.

Original `f4a0` at logical `02:6d71` is `LD #0,ARP`, not missing uploaded
data. The implementation is checked against TI SPRU172C syntax 5 and all
eight immediate pointer values, preserving unrelated ST0 fields.
`46f8` at logical `02:6d2b` is the absolute `LD Smem,DP` form: it consumes
the address extension and loads only the nine-bit DP field. Three independent
values check masking and unrelated ST0 preservation. Reaching
this loaded entry is not proof of the full startup, music playback or
handset boot. DA150 silicon memory extents, data/program RAM aliases, PLL
and bus contention remain unresolved.

The loaded code also uses `fa47` (`BCD ...,ALEQ`) at logical `02:dfa4`.
TI SPRU172C identifies condition `47` as signed A less than or equal to zero.
Five core fixtures cover negative extremes, zero and positive values, predicate
capture before two delay words, and the three-cycle branch cost.

Before controller modeling, the eight-second entry run reached logical `02:9524`, with PMST `202c`,
`illegal=0` and not idle. Original `02:951d` sets AR1 to `0048`, `951f`
selects index 1, and `9521` executes long-offset BITF `61e1 0001 0002`;
`9524` repeats on NTC. TI's related VC5410A register map identifies
`48/49` as McBSP1's control window, index 1 as SPCR2. SPRU302B identifies bit 1
as XRDY. These addresses now map to the partial indexed McBSP device.
Original `61e1 0001 0002` consumes displacement 1 before mask 2. It agrees
with original InitDisk's independently executed long CMPM and GNU's decoding;
SPRU172C places BITF and CMPM in the same Smem-plus-immediate class. The old
mask-before-offset BITF fixture was self-confirming and is corrected. Four
executable fixtures cover both TC outcomes, updating/non-updating addresses,
wrong-address sentinels, TC-only status changes and the three-cycle cost.
SPRU131G's generic offset-last note must not override the recovered three-word
Smem-plus-constant encodings.

Recovered configuration includes SRGR1 `00fa`, SRGR2 `2000`, PCR `1f0b`,
and programmed SPCR2 `0341`.

SPRU302B 2.3.2.2 specifies XRDY becoming one on XRST's zero-to-one transition,
clearing when DXR is written, and returning when DXR transfers into XSR.
`tms320c54x_mcbsp` implements that transmit-buffer lifecycle, a separate
shift register, CPU/2-derived internal bit clock, and MSB-first 8/12/16-bit
output. Executable conformance covers absent external-clock stalling,
queued words, reset cancellation, XRDY interrupts and pending save/restore.
Receiver and wider/multiphase formats remain unsupported; active unsupported
transmit formats fail explicitly. External framed clocks and DMA-ready lines
have separate conformance coverage below.
The fixture's 13 MHz source is not a validated DA150 PLL configuration.

Original MCUSI16 transmits `0c10 0818 0a01 0e53 1023 1201` without illegal
instructions. The gate ends when the sixth word completes, not after a full
startup interval. The subsequent channel-3 mode `c541`, sync `203f` configuration
selects a separate streaming port, described below;
neither its endpoint nor music playback is established by these control words.

### Streaming DMA boundary

Continuing unchanged firmware past serial setup enables channel 3 with source
`1300`, destination `0023`, element count `0001`, sync `203f`, mode `c541`
and DMPREC `4848`. Global indices are DMIDX0 `0040`, DMFRI0 `ffc1`; both
program pages and the older shared reload bank are zero at enable time.
SPRU302B tables 3-8 and 3-10 decode this as McBSP0-transmit synchronization,
16-bit data-to-data transfers, two elements per frame, 64 frames, source sorting
indexing, fixed destination, block interrupts and auto-initialization. CTMOD
is zero: this is not ABU mode. The related VC5410A maps `0023` to McBSP0 DXR1.

Section 3.2.3.5 uses DMIDX0 on non-final elements and DMFRI0 **instead** on
the final element. Initial source addresses therefore alternate
`1300,1340,1301,1341,...,133f,137f`, not a linear copy. The first block has
128 words. The firmware also writes channel-3 reloads through indexed bank
`32..35`: `1300`, `0023`, `0001`, `203f`, at logical `02:9728..9735`.
VC5410A table 3-20 identifies exactly this per-channel bank; the frame reload
has only eight writable bits, so its stored value is `003f`. DA150 therefore
uses the extended per-channel reload layout for this path, not shared `24..27`.
The original-firmware stream has not yet executed through its endpoint.
McBSP0 now has its own mapped data/control device and XRDY-to-DMA event line;
forwarding it into McBSP1 would conflate different ports.
The VC5410A datasheet's IFR diagram puts XINT1 at bit 11; table 3-21's
priority rank 14 is not an interrupt-bit number. The fixture uses bit 11.
The DMA device now has an explicit per-channel reload configuration, selected
by this fixture. Conformance checks all six reload banks, eight-bit frame masks,
reserved `28/29`, indexed access and extended-bank save/restore. The
channel-enable extension remains unsupported.
At captured DMPREC `4848`, INTOSEL is 1; VC5410A table 3-16 assigns IFR
bit 11 to DMA channel 3 rather than XINT1. The fixture routes these sources
according to INTOSEL. This is a partial mux (the attached transmit sources
and channel 3), not a complete ASIC interrupt-controller implementation.

The DMA device now accepts single-word multiframe transfers, all nonreserved
sorting/index modes, shared or per-channel auto-reload, and block/frame completion
callbacks. Each selected sync event schedules one element after the assumed
two-clock bus delay; mismatched events do nothing. A synchronized channel does
not advance merely because DE is set. Isolated tests verify a two-frame sorted
stream, exact completion order, reload, non-reloading DE clearance and pending
save/replay. Completion callbacks identify the channel; the surrounding ASIC
owns the CPU interrupt mux. ABU, double-word transfers and overlapping sync/bus
arbitration still fail explicitly. These tests do not prove original streaming,
physical clock/arbitration timing, codec operation or playback.

XRDY also has a level callback independent of CPU interrupts. DMA remembers
asserted ready lines, including readiness preceding channel enable. Tests cover
that ordering, a held level not becoming a duplicate edge, and channel re-enable.
Both line levels and pending transfers are saved. Pulse-event conformance remains
separate from this peripheral-ready wiring.

### Codec-controlled streaming clocks

NPM-5 schematic sheet 2 identifies U601 as TLV320AIC23PW, with `DSP_CLKOUT`
at 11.9952 MHz. McBSP1 carries codec controls; McBSP0 carries digital audio.
Original controls decode as power `10`, analog path `18`, digital path `01`,
format `53`, sample-rate `23`, active `01`. The related
[TI AIC23B datasheet](https://www.ti.com/lit/ds/symlink/tlv320aic23b.pdf)
corroborates master, DSP-format, 16-bit stereo and USB/272 oversampling;
11,995,200 / 272 gives 44,100 frames/s. The original
[AIC23 EVM guide](https://www.ti.com/lit/ug/sleu003/sleu003.pdf), section 5.3,
independently specifies codec-master DSP-format stereo, McBSP0 audio and McBSP1
SPI control. Detailed timing uses the related B-revision manual and remains
revision-qualified: USB-mode BCLK equals MCLK, not MCLK/4; frames span 272 bit
clocks. LRP=1 places the left MSB after the one-bit frame pulse.

Local ignored references under `roms/reference-docs/npm5/` are
`ti_tlv320aic23_evm_sleu003.pdf` (SHA-256
`f8fe33dfaaa2414b5f6cb80f102fb97f92f85d47375e6802d50b94e41e07cceb`)
and `ti_tlv320aic23b_slws106h.pdf` (SHA-256
`b9c7f9f9ef77333306a6a98cb5ec8c071fd7fa90e691f786f91b8d1ae50bbaf2`).

The partial `tlv320aic23_device` receives decoded 16-bit control words rather
than SPI pin transitions. Activation, master selection and device/clock power
control gate its clock timer. The observed 16-bit DSP USB/272 mode is supported;
other active master formats fail explicitly. CLKIN halves the codec clock;
CLKOUT does not divide BCLK. Output-amplifier power-down does not stop serial
clocks. Registers, clock phase and pending timer state are saved. Isolated tests
verify inactive silence, control decoding, 11.9952 MHz BCLK, the 272-clock frame
divider, reset defaults and pending-edge replay. The digital DIN decoder samples
on rising BCLK; DOUT serializes explicitly supplied, already-converted stereo
samples. There is no analog ADC/DAC conversion, filtering or sound output.
An enabled ADC requires an explicit converted-sample callback, not an implicit
silence source. Related AIC23B figures 2-2 and 3-8 place master frame and DOUT
changes after falling BCLK: the clock callback therefore precedes those pin
updates. This preserves receiver sampling of the previous pin values without
claiming a measured propagation delay or original-part electrical equivalence.

The coupled digital fixture runs codec, McBSP0 and both DMA channels together.
It checks alternating DIN words, converted-sample DOUT delivery, sorted RX
destinations and a mid-word snapshot at 92 microseconds. The following ten
microseconds replay identical clock edges, decoded DIN words and RX completion.

Original McBSP0 configuration is PCR `000e`, XCR1 `0140`, XCR2 `0044`,
SRGR1 `0f07`, SRGR2 `101f`, SPCR2 `0101`. It selects external clocks,
two 16-bit words per frame and frame-ignore behavior. The streaming fixture
connects codec-generated BCLK/frame pins to this transmitter; it does not inject
DMA readiness or firmware state.

Generic `tx_clock_w`/`tx_frame_w` inputs model external single-phase frames.
SPRU302B 2.3.4.1 and 2.3.5.2 define polarity and opposite-edge XRDY;
zero-delay first bits are asynchronous to CLKX. The device avoids double
shifting when frame and clock edges share an emulated timestamp. Isolated
pin-transition tests cover two-word frames, no-frame stalls, MSB-first data,
XFIG early-frame suppression, zero/one/two-bit delays, both polarities,
opposite-edge readiness, cancellation and mid-word save/replay. These are
ordered-edge conformance tests, not measurements of electrical setup/hold.
Underrun and unexpected-frame recovery without XFIG fail explicitly;
active format changes and full reset-activation timing remain unvalidated.

The external-pin save/replay fixture defers checkpoints until synchronized CPU
input events have drained and asserts `scheduler().can_save()`. MAME discards
anonymous synchronization timers on load but does not serialize the input-event
queue; bypassing this prerequisite can strand queued pulses and cause a later
overflow unrelated to serial timing. The fixture checks XINT0 has reached CPU
IFR bit 5 before saving and remains latched after loading. The stream acceptance
rejects queue-overflow warnings.

`make check-mu4-storage-original` uses BIOS `setup`, preserving storage,
loader, DMA/McBSP conformance and six-word original control setup.
`make check-mu4-storage-original MU4_STORAGE_BIOS=stream` selects the same
original bytes but requires a complete 128-word streaming block and DMA
completion. With the original six control words activating codec clocks, the
fixture observes 128 transmitted words and one DMA completion, with `illegal=0`.
These words are buffer contents, not validated music samples. The finite-block
run has no CPU input-queue overflow; it does not establish sustained operation.
This is original TX/DMA execution, not full MU4 startup or audio playback.

The same run programs RX DMA channel 2: source McBSP0 DRR1 `0021`,
destination `1980`, count `0001`, sync `103f`, mode `c055`, reload bank
`2e..31` = `0021,1900,0001,103f`. This is the corresponding two-element,
64-frame receive path with fixed peripheral source and sorted destination.
The generic controller now implements external single-phase 8/12/16-bit receive
through separate `rx_data_w`, `rx_clock_w` and `rx_frame_w` inputs. FSR is detected
on the configured sampling edge, not immediately on a pin write. SPRU302B
2.3.5.1 requires RSR-to-RBR on the opposite edge, then RBR-to-DRR/RRDY on the
following sampling edge. An unread DRR does not overwrite the next buffered
word. DRR1 reads clear RRDY/REVT; debugger peeks do not. RRST cancels pending
shift/buffer state. RJUST selects zero fill, sign extension or left justification.
RINT mode zero pulses on receive readiness; REVT is a separate saved level.
Releasing RRST baselines the frame detector to the current physical pin level;
an already-active idle level is not a new sampled frame transition. This matters
for the original active-low receiver attached to the codec's idle-low frame pin.

Pin fixtures verify frame gating, ignored early frames, handoff edges, two-word
buffering, widths, zero/one/two-bit delays, both clock/frame polarities,
justification, nondestructive peeks, reset, CPU RINT0 and exact partial-word
save/replay after synchronized inputs drain. A composition fixture uses the
original RCR `0140/0044` and PCR `000e`, delivers two known pin-level words
through DRR1/REVT0 to channel 2, and checks deferred sorted destination writes,
readiness clearance, per-channel auto-reload and INTOSEL-1 IRQ10. It uses one
two-word frame, not the original 64-frame receive block. It does not inject a
receive register, readiness event or firmware buffer.

RX registers, pin levels, partial shifts and buffering are saved. Dual phase,
words wider than 16 bits, companding, loopback/SPI/A-bis receive, nonzero RINTM,
overrun and unexpected-frame recovery, active format changes, internal receive
clocks and reset-activation timing remain unsupported or unvalidated. Unsupported
active data formats and overrun/recovery fail explicitly.

The native stream explicitly supplies converted fixture samples `1234/5678`
through the codec's DOUT pin, not DRR, DMA readiness or firmware-buffer writes.
Original channel 2 completes 64 stereo frames and stores all 128 words at
`1980..19bf` and `19c0..19ff`. DIN acceptance compares the first 128 actual
transmitted words against the codec's decoded channel/value sequence; idle-line
samples before transmission are excluded from this observation only.
This verifies finite original RX/TX digital execution with a declared converted
sample fixture, not analog ADC behavior, sustained duplex, music or speech.

`make check-mu4-storage-original MU4_STORAGE_BIOS=sustain` extends the same
unchanged program to eight blocks: 1,024 TX words match decoded DIN, TX and RX
each complete eight DMA blocks, and the RX reload destination `1900..193f` /
`1940..197f` contains the declared converted stereo fixture. No readiness,
interrupt result or firmware buffer is injected. The run reaches PC `003080`
with `illegal=0`; all six original codec controls remain unchanged. This is
bounded repeated digital streaming (512 stereo frames, about 11.6 ms), not
long-duration operation, analog conversion, meaningful music data or playback.

Interrupt consumption is distinct from DMA completion. The original TX vector
at `206c` contains `4a1e,f495,f882,8afc`: save XPC, NOP, then far branch to
`02:8afc`. Read-only fetch observers distinguish execution from program-table,
DMA and debugger reads. The eight-block acceptance requires at least seven TX
vector and handler entries; the final completion may still be pending when the
observation ends. The verified run observes eight of each. This proves repeated
delivery into original firmware, not
complete application startup or audio processing.

The observed mask is IMR `0ac1`: DMA3/source 11 is enabled, DMA2/source 10 is
masked. RX completes eight blocks without executing vector `2068`, and its
pending IFR bit 10 remains set (`0438` at the observation endpoint). Thus lack
of an RX ISR is not by itself a routing defect in this lifecycle. Read IMR/IFR
through CPU state accessors: direct address-space reads at data `0/1` bypass
the core's internally implemented register access and do not measure them.

### Streaming notification consumer

Original TX handler `02:8afc` writes `b633=1` at `02:8b2e..8b30`, loads
AR2 with `806e`, then calls common routines `3f37` and `3ec2`. Original code
at `02:9549` reads `b633` and branches to `02:9676` when zero; code at
`02:9641` clears it. These are an inferred software-notification contract,
not a recovered official task name or proof of audio decoding.

`make check-mu4-storage-original MU4_STORAGE_BIOS=worker` adds a 100 ms
observation tail after the eight-block digital acceptance. It observes 9,844
TX words, 77 TX block completions and handler entries, 76 RX completions,
and `illegal=0` at PC `00306c`. Only the first 1,024 TX words are compared
against decoded DIN; the remaining transfer count is not independently
validated audio content. No controls, readiness or firmware state are injected.

The ISR sets `b633` 77 times; original initialization clears it once at
`02:96e3`. No firmware read or later clear occurs in this bounded run, and
its final value is one. Final inspection disables side effects and does not
count as firmware consumption. Thus repeated IRQ delivery is established,
but the candidate notification consumer is inactive in this fixture lifecycle.
Original initialization at `02:6d89` populates the `806e` object, including
`8074=0002` and `8075=9545`, an address pair matching the consumer's entry
`02:9545`. The worker gate checks that pair without changing it and separately
counts actual entry fetches (zero in the verified tail). Common code at `02:3f41` writes its first word
to zero; `02:3f57` writes `8071=802b`. At the observation endpoint the first
eight words are `0000,bdc8,0010,802b,0000,0000,0002,9545`.

Data `007e` is written to `4000` by original code at `002fa8`, `02:6d89`
and `02:6d0d`, and retains that value through the tail. Common routines test
it against masks `c001` and `c020`; its broader semantics are unresolved.
The object and word are firmware-initialized. Do not label `007e` as a missing hardware
register or assign official task, priority or stack semantics from its value.
The common routines use DP `017b` with CPL clear, so direct operands are
RAM at base `bd80`, not low peripheral registers. In the worker run,
`bdb8=0001`, `bdb9=0010`, `bde3=0000`, `bde4=8028` and `bde7=3da6`.
At `3f5b`, the accumulator is signed `-15` after subtracting `0010` from
`0001`; the AGT branch is not taken. At `3f5f`, loading `bdbc=1` into AH
and shifting left eight produces positive `0001000000`, so the ALT branch
to `3f6b` is not taken either. These are measured operands, not recovered
official priority or queue names.

`bdbc` is zero after initialization. The original TX ISR increments it at
`02:8b0d..8b0e` and decrements it at `02:8b52..8b53`. Thus its value one
inside the common helper is an interrupt-context condition, not absent
hardware initialization. The later common path loads `bde7=3da6` and
actually reaches `3da6`, whose instruction is `RET`. This callback alone
does not enter the notification consumer. After decrementing `bdbc`, the
ISR tests mask `8000` with `BITF` at `02:8b54`. Observed TC is clear and
`BC NTC` at `02:8b56` takes `02:8b6d`, the ordinary interrupt epilogue.
The alternative path through `02:8b63` would replace a saved return word;
it is not observed in this tail. Probes must cover the paged `02:8xxx`
program window, not just the common lower window.

Startup is still performing file initialization before scheduler activation;
the five-second window ends during the first `SETTING1.BIN` write through
`03:8c3e`. This is a bounded-window result, not the current execution ceiling:
the longer `startup` profile completes storage initialization, enters the
startup continuation and passes its 20-second original-consumer activation
gate. Consumer activity alone does not establish decoded audio.
Static original code has a call to common `3f1d` at `6d5e`, inside the
startup helper at `6d44`. Entry code calls that helper at `6dbf`, after the
far call to `02:904b`. The 100 ms worker tail observes one main-entry fetch
and zero fetches at `6dbf`, `6d44` or `3f1d`. Thus this startup continuation
has not executed in the bounded fixture; that is not proof it can never
execute. Do not synthesize a scheduler-enabling event, worker callback,
descriptor state or pending-word clear.

The outstanding main call is at `02:90f4`, targeting `03:9105`; earlier
executed far calls have observed return-point fetches, but `02:90f6` does
not execute in these windows. This routine resets an iterator through
`03:bb09`, reads through `03:bb3d/03:bb5f/03:bbd6`, and fills buffer `1444`
with original directory names (`MCUSI16`, `MP3SI16`, and the other uploaded
files). Later code at `03:9219..03:9221` supplies filename pointers to
`03:bcb6`: the original upload's data at `c146` is `TRACKLST`, and `c131`
is `BIN`. This is a concrete file lookup, not an inferred hardware request.
That lookup returns: execution reaches `03:922c -> 03:941d`. The latter
routine performs another `TRACKLST.BIN` lookup and takes its later path
through `03:bb09`, `03:bfdb` and `03:c898`, then returns. Subsequent calls
`03:90d6` and `03:8be6` also return. Thus an active parent `03:9105` does
not mean its filename lookup is still outstanding.

The last observed parent call is `03:9282 -> 03:8c3e`. Original C
initialization supplies descriptor `16b8` with `SETTING ` and extension
`BIN`; the caller writes ASCII `1` at descriptor +7 and passes a `001c`-word
payload. This requests `SETTING1.BIN`. Later static calls select ASCII `2`
and `3` for `SETTING2.BIN` and `SETTING3.BIN`. These are file operations,
not recovered service-message tags. `03:8c3e` searches the directory and
has file-handling paths through `03:c200`, `03:bb09` and `03:bfdb`.
Read-only call/return observations show the directory iterator finishing:
the name comparison eventually returns -1 and `03:bb4e` returns 1.
The routine then executes `03:bb09`, `03:bfdb`, and two calls to
`03:c2c2`; each returns. The first `c2c2` call receives the original
`001c`-word payload length; the second receives zero length. The next call,
`03:8cb9 -> 03:c404`, has no observed return in the five-second profile.
Thus the open operation is downstream of directory search and these two
transfer calls, not a stalled name lookup. Static `c404` code updates a
directory-entry buffer and invokes further storage helpers; its metadata
finalization/allocation contract and setting-record format remain unresolved.
Within the outstanding `c404` invocation, `03:bd13`, `03:c57f`,
`03:b91d` and `03:b9ae` return; `03:c44f -> 03:c4c7` remains active.
Its observed calls to common `345c` and `2096` return. A first call at
`03:c552 -> 03:c5b2` returns with input address `0001:e901`; the next
call, with input `0001:e980`, has no observed return. The earlier,
successfully completed `c404` invocation traverses the same helpers.
Consequently neither `c404` nor `c4c7` is uniformly non-returning.
`c5b2` uses buffer `b3f8`, cached address `b4f8`, read/flush helpers
`c57f`/`b91d`/`b9ae`, and filesystem-mode-specific masked updates. Runtime
captures establish mode 1: `c5f3` compares AR2=1 with AR0=4, then `c5f8`
compares it with AR0=1 and takes the matching branch. No fetch of the
invalid-mode loop at `c5fb` is observed. Initial cache reads at `c5ea`
return for addresses `0001:e901` and `0001:e980`; execution then reaches
the final section at `c6ad`. Earlier final calls to `b91d` at `c6ba` and
`b9ae` at `c6bd` return. For the active settings operation, the last
`03:c6ba -> 03:b91d` call receives cached address `0001:e980` and has no
observed return. Thus the mode selector and initial cache read are not
the open boundary. Resolve this last buffer/flush preparation call; a
bounded observation alone does not establish a deadlock or failed write
verification. Include the routine's final section when probing it:
truncating observations at `c6ad` omits its load-bearing child calls.
Return-site observations are pre-execution: AR2 is not a claimed result
until the following instruction transfers the returned accumulator.

Separate observation profiles extend time without changing firmware or
device inputs:

| BIOS | Tail | TX/RX DMA completions | NAND reads during tail | Startup continuation/helper/selection-start fetches |
| --- | --- | --- | --- | --- |
| `worker` | 100 ms | 77/76 | 25,434 | 0/0/0 |
| `settle` | 1 second | 697/697 | 134,969 | 0/0/0 |
| `scan` | 5 seconds | 3,453/3,453 | 905,143 | 0/0/0 |
| `startup` | 20 seconds | 13,789/13,789 | 1,870,113 | 1/1/7 |

Run them with `make check-mu4-storage-original MU4_STORAGE_BIOS=settle`
or `MU4_STORAGE_BIOS=scan`. Both retain the first 1,024-word independent
DIN comparison, original descriptor binding, digital conformance gates and
zero illegal-opcode requirement. Additional transfer counts are not validated
music content. In both windows the candidate consumer has zero entries and
`b633` has zero firmware reads. The storage reader remains active: do not
describe an endpoint PC as a stuck instruction or assume a missing peer.
The lower-call census for the settings buffer path observes 57 calls to
`03:c57f`, of which 56 return in five seconds. No page-program or verification
helper is observed in that path during the window. Static `b91d` sets AR6 to
32 and loops over 32 page reads; the recorded activity is a completed pass
and a partially completed pass, not a stalled single read or verification
retry. This bound justifies the separate longer observation profile without
altering the firmware, media or hardware inputs.

`MU4_STORAGE_BIOS=startup` requests a 20-second tail. It observes return
from main's `02:90f4 -> 03:9105` call, then reaches continuation `02:6dbf`
and helper `02:6d44`. Original `02:3d7e` uses `ea00`.
TI SPRU172C pp. 4-70--4-71 identifies `ea00..ebff` as
`LD #k9,DP`, replacing only ST0 bits 8--0 in one cycle. The core now implements
it, with 1,024 executable cases covering all immediate values, CPL/SXM
configurations, untouched accumulator/status fields and cycle timing.

The longer profile reaches the original streaming consumer and observes
repeated `b633` reads at `02:9549` and clears at `02:9641`. The 20-second
gate requires actual consumer entries, reads and non-initialization clears,
not an endpoint IDLE or a fabricated callback. It observes 1,739 entries,
1,739 reads, 1,740 clears and final `b633=0`, with zero illegal instructions.
Two independent fresh runs reproduce the endpoint and these counts exactly.
The endpoint is logical `02:3d9f`, not a claimed stuck instruction; no decoded
music is established. Physical `1820` is the
harness's loader-call IDLE sentinel, not a firmware idle loop; reaching it
cannot establish successful resident-worker execution.

The startup-helper audit narrows this boundary further. Original `6d4b`
enables IMR bit 3 (`0ac1 -> 0ac9`) while IFR is `0438`; the next instruction
is interrupted through vector `204c -> 02:3d73`. Its context-save routine
reaches `POPM BL` at `3d9e`, immediately followed by stack-relative
`STL A,6` at `3d9f`. Exposing the incremented SP `1275` immediately to that
store would place `4044` at `127b`, one slot above
the reserved return slot `127a`. No loader caller or worker event needs to
be synthesized to repair this CPU/context boundary.

TI SPRU131G section 7.5.5.1, table 7-9 specifies a one-instruction SP
latency from `POPM MMR` to a basic compiler-mode direct-address store.
The immediately preceding SP (`1274`) would put that store at `127a`, the
reserved return slot. The core now models this latency for ordinary `POPM`,
`PSHM` and `FRAME`. Twenty-four executable red/green cases cover basic and
extended-shift stores, immediate/NOP-separated consumers and CPL clear/set.
Native execution now writes `127a=4044`, preserves saved page `0002` at
`127b` and resumes `02:6d4e`. Other DAGEN producer groups and longer
latencies remain unvalidated. The bounded bus observer also records
extension reads, so only independently decoded instruction boundaries
establish this sequence.

The next native operation at `02:3e44` uses `fc43`, `RC ALT` under
SPRU172C pp. 4-133--4-134. It is implemented with five executable signed
40-bit boundary cases checking exact PC/SP outcomes and XPC preservation.
Core and ROM4 warm/cold regressions pass; these ISA checks do not establish
complete native audio execution.

Original `f7d9` at `02:9c74` is `INTR 25`, under SPRU172C p. 4-65.
It saves the following PC, selects IPTR plus four times K, clears the
corresponding maskable IFR bit and sets INTM independently of IMR and
prior INTM, in three cycles. The core implements all 32 encodings; 64
red/green cases cover INTM clear/set, two vector bases, stack/status/IFR
outcomes, page preservation and port-measured timing. VC5410A SPRS139I
table 3-21 names K=25 as HPI interrupt/SINT9. Original vector `2064`
branches to `02:9a32`. This software invocation is not proof of a physical
host transaction. The next acceptance contract must identify the original
consumer's command/data inputs and verify its output beyond the initial DIN
prefix; traffic volume alone is not audio correctness.

### Streaming worker mode and software dispatch

Passive instruction-boundary probes in the unchanged 20-second `startup`
fixture record 1,739 visits to the mode gate at `02:954f`, zero visits to
the subsequent flag gates (`9553`, `9557`) and zero visits to the buffer
processing entry (`9559`). The sole observed `bb80` write is initialization
to zero at `02:9099` (post-store PC `02909c`). All eight bounded entry
snapshots show `bb80=0`, `94de=0` and `0062=0`. The worker therefore services
notifications without performing its gated buffer-processing work; this is
not music playback. The static branch skips processing when `bb80==0`,
`94de!=0` or `0062!=0`, then clears `b633` at `02:9641`.

The final `aa22` section stream contains 48,261 unique extended program
words. A literal `bb80` census finds 17 matches, including eight clear
immediate-store candidates; this does not close dynamic or DP-relative
writers. The independently disassembled software dispatcher `03:acf2`
calls queue helper `03:9f5e` with destination `b9d2`, then dispatches the
selector at `b9d4`. Its program tables are `02:40f5` (selectors 41--60)
and `02:4109` (64--73). The latter selects these processing branches:

| Selector | Branch | Mode contract |
|---|---|---|
| `0x40` | `03:afcd` | Writes `bb80=1` at `afe2`, clears six paired counters and the RX buffers, then waits on `b64e` before its response and cleanup. |
| `0x42` | `03:af98` | Initializes `b66d/b66b/b66c/b64e` and writes `bb80=3` at `afbf`. |
| `0x43` | `03:aee8` | Parameter `b9d5=2` selects `bb80=2` at `af79`; parameter 4 selects cleanup ending with `bb80=0` at `b05e`. |

These are numeric software selectors, not established public command names
or physical HPI packets. Queue helper `03:9f5e` checks state `165d` and
indices `1659/165a`, calls `03:9bc2` with selector 2 to fill the supplied
buffer, and returns `0ff0` or `0ff2`. Recover that queue's producer and
framing before introducing external commands. The separately mapped
software `INTR 25` handler uses `02:a15b/a16d`; equivalence between those
helpers and this queue is not established. No mode, queue, callback or
command memory is injected by this fixture.

The processing queue's producer is now traced to serial receive, not the
HPI software handler. ISR `02:8ba9..8bbd` reads data register `0031` into
`1462 + [1657]` and advances the producer modulo 80. Parser `03:9dc7`
consumes this ring using index `1658`; its eight-state program table is
`02:4113`. The normal packet path accepts start word `001e`, a length
strictly between zero and 80, header `00aa`, that many payload words,
a sequence token and a checksum equal to the XOR of start, length, header,
payload and token. A new token with matching checksum enqueues type 2 at
`03:9e13` through `03:9b6d`. A repeated token takes the duplicate-acknowledge
path without enqueueing another command. The parser does **not** compare
the subsequent trailing word to `0055`, although its outbound builder emits
`0055`; do not invent that check. The sequence token is distinct from
fields inside the command payload.
An alternate start `007f` selects an acknowledgement path, not a processing
command. Low-byte versus full-word comparisons differ between states and
must be preserved when implementing wire tests.

Queue storage begins at `1554`, with used-word count `165e` and a 256-word
capacity. `03:9b6d` appends a type, next-record offset and payload;
`03:9bc2` searches for the requested type, copies its payload and removes
the record. The processing reader requests type 2. The response helper
`03:9f85` appends type 1, which `03:9c85` wraps for transmission and puts
through the separate 80-word TX ring `14b2` (`1659/165a`). Helper
`03:9c5b` writes its next word to register `0033` and updates GPIO `003d`
bit 4. The isolated fixture currently provides simplified receive ingress
at `0031`; default and `command` profiles retain storage-only TX register
history at `0033`. The separate wire profiles attach the shared TX device.

A literal far-call scan over the 48,261 uploaded program words finds four
calls to the queue append helper (`03:9d0f`, `9da9`, `9e13`, `9fb7`) and
one to the processing reader (`03:acfe`). These sites are independently
disassembled; this is literal-call coverage, not proof excluding indirect
calls or another overlay. Original framing and transmit/status completion
are verified by the profiles below. Physical board attachment and
processing output remain separate contracts.

The unchanged 20-second `startup` fixture executes the parser, processing
reader and dispatcher 174 times each, and the typed dequeue helper 348
times. It executes neither queue append nor TX-word enqueue. Both serial
ring index pairs, queue word count and parser state end at zero. Thus the
command service is scheduled but has no received packet in this fixture;
the dormant mode is not evidence of a missing software worker. The native
gate still passes with 1,739 notification services and zero illegal
instructions. The non-processing packet controls below distinguish
command ingress from transmit completion; neither selects a music mode
or asserts playback.

`MU4_STORAGE_BIOS=command` supplies the non-processing selector `0x49`
status request through `0031` and receive IRQ6, one byte per 1 ms bench
interval after the original dispatcher first runs. Its packet is
`1e 02 aa 01 49 01 ff 55`: two payload words, new sequence token 1 and
XOR `ff`. No command RAM, mode, callback or interrupt mask is written.
Firmware drains all eight bytes through its original RX ISR and parser,
enqueues one command and constructs the three-word acknowledgement
`7f 01 55`. TX ring indices end at `3/1`, and its first word `007f` is
written to `0033`. Four words remain in the typed queue; the command
reader returns while the TX ring is nonempty, so the status selector itself
has not executed. The startup worker still services 1,739 notifications,
with mode zero and zero illegal instructions. The missing boundary is
serial transmit completion, not proof of an absent firmware producer.
The bench interval is not a measured physical baud rate, and no TX wire or
music processing is verified. A packet without the sequence token drains
through the ISR but produces no queued command or acknowledgement.

#### McBSP2 transmit and acknowledged status query

Original initializer `03:9b07` configures McBSP2 through `0034/0035`:
SRGR1=`0080`, SRGR2=`2020`, PCR=`0000`, RCR1/XCR1=`0000`,
RCR2/XCR2=`0005`, then releases the receive and transmit resets.
The resulting SPCR2 is `0341`. This selects the supported externally
clocked, single-phase 8-bit format with data delay one; it does not establish
the board's clock source or baud rate. TX-ready uses XINT2 (IFR/IMR bit 7).
Original handler `02:8bfe` writes successive TX-ring words to `0033` at
`02:8c41`, advances `165a`, and clears busy flag `165b` and GPIO bit 4
when the ring is empty.

`MU4_STORAGE_BIOS=wire` connects those native TX writes and indexed control
operations to the shared McBSP device. The bench supplies 5 us half-clock
edges, with an 8-bit frame while the original GPIO bit 4 requests work.
These are explicit external test inputs, not calibrated board behavior.
The device generates XRDY and IRQ7 from DXR-to-shifter handoff; no firmware
TX index, mask, busy flag or completion is written. A separate consumer
reassembles TX callback bits and compares each byte with the reported word.
RX remains the previously bounded register/IRQ fixture, not pin-level RX.
The `0033` read helper is bench write history, not a claimed silicon DXR
readback contract.

The status selector `0x49` now reaches original branch `03:ada7`. Firmware
first emits `7f 01 55`, then the complete response
`1e 05 aa 01 71 01 00 00 80 40 55` through the TX pin callback.
The `80` sequence token is independent of the payload and yields XOR
`40`. Do not normalize that initial token to zero: the sender wraps its
next token modulo eight and the acknowledgement matcher masks three bits,
but the wire token and XOR retain the original `80`. Without a peer
acknowledgement, the original protocol emits three
identical copies (36 total wire bytes including the initial acknowledgement),
then clears its queue through its own retry lifecycle. This is not successful
peer delivery.

`MU4_STORAGE_BIOS=wireack` checks the complete response and supplies
`7f 80 55` through the existing receive-register/IRQ path. Firmware then
settles both ring pairs at `11/11` RX and `14/14` TX, empties the typed
queue, clears response-pending `165d`, and emits no duplicate response.
The gate verifies 11 input bytes, 14 output bytes, independent bit decoding,
mode zero and zero illegal instructions. Its 20-second worker window
observes 1,779 entries and 1,780 clears; changing the legitimate command
traffic changes scheduling, so these counts do not replace the unchanged
startup control's 1,739/1,740 oracle. No music processing, physical MA4-MU4
attachment or full native boot follows from this status transaction.

`MU4_STORAGE_BIOS=pins` drives McBSP2 frame/data/clock inputs instead of
writing DRR/RRDY or pulsing the CPU's receive line. The device owns RSR,
RBR, DRR, RRDY and IRQ6. Native `0031` reads and all indexed control reads
are delegated to that controller. The bench uses one delay cycle, eight
MSB-first data cycles and the subsequent receive-publication edges, with
the same explicit 5 us half-clock interval. Each next byte waits until
the preceding frame and RRDY have drained.

The pin fixture reproduces all 11 received bytes and 14 transmitted bytes,
with 11 device-owned receive interrupts, no overrun, both queues empty and
no status-response retry. Its 20-second window observes the same
1,779 worker entries and 1,780 clears as the register-RX `wireack` control.
The firmware generates the parser/command/response transitions; only
external signal inputs and peer acknowledgement are supplied. This does
not alone prove native transport save/replay, other receive formats or the
physical MA4-MU4 wiring/clock relation. The older loader preamble retains
its separately declared register-level ingress.

The resident selector `3a`, parameter `01`, is not an unconditional reload
command. At `03:b0fb`, states `0000` and `0001` in data word `3750` branch
past the mount/loader calls. Other states can reach those calls only with
parameter `01`. The coherent resident fixture reaches state zero: its external
request enters the dispatcher but does not re-enter loader `3538`. Changing
`3750` to manufacture a reload is not an acceptable reset test. This software
branch also does not establish the board's PURX reset lifecycle.

Worker `02:9545` has a numerical-processing path, not yet a demonstrated music
decoder. Selector `40` initializes mode `bb80=1`, two tone phases/increments
and amplitudes, then collects six stereo sample-energy blocks through
`02:87ea`. Tone generation uses `02:876a`, phase/table helper `02:87ae` and
scaler `02:8818`; mode three clears the transmit buffers instead. These
addresses identify original firmware behavior, not a public command API or
proof of playback. Validate arithmetic and response completion using external
serial inputs before assigning broader semantics.

`MU4_STORAGE_BIOS=measure` sends `1e 02 aa 01 40 01 f6 55` after the checked
streaming prefix. The original worker processes six blocks, each with 64
samples per channel. A passive observer independently computes the signed
absolute sample divided by 16, its square, the entry-time FRCT multiplier
and the preceding channel aggregate. All twelve stored results match. It
observes live CPU operand reads rather than an entry-time buffer snapshot:
DMA can update samples while the routine executes. This check is bounded
to nonsaturating sums below `7fffffff`, not all arithmetic edge cases.

The original response is `1e 03 aa 01 68 20 80 7e 55`, preceded by
`7f 01 55`. The peer acknowledges token `80`; all eleven receive bytes and
twelve transmitted bytes drain without retry or queued residue. Status `20`
is the firmware's numerical result for these converted samples, not an
assumed success code. Mode `bb80` returns to zero, completion `b64e` is one,
and the block count is six. No illegal instruction occurs. Cleanup clears
the RX buffers, so this profile does not reuse the idle profile's final
constant-buffer assertions; the independently checked startup prefix and
the untouched status/replay controls retain their own acceptance checks.
This alone does not validate tone generation or music decoding.

The same original command calls generator `02:876a` 90 times. Its two
64-word channels use phase words `bb79/bb7a`, increments `bb7b/bb7c`,
ROM lookup base `17fd` and amplitudes `bb7d/bb7e`. A separate integer model
folds the signed phase into the table index and applies the fractional
amplitude product with high-word truncation. Both ping-pong buffers and
final phase words match on every call: 11,520 sample words, including 26
signed phase wraps. The model explicitly rejects product saturation outside
this fixture's supported range. These are verified generated buffer values;
the separate `bootmeasure` profile below also validates continuous digital
transport and the generator-buffer-to-DMA contract. Analog sound, music and
speech decoding are not established by the buffer arithmetic alone.

The original directory scanner at `03:941d` accepts extensions `REL` and
`LSE` (strings at data `bff3/bff7`), excludes directory entries and caps its
catalogue at 150. Settings lookup uses `TRACKLST.BIN`; valid settings can
restore word `3768`, and the scanner publishes its count there. Selector
`30`, parameter `2`, rejects an empty `3768` before its media-state path.
The final uploaded program contains 26 exact literal operands for `3768`
across 48,261 words; this is not closure over DP-relative or dynamic writers.
A plain MP3 file is not an evidenced fixture for this original firmware.

External format evidence: the [repair-tool author's analysis](https://retrohack.eu/gsm/nokia-5510-dsp-repair-tool/)
reports 384 metadata bytes followed by a 512-byte LockStream header, with
MP3 data starting at byte `0380` (word offset `01c0`). These are independent
claims to check against the original R060 parser, not yet an accepted fixture
layout. Its modified MP3 firmware bypasses those reads and the parser; it is
not a substitute for validating the original firmware.
The [Nokryptia 1.3 author manual](https://man.freebsd.org/cgi/man.cgi?apropos=0&manpath=FreeBSD+6.0-RELEASE+and+Ports&query=nokryptia&sektion=1)
documents MP3-to-LSE conversion but explicitly does not implement Nokia's
encryption/decryption. Consequently, converter availability alone would not
validate the encrypted-container path or all original media formats.

Fixture-source distinctions: the archived [Nokia 5510 disc](https://archive.org/details/nokia-5510)
is promotional audio, not the Audio Manager installer: its original cue
sheet declares four `AUDIO` tracks. The retained ignored BIN has SHA-256
`920df3569a3bb343793aa17ad36855516eb1a2e70e31c35060ed206410c62169`.
The [N-Gage software CD](https://archive.org/details/nokia-ngage) contains
Audio Manager 3.0 help (`roms/nokia-audio-manager-reference/nam30.chm`,
SHA-256 `0fd337fd76798b956337ed02f2e58cc4c1333e76ec364e128683ded26bf490da`),
but that help does not establish 5510/LSE compatibility. Neither reference
supplies an accepted original R060 media container.

An exact-word scan of each final uploaded extended-program image finds
neither `0380` nor `01c0` in resident `aa22` (48,261 words). The three
`0380` operands in `aa44` at `02:de9c/deaa/dfe2` are AR2/AR3 sample-buffer
displacements in numerical loops, not file-offset evidence. Other overlays
also contain these values; equal literals across different segment images
do not establish shared routine ownership or a container parser. Resolve
storage-call argument flow and the selected overlay before assigning the
reported byte offset to an instruction.

The resident direct-call census identifies 22 exact `FCALL 2086` file-read
sites and 20 exact `FCALL 2092` seek sites; indirect calls and other encodings
are outside that count. Entry `03:9fe9` is an ID3 metadata reader: it filters
`REL/DGF/LSE` extensions, seeks to zero (`03:a035`), reads five words
(`03:a040`), splits them into ten bytes and compares the first three with
data `c0f0 = "ID3"`. Bytes 6..9 form a length with shifts 21/14/7/0.
Its frame identifiers at `c0f4..c10d` are `TPE1`, `TIT2`, `TCOM`, `TALB`,
`TYER` and `SYLT`. This establishes a metadata consumer, not decryption,
decoder selection or the stream-start offset. Entry `02:8385` likewise
inspects and can rewrite an ID3 header through the file-write path; do not
classify it as a read-only LockStream parser merely because it reads the
same file extensions.

Resident entry `02:9186` performs a named-file load: it mounts context
`3aea`, enumerates into descriptor index one (`36b0 + 44`), compares both
the supplied name and extension, and invokes unchanged `2080` with index
one only on a matching directory entry. Its exact direct callers are
`02:9fa8`, `03:a90f` and `03:a9c3`, using runtime name/extension buffers
`b54a/b553`. Initialization `02:8d0f` copies the active descriptor's name
and extension. Entry `02:9e90` replaces them with `RERSI16 .BIN` before
its `02:9fa8` load. The separate `03:a90f` path explicitly copies
`USBSI16 ` from data `c1a6`; it is not music-decoder selection evidence.

Media inspection entry `02:a1ed` opens descriptor index two, checks `LSE`,
processes metadata through `03:a3e3/a354`, seeks to zero and supplies file
reads to parser `02:c824`. A successful parser result selects a subtype
stored in its stack context: subtype 2 copies three characters `MP3` from
`c19e` to `b54a`, subtype 1 copies `AAC` from `c1a2`; either sets `3732`
bit 4. The consumer at `03:a9b7` requires that bit before invoking the
named-file loader at `03:a9c3`. This is static argument/state flow, not an
executed media-decode result. The parser's format contract and suffix
provenance at that lifecycle remain to verify; do not force its subtype,
the readiness bit or descriptor selection to claim decoder activation.

`02:c824` is the LockStream header recognizer, with its real entry at that
address (not a mid-routine tap). It expands packed bytes and fields using
`02:c750/c7bf`, then compares the signature with uploaded data
`be00 = "LockStream Embedded"`. It compares parsed double words at context
offsets `14/1a` against uploaded constants `bdfc/bdfe` (100 and 2).
Its explicit returns are `-3` for missing input/output pointers, `-1` for a
signature mismatch, `-8/-10` for those two version mismatches, and zero
after the recognition checks. All are static findings in original `aa22`.
The subsequent initializer `02:c902` rechecks the signature/versions and
performs additional processing through `02:cdf2/ccfa/cd28/cd34`; recognition
success does not establish initialized stream state, decoder execution or
audio output. Keep those as separate acceptance requirements.

Reproduce the initializer's direct-call inventory with
`noki5510_a00_inventory.py <original-a00> --segment aa22 --program-call-census 0x2c902 0x2c9cb`.
It covers 201/201 final uploaded words and reports eight immediate FCALL
encoding candidates, with no missing extension words. The tool preserves
per-segment ownership and final record writes; it does not infer instruction
boundaries, indirect calls, page-end fetch wrapping or physical aliases.
Review every candidate against a known routine-entry disassembly before
promoting it to a call edge. Unit fixtures cover overwritten operands,
conflicting overlays, holes and page-end candidates.

The initializer's processing dependencies are present in original `aa22`:
`02:cdf2` copies bounded template data through ordinary memory helpers;
it is not an allocation or external-service request. `02:ccfa` initializes
local processing state through `ce38/d54c/d5c5/ccb1` and returns 1
unconditionally. That return alone is therefore not a validation oracle.
`02:cd28` stores a processing counter in `ad46`; `02:cd34` handles a bounded
buffer of at most 512 bytes using local `cc06/d7a4/d69e/cc2f` helpers and
updates that counter. Exact algorithm identity and correctness remain
unvalidated. The caller `02:c902` independently compares its processed
block at `02:c99e` and returns `-5` on mismatch. A native initialization
gate must cover that comparison and final caller result, not merely the
nonzero returns from local helpers; later stream decoding remains separate.

The `reset` bench profile interrupts the first resident request after three
data bits, before any RX interrupt or transmitted response. MAME soft reset
clears McBSP2 control state. NAND is retained: unchanged firmware mount,
directory and read routines verify all six original upload payloads again,
then the original loader transfers the resident program. Enumeration also
observes firmware-created `TRACKLST.BIN` (1,800 bytes) and
`SETTING1/2/3.BIN` (56 bytes each); these are preserved, not replaced by
fixture data. Reproduce with
`make check-mu4-storage-original MU4_STORAGE_BIOS=reset`.

This diagnostic does **not** establish successful restart. During the new
20-second worker window, the resident command dispatcher is unobserved and
no request bytes or response bytes complete. The gate checks that bounded
frontier separately from storage/controller reset; it does not apply the
erased-media boot's sustained-interrupt expectation to retained settings.
This routine-level restart omits the original bootstrap's context
initialization; it is a negative control, not the full-startup reset
frontier. The complete uploaded software lifecycle below passes restart
and native status service. Neither establishes physical board reset wiring
or unavailable mask-ROM behavior.
The paired `check-mu4-retained-original` gate runs the `reset` diagnostic
and saves its stalled-endpoint medium through MAME's native NVRAM interface.
It validates the complete 69,206,016-byte device image, copies
it unchanged to the fresh `retained` BIOS's NVRAM name, and disables saving
in that second process. Separate working directories isolate the logs;
the two stored images must remain byte-identical afterward. The source and
copy in the verified fixture have SHA-256
`75a523a9c1b6a0f6d98d47812b3082717e0764a79e0956b47b2de4fa7160d6c3`.
The complete images remain ignored run artifacts, not redistributed ROMs.

The fresh process rereads all six payloads and all four settings entries,
passes sustained streaming/interrupt checks, and completes the exact
11-byte receive/14-byte transmit native status transaction with no retries
and empty queues. Thus persistent NAND contents alone are insufficient to cause
the routine-reset diagnostic's stall. The complete original startup
preserves that medium and supplies its own initialization, as verified
below; the comparison alone does not prove board reset behavior.

The paired gate also captures side-effect-disabled data-RAM observations at
the completed loader and settled endpoints. Each file contains 65,408
little-endian words for addresses `0080..ffff` (130,816 bytes); CPU/MMIO
registers below `0080` are omitted. These are observations, never replay
inputs. At the loaded boundary, the reset and fresh-process paths differ
in 24,345 words. Within `2000..3fff`, only these nine differ:

| Word | Reset | Fresh process |
| --- | --- | --- |
| `374a` | `000a` | `0000` |
| `374d` | `0001` | `0000` |
| `374e` | `0f36` | `0000` |
| `3753` | `0005` | `0000` |
| `3754` | `0005` | `0000` |
| `3756` | `0001` | `0000` |
| `3757` | `222e` | `0000` |
| `3b12` | `0001` | `0000` |
| `3b13` | `e980` | `0000` |

The first seven converge to the reset values by the settled endpoint in
the successful fresh run. The selection words `3732`, `3750`, `3768`,
`b633` and `bb80` are zero in both observations at both boundaries.
This bounds candidate startup-state differences but does not assign their
semantics or establish causation. Trace the original readers and writers
and compare peripheral/interrupt state before any corrective change;
clearing retained RAM to imitate a fresh process is not a reset contract.

Original startup has an observed one-time gate on `374d`: instruction
`02:90e8` loads it, `02:90ea` branches to `90ff` if nonzero, and
`02:90ec..90ee` stores one before the initialization tail. The fresh
retained process reads zero and writes one; the same-process reset leg
reads one and skips this tail. The tail clears `3768`, calls
`03:8f0d` and `03:9105`, and writes ten to `374a`. The `03:9105`
routine constructs a settings/file context starting at `16b8`; its
observed writers populate `3753/3754`, `3756/3757` and `374e` in the
successful retained-media process. This establishes a firmware-owned
initialization difference, not proof that the gate alone causes the stall.

A scan of all 51,921 final logical uploaded words in original `aa22`
finds the literal `374d` only in the load operand at `02:90e9` and the
store operand at `02:90ed`; independent disassembly confirms those two
instructions. This is literal coverage, not an absence proof for indirect
stores or reset-ROM/BSS initialization. The nine candidate words are
observed with read/write taps installed once across reset legs; each
access class records at most 16 samples per word per leg. No candidate
value is altered. The legitimate initialization writer belongs to the
separate serial-bootstrap overlay, not the resident's one-time tail;
do not bypass it by clearing `374d` or invoking the skipped calls from
the supervisor.

The original resident C-runtime entry `02:6d62` sets INTM, clears IMR,
establishes stack/status/vector-base state, and processes its own
count/destination/data table at `02:6dc7` before calling main
`02:904b`. This is distinct from the supervisor's replay of the serial
bootstrap's C initialization. The read-only command
`tools/noki5510_a00_inventory.py InitData_R060.a00 --segment aa22
--program-cinit-address 0x26dc7` accounts for the complete resident table:
240 records, 1,293 distinct data-word destinations, and a terminating zero
at `02:74b4` (exclusive end `02:74b5`, 3,548 encoded bytes).
It initializes the file contexts at `1444` and `16b8`, but none of the
nine differing words above, including `374d`. The separate `aa55` table
at `0f33` has five records writing `1700`, `1849`, `1746`, `1742` and
`1744`; it also does not cover those nine words.

The embedded-table decoder preserves overlay/final-write ownership,
rejects holes, missing terminators and wrapped data destinations, and
reports total versus unique initialized words. It does not invent zero
initialization for addresses absent from either table. Therefore the
current routine-level reset diagnostic does not demonstrate a missing
C-table replay; zeroing BSS or the gate would require independent original
startup/reset evidence. Recover the full serial-bootstrap/reset lifecycle
and its explicit memory-copy prelude separately from these C tables.
Physical board reset remains unvalidated; uploaded software restart and
resident control service are verified below.

#### Original serial-bootstrap lifecycle

Original `aa55` startup `0e41 -> 090f` mounts the medium, enumerates and
selects its files, then calls `08d1` at `09b3` before handing off through
`2080 -> 3538`. Routine `08d1` owns an explicit context initializer:
it clears `3732/3733`, `373f`, `3740/3741/3742`, `3745..3749`,
`374d`, `375c` and `375e/375f`, sets `373e=1` and `374a=25`.
In particular, `08f5..08f7` is the legitimate `374d=0` store. The
resident's `02:90ec` store sets this initialized gate to one later.
The resident-only literal census is not a cross-overlay absence claim.

The `bootstrap` BIOS loads all seven original `aa55` serial-boot sections
(10,760 words) into program RAM and executes the uploaded reset-vector
prelude at `ff80`, which branches to the serial header's `0e41` entry.
Code `ff82..ff87` sets AR0 to `0060`, then repeats
`MVPD 0060,*AR0+` 65,313 times, performing the original program-to-data
initialization before selecting PMST `ffe8`. The program upload includes
the C table at `0f33`, descriptor/template section at `36b0` and constants
at `184b`, in addition to the code slices used by the routine bench.
No program section is silently omitted. It does not assume a hardware
program/data alias; the original CPU instructions perform these writes.

The fixture uses a copy of the previously firmware-created NAND, with
no routine wrapper, supervisor C-table replay, state snapshot or NAND
erasure. This is execution of the uploaded reset-vector software, not
an unavailable mask ROM, a direct serial-ROM entry handoff, or physical
DA150 reset wiring. Reproduce it with
`make check-mu4-bootstrap-original MU4_BOOTSTRAP_SOURCE_RUN=<completed
check-mu4-retained-original directory>`. Its working directory and NVRAM
are isolated; saving is disabled and the copied NAND must remain identical.

The 20-second observation executes the original bootstrap and initializer,
observes `374d=0` at post-instruction PC `0008f8`, enters loader `3538`,
and reaches resident `6d62` once. The original directory-selected descriptor
names `MCUSI16 .BIN` and has length `0001:e1e8` (123,368 bytes), exactly
the original `aa22` payload length. Resident main sets `374d=1` at
post-instruction PC `0290ef`. The endpoint is `02:3d78`, with 762,113
NAND data reads and 1,633,006 stream words. This establishes the original
uploaded startup's resident transfer without a flag-clear shim; stream
volume alone does not validate output content, native music or control
transactions on this profile. The routine bench's earlier omitted-section
observation is superseded by the complete-upload gate, not a firmware
deadlock.

The `bootstatus` variant uses the identical complete upload and retained
medium. The existing external peer waits for the original command
dispatcher to run and for the firmware-owned receiver readiness state,
then clocks the unchanged status request and response acknowledgement
through McBSP2 pins. Run the same target with
`MU4_BOOTSTRAP_BIOS=bootstatus`. The original parser/dispatcher/queues
complete an exact 11-byte RX and 14-byte TX transaction, with no retries,
empty RX/TX queues, drained RRDY and mode zero. Its verifier is shared
with the routine-bench acceptance; no different response shape or relaxed
queue condition is introduced. The passive `bootstrap` profile remains
available without host request traffic.

The `bootreset` variant interrupts the first request after three data
bits, resets the controller and re-executes the complete original upload
through its reset vector. Original startup code clears and rebuilds its
contexts, enters the resident a second time and completes the same exact
status transaction. Run with `MU4_BOOTSTRAP_BIOS=bootreset`. The gate
requires two startup/resident entries, cleared controller state and
retained NAND, without supervisor firmware-RAM clearing. This validates
uploaded software restart, not the unavailable mask ROM or physical
DA150/PURX/rails reset sequence.

An unmapped startup I/O write to port `0080` (`00df`) remains a board-model
boundary. Physical board attachment and independently verified output
content remain unvalidated.

Next recover the accepted media container and storage layout before attempting
native playback. The original recorder is an alternative candidate producer of
valid media; its command/lifecycle boundary is described below. Investigate
physical reset and board attachment separately.
Do not write `bb80`, `3750`, `3768` or replay internal queue objects to select
a mode.

### Original recorder candidate

Nokia's [5510 User's Guide, issue 2](https://www.instructionsmanuals.com/sites/default/files/2019-05/Nokia-5510-en.pdf)
documents recording from radio or external audio equipment, saving a named
track, and subsequent playback (printed pages 58-59). Recorded radio files
can be copied to a PC but are described as playable only on the handset
(printed page 82). This establishes an original-firmware media producer,
not the MU4 selector, codec, container format or a verified recorder bench.

Original `aa22` dispatcher `03:acf2` consumes the selector from `b9d4`
and parameter from `b9d5`. Its `03:ad3d..ad47` subtract/table branch
covers selectors `29..3c`: twenty words at extended program `02:40f5`,
with branch destinations on page 3. Independent GNU C54x disassembly and
overlay-local final-write extraction give these relevant entries:

| Selector | Handler | Verified static contract |
| --- | --- | --- |
| `30` | `03:b349` | Parameter 2 requires nonzero track count `3768` before playback selection; not an empty-media recorder entrance. |
| `31` | `03:b332` | Calls `02:9e1a`; this requires lifecycle `373e == 4` before changing state and reloading index zero. |
| `32` | `03:b30a` | Parameters 2/3 call `02:9e47/9e51`, setting `3742` to 1/2. |
| `36` | `03:b1a9` | Parameter 5 checks `374e != 0`, sets `3750 = 3`, selects `3762 = 7/8` from `ba24`, then calls `02:9e90`. |

Entry `02:9e90` itself requires `373e == 2`. It sets that lifecycle
to 8, copies original uploaded data `c0a9 = "RERSI16 "` and
`c0b2 = "BIN"` to the named-loader buffers, builds a file context
at `9d0e`, and reaches the original `02:9186` named loader at
`02:9fa8`. A successful downstream return clears `3750` and restores
`373e = 2`. This is a recorder candidate supported by file-producing
context setup and the original overlay selection, not proof that the
overlay encodes audio or emits an accepted playback file.

The retained-startup RAM observation in
`run_mu4-retained.Yh686K/retained/mu4_ram_settled_leg0.bin` has
`373e = 0`, `374e = 0f36`, `3750 = 0`, `3762 = 0`, and `3768 = 0`.
That snapshot belongs to the retained routine-startup profile, not a
full-startup recorder run. Nonzero capacity alone does not satisfy the
recorder lifecycle. The concrete next question is which external command
or board condition lets original firmware establish `373e = 2` after
complete uploaded startup. Observe that transition before testing selector
`36/5`; never set the lifecycle or invoke the overlay loader from the bench.

Reproduce the command-table extraction with
`tools/noki5510_a00_inventory.py --segment aa22 --extract-program-range
0x240f5 0x24109 --disassembler-little-endian --output <new-path>` on the
original `InitData_R060.a00`. The twenty-word output SHA-256 is
`6f1e6fa4d54f681f90fea6f512a00127a6aa26b6013d3e55b3cfb4cad8d444c0`.
Coverage is this recovered twenty-entry branch only; other selector tables,
indirect state writers and actual recording execution remain separate work.

The pin bench registers its external receive/transmit waveform state,
request/acknowledgement cursors, serial shadows and NAND GPIO latches for
save states. Its growing TX observation vectors use bounded 64-word
snapshot storage with checked counts, rather than registering an empty
vector's fixed initial address and length.

`MU4_STORAGE_BIOS=replay` waits until the independently checked streaming
prefix completes, then checkpoints after three data bits of the first request
byte. Both original-firmware continuations reach the same worker-window
endpoint, consume the 11-byte request/acknowledgement stream, transmit the
same 14 response bytes and drain the firmware queues without retry. The
uncompressed terminal save streams match across all 1,039 registered
emulation-state items: CPU, RAM, NAND, codec, DMA, McBSP, external peer and
emulation timers. The only exclusions are nine fields of
`timer/lua_engine::resume/`: MAME's Lua post-load callback deliberately resets
that frontend timer, and this fixture runs no Lua script. Restore runs in
a separate control callback so the terminal-check timer retains its saved
start/expiry state. Observer counters and the test-phase sequencer are not
a restored lifecycle oracle. This proves the recovered status transaction's
native continuation, not arbitrary mid-boot checkpoints, native reset,
processing commands, physical board attachment or full MU4 boot.

`check-mu4-bootstrap-original MU4_BOOTSTRAP_BIOS=bootreplay` applies the
same settled mid-byte checkpoint and terminal-state comparison after
the complete original uploaded startup, using retained NAND supplied by
`MU4_BOOTSTRAP_SOURCE_RUN`. Both continuations satisfy the unchanged
status and pin-receive checks, and all 1,039 registered emulation-state
items match, with the same nine Lua frontend-timer exclusions. Original
startup/resident entry counts remain one: restoration resumes the DSP,
not the upload or reset sequence. Read/stream observation counters are
not restored hardware state and therefore accumulate across both legs;
their totals must not be interpreted as one uninterrupted playback.
This extends the verified checkpoint to complete-upload native control,
not music decoding or physical board reset.

`check-mu4-bootstrap-original MU4_BOOTSTRAP_BIOS=bootmeasure` sends the
unchanged measurement request through McBSP2 pins after complete original
startup, using the same retained-medium source. The measurement and tone
observers/verifier are shared with the routine-level `measure` profile,
not a second, weaker acceptance path. Six stereo sample-energy blocks
match independent arithmetic applied to live firmware input reads; the
12-byte response is acknowledged, all 11 received bytes drain, and the
firmware returns to mode zero with empty queues. The fixed 20-second run
also checks 90 stereo tone blocks (11,520 buffer samples) and 26 signed
phase wraps against independent table/phase/scaling calculations. Block
counts are observations of this fixture, not a physical timing contract.
The codec input remains a declared converted-sample fixture. These checks
prove native DSP processing and generated tone buffers after original
startup, not accepted media decoding, analog output or native speech.

The same profile continuously compares every McBSP0 transmitter word with
the independently pin-decoded AIC23 DIN word. A four-word bounded comparison
queue rejects overflow, unpaired codec words, channel-order errors and
sample mismatches; it is registered for save states and cleared on reset.
The complete 20-second observation matches 1,633,006 words, including
11,381 nonzero words, with no pending word at its endpoint. Acceptance
requires coverage beyond the old 1,024-word prefix, nonzero output and at
most one final launched-but-not-yet-sampled word; exact volume is not a
physical clock-rate oracle. This verifies sustained digital transport
during native processing; the source-selection proof below connects tone
samples to that transport. It does not model the DAC/analog output.

Original worker `02:955b..9588` writes DMA source-reload index `32`, selects
`ba79` or `baf9`, calls the generator and flips `bb86`. Each bank contains
64 left samples followed by 64 right samples. A passive data-read observer
uses side-effect-free DMA channel-3 source inspection to distinguish DMA
reads from CPU operands and other channels, without changing the DMA
register index. Every uncancelled generated word matches the independent
tone calculation in alternating left/right source order. Bounded queues
then require every observed DMA source word to match McBSP TX, followed by
the existing independent codec-DIN comparison.

The command's original cleanup matters: `03:b056/b058` zero both tone
banks while the last DMA block is in flight, before `03:b05e` writes mode
zero. The observer recognizes only those exact CPU store boundaries and
zero values, updates its expected source image and accounts for samples
cancelled before DMA read. This fixture generates 11,520 words: 11,362 are
independently verified through DMA and 158 are cancelled by original
cleanup. The complete source stream has 1,633,007 reads, 1,633,006 completed
transmitter words and one pending source word. Acceptance requires exact
generated = consumed + cancelled accounting, no premature bank overwrite,
stereo source order and source-to-TX-to-DIN agreement. These quantities
describe this bench, not real-board rates or a requirement that every
generated sample must play before a command stops.

`noki5510_a00_inventory.py --extract-program-range START END --segment aa22`
reconstructs final logical words in record order (last write wins), rejects
holes and verifies the selected wire checksum. Optional
`--disassembler-little-endian` exports GNU tic54x disassembly input, not
wire bytes or inferred physical aliases. Unit tests cover overlap, byte
order, holes, range rejection and checksum corruption.

McBSP error recovery is modeled for the supported externally framed,
single-phase 8/12/16-bit modes. SPRU302B section 2.3.7.4 specifies that TX
underflow lowers XEMPTY and repeats old DXR at subsequent frame syncs;
fresh DXR raises XEMPTY only when transferred to the shifter. A mid-frame
underflow waits for a new frame sync, even after refill. Section 2.3.7.1
defines receive overrun after three unread words: DRR and RBR survive,
incoming RSR data may be lost, and a side-effectful DRR read clears RFULL.
Executable gates check framing, status, ready edges, reset and save/restore
of both error states. Wider, internally clocked and multichannel modes are
not established by these tests. Routine profiles check the first 1,024 DIN
words; the complete-startup measurement profile checks digital transport
continuously as described above. Neither transfer counts nor pin-decoded
output alone prove music decoding.

Side-effect-free five-second boundary snapshots show words `142c/142d`
and `145c/145d` both remaining `0001,e9ff`; word `145e` changes from zero
to six. Do not label either double-word pair as a byte cursor from this
observation. The last NAND read-address sequence changes from column `00`,
row bytes `02,e9,01` to column `00`, row bytes `98,e9,01` (rows `01e902`
and `01e998` under the validated small-page address grammar). Thus the
bounded startup reaches later physical rows, rather than simply repeating
one row. These two samples do not prove monotonic traversal, the directory
extent or a termination time. In particular, these row samples must not be
attributed to the earlier filename lookup once its return is observed.

At index six, `03:bb7b` performs unsigned `CMPR` with AR2=`0006` and
AR0=`0200`; `03:bb7c` sees TC set and takes the read path. The iterator
limit here is 512 entries, recovered directly from the comparison operands.
Address helper `03:bd13` receives ordinal six. Subsequent
byte-reader entries at `329d` receive A=`0001:e9ff` and stack arguments
`00c0`, `00c1`, …, `00cb` with source descriptor `1422`; the later filename
lookup uses copied descriptor `3aea` with the same address and offsets.
The ordinal-to-offset mapping is six times 32 bytes. Original `03:bbd6`
reads eight name bytes, three extension bytes, then attributes and the later
word/double-word fields of that fixed-size directory entry.

The common reader's cache works in this observed path: desired word index
`0060` is preserved, the count rises from zero to `0060`, and later reads
reuse cached row `0001:e9ff`. At branch opcode `327e`, measured differences
are `005f`, `005e`, `005d`, `005c` as the count increases. The address builder
at `3035` increments the requested row to `0001:ea00`; `3050` is `5700`
(`DLD` into B), and subsequent masks/shifts emit the correct address bytes.
The iterator and this cache are not the current stopping point. Read-tap
register snapshots are pre-execution; probe opcode boundaries, not extension
words, and do not treat a destination register's old value as a return result.

`mu4_native_entry: PASS` establishes the two observed entry reads only;
absence of an illegal opcode is not a complete-startup acceptance criterion.

Three generic CPU contracts are independently exposed by this run:

- `6fe1 0010 0c48` at `3ac3` consumes long Smem displacement before the
  shifted-operation extension, not after it. The old synthetic `6fea`
  encoding is corrected while retaining its result/preupdate/cycle checks.
- `f808` in arithmetic helper `4a2c` is carry-clear conditional branch.
  C/NC, including delayed forms, now have eight status-preserving fixtures.
- `60e1 0002 0001` compares context field +2 with one. Long `CMPM` consumes
  displacement before immediate; four fixtures check match/mismatch and
  preupdate/no-update. The reversed decoder falsely reached trap `4330`
  despite a valid context type. Do not record that trap as a media defect.

The core suite and native ROM4 regression guard these corrections outside
the MU4 routine fixture. Unrelated multi-extension opcodes still require
independent evidence; a synthetic instruction stream is not an encoding
authority.

This exercises generic core contracts that matter beyond MU4: long-offset
`BANZ/BANZD` tests the effective Sind value and consumes displacement before
target (96 cases); `CMPR` compares unsigned ARx against AR0 (192 cases);
and block repeat uses ST1 bit 15 for BRAF without clearing bit 14 CPL.
The core gate asserts active and retired BRAF with CPL set. These rules follow
TI SPRU172C and SPRU131G; memory-copy extension ordering is also checked
against original firmware execution rather than a synthetic test alone.

This is original-routine execution, not a complete DA150 board: the fixture
holds ADD_H enabled, uses a test clock and separate test program/data maps,
and supplies a reset wrapper. It neither seeds a filesystem nor selects an
overlay. U201 is not attached to a handset yet; address-enable ownership,
physical memory mapping and original media placement remain prerequisites.

Next recover the storage-controller/media placement and independently locate
the overlay selector before constructing a native music-DSP fixture. A valid
FAT disk alone is not evidence that any overlay payload has been loaded.

### Firmware consumers

#### MU4-specific receive contract

Receiver `335cfa` first recognizes source node `28`, transport `1e` and
byte 7 equal to `42`, then calls `335c1c`. Local class routing supplies
byte 6 = `d2`. This incoming `d2/42` pair differs from outgoing constructor
`335a50`'s `42/d2`; do not mirror the wrappers blindly.

`335c1c` treats opcode `[message+9] == 06` as the power-up indication and
calls `335aec`, which enables the GPIO output and constructs request `47`
via `335aae/335a50`. Opcode `6f` calls component validator `335b12`:
count zero defaults to five entries; counts 1..5 refresh the cached
results, with bytes starting at message offset 12 equal to `03` for PASS.
Larger counts skip the refresh, not an explicit malformed-packet rejection;
they are outside the established five-component contract.
Failure takes the analog/service-battery branch and can post shutdown
report 7; success publishes internal `cc` through `335ba4`, initializes
GPIO through `3a7502` and changes the local status flags. Neither result
is an excuse to claim the absent music hardware passed self-test.

With initial context bytes at `125c0c+18/19` equal to `00/0c`, opcode
`01` is the ordinary key indication: state `[message+0a]` zero calls
release `313b2c`, state one calls press `313d84`, and the key is at offset
12. Codes `2c/2d/2e/32/33/34/35/36` instead enter `335bf0`. Other context
states redirect the message to the recorded consumer. This is the normal
MU4 consumer, distinct from generic diagnostic selector `0c` below;
QWERTY position-to-code mapping and incoming wire framing still need
independent evidence before host bindings are enabled.

The pinned image's GPIO reader `3a738c` temporarily enables mask bit 1 at
`2006b`, reads `2002a` bit 1 and returns `81` for low or `ff` for high.
It does not scan rows. Decoder `39dc14` maps `ff` to `3e`; otherwise it
uses mode byte `126ec6` to select a 25-byte normal table (`44d164`) or a
five-byte special table (`44d180`). For mode zero, raw `81` selects special
code `3c`. These are numeric firmware codes, not validated host bindings.
The ordinary table's presence does not establish an attached MU4 matrix.

IRQ consumer `3a73d8` independently tests `2002b` bits 1 and 3, calling
`39dae8` and `335624` respectively. The latter posts literal `0128` through
`2cdf42`; its physical source and subsequent input transport are unresolved.
Do not combine these branches into a borrowed handset key map.

Initializer `3a7424` writes `24` to port-direction register `2006a`,
`3f` to data register `2002a`, and mask `75` to `2006b`. Routine `335632`
sets data bit 2 when enabled; its disable branch applies mask `1b`.
Consequently this port is used as mixed GPIO, not merely matrix inputs.
The package checker pins both code extents. NPM-5 now selects optional
mixed-port direction `6a` and pending status `2b` in the keyboard/GPIO
device. Direction bits select output-latch readback; output drive does not
generate input-pending events. This polarity is inferred from the own
initializer and bit-2 writer, not an independent silicon measurement.
Other profiles retain their existing matrix-only contract. Inputs remain
unbound rather than pretending the QWERTY matrix is attached to MAD2.
`noki5510_gpio_conformance.lua` verifies output-low/high readback and pending
status preservation in a synchronous MMIO fixture, restoring the port
before firmware resumes. It is controller self-conformance, not MU4 or UI
acceptance; the hybrid lifecycle gates also pass with this configuration.
Do not equate status bit 3 with connector ROW3 merely because the numbers
match; the column/row pin routing still requires its own decode.

`verify-5510-package` pins the consumer code, literal pools and both tables
in its `input_contract` report. This is static evidence, not behavioral
acceptance. `noki5510_input_observe.lua` observes the GPIO initializer,
reader, IRQ handler, key decoder and secondary interrupt publisher without
changing firmware state. A fresh eight-second `nmp5hle` run confirms GPIO
initialization and two reader entries. Register-bearing debugger actions
must use ARM `r14`, not the unsupported `lr` alias; earlier absence claims
from those actions are withdrawn. No input bindings are enabled by this result.

The candidate serial UI receive path `335cfa` calls dispatcher `335854`.
Selector byte `[message+9] == 0c` selects `33594c`: `[message+0a] == 1`
calls `313d84` with key code `[message+0c]`; state zero calls `313b2c`
with the same code. Other state values select the failure response. The
response uses selector `0d` and result 1 for these accepted branches.
This is a decoded MCU-side message contract, not an established byte stream
or permission to inject messages into the RTOS. The key routines are also
used by the legacy local-key path, corroborating input ownership without
assigning QWERTY positions from another handset.

The observer confirms task entry `335f1e`, initialization `33565a` and
receiver `335cfa`. The received six-byte internal control object has byte
6 equal to `d2`, byte 7 zero and payload bytes `02 02 1e` at offsets 9..11.
The zero byte 7 selects disposal before the key dispatcher; this is not a
keypress or proof of external MU4 traffic. Dispatcher `335854` and probe
constructor `335dac` remain unobserved in this window. The package checker
pins the dispatcher's full code extent. Recover the control/probe route and
wire transport before implementing a peer.

The local class router `362958` scans all 39 eight-byte records at
`437ca4`. Class `d2` selects task `1d` (29); destinations below `1e` are
task numbers, while larger values are indirect handler addresses. Task 7
(`362d88`) runs the message router through `362d10`, not the key consumer.
Constructor `362de0` builds the observed six-byte `d6/d2` control object:
byte 7 is zero, byte 8 is argument 0, bytes 9/10 are argument 1 and byte 11
is argument 2. It queues to task 7 through `2cee1c`. Thus the observed
control object has an MCU-side producer; it does not demonstrate an MU4
response. The package report enumerates the entire local class table, but
the external FBUS-to-class-`d2` ingress remains unvalidated.

Serial ingress helper `399fbc` copies/queues received objects to task 7.
Its decoded call sites include sequenced/fragmented receive completions
`2f58ac/2f5994` and the separate single-wire parser completion `3841e2`.
The latter reads MAD2 register `1a`, accepts sync `1f`, collects the
header/length and body, and calls ingress; it is not the MU4 FBUS parser.
A cold sixteen-second hybrid run captures two objects at `399fbc`, both
from `2f58ac`, with transport `1e`, source `02`, destination zero and
classes `01/04`. These do not establish MU4 traffic. The sole observed
local `d2` delivery follows constructor `362de0` with arguments `01/02/1e`
and router caller `362d5e`, confirming the internal-control interpretation.
The next boundary is the sequenced receive path's external sender and
startup exchange, not an inactive generic receive queue.

The two observed ingress classes are the declared external-service HLE's
discovery replies: `nokia_external_service.cpp` echoes the discovery body
and changes its class from `01` to `04`, using source node `02`. They are
not independent MU4 evidence. Availability setter `399eca` marks a node
when task 7 processes its source address; `399e6e` clears it. Thus node
`28` availability must be established by its own sender, not borrowed from
the service node.

Secondary IRQ routine `335624` schedules timer `0128`; packed delivery
`01e8` selects the third case of `335ea8`. That case checks bit 3 of its
status byte and the three-byte state comparison `335a90`, reaching
`335df4` only if the comparison fails. Event `01e6` is a separate first
case which cancels `0126`, checks the same state and selects either the
probe gate or control `c8`. This identifies possible software entrances,
not the physical source of the secondary GPIO interrupt. Do not synthesize
that interrupt or a node-`28` greeting without the MA4/MU4 wiring contract.
A fresh sixteen-second hybrid run observes only timer event `01e7`, with
no secondary IRQ publisher, probe gate, node-`28` send or availability-set
entry. This is bounded runtime absence, not a proof those paths cannot run.

### Candidate serial-UI task and service-battery control

Task 29's physical MU4 ownership is not established. Its decoded key
consumer and serial routing make it a candidate, but its analog-gated
control branch is consistent with service/test operation, not proof of
ordinary MU4 startup.

Initializer `33565a` arms timer `0127` with argument `0469`. Its RTOS
record at `10c3b8` has owner `1d` and armed state `02`; a longer cold run
observes countdown and packed delivery `01e7` to `335ea8`. The eight-second
window is insufficient to establish that this timer or its consumers are
inactive. A sixteen-second run covers the observed first delivery, not a
measured hardware timing guarantee.

Event `01e7` calls predicate `335b74`. It requires two selector-3 ADC
reads in the inclusive range `00da..010e`, then selector 4 below `015c`.
Success calls `335e18`, constructing control selector `ca` through
`399f12`; failure calls `391830`, posting report 7 to task 1. The current
research inputs return selector 3 = `0253` twice, failing the upper bound
before selector 4 is read. No control `ca` or probe `fe` is observed.
The documented 22 kohm service battery maps inside this window; the normal
BLB-2 maps outside it (derivation below). This supports a service-battery
interpretation, not an MU4 presence detector. Do not tune normal inputs to
make this branch succeed.

Probe `335dac` is a separate constructor reached through `335df4`, not
the successful `01e7` branch itself. Its node-`28` message is rejected by
`399f12` with result 4 if that node's availability bitmap bit is clear;
otherwise it proceeds into the firmware router. Neither a matching peer
reply nor a wire-level key grammar is established by these constructors.
Check timer evidence with `noki5510_bootstrap_check.py --runtime
--serial-readiness --input-lifecycle --input-timer <error.log>`.

### Analog evidence and startup rejection

The [NPM-5 service archive](https://www.eserviceinfo.com/downloadsm/26142/Nokia_5510.html)
contains full primary manuals, unlike truncated recent previews. General
Information page 4 specifies BLB-2, part `0670246`. MA4 power schematic
A3-6 shows R221's 150 kohm BSI pull-up to VBB and R222's 100 kohm BTEMP
pull-up to VREF. Technical Information page 22 identifies VREF as 1.5 V
and says it references some CCONT ADCs. Do not equate VBB and ADC reference.
The [NSM-3 battery specification](https://www.eserviceinfo.com/preview_html.php?fileid=5456&previewid=3011)
independently identifies the same BLB-2's nominal 68 kohm BSI resistance.
The NPM-5 technical manual's battery table instead lists BMC/BLC packs;
it does not override the product-specific BLB-2 identification.

The [Nokia NSE-8/9 CCONT ADC specification](https://manualmachine.com/nokia/3210/8179317-service-manual/)
(System Module, tables 28/29) identifies the BSI input's 1.5 V reference.
Using that shared CCONT input contract with NPM-5's own divider gives
`1023 * 2.8 * 68 / (1.5 * (150 + 68)) = 595` (`0253`) for BLB-2.
The documented 22 kohm service battery gives 244 (`00f4`), inside
`00da..010e`. The 100 kohm VREF pull-up and nominal 47 kohm NTC at 25 C
give BTEMP 327 (`0147`). These are nominal electrical inputs, not measured
NPM-5 calibration; no other product's board divider is imported.

`37bb96` also samples selectors 3/4 during startup, independently of task 29's
later timer. A cold fixture with selector 3 = `013f` and selector 4 = `0147`
enters power-down `3aa5c2` from `37bc4e` before loader entry. The `013f`
calculation assumed an unverified ratiometric VBB reference; it is not an
accepted profile. The NPM-5 research profile now uses the independently
derived `0253/0147` pair. Fresh cold runs preserve both the native missing-mask
boundary and the hybrid discovery/self-test, serial-readiness, task-entry
and first timer-delivery checks. The timer predicate still fails normally;
this does not establish an idle boot. Other ADC channels retain explicitly
unvalidated research defaults.

Full PDFs are retained outside SCM in `roms/reference-docs/npm5/`:

| File | SHA256 |
| --- | --- |
| `02-npm5-general.pdf` | `496cb3c951b12372fde63a78fc229a0ec0dab5a1aee72e20f70a87dfff0f5a23` |
| `03-npm5-techinfo.pdf` | `7e9fa960b8d1e843be092ed4e44015584693f7b58431f9617b2d3ee9793a5043` |
| `04-npm5-userif.pdf` | `59c2505dc498faae9c9f45ebcfa78432f77a3841cd4b8871d087a806d3b77d9d` |
| `npm5-ma4-a3.pdf` | `06763fb93fc2073c02357e20f8c0d7379e5b135bdde931aa7165e3278d8d7531` |

### Task creation and serial readiness

Creator `2ceb7c` consumes 30 twelve-byte descriptors at `419d20` and
constructs contexts with initial scheduler state `05`. Index 29 at `419e7c`
contains entry `335f1f` (Thumb), stack size `0320`, priority byte `64` and
queue fields `28/0a`. Its receive loop accepts message byte 6 equal to `d2`.
The fresh run confirms task-29 state `05` and an allocated stack before
ordinary startup, followed by positive task entry and receiver execution.

Lowest-priority task-0 supervisor `37be9c` tests `399e54` first in its
idle/sleep eligibility chain, not a task-activation barrier.
That predicate requires `38b1a8` and `2f5d72`. The former requires four
serial busy/count bytes (`120c70`, `120c74`, `11cddc`, `11cdd8`) to be zero,
FIQ-mask bit 3 to be set and delayed event `7b` absent. The original research
profile disabled MBUSTIM: `120c70` and `11cddc` remained 1, mask was `c0`,
and the predicate returned zero. Own routine `383fa0` explicitly arms the
terminal timer while starting queued serial work.

The NPM-5 profile now enables the existing MBUSTIM controller model. A fresh
run observes actual queue writes from 1 to 0, mask `c8`, and readiness 1;
no reply bytes or firmware state are injected. The supervisor next passes
`3139a4` but fails `31475c`, which tests byte `124880`, predicate `2c03c6`
and byte `11ac2a`. Failure selects the supervisor's busy-wait path instead
of sleep; task 29 executes despite it. This is not an idle-screen boot gate
and does not resolve erased NV self-tests. The final
service-screen PNG is byte-identical to the timer-disabled control.

Validate verbose `noki5510_input_observe.lua` evidence with
`noki5510_bootstrap_check.py --runtime --serial-readiness --input-lifecycle <error.log>`.
The added check requires ordered task creation, queue occupation, queue
drainage, the ready observation and positive candidate-task lifecycle entries, not just
a final flag or an absence of trace output. Native
`nmp5stage` still passes its separate missing-mask boundary check. MBUSTIM's
existing cadence remains an inherited controller approximation, not a
measured NPM-5 timing specification.

Next recover the MU4 serial interface and seek matching product-state evidence rather
than fill identity/security fields from another phone. Keep the missing PMM, MU4 interface
and resident DSP inputs explicit; no donor provisioning or guessed success
publication may be used to claim graphical or phone-service parity.
