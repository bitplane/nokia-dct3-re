# Nokia 5510 NPM-5 bring-up

## Current boundary

Acceptance is profile-specific; these results must not be merged into a single
claim that the5510 boots or that all recorder backends pass:

| Composition | Accepted boundary | Not established |
| --- | --- | --- |
| `nmp5stage` / `nmp5hle` MCU | Own loader to missing resident code / graphical failure frame | Usable handset UI, SIM, RF or calls |
| MU4 staged recorder, `bootrecmeta` and persistence gate | Original metadata save, retained storage and independent cold reopen | Complete recorded-file suffix or full playback |
| MU4 atomic `bootrecsave` | Retained warm-return/timer negative control | Successful metadata delivery; this is not the staged recorder frontier |
| MU4 staged `bootplaystage` | Bounded original decoding and digital output | Intended EOF/drain policy or whole-track waveform acceptance |
| Standalone C54x instruction/codec controls | Their named matrices and original-code algorithm comparisons | Handset lifecycle, physical timing or native end-to-end speech |

`make check-mu4-recorder-persistence` composes fresh original initialization,
retained six-file readback, staged metadata save and a separate cold-open
process. It uses the saved endpoint as its reopening input, verifies the
original scanner/ID3 title and preserves the source NAND. Evidence is isolated
under `tmp/native-gates/mu4-recorder-persistence.*`; successful metadata or
catalogue checks must not be presented as playback acceptance.

The acquired v3.53 PPM C service package is normalized and statically
validated. The `nmp5stage` research fixture executes its own verifier and
loaders to absent mask address `2c75`. A separate `nmp5hle` fixture reaches
the graphical `CONTACT SERVICE` diagnostic frame. No idle boot, input, SIM, registration,
call or SMS acceptance is claimed. The package supplies MCU and PPM record
streams, not a matching handset PMM or an internal DSP mask image.

The independent MU4 music-DSP bench completes original recording control,
storage and metadata transactions. Fresh finalized record/save/cold-reopen
reaches 376 native AAC calls with consumed-input coverage. The protected and
fresh before-metadata 375-call controls remain separate inputs. Independent
decoder arithmetic and decoded-bank copying cover eight calls; signed output
scaling and the configured five-section processing kernel have bounded operand
checks. The authoritative quantities, source identities, host-comparison limits
and formal-conformance requirements live in the [AAC validation contract](aac_validation.md).
Recording ingress is separately bound through native transport and the
sample-repack ABI to original-word reads within each processing call;
a same-endpoint host projection also carries the captured input signal.
Full recording-transform coverage validates 4539 calls and 4498 contiguous
data windows. Including the stop-time pending RAM carry yields 387 complete
host-decodable packets / 396288 stereo frames, but the independently extracted
file diverges after 143360 matching data bytes. Its missing native suffix is
accounted for in pending buffers, not committed media. Persisted-tail policy,
codec conformance and complete native playback remain open; the
[AAC contract](aac_validation.md#pending-stop-buffers) owns the evidence.

Every captured channel-3 source read binds to an owned DXR bus write, and
every completed TX callback pairs with the next codec DIN sample. Reset-aware
sequence accounting distinguishes fresh-write completions, retained-word
repetition, reset discards and an unattributed pre-reset prefix. Actual
internal DXR-to-XSR loads/completions are now observed over a bounded interval:
64 stereo sample frames follow decoded copying, signed scaling and processing
through owned DMA/DXR epochs to codec DIN. Serial-buffer accounting also spans
the original EOF cleanup and restart, with every captured DXR write and TX
completion accounted for. Full-session decoded-sample attribution and physical
pin timing remain unvalidated. See the
[bounded serial contract](aac_validation.md#actual-shift-buffer-and-din-epochs)
and [transport provenance](#dma-source-to-dxr-ownership).
The [codec control/startup contract](aac_validation.md#codec-routing-and-startup-phase)
accounts for the initial discarded DMA word and paired headphone volume/mute
writes. An [ideal DAC/headphone contribution](aac_validation.md#ideal-dac-to-headphone-output)
now renders native music-DSP samples through MAME speakers; converter filters,
analog mixing/electrical behavior and whole-handset audio remain unimplemented.

For the protected 143680-byte recording, termination is a firmware file-length path, not an established
missing DSP event: 375 refill completions supply 71,834 words; the next
186-word request crosses the 71,840-word file boundary. Original code
enters cleanup before publishing that request's completion. A passive RAM
snapshot still contains a tail matching the host projection at fixed packet
ordinal 375, including eight reference-valid packets and its known-rejected
final packet. The default control is one; the identified input-17 sender
cannot clear it with a zero payload, and the alternate path includes a
seek/read rather than a proven drain-only operation. See
[refill and tail contracts](#refill-control-and-buffered-tail).

The intended product cleanup/drain policy and independent waveform acceptance
remain unresolved; the strict audio-completion gate stays rejecting. The
protected control's 383-packet host prefix is not proof of native decoding beyond 375
calls or validity of its final, 384th packet. Do not force counters, keys,
lengths, file offsets or completions, replay RAM, fit a lag, change the
converter or substitute reference samples. These results establish neither
whole-handset boot, complete music playback, analog audio nor native speech.

A separate original ISO MPEG-2 LC mono vector now has an isolated native
ABI prefix: 342 packets / 350208 mono samples, exact consumed-input coverage
and source-derived mono conversion/duplication. The unchanged final packet
remains behind the wrapper's short-input boundary. The original 24-bit
reference is acquired and checksum-qualified, but waveform alignment and
codec conformance are not established. See
[original AL09 acceptance](aac_validation.md#original-mpeg-2-al09-input).

Processor execution and regression contracts are maintained in
[C54x execution contracts](c54x_execution_contracts.md),
separately from this handset/loader boundary.

## Independent MU4 acceptance

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
DMA/McBSP and codec digital interfaces have isolated conformance and
save/replay tests. CPU IDLE2/3 now stops the McBSP internal bit generator through a
device clock-stop callback while leaving external frame/clock pins active.
Coupled CPU fixtures cover paused-word save/load and masked/enabled external
serial wake. This is not blanket DMA suspension: SPRU302B sections 2.10/3.8
allow externally triggered McBSP/DMA work during deep idle. The generic DMA
device now accepts a separate X2/CLKIN rate and CPU PLL-stop line. Coupled
conformance fixtures verify the IDLE3 source switch, absent-clock pending
transfers, enabled/masked completion wake and save-state replay. MU4 now wires
that switch to the schematic's 11.9952 MHz Y101 input. The original startup
divider/PLL sequence now controls CPU execution, core timer, DMA and internal
McBSP rates after input-derived transition delays. The schematic's reset
straps now select a documented C5416-compatible DIV/2 profile. Exact DA150
reset-table identity, automatic PLL-selected IDLE3 wake timing and sub-cycle
switching remain unvalidated. See
[the CLKMD contract](#original-clkmd-contract).
The `startup` profile passes its 20-second window with zero illegal
instructions and an active original streaming consumer. With selected CLKMD
rates it observes 189 consumer entries/notification reads and 190 clears;
the earlier fixed-rate counts are not acceptance constants. Mode zero skips
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
continuation across the registered emulation-state items selected by its
replay checker. The
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
The separate `bootrecord` negative control reaches the original `RERSI16.BIN`
loader but executes stale resident vectors because program/data RAM are separate.
The provisional `bootrecordram` profile shares RAM at `2000..7fff`, verifies
the overlay destinations, executes its original startup and begins NAND
programming through a clean 20-second execution window. The separate
`bootreccontrol` profile completes native receipt and acknowledgement of a
further control packet, but its previous-command filter rejects the repeated
selector before parameter dispatch. `bootrecpoll` first issues the original
read-only status query, then reaches the control handler and returns through
the native loader to `MCUSI16`. The atomic recorder frontier is warm resident
timer/receive recovery before the save/name transaction, not initial serial
receipt, control dispatch or an
unsupported instruction. The poll sequence is a bench protocol test, not a
recovered handset sender trace.
None of these profiles proves a recorded track.

Nokia's [NPM-5 service manual](https://www.manualslib.com/manual/1166046/Nokia-5510-Npm-5.html)
identifies separate MA4 and MU4 assemblies, including UI-module keypad,
display, USB and music functions. These are product differences to recover,
not permission to borrow another handset's keypad or UI completion events.

Processor execution and regression contracts are maintained in
[C54x execution contracts](c54x_execution_contracts.md),
separately from this handset/loader boundary.

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
| `13fbec`, code `0c` at `24c85a` | `24c612`: sum16 of `0120..0255`, excluding `0154..0155`, compared with u16 at `0256`; sum OR u16 at `0170` must also be nonzero | `32cc != ffff` |

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

### Logical NV request inventory

Own reader `2fd578` takes logical byte offset in R0, destination in R1 and
count in R2. Wrapper `3ac638` saves R4/R5/R6/LR, acquires mutex 5, calls the
reader at `3ac64c`, releases the mutex and returns its status. At reader
entry, the wrapper's saved caller is at SP+12. This is an ABI/context
observation, not proof of copy retirement or the returned bytes. The decoded
range test covers the observed nonwrapping requests; it is not a proof of
behavior for arbitrary overflowing 32-bit arguments.

`noki5510_nv_reads_observe.lua` inventories reader entries and captures the
unchanged logical cache through the existing self-test observer. In a fresh
eight-second hybrid boot it records 132 requests, 121 distinct ranges and
2926 unique requested bytes out of the 14336-byte cache. All observed callers
match 30 direct Thumb-BL sites. The source census covers all 1736703 aligned
halfword pairs in the pinned 3473408-byte image: 75 direct wrapper-call
candidates, one odd wrapper pointer, and one direct low-level reader call
from the wrapper. The other 45 direct candidates are unobserved, not absent
or unnecessary. Computed/ARM calls, direct cache accesses and code/data
classification remain outside that census.

The ordered requests include `0000+0120` from return `2a05a5`,
`0120+0136` from `24c671`, exclusion word `0154+2` from `3368b3`, stored
sum `0256+2` from `24c82f`, and guard `0170+2` from `24c83d`. The two
checksum failures and literal CONTACT SERVICE frame reproduce. Failure code
`0c` is stored by `STRB` at `24c85a`; `24c858` is the preceding register
move and must not be used as that store's identity.

Validate with `noki5510_nv_reads_check.py RUN --firmware
roms/5510-npm5-v353/5510f353c.fls`. Its JSON contains the complete range and
caller inventory, source hash and quantified encoded-reference coverage.
Use the fresh eight-second debug/verbose composition from the hybrid frontier
with this observer. The existing `noki5510_bootstrap_check.py --runtime`
also passes on that capture; its transport evidence requires `-verbose`.
It rejects capped/malformed captures, changed wrapper ABI, out-of-domain
requests, missing ordered checksum consumers and changed erased-NV controls.
This narrows the field-contract investigation; it does not assign semantic
names to the other ranges, certify valid provisioning, or authorize filling
unknown identity/security/calibration fields. No checksum or NV value is
modified by the observer or checker.

### Equipment-identity consumer contract

The own image identifies a concrete use of `000c..0013`: routines `2a0640`
and `2a066a` read eight bytes there and call formatter `2a05cc`. The formatter
consumes the first seven bytes as fourteen high-nibble-first equipment digits;
this does not classify the eighth byte for other consumers. It repacks those
digits into mobile-identity value bytes. Selector 1 produces eight bytes with
type 2/odd digit count and a zero transmitted spare digit. Selector 2 produces
nine bytes with type 3/even digit count, two software-version digits and a final
high-nibble filler. The type/header half-byte is part of these value bytes;
they are not simply the abstract eight-byte packed IMEISV storage format.

These operations match IMEI/IMEISV structure, including the distinction
between display check digit and transmitted zero spare digit described in
[RFC 7254 sections 4.2/4.3](https://www.rfc-editor.org/rfc/rfc7254.html#section-4.2).
The own ROM software-version byte at `44d844` is 6, formatted as `06`, not the
MCU release string `3.53`. Its divide helper `3ad490` returns remainder in R0
and quotient in R1 for these positive operands; assuming the opposite ABI
would reverse the version digits.

The separate ASCII formatter at `29fc00` has these qualified decimal-input
flag forms, modeled in `noki5510_identity_contract.py`:

| Flags | Output |
| --- | --- |
| `01` | fourteen equipment digits |
| `03` | equipment digits plus Luhn check digit |
| `05` | equipment digits plus zero spare digit |
| `09` | equipment digits plus two software-version digits |

The source model is not an NV writer. Synthetic decimal vectors test these
equations and independently decode the mobile-identity type/digit layout;
they are not injected into a running phone. A fresh eight-second hybrid run
with `noki5510_identity_observe.lua` observes one organic selector-1 call,
with input `ffffffffffffffff` and output `faffffffffffff0f`. The paired frame,
source/destination contexts and preceding `000c+8` request match the original
byte model. Those nibbles are not a valid decimal equipment identity. Selector
2 and the positive decimal vectors remain source/reference coverage, not
observed handset execution. The hybrid bootstrap and erased-NV failures
reproduce unchanged.

Decimal identity validity must not be confused with the separate record
reply's nibble filter. At `2a01dc..2a0218`, that filter rejects nibble values
`c..e`, but permits `a`, `b` and `f`. It checks both nibbles of reply bytes
`0c..12` and only the high nibble of byte `13`. Consequently all-`ff` bytes
pass this isolated filter despite being nondecimal equipment digits. The
host reference tests every byte value at all eight positions (2,048 cases).
This does not demonstrate acceptance of an erased record: entry eligibility,
other product/state tests and encoded-record comparisons are independent.
In particular, reply offset `0c` must not be identified with NV offset
`000c` merely because the numbers match. The source-qualified slot-binding
path at `2a02aa..2a02e8` reads 24 bytes from NV `0020 + 24 * index`
and compares its first and second halves with reply offsets `24` and `30`
using the zero-on-equality helper `3aea44`. A separate 24-byte reply body
begins at `0c`. The host binding reference covers all ten slots and rejects
each changed byte in the returned halves; changing the separate body does
not affect this isolated equality result. This is not a cryptographic
authenticity test, a mapping of that body to equipment identity, or full
reply acceptance: the original branch has prior state/feature conditions.

Run `noki5510_identity_contract.py RUN --firmware
roms/5510-npm5-v353/5510f353c.fls` to check the source hash, bounded NV
inventory and formatter observations. Its report explicitly rejects a valid
product-NV claim. Correcting only the two checksums would not establish a
valid identity or product profile; this finding also does not prove that the
malformed identity is a separate boot gate. Neighboring record contents,
their consistency/binding rules and the remaining configuration fields still
need their own contracts before provisioning is justified. No NSE-1 identity,
key or donor record is imported.

### Factory-record DSP request boundary

The own-image constructor at `29f8ec..29f9f6` builds four type-`70`
messages and sends each through `2ce7a4` with destination argument 3.
The first two payload bytes are a subcommand and its following byte count:

| Subcommand | Data bytes | Own MCU data source |
| --- | --- | --- |
| `13` | 4 | flash word: `39cc20(0)` selects flash `200038` or getter `39ce0e` reading flash `54fffc` |
| `14` | 12 | NV `0014..001f` |
| `15` | 20 | NV `0000..000b`, then `000c..0013` |
| `16` | 24 | NV `0020..0037` |

These are not merely inferred consumers of the NV layout. The fresh erased-NV
identity capture records matching HLE TX and transport-consume entries for all
four, at emulated times `1.693595`, `1.693927`, `1.694339` and `1.694726`
seconds respectively. Their complete payloads are `1304c71bbe68`,
`140c` plus 12 `ff` bytes, `1514` plus 20 `ff` bytes, and `1618` plus
24 `ff` bytes. The following `0d00` self-test request is consumed at
`1.694964` seconds. The capture is
`tmp/native-gates/npm5-identity-contract/error.log`; it is reproducible evidence,
not a required source artifact.

The HLE's optional `answer_identity_query`/`answer_record_query` codec is not
enabled by the NPM-5 profile. Its documented ROM4 observations and ROM6 key/
format hypotheses do not establish an NPM-5 reply contract. In particular, the
four observed requests establish transport delivery, not successful decoding,
record authentication, reply generation or valid provisioning. The remaining
question is how this own firmware consumes the corresponding DSP replies and
which algorithms/material those replies require; request ordering alone does
not prove that missing replies cause either observed NV-checksum failure.
Both candidate sources for subcommand `13` are flash, not peripheral MMIO
or an established chip-ID register. The own image contains `19aff258` at
`200038` and `c71bbe68` at its final word `54fffc`; the latter matches the
observed request. Getter `39ce0e` is a literal-address load followed by a
word read. No checksum algorithm or semantic meaning for that word is
established solely by this equality.
The selector is also source-resolved: `39cc20(0)` loads the descriptor at
`44d34c`, whose first word points to PPM `490000`, and scans 36 bytes
starting at PPM offset 4 for a non-`ff` byte. Finding one returns the probe
address, selecting `39ce0e`; an entirely erased probe returns zero and
selects flash `200038`. The acquired own PPM is nonerased in that range.
`factory_request_word_source` checks this selection and the report binds the
observed request to its selected word. This is neither a full PPM-validity
test nor evidence for a hardware identity register.
The extended observer confirms the constructor's R0 value at `29f92e` is
`c71bbe68`, with live flash reads `200038=19aff258` and
`54fffc=c71bbe68`, in a fresh eight-second capture at
`tmp/native-gates/npm5-record-flash-word`. This capture also reproduces
zero reply-consumer entries and passes the bootstrap and identity/NV gates.

The own reply dispatcher reads message byte `08`. Its explicit `34` branch
at `24c9d8` reaches the call at `24cadc` to `29fdc4`; the `35` branch at
`24c9fc` reaches `24cad4` calling `29ffe8`. The `34` consumer copies 13
bytes from message offset `0b` to state offset `20`, then sets state byte
`03` to 1. It does not itself authenticate those bytes or write NV. The
`35` consumer begins with state-dependent acceptance: one path accepts
length byte `32`, while length `34` has a halfword-sum check over the message
body against its trailing word. Its later branches still need complete
qualification before defining a DSP encoder. The source report records these
entry points and code hashes; no corresponding reply execution is claimed.
The bounded entry reference `record_reply_entry_eligible` covers only
`29ffe8..2a005c`: initialize eligibility from record index `< 10`; state 1
bypasses the remaining entry checks; otherwise length `32` preserves it,
and length `34` requires the sum of 25 big-endian halfwords at offsets
`0a..3b`, modulo 65536 and XOR `ffff`, to equal the halfword at `3c`.
Other lengths clear eligibility. Passing this reference is not full record
acceptance: later code can reject identity nibbles, compare both 12-byte
encoded halves at offsets `24`/`30` against the current NV slot, update RAM
state, or launch another record transaction. Tests exercise checksum failure,
both bypasses and the index boundary without injecting handset messages.

`noki5510_records_observe.lua` composes the existing identity/NV observer
with bounded entry counters for both reply consumers. A fresh eight-second
`nmp5hle` run in `tmp/native-gates/npm5-record-consumers` observes zero
entries for each, terminal state byte `1204dd=02`, record index
`1204e6=00`, and receipt flag `1204df=00`. Both the hybrid bootstrap and
identity/NV checkers pass on that capture (132 NV requests). Thus the four
outbound requests are observed without these reply consumers executing in
the captured interval; there is no observed failing reply to decode. This
does not establish that they must precede graphical boot or that supplying
them would repair the independently observed erased-NV checksum failures.
Use the identity checker with `--records` for the extended observer: it additionally
requires four ordered, matching HLE/transport request observations, erased
record payloads, zero consumer entries and the qualified terminal state.
It rejects incomplete, duplicate or changed evidence rather than treating
a missing trace as proof of absence.
It also requires the constructor-word observation and its match to the own
image's final word; the earlier capture without that marker remains useful
historical evidence but does not satisfy this extended gate.

The continuation constructor `29ecf8..29ede4` emits type `70`, subcommand
`17`, body length `30` (48 bytes), through send call `29edd6` with
destination argument 3. It saves its first argument and control state in
RAM, then combines raw argument fields with the selected NV slot at
`0020 + 24 * index`. Normally that 24-byte record is copied intact; when
the fifth argument is 1, its two 12-byte halves are reversed. Body fields
outside the record are copied/masked from the caller rather than derived by
a recovered MCU codec. The source report pins the constructor separately.
Neither HLE TX nor transport-consume logs contain a subcommand-`17` request
in the qualified eight-second erased-NV capture. This is a source-mapped
continuation, not an observed transaction or justification for inventing its
DSP response. Recovering reply `35`'s full acceptance path alone therefore
would not establish the algorithm needed for subsequent transactions.
The reply handler's scan tail at `2a04ba..2a052a` is separately bounded.
Given its post-validation index `< 10`, scan-armed byte `state+7 == 1`
and no retained-result flag (R8 zero), it increments the index, reads the
next 24-byte NV slot and sends subcommand `16`, returning to state 2.
Incrementing slot 9 wraps the requested slot to 0 but stores sentinel index
10 after sending. A subsequent tail with index 10 clears scan-armed rather
than requesting another slot. Thus an all-miss tail sequence requests slots
`1..9,0` after the initial slot-0 request; it is not an unbounded retry.
The host reference qualifies only these tail inputs and branches, not the
earlier validation/state updates or receipt of real DSP responses. A
retained result suppresses this next-slot request. The observed initial
state 2/index 0 does not demonstrate any of these continuations executing.

An adjacent record-update routine, `29fa6c`, must not be mistaken for that
reply contract. Its sole aligned direct Thumb-call candidate is `24f842`,
inside a command dispatcher; this census does not classify computed/ARM
callers or prove dispatcher ownership. In its ordinary accepted path it
requires a byte-sum over message offsets `08..21` to equal byte `22`, and
byte `09` to equal `4f`, before writing 24 bytes from message offset `0a`
through NV writer `3ac608`. The destination is `0020 + 24 * record_index`.
A selector of `ff` replicates that record across ten slots. Successful paths
call `29fa1e`, which recomputes the checksum over NV `0000..011b` and
writes the four-byte result at `011c`. Product-feature-conditioned branches
also exist and are not reduced to this ordinary path. This establishes an
MCU-side record-update/checksum mechanism, not the record's cryptographic
validity or a connection to DSP replies `34`/`35`.

## Input consumer boundary

### Phone-code storage consumer

The own getter `29f876..29f8e2` reads three bytes at logical NV `0110`
through call `29f88a`. It expands six nibbles to characters, validates only
the first five using the own character table at `447e61` (digit mask 4),
and replaces invalid digits with ROM literal `12345` at `29fbf4`. It then
NUL-terminates at output byte 5 and returns whether the stored first five
digits were valid. The sixth stored nibble is neither validated nor returned.
The neighboring setter `2a0548` packs six input characters into three bytes
and uses the checksum-maintaining NV update path; its caller input contract
is not qualified here.

The fresh eight-second capture `tmp/native-gates/npm5-phone-code-contract`
observes one entry, its exact `0110+3` NV request and paired return:
`stored_valid=0`, output `3132333435`, terminator `00`. Existing erased-NV,
identity, record-transport and hybrid-bootstrap checks reproduce. The
identity checker's `--phone-code` option checks the frame, request ownership
and result against `phone_code_from_nv`; its byte-domain tests cover all
sixth-nibble values and each validated position. This is an organic
default-code fallback, not an injected code, physical unlock acceptance,
valid factory provisioning or a reason to bypass the MU4 input boundary.

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
Sheet 3 connects MSP430 U301 P4.6 (pin 42), net `UC_DSP_INT`, to the
DSP U101 active-low INT2 input (pin 66 on sheet 4). This is a separate
controller-to-DSP signal from the serial data/clock/frame nets; its normal
firmware-driven waveform remains unknown. The source-pinned resident IDLE3
routine below selects this interrupt, rather than ordinary command RX, as
its only enabled maskable wake source.
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

The retained `04-npm5-userif.pdf`, printed pages 7/8, additionally specifies
the MU4 startup contract: VBB enables its MCU regulator; low PURX holds the
module in reset. The MCU samples MBUS no later than 1.5 ms after PURX release
and must remain reset when MBUS is low, identifying a phone-programming
connection. During normal operation the module can sleep and wake on either
a keypress or a received serial message; phone power-off removes MU4 power.
These are manufacturer constraints for a future MU4 backend, not measured
timing for the current MA4 MBUSTIM model. They do not supply FBUS baud,
keyboard message IDs, acknowledgement rules or startup GPIO levels.

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
| `5510_DSP_REPAIR_TOOL_v1.00_Windows.zip` | `56c4c6ee44aa106b78e5a48ae4b467d54b41538e8f77b60492ce5a95cf0ab6fb` |
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

The original Group Sense V2.0 guide further limits this reference: programming
disables normal FBUS operation until EXT_UI reset, and its message window
explicitly omits CMT-to-EXT_UI messages. A flashing-tool log therefore cannot
stand in for the normal MA4/MU4 startup or keyboard transcript, even if its
self-test succeeds. The listed INITDISK/INITDATA files initialize NAND and
install music-DSP code; an MCUSW version field in product information is not
an MSP430 executable. Targeted public searches for the cited Maverick FBUS
specification and Nokia 5510 MSP430F135/MU4 firmware have not located either
input. This is a bounded acquisition result, not proof that no public copy
exists. Resume this branch on a matching MSP430 program or normal-operation
wire capture/specification, not service-tool defaults or another MSP430-based
board's software.

The Linux package's `dsp_repair/fbus.py` (SHA256
`9cfc318d185cbebb7bbad9a615adcd334e3ec56d40516c2eccb040620176f2fe`)
and `ORIGINAL_PROTOCOL_ANALYSIS.md` independently agree with the MCU's
`47 02` self-test request, unsolicited opcode `06` and five-component `6f`
reply. The application acknowledges unsolicited frames and answers node
`28`'s D0/04 service startup with D0 payload `05 01 41`. This is a
service-tool session, not a recovered normal MA4/MU4 boot transcript.
Neither acquired distribution includes the cited raw `com_log_00001/2`
captures or the MSP430 program. The Windows archive matches the published
hash; no executable was run. No startup greeting or PASS reply is inferred
from its repaired/modified firmware variants.

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
`0018/0003/0800/0010`. SPRA602F section 2.2.4 and figure 11 explicitly describe
them as ignored hex-utility compatibility words, not live register settings.
That is the referenced TI-family boot grammar, not a dump of DA150's mask loader.
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
is synthesized from it. The independently read-back InitDisk fixture volume
below is confirmed as FAT12; that does not establish a factory handset dump's
format or physical bad-block mapping.

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
The staged frontend additionally checks 256 plain/delayed BIO/NBIO branch
cases with Read sampling, input transitions and save/replay. This proves
bounded CPU/input behavior, not physical board timing or complete startup.
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
validated below. Program-entry execution is a separate acceptance scope;
its current limit is in [Current boundary](#current-boundary).

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
DMA mapping/timing or native music-DSP boot. It also does not establish
program entry by itself; the executed loader and recorder scopes are
separate, with their current limit in [Current boundary](#current-boundary).

### Original CLKMD contract

`tools/mu4_clock_contract.py` pins four original R060 writer sequences, without
merging program overlays. Each loads AR1 with MMR address `0058`, then stores
its immediate through AR1. Source setup addresses and observed next PCs are
distinct; these are not mid-instruction ownership claims.

| Segment | Setup address | Observed next PC | CLKMD written | TI-family requested rate |
| --- | --- | --- | --- | --- |
| `aa55` | `000923` | `000927` | `6004` | DIV/2, 5,997,600 Hz |
| `aa55` | `00092b` | `00092f` | `77fb` | PLL x8, 95,961,600 Hz |
| `aa22` | `02905d` | `029061` | `6004` | DIV/2, 5,997,600 Hz |
| `aa22` | `029065` | `029069` | `97fb` | PLL x10, 119,952,000 Hz |

These rates combine the schematic's 11.9952 MHz Y101 input with
[TI SPRU131G tables 8-18/19](https://www.ti.com/lit/ug/spru131g/spru131g.pdf).
Both PLL requests specify `PLLCOUNT=255`, or 4,080 input clocks before the
additional mode-transition delay. These are decoded firmware requests using
a documented TI-family register contract, not measured DA150 frequencies,
lock times or a complete writer census. The MU4 bench configures the core's
`set_pll_reference_clock(11'995'200)` and `set_follow_clkmd(true)` for delayed
PLLSTATUS and selected execution rates. CPU clock publication fans out to
DMA and all three McBSPs, not to the externally clocked codec. Unconfigured
users retain their declared fixed execution rate and legacy immediate status
behavior. MU4 explicitly configures reset CLKMD `0000` and DIV/2 from the
compatible pin/MMIO contract below, rather than retaining a 13 MHz reset
calibration. Exact DA150 die equivalence is not established.

#### Reset profile

The NPM-5 MU4 schematic, sheet 4, connects U101 CLKMD1/2/3 (pins 77/78/79)
through R101's independent 100-kilohm resistors to DVCC (2.8 V): pairs 7-2,
6-3 and 8-1 respectively. Thus the board strap is `111`, not a guessed default.
The separate R101 pair 5-4 pulls MP/MC down; it must not be confused with a
CLKMD pulldown.

[TI SPRS095P](https://www.ti.com/lit/ds/symlink/tms320vc5416.pdf), figure 2-2,
matches all five clock pins: CLKMD1/2/3 at 77/78/79 and X1/X2 at 96/97.
Its three McBSP subbanks, DMA registers `54..57` and CLKMD `58` also match
the firmware-mapped MU4 layout. Table 3-8 maps `111` to reset CLKMD `0000`,
PLL disabled, input divided by two: 5,997,600 Hz with Y101. This establishes
the explicitly named **C5416-compatible model profile**, not an identification
of DA150's complete silicon or mask ROM. Older family tables differ; applying
SPRU131G's C5410 reset table without a variant match would be unjustified.

The ignored local reference is
`roms/reference-docs/npm5/ti_tms320vc5416_sprs095p.pdf`, SHA-256
`3511b32bf9d3ba338ad5043376a30115c046979f6a3a094d3c30c5cd674261e2`.
The core's optional `set_reset_clkmd` publishes the configured source/rate
at reset and cancels any pending transition. Unconfigured machines keep their
existing reset behavior. The controller fixture checks reset CLKMD and the
CPU/DMA/McBSP DIV/2 rates; the passive original-run checker requires an explicit
compatibility-profile marker before accepting a divider publication at time zero.

Isolated routine fixtures bypass that original clock initializer. Their
conformance windows therefore use peripheral ticks and instruction budgets,
not fixed deadlines assuming 13 MHz. NAND read/program/erase timing remains
unchanged. A busy-poll count is observational: slower execution may first poll
after readiness. The fixture instead rejects data reads while NAND is busy
and retains the minimum ten-microsecond read-latency check.

#### Software-managed IDLE3 relock

SPRU131G sections 6.11.3/8.5.2 establish that IDLE3 halts the PLL and software
re-entry must account for lock acquisition. The core invalidates retained
lock on the actual IDLE3 instruction. A coupled fixture first locks PLL x8,
switches to DIV with PLLON/OFF held high, executes IDLE3, wakes through an
enabled interrupt with INTM set, and requests PLL again. The request remains
in DIV after one microsecond and switches only after the new lock-count delay.
Without the IDLE3 invalidation it would wrongly reuse retained lock.

This validates software-managed DIV wake and subsequent PLL entry. It does
not assign an automatic wake latency when IDLE3 is entered with PLL selected,
nor establish oscillator startup availability or asynchronous pin pulse timing.

#### Transition timing

The tool also calculates exact TI-family transition predictions (SPRU131G
pp. 8-32/33). An unlocked DIV-to-PLL request adds six input cycles and
3.5 destination PLL cycles to the lock count: `77fb` predicts 340.672727 us,
`97fb` predicts 340.665433 us. The processor continues in DIV mode during
lock-up; PLLSTATUS changes only after the source switch. PLL-to-DIV has no
lock-count delay and uses the old PLL configuration for its 3.5-cycle term;
the input-cycle term becomes twelve for PLLMUL=15. A still-locked PLL skips
the lock-count delay on re-entry. Exact rational tests cover these distinctions
and fractional PLL ratios. These predictions are not physical timing
measurements or a complete automatic IDLE3 wake contract.

`check-c54x-core` tests the delayed status one input tick before the predicted
deadline and one nanosecond after it. It saves a pending request, cancels it,
then restores it at the same scheduler timestamp and verifies the original
deadline. This avoids rewinding from inside a later active timer callback,
whose cached timestamp is not rewound by `read_stream`. Additional MMIO cases
cover protected fields throughout a pending DIV exit, cancellation of that
exit, repeated requests without deadline extension, retained-lock re-entry
(PLLON/OFF explicitly held high) and multiplier-change relock. The original
`77fb` has PLLON/OFF clear; it must not be assumed to retain lock after DIV
exit. These are controller
conformance tests, not physical DA150 clock measurements or handset fixtures.
The selected-rate conformance fixture separately checks DIV/2 and PLL x10,
core timer count preservation, pending DMA and internal McBSP completion at
the new rate, and save/restore of all three rates without restarting TIM.
DMA already on the IDLE3 input source retains its original deadline across
system-rate changes in both directions.
Pending intervals retain their remaining whole source clocks; sub-cycle phase
is not established. A clock change requested during CPU execution is applied
by a zero-delay timer outside the old instruction timeslice, so elapsed cycles
are not repriced at the new rate.

The passive `tools/mu4_clock_observe.lua`, started with `-autoboot_delay 0`,
observes all four writes in the original `bootstatus` fixture, from initial
CLKMD zero at time zero to `97fb` at its 20-second endpoint. Native bootstrap,
resident entry and pin-level status service must also complete for the checker
to accept the observation. It uses this MU4 fixture's own retained R060 NAND,
not another product's provisioning, and makes no firmware/controller writes.
The copied input NAND remains byte-identical on disk with `-nonvram_save`.
The timestamps are emulated, not physical clock measurements.
`--clock-rates` additionally requires the four selected-rate publications to
match the original writes and transition delays, with CPU/DMA/McBSP rates
equal and codec clock unchanged at 11.9952 MHz. The three-nanosecond comparison
tolerance covers the trace's nine-decimal timestamp formatting, not a
calibrated latency allowance.
The original startup/status observation, fresh `sustain` and 20-second
`startup` gates pass with selected rates: six files, eight TX/RX DMA blocks,
1,024 DIN words and zero illegal instructions. These digital fixture results
do not establish analog clock stability or complete handset operation.

Attach the observer to an own-source `bootstatus` run with
`-autoboot_delay 0 -script /absolute/path/to/repo/tools/mu4_clock_observe.lua`,
then check the original upload and completed log together:

```sh
.venv/bin/python tools/mu4_clock_contract.py \
  roms/5510-mu4-reference/InitData_R060.a00 --trace "$RUN_DIR/error.log"
```

The acquired primary references are retained, ignored, as
`roms/reference-docs/c54x/ti_tms320c54x_spru131g.pdf` (SHA-256
`d9ea0e695859d605d1f94dabeede327c0e343099102cfd7e224f6f53375a3fa2`)
and `ti_tms320c54x_bootloader_spra602f.pdf` (SHA-256
`f308d45b8c90bcfa0be3afc0048786603bb78624400deae98caec5371daa51c5`).

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
remain outside the validated subset; unsupported active modes fail explicitly.
The device uses four clocks per internal word, as specified by SPRU302B section
3.5 (two for the read, two for the write). Ten coupled CPU/DMA cases and their
save-state replays require a four-word block to finish in sixteen transfer clocks.
The fixture clocks are not measured DA150 rates; arbitration and external wait
states are not validated by this timing check. The single completion timer does
not yet expose separate read/write bus phases.

SPRU302B section 3.8 specifies system-clock operation in IDLE1/2 and undivided
X2/CLKIN operation in IDLE3, with DMA transfers allowed in all three modes.
The CPU's separate PLL-stop callback distinguishes IDLE3 from the shared
IDLE2/3 peripheral-stop line. Two coupled cases select a 3.25 MHz test input
instead of the 13 MHz system clock; both exact transfer durations and their
replays are asserted. Two absent-input cases retain the enabled channel and
unchanged destination for 100 us, then complete after the controller fixture
restores its system clock; restored snapshots reproduce that pending state.
Missing-clock pending ticks and the selected clock source are saved.
Two further cases remove the clock after exactly two system clocks of the first
word. No destination write occurs during the ensuing clockless interval; both
ordinary continuation and snapshot replay require only the remaining two clocks
for that word, followed by three four-clock words. These cases distinguish
preserving a pending transfer from restarting it on clock return.
Clock changes retime a pending transfer at the next whole transfer-clock
boundary; sub-cycle switching and PLL stabilization are not modeled. No MU4
PLL register or handset clock rate is inferred from these fixture values.

The [NPM-5 MU4 schematic, sheet 4](https://www.s-manuals.com/manuals/phone/nokia/nokia_5510_npm-5_schematics.pdf)
labels Y101 as 11.9952 MHz and connects it to DA150 U101 X1 and X2/CLKIN
(pins 96/97). This supplies the DMA's undivided IDLE3 input independently of
the codec clock setting and the selected system rate.
The MU4 configuration connects the CPU PLL-stop output to this DMA clock
selection. The input is crystal-derived; the schematic does not establish
its instantaneous startup availability or a PLL multiplier/lock delay.

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
The fixture uses the C5416-compatible DIV/2 reset profile above; complete
DA150 equivalence is not claimed.

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
McBSP0 now has its own mapped data/control device and XRDY-to-DMA event line;
forwarding it into McBSP1 would conflate different ports.
The VC5410A datasheet's IFR diagram puts XINT1 at bit 11; table 3-21's
priority rank 14 is not an interrupt-bit number. The fixture uses bit 11.
The DMA device now has an explicit per-channel reload configuration, selected
by this fixture. Conformance checks all six reload banks, eight-bit frame masks,
reserved `28/29`, indexed access and extended-bank save/restore. The
channel-enable extension remains unsupported.

DE disable/re-enable must preserve the original frame-element shadow, not
replace it with the decremented live counter. SPRU302B sections 3.2.2.1 and
3.2.3.2 distinguish channel enable from the original DMCTR count retained
for each frame. R060 performs this pause around a call: AA22
`02:90b7..90c1` and AA99 `02:7944..794e` clear bit 3 of DMPREC `0054`,
call `02:97be` or `02:7b92`, then set the bit. The context tool pins the two
11-word sequences in separate overlays; it does not infer all callee effects.
The playback MMIO observation places its pause after the first element,
while live count is zero and original count is one. Replacing the shadow
there produces one-element frames and repeated `-63` source steps into
unrelated data. CPU count writes and block autoinitialization update the
shadow; changing DE alone does not. The focused two-frame sorting fixture
pauses after its first word, saves while disabled, disturbs DMCTR through
MMIO, restores and resumes. It requires all four source words, two frame
interrupts and count one after the first frame; the defective model fails
that exact count assertion. Indexed inspection is separately checked to
leave the CPU-facing index unchanged and reject invalid register numbers.

At captured DMPREC `4848`, INTOSEL is 1; VC5410A table 3-16 assigns IFR
bit 11 to DMA channel 3 rather than XINT1. The fixture routes these sources
according to INTOSEL. This is a partial mux (the attached transmit sources
and channel 3), not a complete ASIC interrupt-controller implementation.

The DMA device now accepts single-word multiframe transfers, all nonreserved
sorting/index modes, shared or per-channel auto-reload, and block/frame completion
callbacks. Each selected sync event schedules one element after the assumed
four-clock internal transfer delay; mismatched events do nothing. A synchronized channel does
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
samples. An ideal control-gated DAC/headphone contribution supplies MAME sound;
analog ADC conversion, analog mixing/electrical behavior and converter filters
remain unimplemented. See the [DAC output scope](aac_validation.md#ideal-dac-to-headphone-output).
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
words wider than 16 bits, companding, loopback/SPI/A-bis receive, RINTM modes
1/2, active format changes, internal receive
clocks and reset-activation timing remain unsupported or unvalidated. Unsupported
active data formats fail explicitly. Mid-word receive
frame recovery and receive overrun are covered by the error-state gates below;
mid-word transmit-frame recovery is covered by the same error-state gates.

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

The retained frontend has a separate `software-intr` acceptance profile:
256 cases and restores cover all 32 vectors, both initial INTM states,
pages 0/2 and snapshots at Decode/vector Fetch. With IPTR `0100` and
IMR zero, the fixture checks one frame write at clock 6, handler Execute
at 9, one RET stack read at 9, and continuation Execute at 15 (RET takes
five cycles). Changing the source opcode, fetched handler word and
already-read stack word cannot change retained execution. Arithmetic
state, PMST and page are preserved; INTM is set and only the selected
maskable IFR bit is cleared. These are manual-derived pipeline projections,
not measured silicon phases. IPTR-producer latency, nested software entry,
pin-level IACK and INTR inside repeats/control delays are not accepted by
this fixture; MAME's external IRQ-source callback is not a physical IACK pin.

Unsigned peripheral-window loads are a separate CPU acceptance surface.
The `ldu-periph` profile covers 9,216 cases/restores over every address
`0020..005f`, direct/absolute/short-indirect forms, four unsigned words,
both accumulators, two pages and three snapshots around the paid bus wait.
A synthetic endpoint supplies one word exactly once; literal/pointer changes
after capture cannot change the retained transfer. The existing peripheral
access path supplies one extra clock. This does not identify physical registers:
SPRS095P table 3-16 marks `002a` reserved for VC5416, while MU4 explicitly
sets AR1 to `002a`, writes through it at `02:9ed9`, then reads at `02:9edc`.
The bench currently backs this unowned gap with RAM. Its DA150 hardware
meaning remains unverified; execution beyond this access is bench behavior,
not validation of a new peripheral or Nokia register map.

SPRU131G 7.3.2 specifies an internal-bus bypass when a younger on-chip
data read addresses the same location as a pending write. The retained
frontend applies it to STL/STH A/B and T/TRN stores, in addition to RMW
producers. The `store-bypass` fixture covers 288 cases/restores over six
producers, unsigned-load consumers targeting both accumulators, adjacent/NOP-separated loads,
same/different locations, two pages and three snapshots. An adjacent
same-location load consumes the pending value without a backing read;
the real store still writes once at Execute. Different locations and
NOP-separated loads perform one real read. Stack instructions retain their
separate ordering contract, and peripheral/bank contention is not covered.
In original MU4 object initialization, `02:a39d/a39e` and `02:a386/a387`
are adjacent store/load pairs. Reading stale locals selected an invalid
object branch, wrote `bf7a` through a null pointer into IMR and accepted
source 4 into the self-loop at vector `2050`. Repeated pushes eventually
overwrote PMST and exposed zero program memory. This was not missing ADD
support or an absent resident word; the ordinary-store bypass removes it.

STLM A/B,BK publishes through the existing Execute-owned MMR path.
Circular modifiers retain BK in the saved per-instruction Decode state,
shared by short/long-offset and dual-operand addressing. Atomic execution
continues to use live BK. SPRU131G table 7-3 forbids circular use in the
next three words after STLM BK; Decode sampling gives the new value to
the first legal short consumer. The `stlm-bk` fixture covers 384
cases/restores: both accumulators, ordinary and Xmem load consumers,
BK 0/3/4/13, four gaps, two pages and
three snapshots. It changes live BK after consumer Decode and checks one
Execute publication, one source read, exact circular wrap and preserved
status. Results inside the three forbidden words are explicitly model
projections, not guaranteed silicon behavior. The existing DAGEN-bank
write-conflict path also recognizes BK as an older STLM destination
(SPRU131G example 7-23); that composition needs dedicated BK acceptance
beyond this single-consumer fixture.

Absolute ADD may also read BK at `0019` through its retained data-Read
path. The `add-bk` profile reuses the independent arithmetic matrix for
2,048 cases/restores over BK/RAM sources, four words, four accumulator
initial values, SXM/OVM, both accumulators, two pages and four snapshots.
Changing BK and the address literal after their capture cannot change the
result. One source read and literal fetch are preserved, with independent
40-bit result/carry/overflow/saturation checks. The `add-absolute` control
keeps the existing 2,048 AR/RAM cases unchanged. This does not admit
status/peripheral sources or short-form ADD from BK. SP has the separate
contract below.

### Absolute ADD from mapped SP

Absolute ADD `00f8/01f8 0018` now reads mapped SP, independent of CPL.
This is not SP-relative direct addressing: the extension is the absolute
register address, and the operand is captured once in Read. SPRU172C's
ADD single-memory syntax and SPRU131G section 7.1 define the data-operand
read; section 7.5.5's compiler-base and stack-operation latency categories
must not be applied to this operand merely because the register is SP.
The existing Read ordering captures it before a younger FRAME changes SP.
No phase, forwarding, loop or status implementation is otherwise changed.

`add-sp` covers 4,096 arithmetic/save-replay cases across four source words,
four accumulator initial values, SXM/OVM, A/B, SP/RAM, two pages, CPL
clear/set and four snapshots. Both the address literal and source are
changed after capture; independent arithmetic verifies result, carry,
overflow and saturation. Another 576 cases/restores place FRAME, PSHM,
POPM or STM before ADD with zero, one or three NOPs, positive/negative
SP values, both CPL states, pages and accumulators. An adjacent younger
FRAME and later SP replacement must not change ADD's captured input.
Twenty negative cases preserve rejection of IMR/IFR, status, repeat,
mapping and peripheral sources before operand reads or arithmetic effects.
The fixture fails against the old SP guard and passes with SP admitted.
It establishes this bounded staged contract, not physical bank timing,
all SP-producer hazards, interrupt composition or new delay-slot support.

MVMD also accepts BK as its direct register source, sharing the existing
retained register-to-AR/BK move path. The `mvmd-bk` profile has 432
cases/restores: isolated transfers and STLM A,BK followed by NOP/MVMD,
all eight AR destinations plus BK self-copy, four source words, two pages
and three snapshots. It verifies one source read and destination write
(plus the producer's one write), source changes after capture, exact
publication clocks and preservation of other registers/status. The existing
432 T/TRN cases remain controls through the same fixture. Immediate
STLM/MVMD DAGEN-write conflicts and control-delay composition are not
claimed by this acceptance surface.

A two-word long-offset/absolute STL/STH may occupy the complete two-word
control delay (SPRU172C B[D], BC[D], CALL[D], RET[D]). The decoder
counts both words and retains the literal as data, including literals
that resemble RC, INTR or BACC. It does not admit a store that starts in
the final one-word slot. `store-delay` passes 2,880 cases/restores over
BD, taken/untaken BCD, CALLD and RETD; both accumulators and low/high
stores; offset/preincrement/circular/absolute forms; six literals, two
pages and three snapshots. It checks exact word fetch/write counts,
retained addresses after pointer mutation, stack ownership and the selected
continuation. The sixteen `st-delay-edge` cases/restores reject partial
instructions without a data access or atomic fallback. RETD retains its
early-prefetched words until the stack Read completes (SPRU131G example
7-8); BCD has its own extra cycle relative to BD/CALLD. These are retained
frontend projections, not physical timing measurements. Interrupt arrival
during a long delay store and far-control composition remain outside this
new fixture's acceptance scope.

MVMD non-accumulator sources and MVDK RAM sources may target T/TRN
through the retained move path. Source Access captures one word; the
move's Read publishes it without repeating the source access. Following
T consumers need no additional NOP (SPRU131G table 7-3/category 6 and
table 7-12). `move-t` passes 1,920 cases/restores over MVMD AR6 and
direct/short/offset/absolute MVDK, both destinations, four source values,
FRCT/OVM, gaps zero/one, two pages and three snapshots. It mutates the
source after capture and the destination after consumer sampling, checking
one move write and independent signed-MPY or mapped-LDM results, status
and saturation. Accumulator-sourced MVMD to T/TRN, other control-register
destinations, delayed-control and interrupt composition are not admitted
by this acceptance claim.

Signed memory MPY A/B may occupy one delay word, or both words when it
has an offset/absolute extension. The same full-instruction budget check
rejects a two-word MPY starting in the final slot. `mpy-delay` passes
18,432 cases/restores over BD, taken/untaken BCD, CALLD, cross-page FCALLD
and RETD; short, absolute RAM, absolute AR7 and offset sources; both
accumulators; sixteen operand pairs, FRCT/OVM, two caller pages and three
snapshots. The fixture changes the pointer after Access, the source/T
after Read and the literal after Fetch. The selected callee copies the
product, verifying availability at the boundary as well as independent
40-bit arithmetic, overflow/saturation, frame/page ownership and one source
read. Sixteen `mpy-delay-edge` cases reject straddling instructions without
data accesses or atomic fallback. The signed MAC/MAS extension is covered
separately in [Signed MAC/MAS in control delay words](c54x_execution_contracts.md#signed-macmas-in-control-delay-words).
Rounded/unsigned MPY and immediate-multiply delay compositions remain excluded. Exact stages remain
manual-derived frontend projections, not silicon measurements.

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

The response carries a numerical classification, not a success code.
AA22 `03:b01d..b037` initializes result `01`, clears C16 and compares each
32-bit energy aggregate with 10,000,000 using `DSUB` and `ALEQ`. A left sum
strictly below the threshold selects `1f`; a right sum strictly below it
overrides that with `20`. Equality does not select either low-energy result.
`mu4_context_contract.py` pins all 27 words; tests cover threshold edges,
right precedence, invalid domains and mutation of every pinned word.
The current full-upload bench sums are `029abd24` and `3ad39fc4`, both
above threshold, so the correct response is `1e 03 aa 01 68 01 80 5f 55`,
preceded by `7f 01 55`. The old fixed `20` assertion was not portable across
CPU correctness changes. Execute-entry checks now require both comparison
accumulators and the selected stack result to match independently checked
sample energies, and require the transmitted result to match too. The peer
acknowledges token `80`; all eleven receive bytes and twelve transmitted
bytes drain without retry or queued residue. Mode `bb80` returns to zero, completion `b64e` is one,
and the block count is six. No illegal instruction occurs. Cleanup clears
the RX buffers, so this profile does not reuse the idle profile's final
constant-buffer assertions; the independently checked startup prefix and
the untouched status/replay controls retain their own acceptance checks.
This alone does not validate tone generation or music decoding.

The same original command repeatedly calls generator `02:876a`. Its two
64-word channels use phase words `bb79/bb7a`, increments `bb7b/bb7c`,
ROM lookup base `17fd` and amplitudes `bb7d/bb7e`. A separate integer model
folds the signed phase into the table index and applies the fractional
amplitude product with high-word truncation. Both ping-pong buffers and
final phase words match on every call, including signed phase wraps.
The model explicitly rejects product saturation outside
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
`b54a/b553`. Descriptor stride is 44 decimal (`2c` words), independently
visible in original `aa55` loader `2eda`. Initialization `02:8d0f` copies the active descriptor's name
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
For a non-LSE file, `02:a227 -> a286` instead passes the existing extension
pointer to the prefix copy. It skips the AAC source assignment at `a284`;
the common copy call does not make every input AAC. The original REL input
selects `RELSI16`, as confirmed by the native overlay comparison below.

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

Complete uploaded startup with the read-only status command has
`373e = 2`, `374e = 0f36`, `3750 = 0`, `3762 = 0`, and `3768 = 0`.
The original main-loop state-1 branch at `03:aac9` advances `373e` to 2.
The older retained routine-startup snapshot has state zero; it must not be
used to infer a missing prerequisite in complete startup.

The separate `bootrecord` probe sends pin-level packet
`1e 03 aa 01 36 05 01 84 55`: payload class 1, selector `36`, parameter
5, then distinct sequence token 1. Native acknowledgement is `7f 01 55`.
It enters `02:9e90` with state 2/capacity `0f36`/mode 3, reaches
`02:9fa8` in state 8, and invokes the original loader with descriptor
index one naming `RERSI16 .BIN`, length `0002:aca2` (175,266 bytes).
The resident runtime entry is observed twice. The second entry is stale code,
not the selected overlay's startup: `aa88` writes its vector table through
data space at `2000`, branching to `0073f8`, while the separate program view
still branches to resident `02:6d62`. No firmware state is written by the
command peer.

The bounded 20-second endpoint remains state 8/mode 3/continuation 8,
with zero tracks, nine received bytes and only the three-byte request
acknowledgement transmitted. Ten data writes of byte `01` reach the NAND
port after command byte `01`; the controller reports them as unexpected.
These writes follow stale-vector execution and are not evidence for a new
NAND opcode or a recorded file. The negative-control dispatch gate proves
command/loader reachability only; it explicitly reports `music_recording=0`.
All 73,762 non-register overlay destination words match the original image
before execution (4,622 records, 73,766 total words; four MMR writes excluded).

`bootrecordram` tests static physical program/data sharing at `2000..7fff`.
The DA150's exact shared-RAM extent is unvalidated: this is a separate
diagnostic profile, not a promoted board configuration. PMST.OVLY's CPU
extended-page folding is distinct from physical RAM sharing; making this
alias conditional on OVLY breaks the original AA55 copy sequence when it
temporarily clears OVLY.

With the provisional map, the original recorder startup at `0073f8` executes.
It reads DRR without RRDY at `03:8edb`; the bench now forwards this to the
existing McBSP latched-register behavior and counts empty reads separately,
rather than asserting that every DRR read consumes a fresh character.
Native replies have class/selector pairs `1/5e` (11 words, token `80`) and
`0/02` (16 words, token `01`), with independently checked framing and XOR
checksums. The external peer acknowledges both through serial pins.
At about 6.596 seconds, original code `02:891d` writes `REL_001 ` directory
bytes after NAND program command `80`, replacing the negative control's
invalid command sequence. This is initial storage activity, not a complete
recorded file or accepted media.

The provisional shared-RAM probe completes its 20-second execution window
without an unsupported instruction. It observes 160 NAND program setups and
160 confirms, with 84,480 data bytes (160 pages of 528 bytes). This does not
verify a finalized recording. The native lifecycle remains state 9, mode 3,
continuation 8, with zero accepted tracks.

The CPU conformance suite covers 512 compact-store variants, all twelve
signed/zero A/B delayed-branch predicates over five accumulator values,
nine delayed-call cases and 128 ASM-controlled store cases. Combined
accumulator/overflow call predicates use conjunction. ASM-store coverage
uses SXM set and saturation disabled; it is not exhaustive saturation
coverage. The store encoding and
shift contract are documented in [TI SPRU172C](https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/81/6136.spru172c.pdf),
pages 4-172--174; GNU tic54x disassembly independently identifies the original
instruction stream. Next derive the original recorder stop/save command
from its receive parser, then verify storage completion and the resulting
media before claiming playback.
Do not inject file contexts, lifecycle state or a guessed media header.

The AA88 receive path is separately mapped: McBSP2 RX vector `2058`
targets `03:8543`, which queues bytes at `f7e9` using head `f9de`.
Parser `02:82c0` is pumped by `02:8418`; dispatcher `02:84cd`
obtains its request through `02:8457` into buffer `045f`.
Its selector `36` branch at `02:85a8` tests parameter `0462`:
`3` clears `045e` and calls `02:8805`, `6` sets it and calls the
same routine, `5` rejects through the default response, and `4`
clears `3750`, selects continuation `9` or `10` according to the
old continuation, and calls `03:84fc` and `03:86ec`.
This is an original control-command route, not yet proof that parameter
`4` completes and saves a recording. Reconstruct the dispatcher range
`284cd..28560` and control range `285a8..2877a` with the inventory tool
and `--segment aa88 --disassembler-little-endian`; their output SHA-256
values are `0f11d770bd6456ddcb613a9bf0823d73388bbb75ced9095d865d26cac5a5ec96`
and `95437b37da97d1f0b85161d13b4af80f0a1fac7cb4d2af279bc0ed21698cd452`.
These addresses are overlay-specific: the resident image has different
instructions at the same addresses.

`bootreccontrol` sends the evidenced parameter-4 control packet
`1e 03 aa 01 36 04 02 86 55` (sequence token 2) at emulated time
10 seconds, after both startup replies and their acknowledgements.
All nine bytes reach the native receive path and receive `7f 02 55`.
At the 20-second endpoint RX is drained/not busy, ring head and tail
`f9de/f9df` are both 15, and parser state `f9e8` is zero. Total receive
events are 24, versus the baseline's 15; native output is 33 words.
The application dispatcher receives one request at 10.667 seconds: class
`1`, selector `36`, parameter `4`. The previous-command fields `3766/3767`
are also `1/36`. Its duplicate filter at `02:84db..84ec` rejects a matching
class/selector when the selector is greater than `28` hex (GNU's immediate
`#40` is decimal), before parameter dispatch.
The application control handler at `02:85dc` therefore has zero entries.
Transport acknowledgement is not application completion or saved media.

The separate `bootrecpoll` fixture retains that direct-repeat negative
control and adds a read-only status query before parameter 4. AA88 selector
`49` at `02:856b` packages mode/state into a class-1 selector-`71` reply;
it does not write recorder lifecycle state. The query packet is
`1e 02 aa 01 49 02 fc 55`, followed, after its native reply and pin-level
acknowledgement, by control packet `1e 03 aa 01 36 04 03 87 55`.
Both packet checksums are compile-time checked. The dispatcher receives
the query at 11.154 seconds and the control at 11.201 seconds, with previous
selector `49` rather than `36`. Handler `02:85dc` executes once; the native
processing completion reaches `8009`, and the original named loader
returns to `MCUSI16 .BIN`. At 20 seconds lifecycle is 2, mode 0,
continuation `0a`, with zero tracks. Native NAND activity is 224 program
setups/confirms and 118,272 bytes (224 pages of 528 bytes), versus the
no-control baseline's 160 pages. RX/TX totals are 35/47 words, three
checksum-valid replies and two subsequent request acknowledgements.
This proves the control transition and native return, not a finalized
recording or that the handset's ordinary sender uses this exact poll sequence.
The raw media inventory and save/name grammar are recovered below; native
warm-return receipt remains unresolved before that transaction can execute.
Disk NAND stays unchanged because NVRAM saving is disabled.
Reproduce with `MU4_BOOTSTRAP_BIOS=bootrecpoll` on the same isolated gate.
The profile exports `mu4_recorder_endpoint.nand` using the NAND device's
storage serialization without issuing commands or changing CPU state.
`tools/mu4_recorder_media_inventory.py` compares it with the source and
the gate records `media_inventory.json`. The 69,206,016-byte endpoint has
SHA-256 `1e5be7246427efedfa856b63b87bb860d9cf0b34326895e1d153fbe10a960cfd`.
Only pages 34, 161, 288 and 352--372 differ; spare bytes are unchanged.
This observation includes the generic INTM-following-instruction guard and
IMR-qualified IDLE1 wake corrections. Masked pending requests no longer
resume idle code. The changed-page extent remains unchanged; the older
pre-INTM scheduling profile changed only through page 369.
The upload/control counts remain unchanged. The endpoint hash is an
observation, not an acceptance constant or codec-validity oracle.
Page 288 begins with literal `REL_001 `; page 352 begins with `ID3` and
contains literal `POCP` at data offset 110. These are raw signature
observations, not proof of a valid filesystem entry, decoded codec,
protection mechanism or playable track. No payload transformation or
protection bypass is performed. The inventory's five synthetic tests
cover page boundaries, separate spare changes, disjoint runs and size errors.

### Recorder metadata request contract

The resident AA22 selector-`36` parameter-`7` branch at `03:b1f8`
consumes three length-prefixed byte strings following the parameter:
new base filename, TPE1 artist and TIT2 title. The order is verified from
the resulting NAND header, not inferred from argument-register names. It
copies each to a local zero-terminated buffer and calls `03:8dc3` with
the firmware-owned context at data `bf1c`, operation argument zero and
the three pointer/length pairs. Return zero selects the class-1
parameter-7 success reply via `03:b6ce`, then refresh routines
`03:941d/03:bd34`; nonzero selects its failure reply. Parameter `8`
calls the same routine with operation argument one and zeroed string
arguments. These are static call contracts, not verified save/delete
semantics. The extracted dispatcher range `03:acf2..b4b0` has SHA-256
`70cc83f46258fe7b29ddb51188b62383c4055c16284f39c4643a36c0f5d71144`;
routine `03:8dc3..8f0d` has
`61c127d1a53db86b5d2a84fbc9c2407d89aa16a1ea5c96515fbadeba2ae6aa37`.

The observation-only endpoint context capture reads `bf1c..bf2b`.
Its name is `REL_    `, terminated at `bf24`, with extension `REL`
at `bf25..bf27`. This is a prefix selector, not an incomplete selected
filename: `03:8e01..8e18` compares only four name bytes using `02:e3a7`
(a bounded character comparison), then compares the extension using
`02:e36f`. The entry scan independently tests suffix bytes 4--6 for
ASCII digits and ranks their numeric value. The NAND name `REL_001 `
therefore is not excluded merely because the context contains padding.
The comparison extraction `02:e300..e420` has SHA-256
`0c867e27045d0684472c883a12bab4dbf10ffac27caab0244d3c0fbeeca2f7c3`.
No filename digits need to be injected. The `bootrecmeta` fixture sends the
mapped length-prefixed metadata through serial pins; the native reply and
endpoint NAND delta are separate observations. A scan of 48,261 AA22 extended words finds only
the two direct `bf1c` operands in these callers; indirect writes and
initialization/data references remain outside that literal census.
An exhaustive runtime write watch shows that the recorder overlay reuses
this data range for codec work and resident initialization restores the
prefix at 13.908 seconds. The broad watch is retired after harvesting that
ownership observation; the small endpoint capture remains. The endpoint
media/context export now precedes acceptance assertions, so failed runs
retain postmortem storage without issuing NAND commands or saving the source
NVRAM. An exported image is an observation, not a passing test.

The staged empty-field control reaches both file scans in `03:8dc3` and
selects `REL_001 .REL`. Its call at `03:8ec7` to updater `02:8385`
returns `3`; the outer routine takes `03:8eef` and returns `1`. This is
not the outer routine's no-match return `2`. The updater recognizes the
existing ID3 header and calls builder `02:8519` at `02:844c`. The builder
requires both metadata pointers to be nonzero and both lengths positive
(`02:8535..8543`); it returns `1` for the fixture's zero lengths. Updater
`02:8452` translates that into `3`, then the protocol reply reports failure.
Accepted title/artist lengths are capped to twelve bytes. The filename is
handled separately by the later directory-name copy at `03:8ed8`.

`bootrecstage` retains the empty-field input. `bootrecmeta` uses the same
original staged execution, input media and pin protocol, but supplies
filename `TEST`, artist `TEST`, title `MAME`. Its request has 18 payload
bytes, 24 total bytes and XOR checksum `93`; its positive acceptance requires
the native success response, not merely any parameter-7 reply. It introduces
no firmware-state writes. Both profiles require exactly one instruction-owned
startup entry; no duplicate suppression or relaxed expected count is used.

The protected earlier 69,206,016-byte NAND retains the independently created input,
whose unchanged SHA-256 is
`75a523a9c1b6a0f6d98d47812b3082717e0764a79e0956b47b2de4fa7160d6c3`.
Changed data pages are 34, 161, 288, 352--631 and 127008; no spare bytes change.
Page 288 begins `TEST` followed by a zero terminator and retains extension
`REL`; the following padding is not normalized. Page 352 begins an ID3v2.3
header with size `00000064`, then `TIT2` containing encoding byte zero plus
`MAME`, and `TPE1` containing encoding byte zero plus `TEST` (five-byte
payloads each). This corrects the static title/artist order inference and
proves these particular stored fields, not filesystem consistency, audio
completeness or playback. Resident-memory comparison still passes exactly.

In that protected artifact, post-save refresh reaches idle without an instruction exclusion and reports
one track. The endpoint hash
is `93548b594a91df0ba87acc316435b12fb6fa35da97b066f0c7141748c5d94bb8`;
directory and ID3 bytes match the fields above. Page 127008 changes after
refresh but is not yet decoded as a filesystem commit. The internal native
metadata check passes; this is not a recorded-audio or playback acceptance.

Reproduce from a completed retained run:

```sh
make check-mu4-bootstrap-original \
  MU4_BOOTSTRAP_BIOS=bootrecmeta \
  MU4_BOOTSTRAP_SOURCE_RUN=/path/to/completed-retained-run
```

Use `bootrecstage` for the empty-field rejection control. Each run copies
that bench's own NAND and checks the copy remains unchanged. The staged
runner requires exact resident/overlay comparison, one Execute-owned startup
entry, original request acknowledgement, native dispatch and the expected
success/failure reply. It does not use the atomic profile's live-PC-qualified
loader-name read tap as a staged loader identity oracle.

The fresh `bootrecmeta` gate also passes with the corrected DMA frame-count
shadow and staged memory/immediate MAC support. It reaches the native
parameter-7 success reply and the 20-second endpoint at `02:98ea`, with
one Execute-owned recorder startup and exact resident/overlay comparisons.
Its NAND SHA-256 is
`523e9417a37fd2353edbcf3ad4d3f8bdc5231c184e8ac88ec8ae9639da8fc2f8`;
it is separate from the protected earlier playback input, not a silent
replacement. Independent 7-Zip 26.00 extraction of the fixture volume reads
the 143,168-byte `TEST` file, SHA-256
`b986b3fa41950b04711981460901c21f09b8f62778616f703a7ac2868509adc8`.
The first difference from the earlier extracted file is byte offset `0564`;
the stored metadata prefix remains identical. Forced AAC probing still
reports sample rate zero, no decoded-frame count and decoder errors
(672 diagnostic lines). As with the earlier file, that is neither decoded
audio acceptance nor proof that this native container is invalid.
The independently verified result is protocol/storage reproduction; native
codec output must be checked separately against this fresh input.

### Metadata boundary comparison

The recorder bench serializes `mu4_recorder_before_metadata.nand` immediately
before transmitting the first parameter-7 request byte. This calls only the
NAND's buffer-to-stream serializer, without NAND commands, device-state
changes or emulated-time advancement. The 20-second endpoint remains
byte-identical to the fresh recorder hash above, including with this observer.
The pre-request NAND hash is
`86bef66c843aff4933045bc0a4799c90e85b139c1153b460ceb108ce95cd6a0a`.
Only physical pages 288, 352 and 127008 differ afterward; no spare bytes
change. Page 127008's role is not established by this comparison.

Independent 7-Zip 26.00 FAT extraction reads `REL_001.REL` before the
request (142,958 bytes, SHA-256
`520a27329f3c058658aa905668b9e019561c9a1a4b521cfd80f40998593da6f0`)
and `TEST` afterward (143,168 bytes, hash above). In their common extent,
exactly 33 bytes differ, all within the first 56 bytes of ID3 metadata;
all bytes from offset 56 through the earlier EOF are identical. The added
210 bytes are all `ff`, not verified audio or benign padding. `POCP` is
absent before the request too. Thus this rewrite does not remove that
marker or alter the common payload, but its EOF extension remains a
separate correctness question. Forced AAC probing of the pre-request file
also reports sample rate zero, no decoded-frame count and decoder errors;
neither readback independently validates the native container or codec.

The existing NAND inventory reports physical-page changes. Its separate
`--extracted-files` mode compares independent file readbacks, quantifying
common changed ranges and added/removed byte counts without interpreting
FAT or codec syntax. Synthetic tests cover changed ranges, appended bytes,
shortening, empty inputs, read-only CLI behavior and incompatible export
options. All 2,950 tool tests pass (one skipped).

The untouched pre-request NAND also runs the original player for 20 seconds:
`illegal=0`, PC `02:b5e1`, 87,390 completed arithmetic-loop calls, one
bootstrap/twelve resident/24 loader-API Execute entries and fourteen replies.
Its 1,524,209 TX words, 1,691,842 DIN words and 1,363,518 actual DMA source
reads are all zero, with the same hashes and `1000..137f` source range as
post-update playback. This falsifies the metadata step, including its EOF
extension, as the sole cause of the observed silence; it does not explain
the extension or establish valid recorded audio. This is a negative coverage
experiment, not a passing playback gate. Its title publication begins
packed `3030,3039,3038` (`000908`), not `MAME`; `title_verified=0` is retained,
and the existing strict playback-observation checker does not accept it.
The native audio diagnostic still intentionally fails. No verifier was
relaxed to bank this comparison.

The baseline converted-ADC fixture supplies constant `1234`/`5678` channel
values. Native filtering behavior, encoder consumption and decoded-sample
provenance must be established rather than assuming that this input should
survive unchanged. The separate varying-input fixture below exercises a
different external source without changing the baseline.

### Varying converted-input fixture

`bootrecac` uses the same original staged recorder, pin-level requests,
nonempty metadata and independent seed NAND as `bootrecmeta`. Only its
converted-ADC source changes: the left channel alternates +8192/-8192
over 64 requested frames, the right +4096/-4096 over 32. These are sample
periods, not measured physical frequencies. Generation counters determine
phase and are reset/saved with the bench; phase replay is not newly accepted
by this experiment. No firmware state or waveform is written into DSP RAM.

The complete recorder gate passes with 871,742 generated stereo frames.
Actual channel-2 source reads at data `0021` observe counts
`201120,201155,201141,201134` for `2000,e000,1000,f000`, respectively
(804,550 reads, no other values). Attribution uses the active-source-read
API, not the enabled channel's address. Another 902,505 reads are outside
that ownership selection remain unclassified for that protected artifact;
the finalized recording's separately checked all-owner census is documented
in [Recorder DRR read ownership](aac_validation.md#recorder-drr-read-ownership).
Complete delivery/drop accounting and encoder consumption are not inferred. The strict
`--recorder-input` check independently verifies square-source statistics
and both received polarities for both amplitudes. This proves varying
peripheral input, not encoder consumption or decoded audio.

The captured endpoint NAND SHA-256 is
`5b857e584ac4ebb1806cc54cc5bb6d6bd4825c6605c490f3645cf94a649014d9`.
Independent FAT extraction reads `TEST`, 143,680 bytes, SHA-256
`5b9eeb7a4d194d202a332bd4d17bddf8cd072fd7f8658a05e57b793db6b7aee5`.
Against the constant-input file, 43,567 common bytes differ, starting at
offset 1005 (decimal), and length increases by 512 bytes. Forced AAC
probing still produces errors, sample rate zero and no decoded-frame
count; format/codec validity is not established. Re-running the unchanged
constant profile on the same build reproduces NAND hash `523e9417...` above
exactly. All 2,952 tool tests pass (one skipped).

```sh
make check-mu4-bootstrap-original \
  MU4_BOOTSTRAP_BIOS=bootrecac \
  MU4_BOOTSTRAP_SOURCE_RUN=/path/to/completed-retained-run
```

Playback of this separate AC-input NAND retains the ordinary constant
playback source and the existing audio-unvalidated diagnostic. At 20 seconds
it has `illegal=0`, PC `00:3276`, a verified `MAME` title/overlay and 22
replies. Execute observations record one bootstrap, twenty resident and
forty loader-API entries, but no calls/iterations/returns in the tracked
`02:a2c6` arithmetic loop. This differs from the constant-input file's
87,390 completed calls and twelve resident entries. It is an input-dependent
earlier/different route, not evidence of a stuck filter or missing opcode.
#### Playback reactivation contract

**Open question:** can an independently validated, generated AAC media
fixture separate native playback behavior from recorder-output/framing
defects? Retain the recorded files as controls. Do not force a bit, wakeup,
node, callback, saved context, method result or input value.

The first verified AA99 sessions start at 0.818115786 seconds and end at
2.368206564 (constant) / 1.726312648 (varying). Source-checked observation
covers all 3,122 words at `0200a0..020cd1`, with zero opcode mismatches:

| Execute-entry observation | Constant | Varying input |
| --- | ---: | ---: |
| Distinct visited addresses | 1,162 | 954 |
| Total entries in range | 957,582 | 336,192 |
| Parent `02a10c` | 1,478 | 0 |
| Service `020495` | 791 | 1 |
| Processing entry `0204d6` | 743 | 0 |

These are entry observations, not retirement or complete branch outcomes.
The method-9 dispatch at `0205d0` and method-15 selector at `0205ac` are
absent in varying playback; the getter's word 13 is not an evidenced cause.
If latch `aa56` remains zero, `0204d4` bypasses processing to `0205ae`.
Constant playback first writes one at 1.043100872 seconds. Fixed-time
snapshots miss transient operands and cannot establish writer ownership.
`7211 aa59` is `MVDM aa59,AR1`, a memory-to-register load
(SPRU172C 4-110), not an AR1 store. The paired `7217 aa60` loads AR7.
The referenced area contains return-like words and must not be named a
buffer descriptor from its shape.

**Original initialization and source coverage.** The AA99 literal census
covers 49,843 mapped words. Its sole `0495` at `02716a` belongs to a
count/destination/data record, not a standalone invocation packet:

| Source record | RAM destination | Initialized words |
| --- | --- | --- |
| `027167` | `804a..804f` | `0002,0495,d7b8,d796,0001,054f` |
| `027079` | `d7b0..d7b5` | `0002,0419,0000,8044,0001,054f` |

The preceding structure is
`8044..8049=ffff,80a4,0010,0000,0000,0000`.
The C-initialization parser walks 228 records from `027000` to exclusive
end `027692`; this alone does not establish the table's true beginning
or runtime initialization ownership. The only `aa56` literals are at
`0204b4,0204d0,0204d3,0205af,027a05`; the last is zero initialization.
Initialization also sets `aa59=d796` at `027a0d`. Computed aliases remain
outside this literal census. All six `804a` descriptor words match their
initialization at 0.9, 1.116666667 and 1.4 seconds in both runs, not proof
against transient corruption. After varying playback's first session,
reads at 1.741077 seconds instead yield `0002,8f56,12a2,1280` at the same
RAM addresses: identities must stay overlay/session-qualified.

**Observed activation chain.** Actual descriptor reads and source-checked
executed histories establish:
`d7b0` descriptor -> trampoline `020478..020494` / `02048f FCALA B` ->
wrapper `020419` -> `02041f CALL 0202d7` -> node `8044` insertion ->
scheduler `020341` / `020375 CALL 020478` -> service `020495`.

Both runs initially select the service descriptor at 0.834083498 seconds.
Constant playback's later callback reaches the trampoline at 1.022780485,
wrapper at 1.022780685 and enqueue helper at 1.022780785. The node is
written zero at 1.022780877, then ffff at 1.022781944 before the next
service entry at 1.022782161. Varying playback has only the initial three
node writes (zero, zero, ffff), ending at 0.834083281, throughout the
common 0.8182--1.7-second observation interval; this is below the cap of
32 per address. Source `1282,8292,fc45` near `0202d7` loads the node's
first word, writes zero and returns if the loaded word was zero;
the nonzero path updates links. The scheduler writes ffff near
`02035c..020363`. Live PC is context, not bus-owning instruction identity.

The first `d7b0` transfer comes through `0201de`; later samples also use
`02018b`. Constant playback reads `d7b4=0001`, then
`d7b0..d7b3=0002,0419,0000,8044`; all eight sampled reads of each field
match original initialization. Varying playback has no selected `d7b0`
entries in its first session and no reads in the common interval.
Helper `0201b3` entry history begins at call `02045d` in `020454`:
initial raw context is AR1=`d7b8`, AR2=`d7cb` at 0.820211608.
Constant playback's later entry at 1.022780152 has AR1=`0000`,
AR2=`d7a9`, A=`0008000000`, B=`0000023e64`. These are raw contexts,
not retained operands or proof of the referenced structure's identity.

A 128-entry bounded Execute history before the first `d7b0` callback
reaches beyond these helpers: it traverses `029c6f..029c7e`, returns
through `029bfc` to `023eea`, returns at `023ef7` to `0293da`, and reaches
`0293ed -> 020454 -> 02045d -> 0201b3 -> 0201de`. Every retained word
matches original AA99 source; its final four entries also match the
independently retained immediate callback history. The enclosing source
calls method-9 wrapper `029873` through receiver reference `15eb`, then
accesses result word `15f0` before the publication branch. Initialization
sets `15eb=aaac`; the C-initialization table sets `aaac=c686`, whose
method-9 pointer at `c698` is `0002,3e64`. This names a source-supported
method candidate consistent with the executed return, not a claim of
codec/decoder ownership or complete indirect-call closure. Varying playback
has no selected callback and therefore emits no trigger-qualified path.
The path checker rejects missing/duplicate terminals, wrong counts/indices,
source mismatches, reordered entries and disagreement with the immediate
history; the observer ring is bounded, reset/saved and never changes CPU
execution. Fresh paired runs reproduce the established counts.

Separate first-session Execute counters now establish the method split:

| Selected method entry | Constant | Varying input |
| --- | ---: | ---: |
| Entry `023e64` | 331 | 332 |
| Early-result entry `023e84` | 284 | 332 |
| Longer-body entry `023e99` | 47 | 0 |
| Epilogue return `023ef7` | 331 | 332 |
| Caller result access `0293da` | 331 | 332 |
| Publication call `0293ed` | 47 | 0 |

The varying file therefore reaches and returns from the method; it is
not missing the virtual dispatch. Both files initially take the early
path. Its observed writes include `15f0=0485`, followed by the caller's
`15f0=090a` transformation. These are numeric result words, not a decoded
error-name or codec-validity claim. The source initializes AR2 to zero,
tests `ba4f`/`ba51`, sets AR2 to one only on the nonzero/zero combination,
copies AR2 into result offset 3, then branches to `023e99` when nonzero.
The other branch sets result offset 2 to `0485` and exits. The globals'
literal references are enumerable: `ba4f` at
`023d7f,023d93,023dbf,023e74,025811,0273cd`; `ba51` at
`023da1,023db0,023dbb,023e78,0273d3`. These are references, not a closed
writer census. Their nearby setup includes comparisons with literals
`4144` and `4946`; neither that signature nor an early result alone proves
the saved file's format, validity or the missing behavior. The method
checker verifies original words, bounded per-site samples, totals and
first-session ordering without requiring a guessed successful outcome.
Both fresh runs retain prior range counts, and the full tool suite passes
(2,963 tests, one skipped).

Read-only gate taps narrow this to `ba51`, not an observed `ba4f` change.
In the common 0.8182--1.7-second interval, varying playback has 996 reads
and 332 writes of `ba4f`, all sampled values one with no value-change cap
exhaustion. Constant playback has 576/192 reads/writes with the same value.
For `ba51`, varying playback has 996 reads / 664 writes, constant 549 / 357;
both clear it to zero and later write one. Its transition samples reach
the cap of 32, so the log explicitly does not close all intermediate
transitions. Terminal totals and last-access values remain available.
These are scoped bus observations, not a new physical clock/protocol claim.

The setup at `023da0..023dbb` clears `ba51`, calls `02a81a` with `a8ab`,
and sets `ba51=1` on its nonzero-return path before calling it again.
`02a81a..02a82a` stores SP and register context; the neighbouring
`02a82b..02a83f` restores saved context. The sole immediate-call encoding
candidate for the latter is `02453d` in wrapper `024538`, which supplies
`a8ab` and stack argument one. Its ten immediate caller candidates are
`023dd1,0242ba,0242c1,02438c,02533b,02535c,02587e,025e14,025e28,025e35`.
This is not yet a runtime caller proof or a named format-error contract.
Gate verification rejects missing identities, bad ordinals/counts, wrong
time scopes and inconsistent terminal values; exhausted sample caps are
reported rather than interpreted as stability. Both fresh runs retain the
same method and range counts.

Execute history at wrapper `024538/eefe` identifies actual callers, not
just references. Varying playback's first eight selected callers are
`02535c,02587e,02587e,02587e,02587e,025e35,02587e,02587e`; constant
playback's first eight are all `02587e`. The first varying entry is
1.008694684 seconds, following return from `02510c` at `025142` and
the sequence `025356/f7b8,025357/f468,025358/f478,025359/f845,
02535b/e800,02535c/f982`. Constant playback first restores at
1.008497130 through `02587e`, following the `0256df` call and comparison
at `02587b`. These are bounded samples, not a complete caller-frequency
census or evidence that the shared caller is always an error.
Both 128-entry histories match original AA99 words and their immediate
caller tails. The same bounded-path reporter serves callback and restore
observations without changing CPU behavior. Fresh runs preserve the
method counts and full selected-range counts. Input bits, saved-stream
positions and the return-value predicates now need independent checking;
neither a numeric restore result nor a generic file probe establishes
invalid media or a missing peripheral.

**Bit-reader comparison.** The scalar routine `02510c..025142` uses
remaining count `a854`, a 32-bit high/low reservoir at `a858/a859`, word
index `b331` and base pointer `b332`. An independent host model appends a
16-bit word when the remaining count is **at most** 16 (the source branch
is BGT, not BGEQ), then extracts the requested MSB-side window and reduces
the count. A zero-width request still permits refill; the bounded scalar
domain is widths 0--16 and remaining counts 0--32. Model tests independently
compare binary-string slicing across all widths/counts and four words;
these are model tests, not executable CPU coverage of every case. The
contract pins all 55 original routine words and tests every word mutation
and a wrong-page alias.

Fresh runs retain eight initial calls and up to eight calls separately
from each of `025354` and `025e2d`, preventing one caller from starving
the other. An additional one-attempt optional-field window has its own
cap of 128 calls. Constant playback records 16 pairs; varying records 19.
All 35 raw entry/return contexts agree with the model's returned value,
remaining count, reservoir, index and unchanged base. The observations
cover widths 0, 1 and 15, not every width or all calls. Source-adjacent
literal candidates are reported separately from raw requests; neither is
promoted to a retained-operand API. Counters, selection and pairing state
are reset/saved. Malformed pairs, source/caller mismatches and out-of-session
timestamps fail the observation checker, while an actual model disagreement
is reported as a research result rather than hidden or accepted as audio.

The varying file's `025354` call at 1.008694075 seconds has raw width one,
remaining `0010`, reservoir `bc2cce39`, index `01d7`, base `162e` and next
word `3785`. It returns one, remaining `001f`, reservoir `ce393785`,
index `01d8` and next word `99c7` at 1.008694551. The local byte sequence
`bc2cce39378599c7` occurs exactly once in independently extracted `TEST`,
at byte offset 1048; the selected MSB of `ce39` is at byte 1050 and is one.
This ties the particular input window to the saved data, not a global
file-offset mapping, valid container/codec interpretation or decoder
correctness. Constant playback has no captured `025354` call in this
session, so comparing the same file offset is not a matched parsing proof.

The pulse/TNS/gain-control tail in the upstream
[FAAD2 AAC syntax parser](https://raw.githubusercontent.com/knik0/faad2/master/libfaad/syntax.c)
provides a candidate interpretation for the consecutive checks around
`025354`. The ROM's `025232` reads 2/6-bit fields followed by a 5/4-bit loop,
matching the pulse-data shape. Its parent reads a flag, optionally calls
that routine, reads another flag, optionally calls `025190`, then checks
the final bit. The uncapped first varying-input window at
1.008693067--1.008694626 contains exactly three width-one calls:
`02532f=0,025344=0,025354=1`, all agreeing with the scalar model.
The conditional data parsers are skipped in this attempt; the final one
causes the observed restore. Constant playback selects no such window
in its first session. Boundary/ordinal/source checks preserve that absence
without synthesizing a matching attempt. This strengthens the local
optional-field interpretation, but earlier section/scalefactor parsing and
the global stream position are not independently validated. No external
implementation is imported, and this observation does not prove invalid
media or justify clearing a bit.

**Do not infer ownership from aggregate counts.** Immediate-call encoding
candidates for the trampoline are `02018b,0201de,0201ec,02023d,020375`,
with counts `7,1571,2,1131,4271` / `2,1,2,601,1669`.
Helpers `020454` / `02046b` call `0201b3` at `02045d` / `020472`;
their counts 787/784 versus 1/0 sum to the respective `0201de` counts.
This neither proves nor excludes a particular callback's causal role.
Both runs skip `0201d6` and `0202a4`; that status branch is not the
distinction. Direct far-call candidates for wrapper `020419` at
`020cba,0295c8,038c40` carry preceding literals `8052,8060,807c`, not
`8044`; indirect/cross-overlay caller closure remains unproved.

**Observation safeguards.** `mu4_context_contract.py` pins source
relationships and rejects changed witnesses/extra census references.
`--playback-range --source-a00` checks the complete source range, bounded
histories, register-context pairing, callback field values and time/ordinal
scope. Histories select live AR4/AR2 only as context, not an operand-ownership
API. A live-A wrapper selector missed observed calls and is not an absence
proof; wrapper history is retained unconditionally and paired at enqueue.
Observation state is reset/saved. Lua bus taps exclude snapshots and
side-effects-disabled C++ reads. No observer writes firmware state and no
observation check accepts decoded audio.

#### Saved-file ADTS candidates

Read-only `mu4_recorder_media_inventory.py --extracted-files --adts-headers`
decodes candidate fixed headers and follows declared lengths from the first
candidate. It does not invent a 372-byte grid, discard inconvenient headers,
validate CRC/payload or assume that the first candidate is the correct start.
Both independently extracted files yield the same initial chain:

| Byte Offset | Raw Fixed Header | Declared Bytes | Channel Config | Blocks Minus One |
| --- | --- | ---: | ---: | ---: |
| 995 | `fff950802e8600` | 372 | 2 | 0 |
| 1367 | `fff950802e8600` | 372 | 2 | 0 |
| 1739 | `fff950442518c7` | 296 | 1 | 3 |

The next declared boundary is 2035, with no candidate header; another
candidate exists at 2111. These headers encode profile bits 1 and sampling
index 4, but that does not establish valid AAC payload. The constant file
has 335 candidate headers, varying 336. Payload false positives, leading
data, legitimate configuration changes, storage layout and recorder defects
remain distinct possibilities. In particular, this shared chain break is
not proof that changing the converted input alone broke the encoder.
The ID3 header declares a 100-byte tag body (audio region begins at 110);
this alone does not identify subsequent framing or explain its leading data.

#### Generated AAC external-media control

`mu4_generated_aac_fixture.py` authors a separate copy of this bench's own
NAND with four seconds of independently generated AAC: MPEG-2 AAC-LC,
44.1 kHz stereo, left 440 Hz and right 660 Hz. FFmpeg's
`mpeg2_aac_low` profile excludes MPEG-4 PNS; setting the ADTS MPEG-2 header
bit alone is not an equivalent encoder constraint. FFprobe checks the
configuration, host decoding must report no errors, and independent tone
measurements must retain both requested channels. This establishes input
media validity, not native DSP output or speech.

The fixture uses mtools to replace the existing short name `TEST.REL`
(long name `TEST`), then independently extracts `TEST*` through 7z and
requires byte equality before publishing NAND. The checked legacy FAT12
volume has BPB media `f8` but reserved FAT bytes `ff ff ff`; only this
specific geometry/mismatch permits mtools' compatibility override. Spare
bytes, the boot sector and everything outside the mapped partition remain
unchanged. The original NAND is never modified. Unknown layouts and
readback mismatches are errors, not reasons to weaken verification.

With FFmpeg 8.0.1 and mtools 4.0.49, repeated independent generation yields:

| Artifact | SHA-256 |
| --- | --- |
| Authored NAND | `b344ba14f993299bea895910357261ebd0da0c082f101e9f137b0ac423d4a432` |
| Extracted 65,449-byte track | `f24f23ba9eaa19ba9187a04fb2376597a04e3e75e7b05e5b38e14ba6b8109f32` |
| Host-decoded stereo s16 PCM | `fec0191741b174a8fe7fc032bf3449ed4fe06d05edd5fafc9257cd87808e7bdf` |

The encoded stream contains 174 complete declared-length ADTS frames;
host decoding yields 178,176 stereo frames. The fixture changes 131 NAND
data pages. Reproduce with the generator's source-NAND and new-output-directory
arguments, adding `--mcopy PATH` when mtools is not installed globally.

Native `bootplaystage` on this input still stops at instruction `025137`
(`f48f`, NORM), reported at PC `02513b`, with raw T=`0029` at cycle
111,871,465 after the shifted-store bypass correction below. This lies
inside the pinned bit reader. T's low six bits encode
-23, outside TI's documented -16..31 range. Plain MAME exit status zero
does not accept this run: the strict playback checker still rejects its
illegal instruction and incomplete session. No native decoded PCM is
established. Do not extend the CPU's shift range to accommodate this state.

The operand-localization capture before that correction observes raw A=`0032`,
remaining=`0009`, reservoir=`28254497`, index=`0017`, base=`162e`, next
word=`9271`, SP=`d70c`, ST1=`610f`, with call-site history `024343` at
0.993192041 seconds. Interpreting raw A as width 50 would give
`9 + 16 - 50 = -25` remaining after refill, hence shift `16 - (-25) = 41`,
matching T=`0029`. This is outside the reference model's width domain;
it explains the arithmetic but does not establish retained argument ownership
or a CPU defect. Source immediately before that call is
`024340: 62e6 0003 0005; 024343: f982 510c`.

The multiply's retained Execute-entry operands establish the immediate
predecessor: `024340` reads source `a88b`, value `000a`, literal `0005`,
with ST1=`610f` (FRCT clear), at 0.993192000 seconds. Its signed product
is 50, matching the next bit-reader entry's A. The preceding multiply at
`02433a` reads `a88a=0003` with the same literal. These are captured
pipeline operands, not late memory snapshots; the diagnostic accessor
rejects mismatched instructions and performs no bus access. Existing
MPY/MAC conformance verifies the accessor alongside operand mutation,
exact access counts and save-state replay (98,304 MPY, 262,144 MAC and
1,664 exclusion cases). This does not make the complete parser valid.

The bounded parser-record write trace observes `a888..a88c` becoming
`0001,000f,0003,000a,000e`, with `a88b=000a` written at
0.993186456 seconds (bus context PC `0242eb`). The source sequence is
`0242e6: e804; 0242e7: f982 510c; 0242e9: f495;
0242ea: 80e6 0003`: a four-bit reader call followed by a store.
The larger field-width/order sequence resembles an AAC program-configuration
element, including five-bit channel entries. This is a syntax hypothesis,
cross-checked against [FFmpeg's PCE decoder](https://ffmpeg.org/doxygen/trunk/libavcodec_2aac_2aacdec_8c_source.html),
not proof of correct stream alignment or an imported codec implementation.

More importantly, the native buffer does not retain the host-valid header.
Independent write taps on `162e..1631` observe:

| Time (seconds, first word) | Words | Bus Context PC |
| --- | --- | --- |
| 0.983259227 | `f9ff 8050 9f26 defc` | `003356` |
| 0.988100171 | `fff9 5080 269f fcde` | `0032f5` |
| 0.988777742 | `ca8b 4136 269f fcde` | `029e7a` |

The middle row matches the generated ADTS header exactly; the last row
matches the bit reader's observed initial words. Thus a later native
buffer transformation, not authoring/readback alone, lies between valid
stored AAC and parser input. Nearby source is
`029e74: e8ff 9508 18e2 0001 f2a0 8093 6dea 0002`.
Bus PCs are context, not a retained writer-identity API. Neither the
transformation's purpose nor the native external-media packaging contract
is established; do not call this proof of corruption, decryption, or a CPU
arithmetic defect.

The loop near `029e74` is a byte-packing stage, not the first observed
change: its data reads from `aa24..aa2b` are already byte-expanded output
from the processing helper. The surrounding load/AND/OR/store
sequence is consistent with packing adjacent low bytes into output words.
Earlier write taps on `aa24..aa27` distinguish unpacked input
`00ff,00f9,0050,0080` near `029e5c/029e5f` from replacement bytes near
`02b5ac/02b5b8/02b5c0/02b5cc`.
Source `02b5a1: fb82 c9ac` calls a processing helper before those stores;
`02c9ac` accesses state near `b9ef` and calls `02c62b`. This identifies
the upstream processing boundary, not its format/key initialization semantics.
Do not repair the later packer or pre-adjust external media to cancel bytes.
Host-valid AAC alone is not proof of valid native REL packaging.

#### Shifted-store dependency and prefix block model

The complete AA99 helper `02c62b..02c641` selects one nibble from a 16-bit
word: `(value >> (4 * (3 - selector))) & 15`, for selectors 0..3.
It uses `6f04 0c82` (shifted store) immediately followed by `1004` (load
of the same stack location). The core omitted that store form from its
existing same-address pending-write bypass. For input `c410`, observed
indices were `2,4,8,0` instead of `c,4,1,0`: the load saw the unshifted
count rather than the pending shifted value.

[TI SPRU131G section 7.3.2](https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/81/6114.spru131g.pdf)
documents bypassing same-location on-chip reads through an internal bus
before the pending real write. The correction extends that existing path
to explicit-shift stores and shares its value calculation with the real
Execute write. No firmware address, key, file byte or shift-range guard
is special-cased. The original complete helper fails the access-count
check before the correction and passes after it: 128 cases/replays,
four selectors, eight inputs, two SXM settings and two pages, one count
read, one input read and two real count writes. Original store, Xmem,
ASM, shifted-register bank, delayed and repeated-block regressions pass.

`mu4_context_contract.py` pins all 23 selector words and all 872 words
of processing routine `02c9ac..02cd13`. Its independent scalar block model
uses 18 state words plus four 16-entry lookup tables, 16 rounds and two
16-bit halves. A read-only native snapshot captures those 82 words and
the helper's stack inputs `f9ff,8050`. After the bypass correction, the
model yields `8bca,3641`, matching native replacement bytes
`ca,8b,41,36` and the packed words above. Tests retain that captured vector,
an independent nibble/digit oracle, domain rejection and mathematical
forward/reverse controls. This establishes one block under captured state,
not initialization, all blocks, a named cipher or valid native packaging.
The earlier `ef76,bdc5` prefix belongs to the pre-correction core, not the
current fixture oracle.

The generated control still fails native playback. Keep the processing
initializer and format selection explicitly unresolved; host AAC validity
does not identify the native file contract.

#### Fresh Native Recording/Playback Control

A new `bootrecac` run on the shifted-store-corrected core starts from the
untouched firmware-created NAND seed, uses ordinary serial recording/stop/
metadata requests and closes with native metadata success. Exact resident
comparison passes (48,261 program plus 3,656 data words). The converted-input
checker observes 871,742 generated stereo frames and actual DMA symbol-read
counts `201120,201155,201141,201134`; these are digital bench observations,
not analog input or valid encoding. The saved endpoint changes 285 data
pages and no spare pages. Independent 7z extraction yields:

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Native saved NAND | 69,206,016 | `e845f9aebdf6ef0a5877fe953dd49c2a989d4c77fca794ab20c1d654492413e7` |
| Extracted `TEST` | 143,680 | `c9da011f76ca98b91d3680d032439efea68454ee87c9ad7918bb63a264a7f231` |

The new file differs from the pre-bypass varying capture in 40,478 bytes,
starting at offset 622. Its declared ID3 body remains 100 bytes, followed
by 512 zero bytes at offsets `[110,622)`. It still has 336 candidate ADTS
headers. The initial declared-length chain is `995 -> 1367 -> 1739 -> 9529`;
the third candidate declares 7,790 bytes/channel 1/two raw blocks. That is
not validated framing or a reason to trim the file to the first sync.

Independent cold `bootplaystage` reaches its complete 20-second endpoint
with `illegal=0`, verified title/overlay and a completed first AA99 session.
It still produces no nonzero samples: 1,125,070 actual channel-3 reads,
1,402,561 TX words and 1,691,842 DIN words are all zero. The strict DMA and
source-range observation checks pass; the deliberate audio-completion
diagnostic still rejects acceptance. All 17 sampled bit-reader pairs match
the bounded scalar model; method `023e64` takes its early result on all
332 calls and never publishes its longer-body result.

The first captured processing input is 32 zero bytes under the same
82-word state as the generated-media control. The independent block model
maps its first zero pair to `c641,474f`, exactly matching the bit reader's
packed words `41c6,4f47`. This supports local processing arithmetic, not
the association with a unique file offset: the file has multiple zero
regions and no retained file-cursor ownership was captured here.
The file-start/header contract remains unresolved; the more discriminating
initializer-input boundary is mapped below.
Do not remove the zero region, change a seek or synthesize header bytes to
claim playback. The recorder contains an exact copy of the 23-word nibble
helper at AA88 `03f0e6`, a software-accessible anchor for that comparison.

The Lua observer detaches all bus taps after the last 2.4-second
observation window instead of retaining hot-stack callbacks for the entire
run. A separate three-second repeat reproduces all 708 selected first-window
lines exactly and closes the taps at 2.416666667 seconds. That is observer
equivalence over the stated window, not a shorter substitute for the complete
20-second playback observation or decoded-audio acceptance.

#### Recording Header and Selected Windows

**Open question:** why do seven of the first eight source-framed packets
remain rejected directly from the recorder's captured pre-transform arrays?
The selected-block phase/state and saved-byte correspondence are accounted
for across this region. Compare bit-writer inputs and their output arrays
before attributing the problem to encoder field values or CPU packing.
Partial tails, global phase ownership and complete REL framing remain open.

AA88 `03a930..03a9c5` contains fifteen ten-word field blocks. Each loads
AR6-relative source offsets `0227..0235`, places a 32-bit value and bit width in its
caller frame, then calls `03acc5`. The widths are
`12,1,2,1,2,4,1,3,1,1,1,1,13,11,2`, totaling 56 bits. The source tool
checks all 150 words through their field grammar. The passive observer
selects each frame on an actual high-word argument read; low word, width,
receiver and return address are caller-frame snapshots, not retained CPU
operand ownership. Its first eight headers supply 120 ordered records;
missing/duplicate fields, wrong calls/widths, order, scope and closure fail.

Independent MSB field concatenation produces `fff950802ea600` for the
first header (373 bytes), and `fff950802e8600` for each of the next seven
(372 bytes). The corresponding projected file positions are
`622,995,1367,1739,2111,2483,2855,3227`. Positions 995 and 1367 match
the saved seven-byte headers directly; the first header begins at the
previously observed transformed block. Do not confuse native header 2 at
995 with header 1 merely because it is the first clear sync candidate.

The apparent header-length anomaly at 1739 is explained by the transform:
source fields predict suffix `802e8600`, while saved bytes at 1742 are
`4fcdd201`. The independently captured recording state maps little-endian
words `2e80,0086` to `cd4f,01d2`, and the inverse recovers them exactly.
It is not evidence of a malformed header field or bad bit packing.

96 consecutive native transform calls at `03ee95` match the scalar block
model. The first input is `f9ff,8050`; the next seven are zero pairs,
producing `fe90,b156`. The observer captures the full 32-word byte array
before each transform as well as actual output writes. In all 96 samples,
the array's first four bytes agree with the inner caller-frame inputs.
These remain data-read-selected context snapshots, not retained CPU operands.

The inner frame places its far return at SP+8; candidate wrapper SP is
SP+10. Wrapper snapshots show source `fd4e`, local position zero and
words `16..1b` equal to `0000,0020,ffa0,0000,001c,0004`. The cursor
snapshot advances from 0 to 3,040 in exact 32-byte steps, and all 96
captured 82-word states are identical. The pinned wrapper prefix sums
arguments `1a/1b` and calls `03e69b` with the retained span and cursor.

The actual first output block plus unchanged source-array tail uniquely
matches saved `TEST` at offset 622; the checker does not assume this origin.
All 96 native output blocks plus their 28-byte source tails then match the
next 3,072 saved bytes exactly. This establishes the selected first-four/
preserve-28 correspondence across the first eight framed packets, not just
a recurring byte pattern. The concatenated captured plaintext SHA-256 is
`58b898113ee90aa6aeaf2d5e75ea3a75650d271137692e9367765c4a3ee94da8`.
It does not close the complete modulo helper, arbitrary starting phases,
later state changes or partial-tail rules.

`mu4_recorder_transform_check.py` verifies upload provenance, sample order,
time and frame domains, scalar/native outputs, input-array correspondence,
unique file origin, cursor continuity and saved chunks. Malformed evidence
fails; genuine model/file disagreements remain explicit research results
with exit 2, never coerced to match. Tests cover changed states per call,
ambiguous origins, corruption, malformed records and read-only CLI behavior.

In a host-only experiment, this inverse model processes bytes
`[622,3630)` in memory and supplies only the first 2,977 bytes to FFmpeg
8.0.1-3ubuntu2. All eight resulting headers match the source-derived values
at their declared boundaries. FFmpeg reads eight packets and decodes one
frame/1,024 samples at 44,100 Hz stereo, but rejects seven packets with
band-count errors. Exit zero is therefore not whole-stream acceptance.
No derived bytes are fed to MAME. This is partial host validation of
native-produced media, not native decoded PCM, analog audio or speech.
The same host test using the first 2,977 captured pre-transform bytes directly
gives the same one-frame/seven-error result. Transform state/phase or saved
file changes therefore do not explain those errors within the observed region;
encoder fields, CPU packing and their syntax interpretation remain open.
The earlier host test on raw bytes omitted this transform layer and cannot
by itself establish encoder failure.

#### Post-Publication Input Branch

**Open question:** how does the original recorder produce the fields of the
sampled saved-data region, and is that region being interpreted under the
correct framing contract? Do not clear the rejected bit, force a successful
result or synthesize another activation to bypass the consumer gate.

After the first `0293ed` publication, a new transaction sets `15d9=1`
and reaches the restore wrapper through `02533b` at 1.017096345 seconds.
The complete 21-word local branch `02532e..025342` requests one bit,
stores it at `ba66`, skips the conditional branch if zero, and otherwise
loads `*AR1`. A zero indirect word calls `024538`; a nonzero word calls
`025232`. Read-only taps observe `ba66=1`, candidate AR1=`ac36`, and an
actual read of `ac36=0` in the matching branch context. This establishes
the local predicate, not a retained-register API or global stream validity.

The 45-word helper `025f94..025fc0` sets the receiver's first word to
zero when its selector is 2 and to one otherwise. The preceding parser
requests two bits at `025dee` and stores them at receiver+1. Actual writes
observe `ac37=2` at 1.016848687, followed by `ac36=0` near `025fa1` at
1.016849329. Earlier and later attempts instead show selectors 1/3/0
with first word one. The helper and branch are source-pinned with every-word
mutation tests; local route tests cover all four selectors and both flag
values across every 16-bit indirect word. These are scalar/source contracts,
not complete executable CPU or codec acceptance.

At the rejected flag write, remaining bits are 31, reservoir is `ce393785`,
word index is `001c`, and base is `162e`. The nearby captured words are
`ce39,3785,99c7,26f0`. Their eight bytes occur exactly once in independently
extracted canonical `TEST` (`c9da011f...231`), at offset 1050. The selected
leading bit of `ce39` is one, consistent with the captured one-bit result
and post-read reservoir. This ties the particular input window to saved
bytes, not all cursor ownership or preceding parsing alignment.

A host-only diagnostic sends unchanged bytes `[995,1739)` to FFmpeg
8.0.1-3ubuntu2's AAC
demuxer: these are the two 372-byte candidate frames at 995 and 1367,
not an authored replacement or a MAME input. It reads 2 packets/744 bytes,
decodes 0 frames/0 samples and reports band counts 19>14 and 53>49.
The native branch and independent decoder therefore both reject aspects of
this interpretation of the saved bytes. This does not prove valid frame
boundaries, an invalid complete REL container or an encoder defect: an
unrecovered transformation/framing layer remains possible. No trimmed media
is fed back to the guest and no native-audio claim follows.

#### Processing-Latch Consumer

**Boundary:** the receiver word-22 and `15d9` interaction explains why the
first publication does not immediately enable the processing latch. Its
full producer/lifetime contract is not closed; the subsequent transaction
does execute, and its input branch above is the current discriminating lead.
Do not name either field as a missing readiness report from its numeric
shape, or force the latch/counter to enter the processing body.

The newly observed publication activates service `020495` at
1.0167937 seconds. `7211 aa59` / `7217 aa60` load AR1/AR7 from memory;
SPRU172C 4-110 specifies this MVDM direction. Read taps observe
`aa59=d796`, `aa60=d7b8`, `aa54=1`, `aa55=0`, `aa56=0`,
receiver word 6 (`d79c`)=1 and receiver word 22 (`d7ac`)=1.
The service reads `15d9=0` and later reads `aa56=0`, then writes the
zero latch again near the `0205ae` tail. The observations are actual data
bus events; logged PC values remain context rather than operand ownership.

The pinned 21-word gate `0204c1..0204d5` first skips its update if
receiver word 6 is zero. Otherwise it loads `15d9` shifted by 16, adds
receiver word 22 shifted by 16, stores the high half in AR2, and uses
BANZ to skip `aa56=1` when AR2 is nonzero. The observed 0+1 therefore
skips the update. `LD aa56,A; BC AEQ,05ae` then bypasses processing.
This stops before the result-flag and output-configuration calls at `0204f2`;
one result publication does not imply those consumers ran.

Receiver word 22 is initialized to 2 near `02771b` at 0.818124231 seconds
and changes to 1 near `020184` at 1.007644005. The adjacent source
`020182: ADDM #-1,*AR2` provides a decrement candidate, not a closed
writer/receiver-ownership census. Receiver word 6 changes from 0 to 1 near
`0201cc` at publication. Importantly, word 22 then reaches zero near
`020184` at 1.016818475, after the service check; `15d9` changes back to
one near `0293c6` at 1.016818725. It was one at 1.007644906, then zero
near `0293e4` immediately before publication. This is not proof of a
permanently stuck counter or missing hardware event. Follow the subsequent
transaction and why it supplies no further activation, alongside its input
region, before assigning meaning or inventing a peer.
The bounded observer removes all taps after 2.4 seconds. Its three-second
probe establishes this local path only, not a replacement for full playback.
With the added taps, all 729 existing bounded Lua observation lines reproduce
the complete corrected-core playback log exactly (excluding new service
records and the changed tap-count line). The 25 pinned source words include
the two receiver loads and the complete gate; every word mutation is tested.

#### Recording Transform and Initializer Inputs

The read-only `mu4_recorder_transform_observe.lua` samples the first
observed data-path forward-transform call, excluding reviewed key-expansion
return addresses. AA88 `03f0fd..03f466` is pinned as 874 words; its
82-word state is at `fc6c..fcbd`. The observed caller return is `03ee95`;
`03edae` was not observed in its bounded window, not proved absent.
At 1.125195309 seconds the sampled input is `f9ff,8050`, representing
ADTS bytes `fff95080` in little-endian halfwords. The independent forward
model yields `2ee7,dcc0`, matching actual writes to `6e9c/6e9d` and the
independently extracted file's bytes `e72ec0dc` at offset 622. Applying
the inverse model under that recording state recovers `f9ff,8050`.
This validates the sampled block boundary, not complete AAC encoding or
a unique file-offset association. The observed full recording repeats the
unobserved NAND byte-for-byte, with the same converted-input counts.

The recording state differs from the initial playback state. Different
header/content stages could legitimately use different state; the difference
alone is not a defect. `mu4_transform_key_observe.lua` narrows it to these
initializer-frame and actual bus-read observations:

| Overlay | Buffer | Length In Caller Frame | Actual Seven-Word Read Sequence |
| --- | --- | ---: | --- |
| AA88 recording | `ff84` | 7 | `0000,0000,0000,0000,0000,0000,0000` |
| AA99 playback | `e3d0` | 7 | `0000,0000,0000,0000,0000,0000,ffff` |

The initializers are AA88 `03f7cf` and AA99 `02cd14`; their source uses
the buffer/length in its key-mixing loop. The first seven data-bus reads
cycle through all seven addresses; this is not an inference solely from
late RAM contents. Snapshots remain frame context rather than a retained
CPU argument API. Before initialization, only the first six buffer words
receive zero writes near `03e446` (recording) or `02d028` (playback).
The preceding helper is a copier, not a clear routine: its `e589` body is
`MVDD Xmem,Ymem`, with source through AR2 and destination through AR3.
Both 17-word copies (AA88 `03e438`, AA99 `02d01a`) are pinned by the
source tool. Under CPL=1/far-call layout, it reads source pointer from
`*SP(2)`, count from `*SP(3)`, and transfers incoming A to AR3. Playback's
actual argument bus reads include `d75f=0006` and `d75e=e3e2`; the latter
is the source, not the destination receiving the zero writes. The paired
snapshot records AR3=`e3d0` and return context `02ab45`, following call
`02ab43`. Its loop
writes BRC=`0005`, consistent with six copies, not evidence of a lost
seventh iteration. Zero-valued writes alone did not establish clearing.
The last destination word `e3d6` still holds
`ffff` from a prior observed write near `03c862` at 0.309186156 seconds;
no write to recording's seventh word was seen in the installed-tap epoch.
Neither fact licenses a claim about physical power-on RAM contents.

The table and six-copy observations above are pre-immediate-ALU correction.
The original AA99 prefix `02ab09..02ab15` computes
`((bit_length - 1) >> 3) + 1`, then stores the byte count at object offset 7.
For the observed 56-bit source it must produce 7. Its two-word `ADD #1,A`
is immediately followed by `LDM AL,A`. SPRU131G table 7-28 specifies zero
mapped-accumulator latency for this producer; the previous core sampled AL
before the older ADD executed, returning 6. The exact 13-word prefix
reproduces that wrong read in an isolated CPU test. Deferring that retained
mapped read until after the two-word immediate producer corrects it without
changing guest inputs or the one-word producer restriction.

The prefix passes 16 cases/restores over eight positive lengths and two pages;
960 immediate ADD/SUB/AND/OR/XOR mapped-consumer cases/restores and the
existing 3072 immediate-multiply cases also pass. This is bounded instruction
dependency acceptance, not complete Table 7-28 or silicon-phase validation.
An unchanged native NAND in a two-second playback probe now publishes
`e405=7`, passes copy count 7 with BRC=6, and initializes from pointer `e3ce`
with a seven-zero-word snapshot. Allocation moved from `e3d0`; a repeated
probe with bounded heap-neighborhood taps observes the first seven actual
initializer reads at `e3ce..e3d4`, all zero. Historical fixed-address taps
could not establish this after relocation. The older silent playback capture
alone is not evidence of corrected output; the complete repeat below is.

A complete `bootrecac` repeat on the immediate-ALU-corrected core closes
through native metadata success and reproduces the canonical NAND above
byte-for-byte (`e845f9ae...413e7`). The seed remains unchanged. Resident
comparison again checks 48,261 program and 3,656 data words; the converted
input again has 871,742 generated frames and DMA symbol counts
`201120,201155,201141,201134`. The correction therefore changes the observed
playback initialization without changing this recorded media. This is a
controlled media-generation result, not codec validity or audio acceptance.

Cold playback of that exact media completes the 20-second observation with
`illegal=0`, verified title/overlay and 22 replies. Attributed channel-3 reads
total 1,125,032, TX words 1,402,542 and DIN words 1,691,842; all are zero.
The strict DMA and source-range observation checks pass, while deliberate
audio-completion rejection remains. The first captured zero processing block
under the corrected 82-word state gives scalar `1bce,8003`, matching native
packed output `ce1b,0380`. This is captured-state arithmetic, not file-offset
ownership or valid stream alignment.

The first-session method `023e64` now has 378 calls: 377 early returns and
one longer-body/result publication through `0293ed`. Previously the varying
control had no such publication. The observed longer body writes `ac44`
at `15f5`, `0002` at `15f6` and `0ba0` at `15f7`; entering and publishing
this result does not validate AAC syntax, sample meaning or decoded output.
The downstream consumer and input branch above map that publication.
Do not reopen the resolved
six-word-copy premise or compensate with file trimming, key edits or seeks.

The reviewed initializer wrapper at `02b25c` has an immediate call at
`02b268` to `02cd14`, after copying its caller's length slot to the outgoing
frame. This explains the seven-word argument; it is not a terminator convention.
Do not compensate by changing reset RAM policy or replacing either word. No key, pointer,
length, seek or file byte is forced. The key observer detaches after
1.8 seconds; its two-second probes are local initialization evidence, not
complete recording/playback runs. Retain these bounded diagnostics until
the producer/initializer contract is normalized, then remove redundant taps.

The metadata rewrite is not a new primary suspect. Fresh pre-request
`REL_001.REL` is 143,470 bytes, SHA-256
`f2d1e98be033aaf66d473dff58e2e4df0b3828bfe4fd42753265b98028965add`.
It already contains the same zero region and no `POCP`. Compared with
post-request `TEST`, only 33 common bytes change (all before offset 56),
and 210 `ff` bytes are appended. This reproduces the earlier narrow
metadata-boundary result; it does not resolve the EOF extension or certify
record completion.
The observer retains at most eight unusual entry contexts, is reset/saved,
and never changes firmware state. Preserve original recorded media and
acceptance constraints. A separate broader AAC-LC control (MPEG-2
header flag without the MPEG-2 encoder profile) stops at `02568a`, T=`ffee`;
it is not interchangeable with the constrained control above.

The inventory handles MPEG-2/4 header IDs, CRC-dependent header size,
declared length, raw block count and file truncation. Tests include embedded
sync candidates that must not replace the declared-length chain, divergent
but parseable headers, missing boundaries and read-only CLI operation.
The complete tool suite passes (2,986 tests, one skipped), including eight
generated-media tests covering authoring boundaries and failure paths.

In the pre-bypass constant-file control, all 1,125,032 actual channel-3
source reads in `1000..137f` remain zero,
as do 1,402,542 TX and 1,691,842 DIN words. Their hashes are
`fc93daf5`/`1e4f2315`; clocks/frames remain 230,090,537/845,921. The strict
`--playback-dma` observation check passes without any changes, and the
coverage diagnostic still intentionally fails audio acceptance. Nonzero
output, if later observed, requires waveform/media provenance rather
than a weakened predicate. The varying source demonstrably reaches the
peripheral and changes the saved file, but valid encoding and decoding
remain unproved. Both sample input and file-format interpretation remain
separate from analog audio or native speech acceptance.

Public format clues must preserve the recording/import distinction.
[Nokryptia's author-written manual](https://manpage.me/index.cgi?apropos=0&manpath=FreeBSD+11.1-RELEASE+and+Ports&q=nokryptia&sektion=1)
describes MP3 packaging without encrypting or decrypting the payload; it
does not provide a REL recording decoder. It is not a plain-AAC validity
oracle for these captures. No header workaround or firmware patch from
external tools is used by the native recorder/playback acceptance path.

### Cold Saved-Track Catalogue

`bootopenstage` starts a fresh process at the original serial bootstrap,
using the staged CPU and the same explicitly provisional bus/memory model.
Its input is the untouched `bootrecmeta` endpoint above, not a RAM snapshot
or donor medium. It sends only the ordinary pin-level status request;
firmware mounts the NAND and runs scanner `03:941d` and ID3 reader
`03:9fe9` once each. Exact resident comparison passes (48,261 program and
3,656 data words). The final lifecycle is 2, mode zero and track count one.

The native reader reconstructs title `MAME` in `b953`, and the scanner
copies it to `b6c9` and packs it at `c682`. The Execute-owned witness at
`03:95af`, immediately after the packing loop, checks both the unpacked
`004d,0041,004d,0045,0000` and packed `4d41,4d45,0000` forms. `c682`
is later overwritten by storage work (observed around 9 seconds); it is
scratch storage, not a persistent endpoint invariant. Neither a late RAM
comparison nor merely entering the reader establishes this title result.

The resident also sends 16-byte class-0/selector-`02` notifications after
the initial status reply. The external peer validates length, framing,
checksum and next modulo-eight sequence, then returns the ordinary
`7f <token> 55` acknowledgement through receive pins. Four notifications
with tokens 1--4 produce 23 RX and 78 TX bytes in the verified twenty-second
window; both rings and the typed queue drain, with no response pending or
retransmission. Counts are checked against the accepted frames, not assumed
to remain the status-only profile's 11/14 bytes. Payload semantics and
physical cadence are not established by acknowledging these frames.

```sh
make check-mu4-bootstrap-original \
  MU4_BOOTSTRAP_BIOS=bootopenstage \
  MU4_BOOTSTRAP_SOURCE_RUN=/path/to/completed-retained-run \
  MU4_BOOTSTRAP_INPUT_NAND=/path/to/bootrecmeta-run/mu4_recorder_endpoint.nand
```

The source run supplies the same original ROM inputs; the explicit NAND
input is copied into an isolated cold profile and checked unchanged after
execution. Its hash remains
`93548b594a91df0ba87acc316435b12fb6fa35da97b066f0c7141748c5d94bb8`.
The gate proves cold catalogue/title recovery and settled pin transport,
not the recorded audio body, decoder selection, playback, physical MU4 reset
or a handset boot. Cold-open save/replay is not established: its 78-word TX
history exceeds the older status peer's 64-word snapshot bound.

The same cold profile with the original no-track seed is a negative control:
it completes the same 23/78-byte transport but fails the catalogue predicate
with one scanner entry, zero ID3-reader entries and zero tracks. The fixture
does not manufacture a title or accept an ordinary status reply as file proof.

### Independent Recorded-File Readback

`mu4_recorder_media_inventory.py --export-fixture-volume <new-image>` exports
the data area of the single partition created by original InitDisk. It is
specific to this fixture: physical page 1 holds the MBR; its partition LBA
31 becomes physical page 32 through original lower reader `3035`'s row+1
mapping. The BPB's hidden-sector field is 32, not 31, and is not used as a
replacement partition address. Extents, signatures and 512-byte sectors are
validated; 16 spare bytes per NAND page are omitted. Existing outputs are
refused. No filesystem is synthesized or modified by the exporter.
Thirteen inventory/export unit tests cover extents, spare omission, invalid
formats, unchanged inputs and refusal to overwrite an existing output.

```sh
python3 tools/mu4_recorder_media_inventory.py source.nand endpoint.nand \
  --export-fixture-volume recorded-volume.img
7z l -slt recorded-volume.img
7z x -orecorded-file-readback recorded-volume.img TEST
```

On the saved endpoint, 7-Zip 26.00 independently identifies FAT12 with
16,384-byte clusters, despite the BPB's `FAT16` text. It extracts one file
named `TEST`, size 143,168 bytes, occupying 147,456 allocated bytes; the
native directory separately retains extension `REL` and non-normalized
short-name padding. Exported volume size is 63,946,752 bytes (124,896
sectors), SHA-256
`c89c8e4695fa7939f9b5789a98079024a09166250e5e98be66beeaaf5242eebb`.
The extracted file's SHA-256 is
`832058a7d5042cfa1c559a42a062d7dfb38bbaeabe9e60436fb6f46b8af87021`.
This verifies independent filesystem/extent readback of these bytes, not
their audio correctness. `ffprobe` rejects the file as generic audio;
that is not proof that the original Nokia consumer rejects its REL format.
Forcing `ffprobe -f aac -count_frames` recognizes AAC-LC-like headers and
two channels but reports sample rate zero, no decoded-frame count and
repeated decode errors. Its zero process exit status is not a successful
decode. This result does not distinguish a malformed recording from REL
packaging/transforms; the extracted file hash remains unchanged.

A separate [R060/R061 patch author's report](https://retrohack.eu/gsm/nokia-5510-dsp-repair-tool/)
identifies an existing MP3 decoder and an LSE payload offset of 896 bytes.
This is a lead for checking the original LSE consumer and constructing an
independent external media fixture, not a REL specification or a reason to
patch firmware to manufacture playback acceptance.

### Saved-Track Playback Boundary

`bootplaystage` starts from the same saved endpoint with the recorder's
provisional shared-DARAM mapping, but it does not request recording.
The original dispatcher receives this twelve-byte external request:

```text
1e 06 aa 01 30 02 00 01 00 01 81 55
```

Class 1, selector `30`, parameter 2 are followed by no-shuffle value zero,
track ordinal one and reply-cookie zero, then sequence token one and XOR
`81`. At `03:b3b0` the firmware checks the nonempty catalogue; `02:9d8c`
handles the shuffle field, and `02:9d1d` selects track one and advances
its own lifecycle. No mode, selection index, decoder pointer or internal
message is supplied by the bench. The shared media peer validates and
acknowledges the native frames; its `mu4_native_recorder_response` prefix
is retained for compatibility and does not classify playback as recording.

The verified run enters the original request and inspection paths once,
reaches the named-loader call at `03:a9c3`, and materializes original
`RELSI16 .BIN` (`aa99`), not `AACSI16`. At the common vector entry the
independent final-write comparison accounts for all 3,127 source records,
49,843 written words and 49,839 compared RAM words, with four low-window
words excluded. Incoming replies have class/selector/size
`1/58/12`, `0/02/16`, `0/03/20`, and each receives the normal pin ACK.
Those replies do not alone establish decoder completion or sample output.

Mapped-SP MVKD is supported through the existing retained data-copy path.
SP (`0018`) is a Read-owned data operand, separate from the earlier
compiler-mode destination address. `mvkd-sp` independently verifies 384
copy cases and their replays: direct DP/CPL, indirect pointer modifiers,
absolute/offset destinations and direct MVDM to an AR; four source patterns,
both CPL states, two program pages and three save points. It changes SP
after address formation and again after Read, and replaces fetched
literals, so a reread or recomputed address fails. Another 576 replayed
cases cover older FRAME/PSHM/POPM/STM producers, zero/one/three-word gaps,
a younger FRAME and preserved accumulator patterns. Twenty rejected
control/peripheral-source cases and five rejected control destinations
remain fail-closed. The same fixture fails at the source guard before the
admission; the existing ADD-SP profile also remains green. These are
bounded pipeline-model checks, not measured silicon phase timing.
The complete 288-marker default core suite and the native 5110 save-state
gate also pass with this admission. They protect their existing scopes;
neither is a REL playback or complete native-handset acceptance.

The original REL sequence at `02:a81e`, `7090 0018`
(`MVKD $0018, *AR0+`) now executes. Neighboring instructions copy AR1,
AR6 and AR7, and the adjacent restore sequence uses MVDK to restore SP;
that context-shaped layout does not by itself identify a task or RTOS ABI.
The original `02:6116`, `fb82 510c` (`FCALLD $02:510c`) is the first
instruction of active block `02:6116..6120`.
The block count is initialized to six at `02:610f`; the call's delay words
are `e801 f495`. TI's [SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf),
FCALL[D] page 4-57 and RPTB[D] page 4-148, explicitly restricts far calls
inside block repeats. This restriction does not define a hardware trap or
prove that the supplied program is invalid. TI's separate
[extended-addressing report](rom4_dsp_loader.md#repeat-endpoint-and-page-hazard)
establishes that REA compares only the low PC, including on foreign pages.
The paired atomic/staged hazard fixture passes 384 replays with FCALL and
FCALLD. The bounded delayed-call composition now passes its separate
2,304-case frame/delay/count/replay fixture and the 20 exclusion controls.
This models the explicit instruction/register effects without rescinding
the manual restriction or claiming measured silicon phases or unrestricted
control mixtures. It does not clear BRAF, bypass the loop, synthesize a
callee return or introduce hidden call-owned repeat registers.

`mu4_context_contract.py` pins all 25 caller words and all 55 callee words.
The callee has one forward BGT branch at `02:5117`, with a two-word
`STM #a858,AR3` delay, and a sole FRET at `02:5142`. Both paths converge
at `02:512e`. It has no calls, repeat setups, backward edges, BRC/RSA/REA
writes or BRAF/CPL writes. Its fixed stores target `a858..a859`, `b331`
and `a854`; it pushes AL and pops that word into AR0. Direct offset zero
requires incoming CPL=1. Do not assign the following routine's BRC write
at `02:5170` to this callee: the separate prologue starts at `02:5143`.

The read-only native snapshot matches all 25 loop words in the common
program window, with OVLY enabled, CPL/BRAF set and BRC/RSA/REA equal to
`0006/6116/6120`. Source image and live code agree on the same-page call
and both delay words; initial overlay comparison covers the pinned callee.
This excludes missing loop words or an unclassified callee-owned repeat
setup, not the provisional physical DA150 map or all call/loop hazards.

The unchanged external playback input also executes `02:5fdd`,
`6ff8 0015 0c81`, `STL A,1,*($0015)`. Its destination is C54x AR5
(`0015`), not an unidentified external peripheral. Shifted stores now
admit AR0..AR7 using their ordinary Execute-time write; other low mapped
destinations remain guarded. `MVDK Smem,AL` at `02:6050`, `710a 0008`, also
executes through the tested generic accumulator-destination move path.
The immediate destination is AL, but source mode `0a` denotes AG only
with CPL clear and DP=0; with CPL set it denotes `*(SP+10)`. Do not infer a mapped
source solely from the opcode's low byte.
The run observes
929,701 NAND data reads and 26,211 strobes; unchanged counts do not negate
instruction progress, and neither count is decoder/output acceptance.

`mu4_context_contract.py` also pins the 13 original words at
`02:5fdc..5fe8` and decodes the shifted-store opcode/operation-extension
order independently of the CPU. The preceding instruction writes AR4
(`STLM A,AR4`), not AR5. After the three-word AR5 store come a NOP,
two-word `LD *+AR4(-15826),B` and `STL B,*AR2+`; the first local AR5
consumer is `DLD *+AR5(-15698),B` at `02:5fe4`. Those four intervening
words do not indicate an immediate-use latency violation.

SPRU131G table 7-5 places shifted STL/STH in the store-type group;
table 7-7 gives an AR write two latency cycles before a category-I
consumer such as long-offset indirect DLD, or one before category II.
This is a local static spacing contract, not executed publication evidence.
The early-DAGEN conflict checks include the `6fxx` shifted-store family
alongside ordinary stores, STLM and RMW.

Executable shifted-AR acceptance is split into four isolated profiles:

- `st-shift-ar`: 122,880 replay cases, four extended address forms, all
  eight AR destinations, STL/STH from A/B, four accumulator patterns,
  SST/SXM combinations, shifts -16/-1/0/1/15, self/other pointer aliases,
  two program pages and three save points. Address and operation
  extensions are changed after fetch; address registers and the source
  accumulator are changed between Read and Execute. The test requires
  the retained address but Execute-live source, one write and no data read.
- `st-shift-ar-use`: 30,720 replays of absolute stores followed by legal-gap
  indirect consumers, with gaps 2/3 and the same shift/arithmetic matrix.
  This does not define forbidden-gap behavior or measured silicon phases.
- `st-shift-ar-bank`: 768 replays of short direct stores followed by STM,
  MVDK, POPM or constant ST to the same or a different AR. RAM controls
  publish at clock 7; shared-bank collisions defer the younger write to
  clock 8 without repeating its source read or stack increment.
- `st-shift-ar-edge`: 48 replays retaining twelve control/peripheral
  destination exclusions before and after rejection, with no data access
  or register mutation. These are prototype exclusions, not silicon traps.

These isolated shifted-store profiles are separate from the default CPU
suite; passing that suite does not imply their coverage.

MVDK to AL/AH/AG/BL/BH/BG retains its Access-owned source and writes at
the following clock, like the existing retained RAM-destination path.
Guard writes mask to eight bits and preserve the accumulator's other fields.
`mvdk-acc` passes 8,208 replays over eighteen mapped sources plus RAM,
six destinations, direct/indirect/absolute addressing, four word patterns,
two program pages and three save points. Fetched literals and captured
sources are changed to check retention; full accumulator comparisons check
sibling preservation and source/destination aliases. `mvdk-acc-prod` adds
15,552 replays with mapped STL/POPM and direct LD producers, gaps 0/1 and
all six source/destination fields. Adjacent short-form direct LD results
remain a prototype projection, not a guarantee for code violating
SPRU131G table 7-28. MVMD accumulator
destinations and unrelated control/peripheral moves are not admitted.

### REL copy dependency

The copy loop at `02:45b1..45b2` (`DLD *AR1+,A; DST A,*AR0+`)
uses destination `(BK + 2 * stack[SP+8]) & ffff`. Under CPL=1, the
`08` direct operand is a stack slot, not mapped AL. BK comes from
`MVDK *(SP+14),BK` at `02:4547`, not mapped T: three saved registers,
nine locals and the two-word far-return frame place this at caller
stack argument 0. The `02:47d3` caller supplies bases `7000` and `e800`,
opaque context `ac36`, and entry A values `e800` and `f000`.

The load/use dependency at `02:459c..459f` is:

```text
MVKD $0011,*(SP+8)  ; retained AR1 word -> stack slot
LD   *(SP+8),A      ; adjacent same-address consumer
ADD  *(SP+7),A
```

SPRU131G section 7.3.2 requires same-location on-chip reads to consume
a pending write through the internal bus. The staged read-bypass path
includes MVKD's retained Read operand; it neither re-reads live AR1 nor
advances the real destination write. `mvkd-forward` passes 6,912 replays
over direct/postincrement/absolute destinations, LD A/ADD A/LD B consumers,
same/different addresses, gaps 0/1, SXM/OVM, CPL, two pages and three save
points. Changing AR1 after Read and changing fetched literals checks
retention; source reads and real writes occur once. Bank contention and
the physical DA150 memory extent are not established by this CPU fixture.

Without this bypass, the consumer read stale stack word `ac78` despite
MVKD writing the intended index (0,4,8,...); the following stack store
saved the stale value again. This generated `40f0` and overwrote code.
With forwarding, the observed destinations advance through `7000` and
`e800` buffer ranges; the latter 49 setup samples span `e800..ef40`.
A read-only clean-build snapshot matches all 13 original AA99 words at
`40e8..40f4`, including `0c9e 5682 5483 f622 6f95`, in both program and data
spaces. The argument probes were removed after the CPU correction.
This resolves the observed overwrite, not all memory-map fidelity.

`mu4_context_contract.py` pins all 34 entry/dependency/copy words and
tests CPL=1 pointer arithmetic. Initial overlay equality alone is not
evidence that code survives later data writes; retain the distinction
between a bad computed pointer and a physical aliasing defect.

### REL delayed shifted-store contract

The original playback executes instruction `02:41bc`, `6f93 0d9f`,
`STL B,-1,*AR3+`, with the live PC already `02:41bf`.
These two words occupy the delay span of `RPTBD 41eb` at `02:41ba`;
the ordinary body begins at `02:41be`. The decoder admits a complete
two-word shifted STL/STH only when the retained delayed-control owner is
RPTBD. Other control owners, three-word operands, a store spilling beyond
the delay span and unsupported mapped destinations remain guarded.

`rptbd-shift-st` passes 115,200 cases and save/replays across repeat counts
0/1/2/7, direct and indirect addressing, decrement/postincrement, mapped
AR destinations, all four accumulator/store selectors, signed shifts,
SST/SXM, program pages, CPL and IRQ/NMI arrivals. It checks one delay
write and pointer effect, Execute-live source capture, retained fetched
words, BRC+1 body iterations and balanced interrupt frames. The separate
`rptbd-shift-edge` profile passes 16 cases/replays for span spill,
three-word addressing, BRC destinations and a different control owner,
with no data or pointer side effects. These exclusions are prototype
limits, not claimed silicon traps; physical pipeline phases remain unvalidated.

### REL memory-sourced repeat contract

`RPT Smem` now admits basic direct operands and short indirect stationary,
postdecrement and postincrement forms (`4700..4797`), in
addition to the existing absolute `47f8` form. DP/CPL are retained at
Decode, the compiler-mode SP base at Access, and the count is sampled
once in Read. The selected pointer changes once for count acquisition,
not once per repeated body. The two extra Class 5A/5B clocks remain a
partial timing placement, not measured silicon phases. See TI SPRU172C
4-146/147 and the existing SPRU131G address-latency contracts.

`rpt-smem` passes 10,368 cases/replays over 36 address/source cases,
counts 0/1/7/255, CPL, compatibility mode, program pages, no interrupt/
IRQ/NMI and three snapshots. Eight additional direct-RAM cases cover
unsigned counts `8000`/`ffff` on both pages and two snapshots, including
65,536 body executions for `ffff`. Tests observe one count read, one
body fetch, exactly RC+1 Execute observations, retained count/body despite
backing-memory mutation, balanced deferred interrupts and unchanged
unrelated state. The earliest replay restores the fixture's queued
external interrupt assertion; later snapshots retain the CPU's accepted
or pending interrupt without supplying a second edge.

`rpt-smem-edge` passes 100 cases/replays covering control/peripheral
sources, write-only short MOD=3, long-offset addressing, nested repeat,
a delayed-control owner and a block-repeat body. Its 64-case MOD=3 bank
crosses all eight ARF values, both compatibility states, both pages and
two snapshots. Every exclusion rejects without data or pointer effects.
TI SPRU131G table 5-4 marks short MOD=3 as write-only, so its former
RPT read fixtures were outside the legal domain. The other exclusions
remain prototype limits; no rejection claims a silicon trap mechanism.
Producer spacing, modifying
self-AR count sources and additional repeated-body families require their
own composition evidence. Encoded direct operand `00` does not by itself
establish an IMR or stack-relative source.

### Repeated accumulator loads

The repeat engine admits `LD src,SHIFT,dst` and `LD src,ASM,dst`
(TI SPRU172C 4-66..68). Each iteration consumes the current source
accumulator and updates only its destination and sticky destination
overflow. Same-accumulator loads therefore compound; cross-accumulator
loads do not. SXM governs right-shift extension and OVM governs
left-shift saturation. The instruction remains one word, with one
Execute publication per steady-state clock. Other register-operation
families remain independently guarded.

`rpt-ld` passes 200,704 cases/replays. Its 32,768 arithmetic cases
cover all four source/destination pairs, all 32 signed shifts, both
shift sources, SXM/OVM, eight source patterns and four repeat counts,
with distributed pages/snapshots. Another 165,888 cases cross short/
long immediate, DP, SP, absolute-RAM and absolute-AR counts with three
representative shifts, flags, two patterns, both pages, IRQ/NMI and
three snapshots. The independent oracle compares both accumulators,
sticky flags and RC at every clock, not just the final result. Tests
also poison the backing opcode/count after capture, require a fresh
one-word continuation and balance the deferred interrupt frame.

The remaining 2,048 cases put immediate-shift loads inside short RPT
inside RPTB, with outer counts 0/2 and inner counts 0/1/7/255. The
focused `rpt-ld-block` profile runs this bank alone. It checks every
arithmetic publication, independent counters and one inner continuation
per outer iteration. RPTB's initial discarded sequential fetch must not
be mistaken for an executed body: the fixture gives that fetch a distinct
`e8ff` word, replaces it before the redirected accepted fetch, and proves
only the actual load executes. This is a declared pipeline-model test,
not a measured physical fetch phase. Block/IRQ composition and memory
counts inside blocks remain outside this bank's claim.

`rpt-ld-edge` passes 32 cases/replays for eight other operation families,
rejecting before data, arithmetic or flag effects. These remain prototype
exclusions, not silicon traps.

### Accumulator-to-PMST stores

Ordinary direct `STLM A/B,PMST` is admitted. TI SPRU172C 4-175 defines
the direct MMR destination independently of DP/CPL; SPRU131G table 4-3
explicitly includes STLM under saturation-on-store. The core therefore
uses the Execute-live accumulator and the old SST/SXM values before
publishing PMST. Source accumulators and status flags are unchanged.
SPRU131G 7.5.9/table 7-26 place program mapping at Prefetch; example
7-62 explicitly uses STLM to update PMST. Already prefetched words are
not remapped or flushed by this store.

`stlm-pmst` passes 38,912 cases/replays. Its 4,096 ordinary mapping
cases cross eight 40-bit source patterns, A/B, old SST/SXM, CPL, old
OVLY, two program pages, low/high entry and four snapshots. The source
changes after Read to prove Execute-live capture. Distinct old/new-bank
marker instructions prove retained prefetch and subsequent OVLY mapping.
The remaining 34,816 high-entry cases cover all 16 IRQ inputs and NMI,
early/late arrivals, the same source/saturation patterns, pages, old OVLY
and snapshots. A vector fetched on the publication clock retains the
earlier IPTR/OVLY sample; a later vector uses the newly published values.
Every run requires one PMST write, a balanced optional frame and unchanged
XPC, pointers and unrelated state.

`stlm-pmst-edge` passes 32 cases/replays for delayed-control, single-repeat
and block-repeat owners, RSA/REA/SP destinations, indirect addressing
and peripherals. These reject without data, control or pointer effects.
They are prototype exclusions, not silicon restrictions. This admission
does not establish external-access interlocks or physical MP/MC/DROM
memory decoding; the mapping assertions concern the existing OVLY/IPTR
model, and phase placement remains explicitly partial.

The original AA99 REL image contains `491d f340 0021 f495 f495 891d`
at `02:9fa9..9fae`: an ordinary PMST read/modify/store sequence.
AA22 has a different word (`3763`) at the same numeric store address;
the images must not be merged into one code namespace. Original playback
now passes this sequence without substituting a control value.

### Long-address PSHD contract

`PSHD Smem` (SPRU172C 4-131, class 16B) admits long offset,
long preincrement, long circular preincrement and canonical absolute
forms (`4be0..4bf8`). The complete address word is retained in Decode,
Access resolves the source and allocates the stack word, Read captures
the source once and Execute writes the retained value. The source and
stack address have separate owners. In an isolated entry the declared
model resolves/allocates at clock 5, reads at 6 and writes at 7; physical
bank timing and external-memory contention remain unvalidated.

`pshd-long` passes 99,696 cases/replays. Its 96,000 RAM cases cover all
25 forms, four values, both compatibility/CPL states, BK=0/9, two pages,
early/late IRQ/NMI and three snapshots. Linear offsets are -1/0/4/32767;
nonzero-BK circular offsets are -1/0/4/8, with a valid initial index and
step magnitude at most BK. Absolute cases include the old stack top,
the push destination itself and the end of the 16-bit data space.
Poisoning the fetched literal and sampled RAM cannot retarget or change
the push. Every run requires one source read/write, one pointer update
where applicable and a balanced optional interrupt frame.

The remaining 3,696 cases resolve 26 mapped-register sources through
offset, long preincrement or absolute Smem addressing. Control-register
inputs remain fixed safe state; ordinary register fields use four patterns.
The modifying self-AR case is excluded, not silently tested against its
own updated value. `pshd-long-map` runs this bank alone.

`pshd-long-edge` passes 64 cases/replays for SP, peripheral and modifying
self-AR sources, an unadmitted mode and delayed/repeat/block/XC owners,
across both compatibility states and pages. They reject before stack,
data or pointer effects; address-preview AR/ARP updates are discarded
on an unadmitted source. These are prototype exclusions, not claimed
silicon exceptions. Older-producer, adjacent-stack and compound-loop
compositions need their own coverage. Long MOD=13 preincrement is a
read-capable addressing mode, unlike short write-only MOD=3.

### Extended POPD and playback execution boundary

POPD's encoded Smem is a destination. SPRU172C 4-127 specifies one extra
word and cycle for long-offset/absolute addressing. The staged path retains
the extension in Decode, resolves the destination in Access, reads the old
stack top and increments SP in Read, then writes the captured word in Execute.
It advances past both instruction words. Its isolated clocks are 3/5/6/7
for extension/Access/Read/Execute, a partial model rather than silicon timing.
Address-preview AR/ARP effects are rolled back on rejected destinations.

`popd-ram` requires its existing 50 short/composition cases plus 16,128
extended cases/restores over all 25 encodings, four values, CMPT/CPL,
two pages and three snapshots. Circular cases require BK=9, a valid index
and bounded steps; zero-BK circular cases are omitted with raw IDs preserved.
Literal poisoning, later AR changes and stack-memory mutation require one
retained address, one stack read and one destination write, including a
destination aliasing the old stack top. `popd-edges` retains its 31 existing
checks and adds 64 rejection/replay cases for extended mapped/SP/peripheral
destinations, an unadmitted mode and delay/repeat/block/XC owners. Rejection
performs no operand transaction, stack allocation or pointer change.
Extended mapped destinations, IRQ composition and physical timing are not
accepted by these fixtures. Eight neighboring stack/control profiles,
native 5110 save-state and 2,929 tool tests (one skipped) pass.

AA99 contains `8a10 8be1 0004 f4e4` at `02:a26f..a272`; the admitted
long-offset POPD is followed by FRET, not a missing source image. On the
unchanged protected NAND, original playback executes this transfer without
an illegal instruction. The current complete-window execution and codec
observations below remain separate from playback acceptance.

### Playback execution and codec observations

The Execute-entry observer excludes XC-annulled words. The staged core
implements rejection with an internal NOP while preserving its word address;
that slot must not be reported as an original instruction. A per-slot
annulment marker is shifted, reset and saved with the pipeline and only
suppresses the observer callback, not execution clocks or guest state.
Genuine source NOPs, including accepted conditional NOPs, remain observable.
`instr-observer` passes its original 32 cases/restores plus 64 XC1/XC2
accept/reject cases/restores across atomic/staged frontends, pages, OVLY
states and four snapshots. Before the fix, staged case 33 reports an extra
`f495` at the rejected `e966` address; the corrected event sequence matches
the atomic control. Neighboring XC control/signed profiles and native 5110
save-state restoration pass. Older raw instruction-entry totals can include
these synthetic slots and must not be compared as exact source counts;
the source-validated range census excludes them. This remains an entry
observer, not an instruction-retirement assertion.

The detailed baseline below uses the protected NAND
`93548b594a91df0ba87acc316435b12fb6fa35da97b066f0c7141748c5d94bb8`.
An independent 20-second playback of the fresh, current-core recorder
endpoint documented under [Recorder metadata request contract](#recorder-metadata-request-contract)
also verifies the title/overlay and 14 replies, with `illegal=0` at
`02:b5e1`. It observes one bootstrap, twelve resident and 24 loader-API
Execute entries; 87,390 completed arithmetic-loop calls and 5,592,960
body entries. The 1,524,209 TX words, 1,691,842 DIN words and 1,363,518
actual DMA source reads are again all zero, with source range `1000..137f`
and the same stream hashes as the baseline. This excludes the stale
recording as the sole cause of silence, not a container, encoder, decoder
or buffer-ownership defect. The diagnostic intentionally fails with
`audio completion is not validated`; no audio acceptance is weakened.

Program-bus hits are not invocation counts. The 20-second run has three
physical bootstrap fetch hits but one Execute entry at logical `00:0e41`.
Physical resident address `00:6d62` has 24 fetch hits; the observer records
12 actual Execute entries at logical `02:6d62` under OVLY. The first resident
entry is at 0.227594058 seconds; subsequent observed entries include
2.366754525, 4.002378107 and 5.638001690 seconds. Bootstrap re-execution is
not established by its three fetch hits. The recurring resident entry is real
and follows the named-loader path described below. These are Execute-entry
observations, not retirement or atomic register snapshots.

AA99's `a2c6` loop loads BRC=`003f`, repeats through `a2e9` and returns at
`a2ea`. Opcode-qualified Execute observations record 87,510 starts,
5,600,640 body entries and 87,510 return entries. The bounds
`64 * returns <= iterations <= 64 * starts` hold with all observed calls returned;
the sampled PC is not evidence of a stuck loop. The source calls this loop
`filter` descriptively, not as a recovered Nokia function name. AA22 has
different instructions at these addresses and must not inherit the attribution.

The audio observation window starts at the verified decoder Execute entry,
0.818115786 seconds, not at codec initialization. It counts 1,524,209 McBSP
TX words and 1,691,842 codec DIN words; both streams contain only zeros.
Their whole-stream FNV hashes differ (`7d8d3d4d` versus `1e4f2315`) because
the stream lengths differ. Codec
power/format/rate/active registers are `0007/0053/0023/0001`, with 230,090,537
clock rises and 845,921 frame rises. Counts and hashes are independent:
neither zero-frame alignment nor a one-to-one PCM relation is established.
The fixture's converted-sample ADC input remains an explicit bench source;
nonzero DIN alone is not decoded music or analog audio acceptance.

Within the same decoder-entry window, actual channel-3 data-space source
reads total 1,363,518, all zero, over addresses
`1000..137f`. This attribution uses `source_read_matches`, not merely an
enabled source-register match. The bounded prefix is `1000`, `1040`,
`1001`, `1041`, then `1002`, `1042`, `1003`, `1043`. Eight bounded register
snapshots confirm count `1/0` alternating, frames `203f..203c`, mode `c541`,
reload count `1`, and element/frame indices `0040/ffc1`. This is the expected
two-element frame sorting, not the former downward-63 walk caused by
re-latching the live count on DE re-enable. That defect's 1,277 nonzero
output words are not usable evidence of decoded media. Complete zero-word
streams also do not prove correct media decoding or stream pairing.
No writer-instruction ownership is
inferred from the data read or the CPU's contemporaneous PC.

The focused DMA pause/restore regression passes, and 2,943 tool tests
(one skipped) pass. The fixture deliberately
terminates with `audio completion is not validated`, so the Make target remains
failed before its later exact-one fetch-count checks. Those legacy fetch checks
are not evidence of three actual boots and were not relaxed into success.
The manual checker validates observation completeness, bounded executed-byte
trails and loop bounds, not playback acceptance:

```sh
python3 tools/noki5510_bootstrap_check.py --playback-observation RUN_DIR/error.log
python3 tools/noki5510_bootstrap_check.py --playback-dma RUN_DIR/error.log
```

### Output-copy observation

The AA99 contract pins 74 words around `02:a31f`. When the double fields
at offsets 2 and 4 are equal, the copy count is the minimum of remaining
source extent (`[7]-[8]`) and destination extent (`[11]-[12]`). AR3 and AR2
are formed from bases `[6]` and `[10]` plus the **old** cursors. Both object
cursors advance before `02:a366: MVDD *AR3+,*AR2+`; consequently a snapshot
in the copy body already contains end cursors. Exhaustion/full flags at
offsets 9/13 are set for the limiting side, both on a tie. The reference
model is restricted to nonnegative signed-word extents and bounded cursors;
2,025 small cursor combinations plus boundary cases test its partition.
The unequal-field helper and an all-alias writer census remain outside scope.

`mu4_playback_frame_observe.lua` is a passive two-second instrument. It
selects real bit-reader data reads and copy-context writes during bounded
windows, captures RAM snapshots, then removes its taps. GENPC is a context
selector, not a retained writer identity. It does not require the debugger
or change guest state. The earlier all-zero bank statistics belonged to a
superseded core/media pair and are not current audio evidence.

The fresh run has 2,401 selected bit-reader reads and 310 sampled copy
boundaries, with no cap or partial-word accesses. The initial `162e` RAM
snapshot contains all 2,977 captured AAC bytes exactly at byte offset 512.
Two objects (`13e0`, `1460`) supply chronological 1,024-word channel blocks
from alternating pairs `7000/7400` and `7800/7c00`, copied in 64-word chunks.
Nine complete blocks per object and a partial tenth are retained; the first
eight pairs are compared with the independently decoded reference, without
shifting, scaling or selecting channels. Every observed first copied word
matches its source snapshot at the old cursor. This is sampled copy/material
evidence, not proof of every MVDD read or decoder-call-to-packet ownership.

The first block pair is zero on both sides. The next has the same
448-sample zero prefix and 576 nonzero samples per channel. The corrected
shifted-store DAGEN ordering removes the large pre-DMA differences; the
remaining small numerical differences are described below. This comparison
uses chronological source blocks, not a fitted serial-stream offset.
The bit-reader taps and RAM snapshots remain inputs to the processing
checker. Retire this material/copy probe only when equivalent direct read
and store captures replace those checkpoints.

Use the canonical recorded NAND, `bootplaystage`, an isolated working
directory and absolute paths as for playback, adding:

```sh
-autoboot_delay 0 -autoboot_script tools/mu4_playback_frame_observe.lua -seconds_to_run 2
python3 tools/mu4_playback_frame_check.py RUN_DIR \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00 \
  --reference-aac captured-eight-packets.aac --reference-pcm host-eight-frames.s16le
```

A separate 59-word witness pins `023d73`'s input setup, the processing call
`023de5 -> 024610`, its nonzero-result exit, and the following 1,024-iteration
two-channel conversion loop through `0248a6` with argument 14. The bounded
runtime proof follows below. With OVLY set, low logical addresses
such as `02:3d73` execute from physical page zero; disassembling raw `02:3d73`
can show unrelated zero backing RAM. Source and retained Execute identity,
not such a raw debugger view, remain authoritative.

### Processing And Conversion

The first eight original `023de5 -> 024610` calls return zero and each
completes a 1,024-sample, two-channel conversion loop. Their net consumption
is 373 bytes followed by seven 372-byte packets: exactly the eight declared
ADTS extents. Each consumed byte matches the independent reference through
the entry reservoir or an actual in-call bit-reader bus read, not merely a
header/payload-prefix comparison. There are 186 such word reads per call.
The last three prefetched bytes lie outside the eight-packet reference;
they remain unconsumed reservoir lookahead and are not validated content.

Byte position is `2*B331 - remaining_bits/8`. The first call advances from
512 to 885 in the initial buffer. Subsequent calls carry three header bytes
in the reservoir and advance from -3 to 369 after compaction. This establishes
packet boundaries without waveform alignment or a guessed file offset.
Conversion destinations alternate `7000` and `7800`, with the second channel
at destination+`0400`. Returned destinations match later copy snapshots,
after the corresponding output loop and before the next processing call
that can reuse the same buffers.

FCALL has already reserved its two stack words at Execute entry. The observer
therefore normalizes caller SP as observed SP+2; the processing A argument
is caller SP+6. Sampling the unadjusted stack word would read the return page,
not converter argument 14. This is a pipeline observation constraint, not a
firmware ABI or scheduling correction.

A 152-word witness pins `0248a6`, its `024e38`/`024d43` shift composition
and the selected arithmetic-right-shift helper. For the observed argument
14, the independent scalar contract is:

```text
rounded = (signed32(raw) + 8192) >> 14
output = clamp(rounded, -32767, 32767)
```

Rounding ties go toward positive infinity. The source explicitly normalizes
to signed 32-bit; zero-extended negative DLD inputs are valid even when the
40-bit accumulator guard is zero. This is not a license to alter general
accumulator signedness or other helper arguments.

All 16,384 observed conversions in the eight successful blocks follow
the independent rounded-shift/clamp model and reach their later pre-copy
source buffers unchanged. The large differences were upstream: shifted
stores could resolve an implicit pointer update after a younger load had
already resolved its address. The converter, copied samples and input
packets were not changed to fit the host reference.

Use the same passive two-second run and independently regenerated reference:

```sh
python3 tools/mu4_processing_check.py RUN_DIR \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00 \
  --reference-aac captured-eight-packets.aac --reference-pcm host-eight-frames.s16le \
  --require-eight-blocks
```

The checker requires call/return order, zero results, original Execute
identity, complete packet-byte coverage, conversion-loop extents, buffer
lifetimes, all sampled conversion values and later RAM correspondence.
The strict option rejects legacy two-block captures; those remain readable
only with their smaller coverage explicitly reported. Tests include drift
in the last conversion and the eighth block's later copy buffer.
All 16,384 converter input payloads also match the final-tail vectors.
The four native word/slot streams and all 858 existing arithmetic snapshots
remain byte-identical to the smaller capture. The optional host-reference
error statistics are diagnostic, with no fitted alignment or assumed codec
tolerance. This proves bounded eight-packet binding and conversion,
not correct decoded audio, all media, analog output or native speech.

### Native Transform Arithmetic

The selected short-block driver takes parameter `0080`: 128 real/imaginary
32-bit components, or 64 complex points in 256 data words. This geometry
comes from its source loop bounds, not an imported library prototype.
The wrapper calls pre-transform `023f05`, bit reversal `023fc5`, transform
driver `023fee`, then post-transform `023f68`.

The passive Execute observer captures every short and long vector in the
first eight packet-bound calls: after bit reversal, combined stages 1-2,
stage 3, each weighted stage and the driver return. Coefficients are
captured separately at `f400..f7ff` and `117a/117b`. The `117b` pointer
names the coefficient's low halfword; starting a pair there would capture
the wrong constant.

`tools/mu4_transform_check.py` pins 645 source words across the wrapper,
bit reversal, controller and three kernel bodies. It verifies snapshot
identity, geometry, hashes, enclosing processing call and stage order.
Independent integer formulas reproduce every component at every captured
stage for all 30 vectors, including source truncation points and return
preservation. The 192 checked kernel-stage vectors comprise 124,928 signed
components. The weighted real cross terms share a shift before high-half
products are added; separately truncating them changes low bits. The source
deliberately omits low*low products, so a full-precision multiply is not the
bit-exact reference.

```sh
python3 tools/mu4_transform_check.py RUN_DIR \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

The large discrepancy was a core address-ordering defect, not a missing
DSP peer or fitted sample conversion. Shifted STL/STH resolved their
Read-owned implicit DAGEN update after a younger load's Access. An adjacent
DLD could therefore use the old pointer while the older store itself was
retargeted by the younger update. The isolated `indirect-dld` profile first
failed at shifted-store variant 96: address `1001` rather than `1002`.
Ordinary-store controls passed. Resolving the older implicit update before
younger Access fixes this while preserving Execute-owned data publication.
All 1,152 composition cases/restores now pass, followed by 23,040 indirect
DLD cases. Explicit mapped-AR writes, delayed repeats and paired arithmetic
retain their separate regression gates and partial timing claims.

The shared core also passes `make verify-5110-menu` after this correction:
the native NSE-1 ROM4 handset reaches its exact Phone book menu oracle through
physical input, without a DSP reset request or illegal instruction. This is
a cross-consumer regression, not validation of MU4 audio or ROM4 speech.

The same change removes the 31/29 mismatched components in the selected
third-stage vectors. The first nonzero PCM block's maximum host-reference
error drops from 43,740 to 7 sample units, RMS from 11,553.09 to 0.8426.
Across the eight packet-bound source-block pairs, maximum error is 13 and
RMS 1.1492 over 16,384 signed samples. These comparisons use neither fitted
alignment nor gain. They do not define a codec-conformance tolerance.

#### Audio acceptance prerequisites

Keep the captured-recording comparison distinct from a normative decoder
test. [ISO/IEC 14496-26:2024](https://www.iso.org/standard/86376.html)
describes audio-conformance tests with compressed examples and reference
waveforms in an electronic annex. All eight captured ADTS headers identify
MPEG-2, profile bits 1 (LC), sampling index 4 (44.1 kHz), channel
configuration 2 and one raw-data block per frame. The MPEG-4 suite must not
silently be assigned to this MPEG-2 stream; select the applicable procedure
and establish supported payload tools separately. The present packets and
FFmpeg output are not that official suite.

[ETSI TS 126 406, section 5.1](https://www.etsi.org/deliver/etsi_ts/126400_126499/126406/19.00.00_60/ts_126406v190000p.pdf)
requires at least accuracy K=15 for its AAC-LC RMS/LSB criterion and refers
to ISO conformance procedures. This is an enhanced-aacPlus conformance
specification, not proof that the MU4 implements that target. Neither the
observed maximum of 13 signed sample units nor RMS 1.1492 can be substituted
for its criterion without the specified reference precision, normalization,
evaluation intervals and test sequences.

The [AAC validation contract](aac_validation.md) now pins an acquired official
baseline and draft procedure. Before adding an audio-acceptance threshold,
establish applicability, decoder configuration, delay and measurement point,
and implement the comparison rules independently. Broader original-media
coverage and exact DMA/TX/DIN epoch pairing remain separate requirements;
passing an arithmetic snapshot or this handset regression closes neither.

The passive `--inventory` report observes 30 wrapper entries
across the eight packet-bound processing calls: call 2 has sixteen 64-point
vectors (`7800..7f00` and `e800..ef00`, step `0100`); each other call has two
512-point vectors. The first long vector alternates between `7000` and
`7800`; its companion is `e800`. Entry identity, source opcode, parameter,
vector bounds and enclosing call times are checked. This is execution
coverage; the snapshot checker separately validates all 30 arithmetic paths.
The long driver
uses the same early kernels followed by additional weighted stages at
return sites `02408b`, `0240a6` and `0240be`; its wrapper return is `024946`.

```sh
python3 tools/mu4_transform_check.py RUN_DIR --inventory \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

The current snapshot format uses invocation-numbered files and records the
processing call, parameter, vector geometry and source opcode. It captures
312 snapshots, including per-invocation coefficients and constants. The
checker rejects missing/duplicate stages, altered identities or files,
overlapping invocations and changed return vectors. Synthetic short/long
fixtures include rehashed arithmetic corruption; independent inverse-DFT
impulses sanity-check the model at both sizes without defining a codec
tolerance. The four native word/slot streams and the original eighteen
selected snapshots reproduce the smaller proof byte-for-byte.

Remaining work is understanding the small PCM residual against a floating-point decoder,
coverage beyond these eight calls, and complete digital
transport alignment. Analog output and native speech remain unvalidated.
The earlier per-word arithmetic exclusions were valid for corrupted inputs;
they are superseded by this stage-level contract, not current frontiers.

TI's [SPRA554B double-precision C54x FFT report](https://www.ti.com/lit/an/spra554b/spra554b.pdf)
is an algorithm reference, not an identification of this binary. Its
32-bit data layout and specialized early stages corroborate the recovered
shape. Its scaling-flag descriptions on PDF pages 4 and 5 conflict; the
firmware's own branches remain authoritative. The ignored local PDF is
`roms/reference-docs/npm5/ti_spra554b_c54x_double_precision_fft.pdf`, SHA-256
`0dafa8acb794d328a6835cb718f93d367884adf15bd3e540d9ecd7e7e6fba7a0`.

#### Post-Transform Rotation And Permutation

The original `023f68..023fc4` routine rotates each complex pair using two
coefficient halves, then reverses the imaginary components in place. Its
split-word products omit low*low terms. Unlike the weighted FFT kernel,
each cross-product sum is shifted separately before the two products are
combined. A full-precision complex multiply is therefore not bit-exact.

The passive observer captures input, coefficients and output for every one
of the same 30 transforms, using their existing invocation and packet-call
identities. Short coefficients occupy `f200..f2ff`; long coefficients occupy
`f000..f7ff`. These are observed operands, not fitted constants. Every input
is byte-identical to the checked FFT return. Source-derived rotation and
imaginary reversal reproduce all 16,384 output components across sixteen
short and fourteen long vectors, with no mismatches.

```sh
python3 tools/mu4_transform_check.py RUN_DIR --post \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

This mode pins 738 original source words, including the post-transform's
93-word body. It requires complete FFT acceptance, exact input continuity,
all post snapshots and their geometry, hashes and chronology. Tests include
rehashing a wrong output and comparing split products against independent
full-precision bounds; neither supplies a PCM tolerance. Pre-transform,
window/overlap processing, other media, waveform conformance, analog output
and native speech remain outside this mode's acceptance.

#### Pre-Transform And Bit Reversal

The original `023f05..023f67` routine consumes alternating components from
the front and back of the input. Its split-word products have the same
omitted low*low terms, but it negates the first product before an arithmetic
half-shift. The other three products are half-shifted individually; only
then are the stored values shifted left three. Moving a half-shift after
the store, or negating an already rounded product, changes low bits.
The routine's final imaginary reversal restores the paired output order.

The passive observer captures all 30 inputs, coefficient tables and outputs,
using a shared pre/post snapshot implementation. The source-derived model
matches all 16,384 pre-transform output components. An independent complex
index bit reversal then matches every component of the captured FFT inputs.
These are separate checks: consistently rehashing a wrong arithmetic output
or changing only the permutation still fails.

```sh
python3 tools/mu4_transform_check.py RUN_DIR --pre \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

This mode pins 837 original source words and requires pre-transform,
bit-reversal, complete FFT and post-transform acceptance together. Scalar
tests independently bound the omitted low*low error against full signed
products, including the negative half-shift, at both supported sizes.
The result validates these 30 arithmetic invocations, not spectral input
correctness, window/overlap processing, all media, waveform conformance,
analog output or native speech.

#### Window-Product Helper

The `024a70` call invokes `025bb3..025c19`. Its outgoing count and
coefficient pointer are independent of the preceding FFT parameter. The
helper rotates mirrored left/right sample pairs using forward/backward
coefficient walks, then reverses each half-buffer in place. Each split-word
product is truncated before the products are combined. Captures use the
actual helper count; a long FFT does not imply a long window operation.

`tools/mu4_window_check.py` pins this 103-word body and its 15-word caller
setup in addition to the complete transform-chain witnesses. It checks
input/coefficient/output geometry, hashes, chronological call identity and
the full mirrored-product/permutation model. The helper's input is observed
after scaling and branch-specific operations; it is not claimed identical
to the post-transform output. Dispatch behavior, overlap carry and
final buffer movement remain separate contracts to establish.

All 30 captured helper operations match: eighteen 128-component windows and
twelve 1,024-component windows, totaling 14,592 signed output components.
Two short windows follow long FFTs. The shared snapshot writer avoids duplicating file and hash
logic across these observer boundaries.

```sh
python3 tools/mu4_window_check.py RUN_DIR \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

The combined checker pins 955 source words. Tests include both window sizes,
mixed FFT/window geometry, full-product bounds, metadata corruption and
consistently rehashed wrong arithmetic. This helper acceptance does not
establish correct selection of its inputs, complete overlap-add behavior,
decoded waveform conformance, analog output or native speech.

#### Transform Scaling

Caller `024a46` sends the full post-transform vector through `025c1a`,
which applies `025ce2` to each signed 32-bit component. The scalar subtracts
two signed 16-bit arguments: a positive difference repeats arithmetic
right shifts; a negative difference repeats left shifts. The final store
keeps 32 bits. This is distinct from the rounded/clamped PCM converter.

All 30 captured inputs are byte-identical to the checked post-transform
outputs. Independent shift/store arithmetic matches all 16,384 resulting
components. Observed argument pairs are `(10,14)` fourteen times, `(12,14)`
eight times, `(7,14)` six times and `(27,14)` twice, covering both shift
directions.

```sh
python3 tools/mu4_window_check.py RUN_DIR --scale \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

This combined mode pins 1,042 source words, checks each argument's identity
and invocation, exact post-transform input continuity, and complete scale
outputs before window processing. Tests cover zero/positive/negative shifts,
signed arguments, 32-bit wrap, malformed metadata and consistently rehashed
wrong outputs. This mode alone does not establish callback or tail behavior,
other media, decoded waveform conformance, analog output or speech.

#### Window Callback Buffer Exchange

The two indirect calls at `024a60` and `024a63` select these source-pinned
entry pairs from the observed mode:

| Mode | First callback | Second callback | Observed invocations |
| --- | --- | --- | ---: |
| 0 | `024af8` | `024955` | 10 |
| 1 | `024b13` | `024955` | 2 |
| 2 | `024b3e` | `02496e` | 16 |
| 3 | `024b59` | `02496e` | 2 |

Both calls, actual callee entries and returns are observed for all 30
invocations. The first callback exchanges signed 32-bit samples with a
16-bit buffer: modes 0/1 use 512 history words, mode 2 uses 64. New history
uses `((signed32 + 0x8000) >> 16) & 0xffff`, in reversed order. Old history
is sign-extended and shifted left 16 into the raw buffer. Mode 3 additionally
copies 64 reversed old history words into raw component indices `896..959`
and uses forward old-history order in the first 512 raw components.

All 16,384 raw components and 8,192 history words match the independent
buffer model, including unchanged regions.

```sh
python3 tools/mu4_overlap_check.py RUN_DIR \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

The combined checker pins 1,347 source words, verifies both dispatch targets
against actual entry/return observations, and checks first-callback buffers.
Tests cover all four modes, signed history, rounding boundaries, untouched
regions, dispatch corruption and consistently rehashed wrong outputs.
This mode alone does not validate second-callback memory effects, complete
writer ownership between calls, later output binding, other media, decoded
waveform conformance, analog output or speech.

#### Final Buffer Tails And History Continuity

The mode-specific tail at `024a7e..024af7` runs once per channel object,
sixteen times in the eight checked calls. Modes 0/1 leave these buffers
unchanged. Mode 2 publishes 448 rounded raw values into the first part of
the 16-bit history buffer, moves the old history into the raw prefix and
repositions the remaining raw values through the 128-component saved buffer.
Mode 3 swaps and shifts the saved/raw ranges, then reverses the moved range.
The ordered moves matter because the source and destination ranges overlap.

Ninety-six snapshots cover each tail's complete 1,024-component raw vector,
512-word history and 128-component saved buffer before and after execution.
All 26,624 output values match the source-derived buffer model. Together
with the first-callback exchanges, these tails account for all 15,360 reads
whose history words were previously observed: no differences remain. The
first 1,024 history reads are initial-state observations, not a validated
provisioning contract. This is continuity at captured boundaries, not proof
that no writes occur between them.

```sh
python3 tools/mu4_overlap_check.py RUN_DIR --tail \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

This combined mode pins 1,469 source words. Tests compare the tail formulas
against ordered overlapping copies, inject continuity breaks and reject
consistently rehashed wrong outputs. All 16,384 converter inputs match the
eight paired final-tail blocks exactly at their 32-bit payload boundary.
Valid zero/sign extension is checked separately; comparing signed 40-bit
register values directly would invent differences on zero-extended DLDs.

Second-callback memory effects, complete writer coverage,
other media, spectral input correctness,
decoded waveform conformance, analog output and speech remain unvalidated.

#### Coefficient Cache Publication

The second window callback publishes coefficients using two original
finite-difference decoders. `025c35 -> 025ffa` integrates signed word
corrections through three recurrence states; `025c6a -> 026038` integrates
four high-to-low signed nibbles per correction word through four states.
Each descriptor supplies an explicit prefix, initial states and correction
stream. The host model uses those original upload words, not fitted curves.
The checked descriptors stay in a non-saturating 32-bit domain; other
descriptor and overflow domains are not accepted by this model.

| Descriptor | Coefficients | Correction format | Observed loads |
| --- | ---: | --- | ---: |
| `bbe6` | 1,024 | Packed signed nibbles | 2 |
| `c4e8` | 1,024 | Packed signed nibbles | 10 |
| `bc72` | 128 | Signed words | 18 |
| `c572` | 128 | Signed words | 2 |

Callback `024955` writes `bbe6` at `f000` when its first flag is zero,
otherwise `c4e8`. Callback `02496e` writes `c572` at `f100` when either flag
is nonzero and writes `bc72` at `f000` when either flag is zero. Flags are
observed at the callback's actual argument pointer, not selected by the
harness. The other cache values are preserved in the expected image.

Sixty snapshots cover `f000..f7ff` before and after all 30 callbacks.
Every one of the 61,440 resulting cache words matches the source-derived
publication model. Every captured coefficient operand of the following
window helper is byte-identical to its published cache slice. The four
native word/slot streams and all 858 earlier arithmetic snapshots remain
unchanged; the strict eight-block processing/conversion check still passes.

```sh
python3 tools/mu4_coefficient_check.py RUN_DIR \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00
```

Run this alongside the processing checker with `--require-eight-blocks`.
The combined coefficient check pins 1,750 source words. Tests compare both
recurrences with independent binomial integration, exercise the selector
truth table and reject rehashed corruption in preserved cache regions.
Generator scratch effects, complete writer coverage, unseen decoder/media
paths, independent waveform criteria, analog output and native speech
remain outside acceptance. Cache equality is not absence-of-writes evidence.

### Resident reload contract

The common AA55 API at `00:2080` branches to disk loader `00:3538`.
The 20-second run observes 24 actual API entries. Its bounded first 16 trails
alternate descriptor 0 and descriptor 1. The initial AA55 caller is `00:09b6`;
subsequently AA99 `02:959b` passes A=0 for
`MCUSI16`, while AA22 `02:91c7` passes A=1 for `RELSI16`. All 12 resident
trails show loader `00:3545 -> 00:2000`, followed by the loaded entry's
`00:2002 -> 02:6d62`. This is not an arbitrary FRET into startup or a reset
vector re-entry. Retained executed words identify the calling overlay;
current memory at the same numeric address is not a substitute.

AA99 `02:956f..959c` compares lifecycle `[373e]` with 4 and skips this
cleanup when unequal. `7212 3742` at `02:9575` is **MVDM $3742,AR2**:
the shared word is an input control, not a published worker result. The
AEQ precheck tests control 1; long-offset BANZ then tests
`(AR2 - 2) & ffff` without modifying AR2 (SPRU172C BANZ example 3).
With lifecycle 4, exactly control 2 selects state 2 and an additional call
to `03:9005` with caller `[SP+1]=001a`; all other 65,535 controls
select state 5. Both paths join the mount and descriptor-0 reload sequence.
No nonzero error/control value is required for the observed state-5 path.

`mu4_context_contract.py` pins all 46 AA99 words and the two AA55 API words,
enumerating all 65,536 controls. It makes no EOF/error or PCM-content claim.
The passive three-second Lua observation independently records lifecycle 4
at 0.837819684 seconds, a control read of zero at 2.198474888, state 5 at
2.198475021, and state 4 again at 2.589543040. Its terminal PC is `02:5beb`,
`illegal=0`, with 11 lifecycle writes, three control writes and five control
reads. GENPC selects a bounded read-context range; it does not establish
Access/Read ownership. The data-write tap is not an all-alias writer census.
MVDM, long BANZD, TC-branch and absolute-CMPM core controls also pass.

Use the same fresh copied benchmark NAND, seed ROM path and isolated working
directory as `check-mu4-bootstrap-original`, adding
`-autoboot_delay 0 -autoboot_script tools/mu4_playback_lifecycle_observe.lua`
with an absolute script path and `-seconds_to_run 3`. This short observation
does not run the separate 20-second acceptance diagnostic:

```sh
python3 tools/mu4_context_contract.py roms/5510-mu4-reference/InitData_R060.a00
python3 tools/noki5510_bootstrap_check.py --playback-lifecycle RUN_DIR/error.log
```

Next: follow REL frame/format ingress and the last producer feeding `7000`,
with an independently valid media control where the original container
contract permits it. Then pair valid McBSP words and codec frames before comparing PCM
against the stored media. Do not remove the programmed reload merely to keep
resident-entry counts at one.

The current broad CPU regression and remaining operand-domain audit are
summarized under [Operand-domain acceptance](c54x_execution_contracts.md#operand-domain-acceptance).
The long-PSHD acceptance remains its three scoped profiles, with separate
RPT, stack/repeat and PMST controls. These checks do not establish physical
bank timing, all legal/illegal operand domains or decoded audio.

The external request, saved file and peer replies remain unchanged. The
original REL diagnostic fails closed with `audio_acceptance=0`.

```sh
python3 tools/run_c54x_core.py --profile mvkd-sp \
  --mame mame/mame --rompath roms --output-root tmp
python3 tools/run_c54x_core.py --profile rptb-fcalld \
  --mame mame/mame --rompath roms --output-root tmp
python3 tools/run_c54x_core.py --profile mvkd-forward \
  --mame mame/mame --rompath roms --output-root tmp
python3 tools/run_c54x_core.py --profile rpt-smem \
  --mame mame/mame --rompath roms --output-root tmp
python3 tools/run_c54x_core.py --profile rpt-smem-edge \
  --mame mame/mame --rompath roms --output-root tmp
python3 tools/run_c54x_core.py --profile rpt-ld \
  --mame mame/mame --rompath roms --output-root tmp
python3 tools/run_c54x_core.py --profile rpt-ld-edge \
  --mame mame/mame --rompath roms --output-root tmp
python3 tools/run_c54x_core.py --profile stlm-pmst \
  --mame mame/mame --rompath roms --output-root tmp
python3 tools/run_c54x_core.py --profile stlm-pmst-edge \
  --mame mame/mame --rompath roms --output-root tmp
python3 tools/run_c54x_core.py --profile pshd-long \
  --mame mame/mame --rompath roms --output-root tmp
python3 tools/run_c54x_core.py --profile pshd-long-edge \
  --mame mame/mame --rompath roms --output-root tmp
```

```sh
make check-mu4-bootstrap-original \
  MU4_BOOTSTRAP_BIOS=bootplaystage \
  MU4_BOOTSTRAP_SOURCE_RUN=/path/to/completed-retained-run \
  MU4_BOOTSTRAP_INPUT_NAND=/path/to/bootrecmeta-run/mu4_recorder_endpoint.nand
```

This diagnostic deliberately returns failure while audio completion is
unvalidated. Even if later CPU coverage passes the present stop, it cannot
silently become a playback gate. It preserves the source NAND and claims
neither REL audio validity, completed decoding, analog output nor handset boot.

### Atomic Metadata Negative Control

The retained atomic `bootrecsave` diagnostic schedules a resident status poll
after the native return, then parameter `7` with filename `TEST` and empty
artist/title strings. Its checksummed candidate packets are
`1e 02 aa 01 49 04 fa 55` and
`1e 0a aa 01 36 07 04 54 45 53 54 00 00 05 99 55`.
That observation failed before the metadata packet was sent: the resident poll remained
incomplete at the 20-second endpoint. The retained replay below reaches cursor
28 with RX-ready set, three replies and two subsequent ACKs. Do not interpret
this as a failed metadata command.

Endpoint PC is `02:3d73` in the TINT handler; the live
OVLY-folded vector and handler prologue match the original AA22 upload.
PMST is `202c`, ST1 `6900`, IMR `0ac9`, IFR `0c78`. Debugger-visible
timer registers read TIM `0000`, PRD `0001`, TCR `0000`, giving the current
model a two-CPU-cycle timer period. The original AA22 idle routine at
`02:98d0`, called by the wrapper at `02:3d37`, writes PRD `1` at
`02:98d8` and TCR `0020` at `02:98dc`. It then clears IMR bit 3 with
`ANDM #fff7` at `02:98e0`, enables other wake sources with `ORM #08c0`
at `02:98e4`, and executes IDLE1 at `02:98e9`. Thus the short timer is
firmware-owned and is immediately followed by an intentional TINT mask;
it is not an unexplained final configuration to replace with a slower rate.
The same routine first reads `a531` with absolute LD `10f8 a531` and
branches directly to IDLE1 when that word is nonzero. Only the zero path
performs the setup above, then writes `a531=1` before sleeping. The
read-only context tool pins the entire routine and its caller wrapper,
including an exhaustive local zero/nonzero predicate. Other writers or
resets of that flag are not yet a closed census.

A read-only write/interrupt trace before the INTM guard correction observes
this warm setup at 14.001318 seconds, then repeated TINT entry with return
PC `02:98de`, IMR `0ac9`
and IFR `0c78`. The CPU never reaches the following mask instruction.
This establishes interrupt starvation before masking, rather than a
metadata-parser rejection. The temporary CPU probes are retired.

[TI SPRU131G](https://www.ti.com/lit/ug/spru131g/spru131g.pdf), sections
6.10.7 and 7.2, requires already-decoded instructions to drain before
interrupt-vector execution. The current atomic-instruction core instead
enters immediately at an instruction boundary. This is an evidenced model
gap, but its exact correction still requires a standalone timing fixture:
do not simply defer an arbitrary number of instructions, change the timer
divisor, or suppress interrupts at firmware PCs. Section 8.4.2 specifies
the timer rate as CPU clock divided by `(TDDR+1)*(PRD+1)`, consistent with
the short period. The staged frontend now has executable original-vector,
handler, worker, FRETE and recurring-interrupt coverage in
[the CPU contract](rom4_dsp_loader.md). The native warm profile still uses
the atomic frontend. The staged CPU fixture now composes all 44 original
idle/wrapper words with the 94-word timer vector/context route. With zero
flag and TINT enabled, decoded-instruction drain completes ANDM before
vector execution: the stacked continuation is `98e4`, not atomic `98de`.
After one ISR the original ORM/flag writes complete and the CPU sleeps with
TINT masked. Even/odd stacks and in-flight save/replay reproduce this bounded
result without a post-return delay or timer change. This bounded fixture
does not establish complete surrounding opcode coverage, native warm
execution or the atomic metadata acceptance behavior described below.
The staged execution stop is maintained in [Current boundary](#current-boundary).

`mu4nand -bios bootrecstage` is a separate fail-closed staged coverage
diagnostic using the same original upload, retained NAND and provisional
shared-RAM composition as `bootrecsave`. It enables the staged frontend
before reset-vector execution, without firmware-PC-conditioned switching or
atomic fallback. Existing profiles remain atomic. Reset's STM immediate to
PMST and the original RPT/MVPD copy now have staged conformance coverage.
The serial entry at `00:0e41` is reached. Its original STM/ADDM/ANDM prefix
selects aligned SP=`15fe`, with bounded staged RMW replay coverage.
Immediate ARP setup, READA into AR1 and BANZ also have staged consumer/
replay coverage. The original BANZ takes the loop to `00:0e5b`; its
`47f8 0011` (RPT *(0011), count from AR1) and repeated `7e92` (READA *AR2+)
now execute in the staged diagnostic, as does `00f8 0011` (ADD *(0011),A).
Startup reaches the original `00:090f` routine and executes its compiler
direct stack-frame decrement. It selects the modeled 95.9616 MHz PLL/system
clock, still with zero NAND reads. OR *AR1,A at `00:0936` now reads GPIO
address `003c` through the existing bench model with the staged peripheral
access clock. `75f8 0008 0080` at `00:093b` is PORTW *(0008),0080 (AL to
external I/O), not a data-to-data move. Compiler direct `7600 0001` at
`00:0940` now has staged ownership/replay coverage, and the original PORTW
retires: the diagnostic sink records port `0080`, value `00df`. This is an
observed I/O write, not a modeled external-device response. Direct STLM to
AR0-AR7 now has staged saturation, consumer-latency and save/replay coverage,
including `8811` at `00:3092` (STLM A,AR1). Long-offset and absolute DST now
retain their address and saturated pair, with 3072 save/replay cases covering
ordinary offset, preincrement and circular addressing. Original `4ee1` at
`00:309b` is accepted. Accumulator AND/OR/XOR now have staged coverage over
all encoded shifts and source/destination combinations; original `f180`
at `00:2fed` (AND A,0,B) is accepted. CMPL also has staged source/destination
and save/replay coverage, including original `f493` at `00:2fef` (CMPL A,A).
Compiler direct MVKD `7001` at `00:30b5` now has retained source/address
and replay coverage, including its AR1 source. Original SFTA A,-1,A
(`f47f`) at `00:323f` is accepted, with staged
SFTA/SFTL result/status and replay coverage. SFTA left/zero-shift carry
remains an explicitly unvalidated policy because the TI formula/example
disagree. Long-offset DSUB `54e6` at `00:3244` now has staged paired-read,
arithmetic and replay coverage. Long-offset immediate ST `76e6` at
`00:322e` now owns its offset/value words and cached destination.
BIO-conditioned BC `f803` at `00:2fca` now samples the bench's existing
NAND R/B attachment without overriding its level. MVMM SP,AR5 (`e785`) at
`00:2fbe` now has Decode-source/Access-publication and source/consumer
latency coverage. Short RPT #32 (`ec20`) at `00:2fc1` now executes its
retained NOP delay loop. The staged diagnostic observes the preceding NAND
strobe: byte `00`, HD0/CLE asserted, HD2/CE low, direction `7`, ready high.
That write observation alone does not prove NAND data-read or startup completion.
`9634` at `00:2fc3` now executes as `BIT *AR5,4`: memory bit 11 is
published to ST0.TC, with no pointer modification (SPRU172C 4-21).
The following two NOPs provide the tested visibility window for `XC 1,NTC`.
The original run retires five NAND strobes: command byte `00` with CLE high,
then address bytes `00,01,00,00` with ALE high; CE remains low and direction
is `7`. The write sequence alone is not evidence of a completed NAND read.
PORTR `74f8` at `00:302f` now reads port `4000` into AL. The bounded
device-boundary observer records the first 512 bytes. The prefix is
`fa33c08ed0bc007c8bf45007501ffbfc`, with CLE/ALE/CE low and ready high;
all 512 captured bytes compare with retained NAND row 1 at raw offset `528`,
with SHA-256 `7876777bc75f8e33789d0b917ac2ca71debc977170e6bc6b0f2dcf9b2e2c9310`.
Two further observed commands `00` with address
bytes `00,20,00,00` select row 32. Only the first 512 bytes are captured
and verified. The first-page comparison
verifies device-read data, not
the firmware buffer, spare-area handling or startup.
`80e6` at `00:3269` now has staged long-offset
STL conformance. Long-offset MVDK `71e6` at `00:326b` now has source,
DAGEN-publication and replay coverage, including a coherent fixture of the
preceding STL and immediate AR consumer. `BC NTC` (`f820`) at `00:3270`
now has retained-predicate and producer/replay coverage. Counter ADDM
`6be6` at `00:3277` has staged long-offset RMW conformance.
RPTB `f072` at `00:32ee` now executes. The current one-second original
bootstrap observation constructs `MCUSI16 .BIN` at descriptor `36b0`,
with length words `36c2=0001`, `36c3=e1e8`, matching the atomic reference.
NEG `f484` at `00:0e35` now executes with one-word Execute-owned
semantics. The pre-correction one-second observation enters loader `3538` with
index zero after 4,612 NAND reads, correct name/extension and length, then
reads the first file-backed block count `0010`. `SUB *(0008),B`
(`09f8 0008` at `00:2f46`) now consumes mapped AL and the run reaches
the original DMA-completion polling sequence. `BITF *(0008),0001`
(`61f8 0008 0001` at `00:2fa5`) now reads its mapped AL source after
the preceding DMPREC load. That observation reaches resident `02:6d62` and stops
on unsupported `STM #lk,IMR` (`7700` at `02:6d64`, live PC `026d66`).
That pre-correction entry was not a correct-transfer acceptance: a
side-effect-free comparison at the first resident Fetch checked all
48,261 program words from the original `aa22` section stream and found
40,657 mismatches. The first was `02:8e8b`, actual `826c`, expected
`0001`. The source has
51,921 words in total; 3,660 data words are explicitly excluded from
this program-only observation. The configured common-window mapping
is used, not asserted as physical DA150 bank identity. A separate fresh
atomic `bootrecsave` run with the same retained NAND and shared-memory
profile matches all 48,261 program words, with zero mismatches. Therefore
the discrepancy is staged execution/loading, not a mismatch shared by
the reference mapping. First divergence is word 11 of source record 434
(destination `02:8e80`, 16 words), file byte offset `4000`. The first five
bad words, byte-swapped, match the original sequence at `3fda`, exactly
one 38-byte record earlier. This is a boundary/stale-data witness, not
proof of a specific cache or byte-swap defect. Bounded write observations
show that the crossing record receives only 11 new words at `3c21..3c2b`,
then the original swap and DMA process all 16 words, including five stale
words. DMA source ownership matches the original buffer. The atomic
reader advances descriptor cluster fields `36c4/36c6` from 3 to 4 and
fills the remaining five words; staged resets cursor `36c5` but leaves
both cluster fields at 3. The initial cluster comparison reads 3 and 3
in both engines. The staged next-cluster helper `343a` subsequently
returns `0fff`, whereas atomic returns 4. Earlier fetch snapshots in
offset helper `345c`, at `3482`, already differ (atomic A=2, staged A=0).
An actual mapped-AL read tap now isolates the difference: `MPY #3,A`
(`f066 0003`, `3479..347a`) is immediately followed by `LDM AL,A`
(`4808`, `347b`) and `SFTA A,-2,A` (`f47e`, `347c`). Atomic reads AL=9;
staged reads AL=3, the pre-multiply value, giving offset 0 instead of 2.
SPRU131G table 7-28 explicitly assigns zero required gap to a two-word
direct accumulator producer followed by a mapped accumulator consumer.
The CPU now finishes a matching mapped accumulator Read after the older
two-word immediate MPY Execute, retaining exactly one operand sample.
Other reads and the one-word producer restriction are unchanged.
`tms54test -bios mpy-mapped` passes 3,072 cases and saved-state replays
over AL/AH/AG, BL/BH/BG, both producer/consumer accumulators, signed
operand patterns, FRCT/OVM, and zero/one-word gaps. It rejects the old
ordering, poisons T after its capture and the producer accumulator after
the consumer Read, and requires one mapped read and one literal fetch.
Long-offset `MVKD` (`70e1 0016 0012`, `00:33aa`) now executes through
the same retained move path. It copies the recovered next-cluster value
from mapped AR2 (`0012`) into `*AR1(0016)` (`36c6`) without injecting a
descriptor field. Its two extensions retain offset-before-source order;
Access owns the resolved destination and any pointer modification, Read
owns one source sample, and the existing write phase owns publication.
`tms54test -bios long-mvkd` passes 19,200 saved-state replays covering
25 forms, four offsets, selected-AR/AR2/RAM/alias sources, four values,
two compatibility states and two program pages. Snapshots cover partial
Decode, retained Read and completed write; later literal/source/pointer
mutations cannot retarget or duplicate the move (SPRU172C 4-113).
The full C54x conformance runner and native 5110 save-state gate pass.

The current one-second original bootstrap reaches resident `02:6d62`
with all 48,261 compared program words correct, zero mismatches. The
16-KiB crossing record now has all 16 correct words. A separate read-only
data audit checks all 3,656 ordinary data destinations, with zero
mismatches in both staged and atomic controls. Later program writes that
alias previously initialized data words retain record order under the
configured common-RAM profile; physical banking remains unvalidated.
Four low register-window writes at `007c..007f` (source values
`0000,0000,4000,017b`) are explicitly excluded: live register contents do
not prove their write effects. The inventory has 3,660 unique data
destinations (`007c..c281`).
`python3 tools/noki5510_bootstrap_check.py --resident-memory LOG` requires
one ordered program/data report, exact counts and zero mismatches, and
rejects duplicate or failed evidence. It accepts original RAM contents
only, not hardware register effects or full boot.
`STM #0,IMR` (`7700 0000`, `02:6d64`) now executes via the retained
non-DAGEN control-write path, with one literal fetch, no destination read
and one Execute publication. The isolated `imr-store` fixture passes
4,608 cases and saved-state replays over all 16 IRQ bits, four masks,
both initial masks/INTM states, no source/IRQ/NMI and two program pages.
It preserves IRQs accepted before a later clear, leaves masked IRQs
pending, and admits newly enabled IRQs only after publication. Physical
bus-phase timing remains a partial model (SPRU172C 4-176/class 12A;
SPRU131G 6.10.2).
The full C54x conformance run, native 5110 save-state gate and original
resident-memory checker pass. Fixture inputs return low before the next
test group so its IRQ/NMI edges remain independent.
The current original run retains both complete memory-audit passes and
advances through resident startup to `STM #7fff,*(0028)` (`7728 7fff`,
`02:6d05`, live PC `026d07`). Address `0028` is SWWSR, the software
wait-state register; the following `7729 8006` at `02:6d07` targets BSCR,
the bank-switching control register (SPRU131G 10.3.1/10.3.2). These
external-bus controls are the next contract to recover, not arbitrary
register storage to admit silently. Program/data RAM contents are
verified; low register effects, physical banking, full boot and recording
remain unaccepted.

#### External-bus compatibility contract

`tools/mu4_external_bus_contract.py` pins the original resident prefix at
`02:6d00..6d08` against the R060 source hash. It decodes the two writes
under the existing C5416-compatible hypothesis, not exact DA150 identity.
The authority is [TI SPRS095P](https://www.ti.com/lit/ds/symlink/tms320vc5416.pdf),
sections 3.6.1-3.6.3 and tables 3-3/3-4/3-5.

| Register | Address | C5416-compatible reset | Writable fields |
| --- | --- | --- | --- |
| SWWSR | `0028` | `7fff` | XPA and five three-bit wait counts |
| BSCR | `0029` | `f000` | CONSEC, DIVFCT, IACKOFF, HBH, BH (`f006`) |
| SWCR | `002b` | `0000` | SWSM (`0001`); other bits unaffected by writes |

The CPU's opt-in `set_c5416_bus_profile` now owns these registers, their
masked/partial writes, reset values and saved state. It is enabled only
by the isolated `tms54test -bios bus-registers` configuration, not MU4 or
other handset profiles. `clkout_divisor_cb` publishes the exact divider;
the existing system-clock callback remains the separate base clock.
`iack_enable_cb` publishes output gating, not a physical IACK pulse, and
cannot suppress logical interrupt acknowledgement. Reset and post-load
re-drive these derived configuration outputs.
`run_c54x_core.py --bus-registers` requires exhaustive 65,536-encoding
native register conformance, partial writes, saved-state restoration,
reset and a logical IRQ with IACKOFF set. Its marker explicitly says
`bus_timing_acceptance=0`; it is not a bus-timing or full-core gate.
The staged SWWSR/BSCR instructions remain unsupported until wait payment
is integrated. The original resident frontier is therefore unchanged.
The isolated profile, full legacy CPU suite, native 5110 save-state gate
and original resident-memory checker pass on this register-state tree.

This BSCR dialect is **not** SPRU131G table 10-5's BNKCMP/PS-DS/IPIRQ
layout. In the C5416-compatible dialect, resident `8006` requests
consecutive external memory reads (three-cycle framing per read),
undivided CLKOUT, enabled IACK output and both bus holders. A generic
bank-compare interpretation of the same value would be the wrong model.
Resident SWWSR `7fff` selects seven wait states for every external range,
assuming SWCR remains at reset; SWSM doubles the count to fourteen.
XPA changes the program-field boundary from bit 15 to bit 22. Internal
program/data accesses do not pay these external waits. C5416 PORTR/PORTW
I/O uses the external IS bus; MU4 NAND is attached at I/O `4000..7fff`.

The tool's tests exhaust all 65,536 BSCR encodings and 655,360 SWWSR/
multiplier/range selections, plus internal/external ownership and the
original pinned source. These are field predictions, not runtime timing
acceptance. There is no complete SWCR-writer census, and neither these
values nor the test bench's flat RAM map establish physical bank extent.

Implementation requirements:

1. Select the C5416-compatible bus dialect explicitly; do not change
   other native handset profiles by importing its fields/reset values.
2. Own masked/resettable/saved register state in the CPU. Separate the
   physical IACK output from logical interrupt acknowledgement, and
   propagate CLKOUT division without changing the system clock itself.
   These are peripheral MMRs (SPRS095P table 3-16), unlike IMR: preserve
   the additional peripheral-access cycle in class-12 writes.
3. Pay external I/O waits while retaining one address/data transaction
   across stalls and replay; clocks continue and interrupt entry cannot
   split that transaction. READY is only sampled after software waits,
   with at least two software waits programmed (SPRS095P section 5.5.5).
4. Add program/data framing and bank penalties only with explicit bus
   ownership. Physical DA150 banks, READY wiring and electrical bus-holder
   behavior remain separate unknowns, not reasons to fake register reads.

The staged frontend now pays the C5416-compatible external-I/O wait
component before a PORTR read or PORTW write. It freezes the programmed
count and READY eligibility at transaction start, retains the existing
instruction address/source slots, and samples READY only after at least
two software waits. Zero/one-wait requests ignore READY. While stalled,
clock consumers continue and new interrupt entry cannot split the
transfer; its single side effect occurs only when the waits finish.
The wait counter, eligibility and per-slot payment flags are saved.
The complete-pipeline stall placement is a declared partial model, not
a recovered silicon pipeline waveform. Simultaneous read/write I/O bus
arbitration remains fail-closed rather than guessed.

`run_c54x_core.py --bus-io` requires 576 cases and saved-state replays:
eight base counts, both SWSM values, both directions, READY held/released,
no source/IRQ/NMI and three checkpoint positions. It checks single I/O
side effects, retained literal/address/data despite later changes, timer
continuity and acknowledgement after the transfer. Input pulses settle
before same-transaction snapshots; queued scheduler events are not a
pending CPU interrupt. The original source and handset profiles remain
unchanged: the profile is selected only by isolated conformance and the
fail-closed MU4 `bootrecstage` original-firmware diagnostic.

Atomic execution now retains I/O transactions and retirement context.
`run_c54x_core.py --atomic-io` passes 2,880 transfer cases and 2,880
save/restores across short, long-offset, absolute, peripheral-MMR and
AR-alias addressing, with
the same software-wait, READY, IRQ/NMI and timer matrix. Extension taps
check both three-word literal orders and reject refetch after mutation.
Base transfer clocks are retained individually rather than charged as
upfront cycle debt. Selected repeat and delayed/guarded compositions
have their separate bounded fixture below. The next software target is
accumulator-to-IFR acknowledgement at the original MU4 frontier.
Program/data bus ownership, physical timing and
the remaining pin behavior remain open; neither register nor staged-I/O
conformance is full bus or boot acceptance.
Both isolated bus gates, full legacy CPU conformance, native 5110
save-state restoration and the unchanged original memory audit pass.

The atomic integration must defer retirement as well as the I/O callback.
`execute_run` currently applies repeat redirects, delayed-word accounting,
XC protection and INTM-guard consumption immediately after `execute_one`.
A retained PORTR/PORTW must therefore own the instruction PC, original
delay/XC eligibility, repeat-armed state and consumed word count until
completion. Resume a pending transaction before `service_interrupt` or
another fetch; retire exactly once after its single bus side effect.
Preserve the existing extension order: long-offset forms fetch the port
before the address extension, while short/absolute forms prepare the
address before fetching the port. PORTW also retains its sampled source;
PORTR retains its destination and must not read the port before READY.
Pointer modification must occur once, at a documented phase, rather
than being repeated on each resumed clock. All pending transaction and
retirement fields require save-state coverage. Acceptance must include
two/three-word forms, repeated transfers and delayed/guarded followers,
in addition to the staged gate's wait/READY/IRQ/NMI cases. Subtracting
extra cycles after an immediate callback does not satisfy this contract.

The isolated `bus-io-compose` fixture additionally tests 24 saved/replayed
cases: RPT counts 0..3 and read/write transfers, delayed-branch slots and
RSBX INTM followers, all with seven software waits. It checks one pointer
update per transfer, deferred IRQ acknowledgement and the stacked
sequential/delayed continuation. This is a bounded composition matrix,
not exhaustive support for every repeat/control form or peripheral MMR
operand. Full legacy CPU conformance and native 5110 save-state restoration
pass with the integration. The original MU4 audit still matches all
48,261 program and 3,656 ordinary data words; no handset profile has changed.
Peripheral acceptance uses the ordinary mapped register-window word
`0030`: both directions retain its additional access clock rather than
charging an upfront instruction debt. READY eligibility still depends
only on programmed external software waits, not the extra MMR clock.
The mapped AR1 alias uses `*AR1+` with AR1 initially `0011`; PORTR writes
the received word to AR1 before its one postincrement. Its postincrement
mode is saved until completion, while PORTW retains the source before
its pointer modification. This validates that alias and representative
peripheral timing, not every timer/control-register side effect or
measured silicon bus phase. Production profile promotion remains separate.

`run_c54x_core.py --bus-stm` accepts 36 cases and save/replays for immediate
SWWSR, BSCR and SWCR writes: four literals, three destinations and three
checkpoint positions. The literal is retained despite a later program
mutation, no destination read occurs, and one masked write publishes at
clock 8 rather than early DAGEN publication. The extra peripheral clock
uses the existing saved per-slot wait state. Admission is conditional on
the C5416-compatible profile; other STM controls remain excluded.

With that profile enabled only on `bootrecstage`, the original MU4 loader
still matches all audited resident program/data words and executes past
`7728 7fff` / `7729 8006`. READA `7ef8 000b` at `02:6d12` transfers the
program word at `02:74b5` into BL. Its six accumulator-word destinations
are now accepted by `--mapped-read-a`: 288 cases and save/replays across
direct/absolute forms, four values and two source pages. A changes after
capture without redirecting the held program read; one write preserves
untouched accumulator words and status. Repeated mapped destinations,
other controls and peripheral destinations remain excluded.

Absolute LD Smem,DP `46f8` now shares direct LD's retained sample and
zero-gap DP forwarding, with one retained address extension and correct
two-word retirement. `--dp-absolute` tests all 512 DP values across 1,536
saved/replayed cases: source Read at clock 6, DP Execute at 9 and adjacent
consumer Execute at 10. Later source/extension mutations cannot redirect
or resample the transfer. Other indirect memory-to-DP forms remain
excluded; the absolute fixture uses ordinary RAM, not peripheral input.

The original `46f8 007f` at `02:6d2b` executes. The retained-initialization
checkpoint passes full CPU conformance, native 5110 restoration, seven
isolated gates and 101 targeted Python tests. Physical program/data bus
ownership remains unvalidated; this is not full board boot or native speech.

The next MVDM contract is pinned by SPRU172C 4-110: a two-word,
two-cycle Class 19A transfer from absolute data address DAR to an MMR,
without ordinary status-bit changes. Its repeat stream increments DAR
and has a separate single-cycle steady-state contract; non-repeated
admission must not imply repeat acceptance. SPRU131G tables 7-7, 7-9
and 7-13 group MVDM with MVKD for AR, compiler-mode SP and T latency,
not with MVDK's earlier publication. The existing retained Class 19A
MVKD path is therefore the candidate implementation boundary. Required
tests are one owned source literal/read/write, source/destination aliasing,
saved pre-read/post-read publication, AR consumer gaps, and separate
compiler-mode SP/T consumers before admitting those destination classes.

Direct MVDM destinations AR0..AR7 now share that retained Class 19A path.
`run_c54x_core.py --profile mvdm-ar` requires 1,152 cases and save/replays:
eight registers, RAM/self/other-AR sources, four values, zero/one-gap CMPR
consumers, two execution pages and three snapshot positions. One absolute
source literal is captured; one source read and one encoded MMR write
occur at the complete slot's Read clock 6. DP does not select the MMR.
Later RAM/literal changes cannot alter the owned transfer. Repeat and
non-AR destination classes remain excluded.

The original `7212 b6be` at `02:df9b` executes. MVDM
`7211 b6a8` at `02:dfa6` occupies the two delay words following BCD
`fa47 dfbe` at `02:dfa4`. The diagnostic records `decode_words=2`,
`control=02dfa4`, `control_words=4` and the branch in Access; live PC is
`02:dfa3`, not the stopped instruction address. This is a real excluded
Class 19A delay-slot composition, not a wrong-literal or delay-owner
inference. `--profile mvdm-delay` now accepts 576 cases and save/replays:
eight AR destinations, four values, both outcomes of the original ALEQ
branch family, none/IRQ/NMI and snapshots before/after publication and
during entry. Branch and MVDM literals are owned separately, the transfer
publishes once before acknowledgement, and RETE resumes the selected
continuation with SP restored. Snapshots at clocks 8/9/12 now straddle
the observed source/publication boundary, rather than assuming clock 8
was after publication. Direct-AR MVDM and basic two-word MVKD are exempted
from the long-memory delay exclusion; extended forms remain excluded,
and this is not exhaustive acceptance of other control contexts.

That original delay slot executes. `fb82 dfc2`
at `02:dfaf` is FCALLD to `02:dfc2` in extended-program
mode. Its two delay words are NOP at `02:dfb1` and LDM AR6,A at
`02:dfb2`; the return continuation is `02:dfb3`. Its accepted contract
covers caller-fetched words and stacked PC/XPC ownership across those delays.
`--profile far-call-delay` accepts 243 cases and save/replays: three
caller and target pages, three delay layouts, none/IRQ/NMI and three
snapshot positions. PC after both delay words is stacked before the
retained caller page; FRET restores both and SP. RETE removes only the
separate interrupt frame. Layouts cover two one-word operations, one
two-word STM and the original NOP/LDM pair.

Mapped XPC visibility inside a delay-slot Read is unvalidated. The
primary instruction text (SPRU172C 4-57/4-58) specifies the stored frame
and fetched delay words, not that cycle-level read. A caller-2/target-0
probe observes XPC=0 because the current frontend publishes its speculative
Fetch page into the shared XPC field. The fixture retains this probe but
does not assert its value or change global publication to enforce a guess;
its acceptance marker explicitly excludes that fidelity claim.

The original call enters `02:dfc2`. Its `29f8 0011` at `02:dfc9` is
absolute MAC Smem,B reading mapped AR1, with T initialized to 3.
MAC/MACR/MAS/MASR now share arithmetic with the atomic interpreter while
retaining staged memory and T in Read. The accumulator is consumed at
Execute, consistent with the existing accumulator-feedback path.
`--profile mac-smem` tests 73,728 cases and save/replays: four operations,
both destinations, indirect/absolute RAM and mapped AR1, 64 signed input
pairs, 16 FRCT/OVM/SMUL/SXM combinations and three checkpoint positions.
Rounding, product saturation, sticky target overflow, the untouched
accumulator and absence of T/data republishing are checked. Later source,
T and extension changes cannot alter the sampled inputs. SQUR's staged
T side effect and repeated MAC forms remain excluded. Complete control-delay
MAC composition has separate acceptance above; other accumulator-producer
compositions need separate acceptance.

The original MAC executes. MVKD `7084 0013` at
`02:dfd8` copies AR3 to the RAM word addressed by AR4 in both delay
words of BCD `fa4d dfdd` at `02:dfd6`. The diagnostic records two remaining
decode words and that control owner; live PC is `02:dfd5`. Its accepted
contract is the two-word Class 19A data-copy delay, not its three-word
extended forms. `--profile mvkd-delay` accepts 1,728 cases and save/replays:
eight pointer registers, both outcomes of the original BEQ branch family,
AR sources, RAM-source postincrement and same-address RAM copying,
none/IRQ/NMI and snapshots before Read, after Read and during entry.
Read is observed at clock 9 and RAM publication at clock 10. Mutating the
source after sampling proves retention, including the same-address case;
one pointer update and complete RETE return are checked. The reused MVDM
gate still passes its 576 cases with the corrected snapshots.

The original MVKD delay executes. MVMD `7316 b6a9`
at `02:e078` copies encoded MMR AR6 to absolute data address `b6a9`.
Direct MVMD sources AR0..AR7 now share the established Class 19A MVDK
transfer path, but bypass DP/indirect source addressing. `--profile mvmd-ar`
accepts 576 cases and save/replays: eight registers, four values, RAM/
self-AR/other-AR destinations, two execution pages and snapshots before
Read, after Read and after publication. The source is sampled at clock 5;
RAM publishes at 6, DAGEN destinations at 5. Later source/extension
changes cannot alter the retained transfer. Status and both accumulators
are preserved. Repeat, indirect and non-AR source classes remain excluded;
SP/BK destination consumers and source-producer gaps need separate coverage.

The original MVMD executes and returns to the initialization spine. The
LDM IFR,A `4801` / STLM A,IFR `8801` pair at `02:6d3e..6d3f` now executes
after the interrupt-register acceptance below. Absolute CMPM `60f8`
at `02:909c` now executes with following words `3762 000c`.
SPRU172C page 4-33 identifies `60f8` as
absolute CMPM: the address extension `3762` precedes the comparison
constant `000c`. It reads a 16-bit source and sets TC on equality, without
a destination write. The absolute form adds one word and one cycle to
the two-word/two-cycle base instruction. A retained source/address/literal
fixture and adjacent TC-consumer coverage are supplied by the isolated
`cmpm-absolute` profile: 96 cases/restores cover equal/unequal sources,
both initial TC states and TC/NTC branches. The address and immediate
fetch exactly once in order; later program/source changes cannot alter
the retained comparison. Read occurs at clock 7, TC publishes at 8, and
a taken adjacent branch fetches its target at 10. Both accumulators and
the non-TC status bits are preserved. Mapped-register sources remain
excluded. The shared mapped-BITF gate still passes 13,824 cases/restores.

The original selects the high-rate PLL clock and executes MVKD
`7082 0010` at `02:8fbd`: source `0010` (AR0), destination `0049`
through AR2. MU4 maps `0048/0049` to the existing McBSP control index/data
pair. SPRU172C Class 19A (page 3-40) requires one additional cycle for a
peripheral memory-mapped access. Peripheral destinations now publish in
Execute after that extra clock, rather than using the early Read-stage
DAGEN-register write. The retained source is not read again during the
wait. Low control destinations remain excluded.

The `mvkd-peripheral` profile passes 648 cases/restores: AR0/BK/RAM sources,
indirect/postincrement/absolute destinations `003b/0048/0049`, four
values, two execution pages and three snapshots around Read/wait/write.
The source read is at clock 6 (7 for absolute), with publication at 8
(9 for absolute). Source and extension changes after capture cannot
alter the transfer; pointers modify once, status/accumulators remain
unchanged, and the destination is never read. These are CPU bus/replay
checks, not physical McBSP timing measurements or serial output proof.
BK reads use the same retained Read-owned source path. Later changes to
BK cannot alter the transfer; adjacent BK-producer timing is not covered
by this fixture and remains a separate acceptance requirement.

The original advances beyond the BK-source boundary and executes the
T store `8c82` at `02:8fed`. Peripheral T/TRN stores use the existing
Execute-owned source/publication path and pay the extra peripheral clock;
low CPU-register destinations remain excluded. The `t-peripheral` gate
passes 864 cases/restores with instruction-driven STLM producers, T/TRN
sources, gaps 0/1/2, indirect/postincrement addresses `003b/0048/0049`,
four values, two execution pages and snapshots before the producer,
during the wait and after publication. Producer writes at clock 6;
peripheral publication is at 8 plus the gap. No destination reads or
duplicate writes occur, and status/accumulators are preserved. This
tests the admitted short store composition, not every possible producer.

Original words `61e1 0001 0002` at `02:9521` now execute and test an
AR1-plus-one source with mask `0002`. The preceding `7711 0048` and
`7681 0001` select McBSP control register 1 through the `0048/0049`
index/data pair. The existing McBSP register-1 read exposes transmit
ready in bit 1. BITF now participates in the peripheral-read wait path.
The `bitf-peripheral` gate passes 384 cases/restores with long-offset and
absolute addressing, two execution pages, both initial TC states and
TC/NTC branches. It samples the source once at clock 8, publishes TC at
9 and fetches a taken adjacent branch target at 11. Changing peripheral
status after Read cannot change the result. No destination write occurs.
These clock positions describe the executable partial pipeline model,
not measured silicon bus phases.

The resident AA22 image emits six native McBSP TX words (`0c10 0818 0a01 0e53 1023
1201`). Its FCALA B `f5e7` at `02:40ef` executes in the resident run
after the frame, interrupt and producer gates below. SPRU172C
page 4-55 identifies this as nondelayed FCALA B: push PC+1 and caller
XPC, then load PC from B[15:0] and XPC from B[22:16], with no status
change. It is a one-word/six-cycle Class 30B instruction and cannot be
repeated or placed in a block repeat. Retained accumulator target,
caller-page/return-frame ownership, producer timing, interrupts and
save/replay require executable acceptance. Serial word output
alone does not prove board boot, recorder readiness or audio.
This is not the AA99 REL image's `6f8c 0c9e` at the same logical address;
source-image identity is part of every instruction claim.

FCALA/FCALAD are implemented by the atomic extended-transfer
decoder `(op & 0xfcfe) == 0xf4e6`; the disassembler recognizes that
family too. Its acceptance is independent of the staged fixtures.
The isolated `far-atomic` gate passes all 16 existing branch/call/return
encodings, including A/B accumulator calls and delayed forms, from caller
page 1 to target page 2. It verifies return frames, caller continuation,
delay-word execution and target retention. It explicitly claims no
staged acceptance, save/replay or silicon phase validation. Its setup
initializes PMST, timer and interrupt controls rather than inheriting the
full suite's preceding fixture state.

The nondelayed staged prototype owns one instruction word, PC+1, a
saved 23-bit accumulator target and the Class 30B six-cycle cadence.
The generic operand slots are 16-bit and cannot hold that full target.
A front-end barrier prevents younger sequential instructions from
entering while target and frame ownership settle. Caller XPC comes
from the retained instruction page, not target prefetch. The two stack
writes have distinct saved ownership. The isolated `fcala` profile
passes 216 cases/restores: both accumulators, caller pages 0/2/7f, target
pages 0/3/7f, ignored high accumulator bits, post-sample source changes
and four snapshots around target/frame publication, with no interrupt,
IRQ or independent NMI. Target sampling is
at clock 5; return PC and caller XPC publish at 5 and 6; target Prefetch
is at 7, Fetch at 8 and first callee Execute at 12. FRET restores SP and
the caller continuation, without duplicate frame accesses or changes
to the unselected accumulator or non-INTM status. IRQ/NMI arrival during
frame construction is acknowledged only after both call words publish.
Its stack word is above the complete call frame, RETE removes only that
word and resumes the target, and the callee executes once before FRET.
The interrupted target has one initial Fetch and one legitimate return
refetch. Interrupted snapshots allow the queued line pulse to settle
before saving. These are declared partial pipeline placements, not
measured silicon phases. The separate `fcala-producer` gate passes 216
cases/restores with DLD building an A/B target from two RAM words, gaps
0/1/2, nine caller/target page pairs and four snapshots. DLD samples
its pair at clock 5; later RAM changes cannot change its publication,
and post-FCALA-sample accumulator changes cannot change the call target.
Zero-gap FCALA samples after older DLD Execute publication in the same
clock. This validates that producer family, not every accumulator
producer or a combined DLD-plus-interrupt cross-product. The separate
[FCALAD contract](c54x_execution_contracts.md#delayed-far-accumulator-call) covers delayed calls but
does not inherit the nondelayed DLD-producer acceptance. The atomic,
FCALLD and IFR reference gates still pass.
SPRU172C page 4-55's
detailed operation pushes return PC then XPC; SPRU131G section 6.4.3's
short prose reverses their mention order and must not override the
instruction operation or the tested FRET frame layout.

CC `f945 3ff1` at `02:3f3c` now executes. Nondelayed AEQ/BEQ calls
retain the branch predicate and target, then pay Class 31B's additional
clock on both paths. Only a taken call allocates SP, publishes PC+2
and updates RTN. The `conditional-call` gate passes 64 cases/restores:
A/B predicates, four 40-bit patterns including guard-bit-only nonzero,
two pages and snapshots around decision/held-clock/publication. Later
literal and accumulator changes cannot change the retained outcome.
The taken frame writes at clock 7 and target Fetch follows at 8; RET
restores the caller. Untaken cases check unchanged SP on every clock,
with no stack reads or writes. Both cases preserve accumulators and
status. CCD, other condition groups, adjacent producer and IRQ/NMI
composition remain unaccepted. The atomic core's broader CC/CCD family
is unchanged.

Conditional return `fc45` at `02:3f41` and signed `fc43` at `02:3e44`
now execute. The `conditional-ret` gate passes 768 cases/restores for all
twelve nondelayed signed A/B predicates (GEQ, LT, NEQ, EQ, GT, LEQ),
eight 40-bit patterns, two pages and four snapshots. Sign is determined by
bit 39, including positive values with bit 31 set and nonzero guard words.
Read samples the predicate after older Execute publications, before a
conditional SP increment. Only a taken return reads
its captured stack address at clock 5; later stack-word changes cannot
change the return target. Untaken returns preserve SP on every clock
and never read or write data memory. Accumulators, non-INTM status and
INTM are preserved. Changing the selected accumulator after Read to an
opposite-predicate value does not change the decision. RCD, other condition
groups and IRQ/NMI composition remain unaccepted. Adjacent unsigned loads
have the separate `ret-load` composition coverage described above.

The isolated runner rejects MAME driver-validity `Errors:` output even
when the conformance marker and intentional completion exit are present.
BIOS identifiers must fit MAME's 16-character limit; a passing CPU
fixture cannot substitute for a valid driver definition.

SUBS `0a39` at `02:3f5a` now executes. SPRU172C page 4-194 specifies
an unsigned 16-bit memory operand regardless of SXM, with C and sticky
destination overflow updates and OVM saturation. The staged path reuses
retained memory/address ownership and the shared arithmetic helper but
bypasses sign extension for this family. The `subs` gate passes 52,224
cases/restores: 17 read-capable encoding forms, eight address registers, A/B,
eight value patterns, SXM/OVM states, both compatibility modes and three
snapshots. Post-capture source/literal/pointer changes cannot alter the
result, and no data write occurs. Mapped-register sources, C16 arithmetic
and adjacent producer/branch composition are not accepted by this gate.

MVMD AL/BL to RAM now has a separate `mvmd-acc` gate: 288 cases/restores
with POPM and mapped STL producers, gaps 0/1/2, four values, two pages
and three snapshots. Zero-gap mapped-source reads follow the older mapped
write's Execute publication in the same clock (SPRU131G section 7.5.10,
example 7-67's no-conflict rule). Direct-ALU producers retain their
separate latency rules; this change does not forward all accumulator
updates. The source reads once at clock 6 plus the gap, and RAM publishes
at 7 plus the gap. Later low-word changes cannot alter the captured move;
other accumulator/status and stack ownership are preserved. AH/AG/BH/BG,
non-RAM destinations and direct-ALU producer composition remain unaccepted.

The verified pre-status checkpoint stopped at the destination guard for MVMD AL
`7308 0007` at `02:3f62`, live PC `02:3f65`, directly after POPM AL
`8a08` at `02:3f61`. Retained instruction and destination observations
confirm ST1 (`0007`). The staged subset admits AL/BL to ST1 and
publishes the retained word at the move's Read clock, rather than Execute.
Its `mvmd-st1` fixture passes 288 cases/restores (POPM/STL producers,
zero through two gaps, two pages, four full-word patterns), with one source
read, one status write and no destination read. The `mvmd-cpl` fixture passes
16 cases/restores: both CPL directions, basic LD/ST consumers and zero
through three NOPs, checking the two-cycle consumer latency.

The adjacent arithmetic fixture `mvmd-status-alu` passes 768 cases/restores:
SXM-dependent indirect LD and ASM-dependent register LD, both SXM states,
all 32 ASM fields, two pages and three snapshot positions. The old status
fields deliberately differ; each case checks the independently computed
40-bit result, source preservation and one status publication. These tested
consumer forms have zero latency. The `mvmd-block` fixture passes 24
cases/restores with immediately adjacent RPTB, counts 0/1/2/7, two pages
and three snapshots. It checks the BRAF update survives status publication,
then checks every byte-swapped body word and count-plus-one completion.

Original `3f83` at `02:3df3` is `ADD *AR3,16,B,B`. The memory shift-16 subset executes
the Class 3 ADD/SUB family using retained addresses, extensions and source
words. Its separate `memory-shift16` fixture passes 208,896 cases/restores:
17 read-capable encoding forms, eight AR selectors, all eight operation/source/destination
selectors, eight arithmetic patterns, four SXM/OVM settings, both CMPT
states and three snapshots. The reference independently derives signed
40-bit results, sticky destination overflow and OVM saturation. ADD sets
C only when a carry occurs; SUB clears it only when a borrow occurs
(SPRU172C syntax 3, pages 4-5 and 4-188). Other cases retain incoming C.
RAM is read once; there are no data writes; mapped-register sources remain
excluded.

Unsigned `LDU *AR2,A` (`1282`) at `02:3dfa` reads source `0000` (IMR),
after `MVDK *AR2,AR2` at `02:3df8`. LDU now admits IMR via the ordinary
mapped read path, not a substituted zero. The `ldu-imr` gate passes 384
cases/restores across direct, absolute, indirect and adjacent MVDK-producer
forms, four actual IMR values, both SXM states, both accumulators, two pages
and three snapshots. Each case reads IMR once and retains the unsigned
result despite later IMR changes. The 96 producer cases check one RAM
read, one AR2 write and captured address ownership despite subsequent
pointer changes. All cases preserve status and the unrelated accumulator;
other newly untested unsigned control/peripheral sources remain excluded.
These fixtures establish instruction composition, not that the original
zero-pointer lifecycle is correct; that distinction remains explicit.

Short accumulator STL/STH now admits preincrement modes `98..9f` and
increments the selected AR before capturing its Read-owned destination.
The `preinc-store` gate passes 6,144 cases/restores across four STL/STH
selectors, eight registers, four source/address patterns, four SST/SXM
settings, both CMPT states, two pages and three snapshots. It checks one
pointer update, no destination read, one write, 16-bit wrapping into actual
IMR and Execute-live source saturation despite later pointer changes.
T/TRN preincrement forms remain excluded.

`STM #k,BRC` now publishes early enough for an adjacent RPTB, as required
by SPRU131G tables 7-23/7-24. It does not inherit DAGEN-bank arbitration.
The `stm-brc` gate passes 30 cases/restores with counts 0/1/2/7/127,
two pages and three snapshots, beginning with a deliberately different BRC.
It verifies one retained literal, no counter data read, one counter write
and complete count-plus-one byte-swap loops. In-loop BRC modification stays
excluded because table 7-25 requires a separate latency contract.

Short immediate preincrement ST passes 1,536 cases/restores in `preinc-st`:
eight registers, four literals, four SST/SXM settings, both CMPT states,
two pages and three snapshots. Program-literal and pointer changes after
capture cannot affect the one write; unrelated accumulator changes cannot
replace the retained literal. Single-instruction repeat of this new mode
and its interrupt cross-products remain unaccepted.

These changes execute the original `809a` at `02:3e1c`, then `771a 007f`
at `02:98a5` and `769a 0000` at `02:98b1` in the clearing loop.

Long-offset immediate ST now admits peripheral destinations `20..5f`,
while low CPU-control destinations remain excluded from that form. The
`st-long-periph` gate passes 6,144 cases/restores across eight AR selectors,
offset/preincrement forms, four values, both CMPT states, offsets 3b/38/49,
RAM/peripheral spans, two pages and four snapshots. Identical offset words
with different pointer bases distinguish actual peripheral accesses from
RAM. The captured destination/literals survive pointer and program changes;
there is one write, no data read and one extra clock only for the resolved
peripheral address, including replay of the held write.

Long-offset ADDM/ANDM/ORM similarly admit peripheral destinations, with
low controls and untested short ADDM controls still excluded. The
`rmw-long-periph` gate passes 368,640 cases/restores: three operations,
eight AR selectors, offset/preincrement forms, offsets 3b/39/49, both spans,
eight value pairs/eight status states, both CMPT states, two pages and five
snapshots (including the paid-wait clock). One captured read and one write
share the single peripheral extra clock; RAM adds none. Source/pointer and
program changes after capture cannot alter the result. These are CPU MMIO
conformance fixtures backed by RAM, not a validation of device side effects.

The original `ST #1,*AR1(38)` (`76e1 0038 0001`) at `02:97d9` now writes
destination `0038` and the following `68e1 0039 fffe` executes. The
C5416-compatible map identifies these as McBSP0 SPSA/SPSD; the native
diagnostic routes them to the McBSP0 device, rather than a fabricated reply.
The `mcbsp-control` fixture now connects the CPU's 38/39 window to the actual
McBSP device rather than RAM and executes the six original selector/ANDM
words. Its 16 cases/restores cover two pages, both SXM states and semantic
snapshots at partial Decode, selector publication, captured read and reset.
The device initially exposes XRST/XRDY (`0003`). A DXR write after the CPU
read makes it busy (`0001`); the CPU must still write the captured masked
word (`0002`). The device then reports actual transmitter reset (`0000`).
Selector/read/reset each happen once through replay; CPU accumulators,
status and AR1 are preserved. This validates the coupled model boundary,
not the physical DA150 register map or measured silicon phase placement.

`STLM A/B,IMR` is now admitted using the existing Execute-live saturated-
store path. The `stlm-imr` fixture passes 9,216 cases/restores across both
accumulators, all 16 IRQ sources, four masks, two prior masks, both INTM
states, none/IRQ/NMI input, two pages and three snapshots. Changing the
source at clock five must affect the sole Execute-clock-six write. There
are no destination reads or extension fetches. Pending-mask enable/clear,
previously selected interrupt ownership and NMI independence are checked
alongside restoration. The ISR marker does not perform RETE; this fixture
does not establish complete ISR return/context behavior or every SST source
pattern, which require their separate existing/future contracts.

The original stream observation around `8900` at `02:8b14` contains three
zeros and `0480`; four `beef` words continue after a core halt.
Peripherals continuing after a CPU halt is distinct from validated stream
content: these words do not establish decoded media or native speech.
Current execution and resident-memory acceptance are indexed above.

SPRU131G section 7.5.9 assigns PMST OVLY/IPTR/MP/MC evaluation to
Prefetch; table 7-26 gives POPM (the "all other instructions" row)
consumer-category latencies of 0/1/2/3. DROM has a separate Access/Read
contract in table 7-27. The frontend saves the physical mapping alongside
each logical Prefetch address and carries it into Fetch. The STM-based
`pmst-map` later-branch cases do not alone establish intervening PMST
publication ownership; the distinct Prefetch/IPTR checks below delimit that
coverage. MP/MC and DROM physical decoding remain separate scope, not
implied by an OVLY result. POPM REA/RSA/BRC (`8a1c/8a1b/8a1a`) also
require their own block-repeat ownership contract, not a blanket pop-range
change; separated outside-body active restoration is covered by the
[retained context contract](c54x_execution_contracts.md#active-repeat-context-and-stack-bypass).
For POPM BRC followed by a new RPTB, SPRU131G table 7-24 and example
7-54(d) require one intervening instruction; STM BRC has zero latency.
This is distinct from the in-loop BRC-write latency in table 7-25. A
restoration fixture must separate these cases and independently count loop
iterations rather than treating a stored BRC value as execution proof.

The bounded `pmst-pop` prototype now passes 64 cases/restores: four values,
two source pages, lower/upper branch targets and four snapshots. It captures
one stack read at Read and publishes once at Execute; changing the stack word
after capture must not change the published PMST. SP advances exactly once.
The ordinary branch is separated by three NOPs, so this fixture does not
establish every adjacent-consumer latency or interrupt-vector composition.
The current original-firmware stop and resident-memory comparison are maintained
above; this PMST fixture does not establish interrupt composition.

The `pmst-map`, `pmst-pop` and `pmst-prefetch` isolated
profiles each pass 64 cases/restores. The last is explicitly a debugger-state
conformance fixture: it changes PMST after the first Prefetch, with an illegal
opcode in the newly selected bank, and saves at clocks 1/2/5/6. Fetch must use
the original selected bank, including after restoration; subsequent slots
use the new mapping. This establishes retained slot ownership, not an
organic firmware transition or every table-7-26 consumer latency.
The `pmst-iptr` fixture exposed an early vector-page selection: with a pop
publishing `PMST=0000` during the foreground drain, the first run executed
the old vector page's `0066` marker. SPRU131G 7.5.9 and example 7-18 put
IPTR evaluation at the later ISR Prefetch. The staged core now retains the
accepted interrupt number but selects its IPTR page there, after older
Execute publications. The fixture passes 1,024 cases/restores across four
PMST values, two source pages, all 16 maskable IRQ sources, early/late
pending latches and snapshots at
clocks 3/5/6/11. Its old/new vector markers are distinct; the expected
new-page marker is `0055`. It supplies a controlled CPU pending latch,
not an asynchronous pin-edge or handset fixture. Both physical banks carry
the same vector marker, so this checks IPTR, not vector OVLY bank ownership.
The ISR does not return. A further mapping change between ISR Prefetch and
Fetch, other interrupt sources and full return composition remain separate
acceptance work. Broad regression coverage is indexed in
[Core regression checkpoint](c54x_execution_contracts.md#core-regression-checkpoint); current resident
execution is described separately in [Current boundary](#current-boundary).

The `pop-repeat` prototype admits direct POPM BRC/RSA/REA only
outside an active staged repeat. Its 192 cases/restores cover the three
registers, four words (0/1/8000/ffff), two source pages, two branch target
regions and snapshots at clocks 4/5/6/11. One captured stack read advances
SP once and supplies one Execute write despite later stack-word mutation;
the other two repeat registers, PMST and status are preserved. Three negative
cases place each pop inside an active RPTB and prove rejection before any
stack read, SP change or repeat-register write. RSA/REA are now exposed as
debugger states, using the existing saved register storage. The `pop-brc`
fixture additionally passes 30 cases/restores: counts 0/1/2/7/127, two pages
and snapshots at clocks 4/5/6. It places one NOP between POPM BRC and RPTB,
mutates the captured stack source, checks one stack read/write publication,
and independently observes count-plus-one byte-swap reads/writes. The
zero-latency `stm-brc` control also passes. Active-loop writes and interrupt
composition are not accepted by these cases.

Staged interrupt Access reserves its stack slot
after older foreground Read-owned SP changes in the same clock. The atomic
entry path is unchanged. The `stack-irq` fixture passes 48 cases/restores:
PSHM AL, PSHD RAM, POPM T and POPM SP; IRQ0/NMI; two source pages; and
snapshots at clocks 4/5/6. It checks source Read at 5, interrupt SP reservation
at 5, foreground push Write at 6 and hardware return-PC Write at 7, with one
correct source acknowledgement at 6. This entry-only fixture makes no
ISR-return acceptance claim. The separate `stack-irq-return` gate passes
80 cases/restores over the same forms, sources and pages, with snapshots
at clocks 4/5/6/10/14. It adds RETE, verifies one return-frame read, resumes
the caller, pops the caller's preserved word for push forms, and checks
final SP/T, cleared INTM and ordinary IDLE. Those extra snapshots cover
the retained return operand and caller cleanup, not just entry allocation.
The earlier core reserved SP before an older PSHM Read, interleaving the
hardware return frame with the foreground push; the independent fixture
failed on that slot ownership. FRAME's older-POPM ordering was already
protected and was not changed.

The native return now confirms the correction: FRETE at `02:8b76` resumes at
the populated `03:9b08`, not `03:0000`, and the original bootstrap readiness
gate reaches 1. LDU at `03:8f13` (`1281`) is now admitted for its mapped
ST1 source at 0007 through the existing retained Read path. The `ldu-st1`
fixture passes 288 cases/restores: direct/absolute/indirect addressing,
eight full-word status patterns, both accumulators, two pages and snapshots
before/after address and source capture. Changing ST1 after Read must not
change the zero-extended result; changing pointers or the address literal
after capture must not retarget it. CMPT's documented ARP update is checked
against the captured mode, separately from the preserved arithmetic flags.
One register read and no writes are observed. This is raw mapped-register
read acceptance, not a claim about every ST1 producer latency or peripheral
source. The existing 384-case IMR unsigned-read control also passes.
Broad regression and native 5110 save-state coverage are indexed in
[Core regression checkpoint](c54x_execution_contracts.md#core-regression-checkpoint), rather than
inherited from the chronology of these individual instruction admissions.

Short direct CMPM now shares the retained BITF/absolute-CMPM path for RAM
sources. The `cmpm-direct` fixture passes 384 cases/restores over DP/SP
layouts, four source words, two comparison literals, both initial TC states,
TC/NTC branch conditions, two pages and snapshots at clocks 3/6/7. Address
capture survives later DP/SP mutation; source and literal survive later
replacement. Read is at 6, TC publication at 7 and the following branch's
taken target Fetch at 10 (one intervening NOP). One source read, one literal
fetch and zero writes are checked. The 96-case absolute-CMPM control passes.
Smem CMPM also shares BITF's retained indirect address/literal path
(SPRU172C, CMPM, page 4-33). The `cmpm-smem` fixture passes 52,224 cases
and restores over 17 read-capable encoding forms, eight auxiliary-register selections,
eight equality/inequality pairs, both initial TC states, four SXM/OVM
settings and both compatibility modes. Snapshots bracket address capture
and Read. Later pointer, source and literal mutations cannot retarget or
change the comparison; offset/address extensions precede the distinct
comparison literal, and only TC changes at Execute. Each case observes
one source read, no duplicate address modification and no data write.
The `cmpm-branch` fixture adds 192 cases/restores over two program pages,
TC/NTC conditions and both initial TC states, with the branch immediately
following `CMPM *AR1,#lk`. Source Read is at clock 6, TC publication at 7
and taken-target Fetch at 9. The post-Access pointer and post-Read source
are replaced to verify retained ownership, not just final equality.
Mapped/peripheral CMPM sources remain excluded by the Access guard;
these tests do not establish physical pipeline phases, repeated CMPM or
control-delay composition.
Focused acceptance passes ten profiles: `cmpm-smem`, `cmpm-branch`,
`cmpm-direct`, `cmpm-absolute`, `bitf-peripheral`, `memory-shift16`,
`mpy-delay`, `store-delay`, `move-t` and `stack-irq-return`. The tool suite
runs 2,801 tests with one skip, including 93 core-result verifier tests;
native 5110 save-state restoration also passes. The original firmware
passes `6081 0006` at `02:a6b1` and loads its recorder overlay, rather
than merely moving to another instruction in the resident initializer.
Failure diagnostics report the retained logical instruction separately
from the ahead live PC. For folded multiword forms, `last_fetch_physical`
identifies the final extension fetch, not the opcode's physical address.

Extended STL/STH now admits mapped AR0..AR7 destinations only. SPRU131G
table 7-3 groups these writes with STLM: publication is in Execute, and the
following two words must not use that AR. The `st-extended-ar` fixture
passes 24,576 cases/restores across absolute, long-offset, preincrement and
circular-preincrement forms, eight destinations, A/B high/low stores,
four accumulator patterns, SST/SXM combinations, self/other pointer aliases
and two program pages. Snapshots at clocks 4/6/7 bracket literal capture,
destination resolution and publication. Replacing every AR after address
capture cannot retarget the write; replacing the source accumulator after
Read verifies the retained path's Execute-live source convention. One
write at clock 7, one literal fetch, no destination read and independent
signed/unsigned store-saturation arithmetic are checked.

The `st-ar-consumer` fixture adds 6,144 cases/restores with legal gaps of
two or three NOP words before an unsigned indirect load. It checks one
AR write and one load of the selected sample, including replay after its
Read. No behavior inside the manual's forbidden two-word window is
claimed. Extended SP, BK and other register/peripheral destinations remain
excluded, as do new control-delay or interrupt-composition claims.
Focused controls cover these two profiles plus `store-delay`,
`st-delay-edge`, `stlm-bk`, `mvmd-bk`, `cmpm-smem`, `cmpm-branch` and
`stack-irq-return`. The tool suite runs 2,802 tests with one skip, including
94 core-result verifier tests; native 5110 save-state restoration also
passes. Original recorder STH A (`82f8 0011`) at `03:f7dd` now executes;
the next stop is maintained in [Current boundary](#current-boundary).

TS-controlled memory loads (`14xx`/`15xx`, SPRU172C LD syntax 2) use the
shared retained Smem address path and capture both memory and T in Read.
The current implementation projects all six-bit codes to -32..31; coverage of
raw codes 32..47 is not evidence that their behavior is architecturally defined.
NORM's separately evidenced negative aliases do not establish the behavior of
this memory-load family outside its documented range; neither fixture should
be used as the other's shift-domain oracle.
SXM selects sign extension and
arithmetic right shifting; OVM does not saturate this load form, and carry
and accumulator-overflow flags are preserved. Atomic and staged execution
share the same shift arithmetic, but only the staged frontend retains the
Read-owned count. Address/offset literals are consumed before this count
occupies the retained immediate slot; PC accounting still uses the original
opcode's instruction length. Mapped/peripheral sources below `0060` remain
excluded from the staged TS path.

The `ld-ts` fixture covers 208,896 cases/restores over 17 legal addressing forms,
all 64 shift encodings, four input words, SXM/OVM combinations, both
accumulator destinations and compatibility modes. AR selection and program
page rotate with shift encoding/destination; this is not their full Cartesian
cross-product. Three snapshots bracket address capture and Read. Pointer,
literal, memory and T replacement after their owned phases must not alter
the result. An independent bit-at-a-time 40-bit shift model checks the
result and unchanged arithmetic flags; each case has one data read and no
data write. Short Read/Execute clocks are 5/6, extended forms 6/7.

The `ld-ts-producer` fixture adds 4,608 cases/restores for immediate STM T,
memory LD T and accumulator STLM T, eight shift encodings, four input words,
SXM/OVM combinations, both destinations and two program pages. STM/LD T
have adjacent consumers; STLM T has the required intervening NOP word
(SPRU131G table 7-3). The consumer retains its count when T is replaced
after Read, without duplicating producer reads/writes or literals on replay.
No behavior inside STLM T's forbidden one-word window, new SXM-producer
latency, repeat or control-delay composition is established by these cases.
Focused acceptance passes `ld-ts`, `ld-ts-producer`, `cmpm-smem`,
`st-extended-ar`, `st-ar-consumer`, `move-t`, `mpy-t-imm`, `mpy-mapped`,
`load-offset` and `stack-irq-return`. The tool suite runs 2,803 tests with
one skip, including 95 core-result verifier tests; native 5110 save-state
restoration passes. The atomic `bootreplay` bootstrap/status control also
passes pin-level exchange, selected-state replay equality and unchanged
NAND after the shared arithmetic extraction. Original `1481` at `03:f0f6` now executes. The
current execution boundary and exact resident/overlay comparison are recorded in
[Current boundary](#current-boundary), not a completed recorder transaction.
The original CMPM at `03:91eb` executes with CPL set, SP=`1246` and
direct source `125a`. The `7725` at `02:6d47` is STM immediate to PRD,
covered by the timer-register contract below. The new Smem and adjacent-branch
profiles have focused acceptance; they do not inherit all-profile coverage
from the separately versioned shared regression checkpoint.

Immediate TIM/PRD/TCR writes (`7724..7726`) now retain their literal and
publish through the timer's existing register writer at Execute, after one
peripheral-access clock; they do not use the DAGEN early-write path. The
`timer-stm` profile passes 36 cases/restores across three registers, four
values per register and snapshots before/during the wait and after publication.
It checks one literal fetch, no destination reads, one write at clock 8,
stopped TIM persistence, PRD without implicit TIM reload, TCR writable masks,
read-only PSC and TRB reload of TIM/PSC. These are stopped-timer transaction
checks, not new running-timer or silicon-phase claims. The original firmware's
current execution boundary and resident-memory comparison are maintained
in [Current boundary](#current-boundary), not inferred from individual
instruction fixtures.
The register-save loop is `STM #000b,AR7; RPT #000b; PSHD *AR7+`:
twelve stack pushes read the mapped register window `000b..0016`, not
ordinary source RAM. The repeat contract covers fresh source sampling,
per-iteration stack allocation/write, AR7 modification and saved partial progress.
The single-push prerequisite is now covered by `pshd-mapped`: 288
cases/restores over IMR, IFR, ST0, ST1, AL/AH/AG, BL/BH/BG, T and TRN,
two pages and three snapshots (Access/Read/Execute). Accumulator/T/TRN
sources use four data patterns; control sources use fixed safe state.
The resolved source is sampled once at clock 5 and written once to the
Access-allocated stack slot at clock 6, despite later source and AR mutation.
This prerequisite alone did not advance the original repeat stop. The
repeated-push contract below adds ordinary DAGEN sources; SP and modifying
self-AR aliases remain excluded because this fixture establishes neither
ordering rule.

`pshd-repeat` passes 768 cases/restores for short and long immediate RPT,
counts 0/1/4/11, upward/downward RAM and mapped-register reads, two program
pages, no interrupt/IRQ0/NMI, and four snapshots. Each copy traverses the
ordinary Decode/Access/Read/Execute slots at a one-clock issue interval;
it is not collapsed into the atomic repeat-stream helper. A saved issue
counter owns copies still to enter Decode; RC owns retiring iterations.
Tests observe count-plus-one distinct reads and retained stack writes,
single opcode Fetch, one AR7 update per iteration, RAM mutation after Read,
deferred IRQ/NMI acknowledgement and a complete ISR far-return round trip
to the exact caller PC and program page. Two negative fixtures confirm
SP/self-AR exclusions occur before stack allocation, AR mutation or data access.
Mapped upward reads cover the original BL..AR6 save sequence. SP,
modifying self-AR aliases and peripheral sources remain fail-closed, as
do memory-sourced repeat counts. This establishes the declared stage model,
not measured silicon bus-phase timing. `4b97` increments AR7 (`MOD=2`);
the former decrement interpretation was incorrect.
The combined acceptance below also covers this repeated-push contract.
The original resident load still matches all 48,261 program and 3,656 data
words. These are execution/model checks, not a full-board boot, physical
DA150 bus-map validation or native speech claim.
EXP appears in the original sequence `LDU ...,B; EXP B; LD #1e,A; LDM T,B`.
SPRU172C 4-52 defines EXP's signed exponent in T without accumulator or
status changes, including zero's special result of zero. SPRU131G table
7-13 requires one instruction of T-consumer latency; the original has that
gap. The `exp-t` fixture now passes 12,288 cases/restores across both
accumulators, sixteen 40-bit vectors, zero/one/two-gap LDM consumers,
eight status configurations, adjacent unsigned LDU producers, two pages
and four snapshots. EXP publishes T in Execute without changing its source
or flags; the adjacent LDM retains old T while a gap exposes the new
exponent. Mutating T after the consumer Read must not alter its result.
The unsigned producer's RAM input is also changed after Read, checking
that EXP sees its retained published value rather than stale accumulator
state or a reread. NORM and interrupt composition are not admitted by this
fixture. Its focused LDU, push and conditional-return controls pass.
ADDS at `02:3fad` is a real one-word opcode, not
an unconsumed extension of the preceding SFTL.

ADDS shares the retained memory ADD/SUB pipeline and unsigned-source
handling with SUBS. SPRU172C 4-10 requires zero extension regardless of SXM,
with C and destination overflow updates and OVM saturation. The `adds`
fixture passes 52,224 cases/restores over seventeen read-capable RAM address forms,
eight address registers, both accumulators, eight arithmetic patterns,
SXM/OVM states, standard/compatibility addressing and three snapshots.
Captured addresses/literals and Read operands survive later mutation;
one source read and no data writes are observed. Mapped/peripheral sources
remain excluded. The SUBS control covers the same 52,224-case domain.
`9421` at the following load is
`LD Xmem,SHFT,A` (syntax 5), not another unsigned ADD or a stray extension word.

Syntax-5 LD now shares Xmem BIT's captured AR2..AR5 address and one-time
Access modifier, retains its RAM word in Read and shifts that word at
Execute through the existing signed/unsigned load helper. SPRU172C 4-66
defines all 512 encodings (two destinations, sixteen Xmem modes and shifts
0..15). The `xmem-load` fixture covers those encodings with eight data words,
eight flag configurations, two pages and three snapshots: 196,608 replays.
Distinct AR2..AR5 addresses detect incorrect register selection; percent
addressing covers BK=0 linear behavior and a nonzero-BK wrap. Pointer and
source mutation after capture must not retarget or change the load. The
old accumulator remains through Read at clock 5 and the shifted result
publishes at Execute at clock 6; other accumulator, T and status stay intact.
There is one opcode fetch and one data read, with no data writes. Mapped/
peripheral sources, repeats and interrupt composition remain outside this
acceptance.

Nondelayed CALA A/B now shares the Class 30B held-Read/retained-target
path with FCALA, but selects only the accumulator's low sixteen bits on
the caller's logical page and publishes one PC+1 stack word, not an XPC
word. The held clock remains even without FCALA's second bus write.
`cala` passes 216 cases/restores over both sources, three caller pages,
three conflicting upper-target fields, four snapshots and no interrupt/
IRQ/NMI, including RET and exact caller-page restoration. `cala-producer`
passes another 216 cases with DLD producers and gaps 0/1/2; RAM and
accumulator mutation after capture cannot change the target. Coverage includes
FCALA, FCALA-producer and stack-IRQ controls. No new saved CPU field was
needed. CALAD, repeated calls, other producer families and producer-plus-interrupt composition
are outside this acceptance; stage placement remains a declared model.

A read-only executed-address trace starts after CALA at `02:40e1` in
`02:3e55` and later returns to `02:40e2`; the original return at `02:3ebb`
is RC ANEQ (`fc44`). The MVDD worker is reached later by FCALL
`02:993d -> 02:a164`, so its FRET does not belong to the near-call frame.
The debugger's low-window mnemonic text in this run prints zero-word ADDs
for nonzero ROM instructions; these labels are not used as opcode evidence.
Addresses were cross-checked against the original AA22 word image.

MVDD (`e5XY`, SPRU172C Class 14 and SPRU131G section 5.5.4) captures
both addresses at Access, reads its source at Read and writes the retained
word at Execute. When both operands select the same AR, Xmod wins; Ymod
must not modify it a second time. `mvdd` passes 49,152 cases/restores over
all 256 encodings, distinct/equal/overlapping address layouts, four values,
BK=0/16, two pages and four snapshots. Post-capture pointer/source mutation
cannot retarget the transfer or change the retained word.

`mvdd-repeat` passes 221,184 cases/restores over all encodings, immediate
short/long counts 0/1/15, three layouts, BK=0/16, two pages, four snapshots
and no interrupt/IRQ/NMI. Each iteration occupies real pipeline slots:
Read of iteration n precedes the older iteration's Execute write in the
same clock, including overlapping buffers. The body is fetched once,
long count literals once, and copies issue one clock apart. IRQ/NMI wait
for repeat completion, return through RETE and restore the caller's page
and stack. Input pulses settle into CPU-owned state before snapshots.
Circular fixtures start inside a valid BK-sized queue whose base alignment
is the next power of two strictly larger than BK, not merely BK alignment.

PSHD and MVDD share the retained repeat issuer and its saved remaining-issue
count. Mapped/peripheral MVDD operands and memory-count repeats remain
fail-closed; external-bank contention and silicon phase accuracy are not
established by these DARAM fixtures. The original run passes this worker
and still compares all 48,261 program and 3,656 data words successfully.
These focused fixtures do not establish complete resident initialization,
board boot or decoded audio.

Absolute AND/OR/XOR now retain the address extension through Decode,
capture the resolved address at Access, sample one zero-extended word at
Read and update only the selected accumulator at Execute. SPRU172C
4-11/4-123/4-201 specifies one additional word/cycle for absolute Smem and
no status changes. `logic-absolute` passes 1,536 cases/restores over all
three operations, both accumulators, four RAM addresses (including the
original `b582`), four value pairs, SXM=0/1, two pages and four snapshots.
Literal mutation after Fetch and RAM mutation after Read cannot change
the result; the next instruction is reached without executing the literal.
Mapped/peripheral operands, interrupt composition and control-delay
composition are not established by this fixture. No new saved CPU field
is required. The original restoration sequence is `STM #001d,AR7;
RPT #4; POPD *AR7-`, followed by `STM #0016,AR7; RPT #000b;
POPD *AR7-`. These restore PMST/REA/RSA/BRC/BK and AR6 through BL;
the mapped/repeated contracts below cover these ranges. Resident-memory
comparison remains exact; this is not complete initialization or native audio.

The shared regression checkpoint covers these contracts without promoting
uncovered instruction combinations, physical bus ownership or silicon phases.

Mapped POPD retains its Smem destination at Access and consumes one RAM
stack word, incrementing SP once, at Read. SPRU131G table 7-4 puts AR/BK
publication in Read; accumulator/T/TRN and inactive control destinations
publish in Execute. An older store to the DAGEN bank defers the mapped
write to Execute without rereading the source. The existing register writers
own partial accumulator-word masks and PMST mapping effects. No new saved
CPU field is needed. `popd-mapped` passes 4,032 cases/restores over 21
destinations (AL through AR7, BK, BRC, RSA, REA, PMST), four values,
direct/postincrement/postdecrement forms, CMPT=0/1, two pages and four
snapshots. Indirect forms use AR7, including a modifying self-AR destination;
pointer mutation after Access cannot retarget the retained write.

`popd-repeat` passes 6,528 cases/restores with short/long immediate counts,
four patterns, CMPT=0/1, two pages, four snapshots and no interrupt/IRQ0/NMI.
RAM layouts cover incrementing/decrementing/fixed destinations and forward/
backward stack aliases with counts 0/1/15; mapped ranges use the original
counts 4/11. Each body copy owns real pipeline slots: Read n precedes the
older Execute write, while DAGEN writes retain their Read-stage ownership.
The final BK publication can therefore precede an older BRC publication in
the same clock; the checker tracks per-iteration writes rather than assuming
publication order equals instruction order. There is one body fetch and one
long-count literal fetch. IRQ/NMI waits for final retirement, returns through
RETE with one balanced frame and resumes the exact caller page, including
PMST common-window changes. Pin pulses settle before any snapshot.

`popd-edges` passes 31 cases/restores: 15 model-boundary rejections and
16 older-STL/DAGEN conflicts over same/different registers and two pages.
Conflicting source Read remains at clock 6 and deferred publication at 7.
SP, status, XPC, peripheral and active block-control destinations remain
fail-closed, as do mapped stack sources, memory-count repeats and extended
POPD addressing. Destination rejection precedes pointer modification and
stack consumption; unsupported stack sources stop before SP increments,
after any earlier legal destination Access. RPTB guard fixtures account for
its setup flush/refetch rather than treating its body as an ordinary stream.
The existing `popd-ram` 50-case control retains compiler SP latency and the
original descriptor-copy ordering.

The POPD checkpoint `862b0ce1` passed twelve isolated profiles: the four POPD
profiles, POPM BRC/repeat controls, PMST pop/prefetch controls, both stack-IRQ
controls and PSHD/MVDD repeats, plus native 5110 save-state restoration.
The shared broad acceptance is recorded in the
[core regression checkpoint](c54x_execution_contracts.md#core-regression-checkpoint).

Nondelayed BACC/FBACC A/B share the retained Class 30 transfer boundary
with CALA/FCALA, but allocate no stack words and leave RTN unchanged.
SPRU172C 4-15/4-54 and class 30A specify six cycles, no status changes and
no repeat support. BACC uses the source's low sixteen bits on the retained
caller page; FBACC selects the low 23 bits, including XPC. The target is
sampled after older Execute publications and retained through the modeled
held boundary, front flush and redirected Fetch. Only calls allocate or
publish a call frame. The existing saved pending/target fields are renamed
from `acc_call` to `acc_transfer`; no new CPU save member is added. Stage
placement remains a declared model, not measured silicon phases.

`bacc` and `fbacc` each pass 216 cases/restores over both accumulators,
three caller pages, three conflicting upper-target fields, four snapshots
and no interrupt/IRQ0/NMI. They verify target sample at clock 5, Fetch at
8 and first target Execute at 12, post-sample accumulator poisoning, no
call frame or RTN update, flushed sequential writes and balanced IRQ return
to the retained target. Interrupt entry, not the branch, owns RTN in IRQ
cases. Pin pulses precede every snapshot.

`bacc-producer` and `fbacc-producer` each pass 432 cases/restores over
DLD and absolute READA producers, both accumulators, gaps 0/1/2, three
caller pages, three upper-target fields and four snapshots. DLD's two RAM
reads and READA's program-table read are retained despite input mutation.
READA performs one AL/BL publication at clock 11 before a zero-gap branch
samples its target in the same clock. The original `READA *(0008); BACC A`
composition therefore uses the published accumulator, not a stale target.
Producer-plus-interrupt composition, delayed forms and block-loop interaction
are outside this acceptance.

The branch checkpoint passes these four profiles plus CALA/FCALA and their
producer profiles, conditional call/return, both stack-IRQ controls, PMST
prefetch and repeated POPD (14 profiles total). Evidence validation and
native 5110 save-state restoration pass. The shared broad acceptance is
recorded in the core regression checkpoint.

Long-offset AND/OR/XOR now use the same retained extended Smem address
machinery as loads. TI SPRU172C's instruction descriptions specify an extra
word and cycle for long-offset Smem, with no arithmetic-status changes.
The literal is retained at Decode, address and pointer modification at
Access (clock 5), source at Read (6), and accumulator publication at Execute
(7). Circular fixtures use BK=13 with a 16-word-aligned queue base and an
initial index inside the queue; BK=0 remains linear. CMPT's encoded-AR0
selection uses the retained ARP, while nonzero encoded ARs update ARP.

The `logic-offset` profile passes 294,912 cases/restores over six operation/
accumulator combinations, 24 addressing encodings, four signed offsets,
four value pairs, four SXM/OVM states, both CMPT settings, BK=0/13, two
pages and four snapshots. Poisoning the fetched literal, all ARs after
Access and source RAM after Read checks single modification and retained
operands. Other accumulator, T, stack and arithmetic flags are preserved.
The `logic-edges` profile passes 24 fail-closed cases for mapped/peripheral
sources, repeats and control-delay composition, with no operand access or
pointer update. Interrupt composition and physical silicon phases are not
claimed. Separate short/peripheral/absolute logic controls pass 576/864/
1,536 cases, and the offset-load control passes 12,672. Both accumulator-
branch producer controls and repeated POPD also pass (nine profiles total).
Native 5110 save-state restoration and the shared broad checkpoint pass.

MVMD now admits T/TRN sources through its existing retained Class-19A
move path (SPRU172C, MVMD instruction). The `mvmd-t` profile passes 432
cases/restores: two sources, eight AR destinations plus BK, four word
patterns, two pages and three snapshots. Source sampling and AR/BK
publication occur once at the modeled Access clock 5; changing the source
after that clock cannot change the destination or cause another access
after replay. Unselected registers, accumulators, stack and status are
preserved. This is not a producer-latency, repeat, IRQ or physical-phase
claim, nor new acceptance of other destination classes. Six neighboring
controls (`mvmd-acc`, `mvmd-st1`, `mvmd-cmpt`, `mvmd-block`,
`mvmd-status-irq`, `popd-repeat`), evidence validation
and native 5110 save-state restoration pass. No new CPU saved fields are
needed; the shared broad checkpoint covers these profiles too.

RAM-to-peripheral MVDD now shares the saved peripheral-wait mechanism.
SPRU172C Class 14 specifies one extra peripheral-access cycle. The
`mvdd-periph` profile passes 1,536 cases/restores across three peripheral-
window destinations (`0020`, `0033`, `005f`), four values, sixteen dual
modifier combinations on AR2/AR3, two pages and four snapshots. Address
selection/modification occurs once at Access (4), RAM is sampled at Read
(5), the extra wait is paid at 6, and the retained word publishes at 7.
Pointer and source poisoning checks that the wait and replay cannot repeat
address modification, reads or writes. These destinations are test sinks
in the CPU fixture, not independent device validation. Peripheral sources,
CPU-register operands and repeated peripheral transfers remain excluded;
IRQ composition and silicon phase placement are not claimed. Existing
ordinary/repeated MVDD (49,152/221,184 replays), T/TRN moves, peripheral
logic, long peripheral stores and actual McBSP control also pass (seven
profiles total), along with evidence validation and
native 5110 save-state restoration. No new CPU saved fields are required.

At original MVDD `03:9c70`, AR2=`14b2`, AR3=`0033` and source word
`007f`: this is ordinary RAM to the bench's McBSP2 TX register, not a
CPU-register transfer. Original recorder entry is `02:9e90`. The input
cursor counts delivered words, not parsed commands; pin-decoded acknowledgement
and output traffic do not establish a completed transaction or decoded audio.
Failure diagnostics distinguish the decoded instruction from the potentially
ahead live PC. See [Current boundary](#current-boundary) for the single
authoritative staged stop; physical DA150 mapping remains unverified.
ABS uses the existing Class-1 register ALU Execute path and shared atomic
arithmetic (SPRU172C, ABS instruction). The `absolute` profile passes
13,824 cases/restores over all four accumulator selections, sixteen
40-bit patterns, 32 status configurations, two pages and three snapshots.
It covers zero carry, sticky destination overflow, OVM saturation including
the 40-bit minimum, unchanged non-destination accumulator/status and 1,536
adjacent immediate-load producer cases. The source is live at Execute,
not a newly invented Read-stage latch. There are no data-memory accesses
or new CPU saved fields. IRQ and repeat composition are not claimed.
The matching NEG control, conditional returns, T/TRN moves and peripheral
MVDD also pass (five profiles total), along with evidence validation and
native 5110 restoration. The shared broad checkpoint
also covers these profiles.

Memory/immediate MPY uses Class 6A/6B (SPRU172C, MPY syntax 3): the
memory word publishes to T in Read, while the retained memory word and
signed literal produce the accumulator result at Execute. Absolute and
long-offset forms retain address/offset before the multiplier literal,
adding one word and cycle. The `mpy-mem-imm` fixture passes 98,304
cases/restores: twelve forms (DP/SP direct, short fixed/postincrement/
postdecrement, long offset/preincrement/circular, absolute
RAM and mapped AL/AH/T/AR3), eight memory values, eight literals, eight
FRCT/OVM/SXM configurations, both destinations, two pages and four
snapshots. DP/SP selection shares the direct-form page cases; CMPT=0
and PMST=0 are fixed. BK=13 circular indexing uses an aligned 16-word
queue with the initial index inside the queue. Address modifiers, one
source read, ordered extension fetches, Read-owned T, signed/FRCT product,
OVM saturation, sticky overflow and carry preservation are checked.
Poisoning literals, the selected pointer, source and T verifies retained
ownership through replay. Peripheral/control sources, repeats, IRQs and
new producer/consumer-latency composition are outside acceptance.
The shared existing T/immediate control passes 86,016 replay cases, and
its mapped-consumer control passes 3,072. Offset loads, ABS and peripheral
MVDD also pass (six profiles including the new fixture), along with
evidence validation and native 5110 restoration. No new
CPU saved fields are introduced. The shared broad checkpoint also covers
these profiles without promoting untested compositions.

The same retained operand path supports `MAC Smem,#lk,src,dst`
(SPRU172C, MAC syntax 4, page 4-82). Smem publishes to T in Read;
Execute multiplies the retained signed memory word and literal, applies
FRCT and SMUL product saturation, adds the selected signed 40-bit
accumulator and applies destination OVM/sticky overflow. No new CPU saved
fields are needed. The compatibility-named `mpy-mem-imm` gate additionally
passes 262,144 MAC cases and replays: eight RAM address forms, eight memory
values, eight literals, sixteen FRCT/OVM/SXM/SMUL configurations, all four
source/destination selections, two pages and four snapshots. Source
accumulator patterns include negative, extended and saturation-boundary
values; source/literal/pointer poisoning checks retained ownership.
Another 1,664 cases/replays reject all 96 mapped/control/peripheral source
addresses and all eight write-only short MOD=3 encodings, across all
selectors and pages and two snapshots, without data accesses or pointer,
accumulator, T or ST0 changes. These are scoped frontend exclusions, not
silicon trap semantics. Repeat, interrupt and new producer/consumer
compositions remain outside this acceptance.

Original R060 AA88 contains `6582 0009` at `03:ba1a`:
`MAC *AR2,#9,A,B`. `mu4_context_contract.py` pins its eleven-word
context in the original upload, with mutation and
wrong-page negatives. This static witness establishes provenance and
syntax, independently of executable core tests or recorder acceptance.

Near B and nondelayed BC retain block-loop ownership. The two-word branch
must finish before REA and target the same block; forward and backward
targets are supported. Conditional calls are not branches for this admission.
A taken redirect reconstructs the
discarded endpoint-fetch prediction from Decode-owned BRC. An untaken
BC restores its held word's body/endpoint/final-first tags alongside its
word and address. Those three saved booleans belong to the
CPU's pending instruction state; they are reset and registered for replay.
BRC decrement and final-iteration BRAF timing remain as specified by
SPRU131G sections 6.8/7.5.8.

The `rptb-branch` fixture passes 12,288 cases/restores across a two-arm
join and a BC immediately before an endpoint NOP, counts 0/1/2/7, four
taken/untaken patterns, both accumulators, two pages, eight save points,
four IRQ/NMI arrival clocks and an uninterrupted control. Source and
selected-arm writes occur once per iteration, with exact terminal
counters, pointers, status and one balanced interrupt frame when requested.
A later ordinary branch clears the held tags before replay, requiring
restoration of the earlier loop-owned tags. Pin arrivals are allowed to
settle before a coincident save, and restored pin changes settle before
CPU resumption. These are declared model clocks, not measured silicon
phases.

The `rptb-br-edges` fixture passes 32 fail-closed replay cases for targets below
and above the block, delayed control at REA, conditional near calls, nested blocks and far
branches, with no control frame or body write. Branches occupying the
endpoint, XC spans crossing the block end, IDLE in the block and software
loop-control modification remain excluded by the RPTB guards. Wholly in-block
XC is specified in the retained contract above. Non-delayed far-call acceptance
and backward-loop acceptance are specified in the retained contracts above;
near-call, outside-block branch and other delayed compositions are not promoted.
The bounded internal BD and signed BCD contracts have their own focused
profiles; this exclusion set does not reject those admitted forms.
The shared broad baseline remains the versioned checkpoint above, independently
of focused acceptance added since that run. These are not measured silicon
phase claims.

All 48,261 program and
3,656 compared resident data words still match. This remains partial
initialization, not complete board boot, native speech or a completed
service transaction.

The former bad return produced a zero-filled walk, a bootstrap restart and
a later FRET into page 8. These were downstream symptoms, not missing ROM
evidence. Zero is a valid ADD opcode, so illegal=0 did not establish useful
execution in the RAM-backed bench. Program-read taps include speculation and
program-data accesses; debugger execution tracing was needed to identify
the earlier return. The atomic `bootrecsave` observation remained in populated
code, but its later stimuli differ and it was not an instruction-equivalence
proof. No empty region was filled and no return target or firmware state was
forced to repair the continuation.

Status acceptance
is limited to the following
tested consumer/composition forms, not every status-register interaction.
The `mvmd-cmpt` fixture initially failed because indirect address selection
and pointer modification sampled live CMPT. The correction retains CMPT
in the saved Decode slot, separately from the DP/CPL sample (CMPT's bit
position overlaps DP in that packed sample). Staged short/long address
selection and AR modifications consume the same retained mode; atomic
helpers keep their live-status defaults. The fixture passes 16 cases/restores
across both directions, LD/ST with postincrement and zero through three
NOPs, checking the old mode with zero/one and the new mode with two/three.
ARP-writer latency and other indirect consumer categories require separate
coverage; this fixture establishes the basic CMPT category only.

The `mvmd-status-irq` fixture passes 576 cases/restores across IRQ/NMI,
four requested INTM pairs, enabled/disabled IMR, arrivals at clocks 4/6/8,
two pages, three snapshots and one-/two-word followers. It checks one
status publication, accepted continuation/frame ownership, no masked frame,
one foreground marker, RETE restoration and masked-pending IFR persistence.
The shared register map prevents memory writes from setting INTM. A
write-one cannot make an unmasked source ineligible; old-mask/set-mask
selection races are therefore not established by these cases. Zero-write
clearing remains a legacy, unverified policy: see the
[ST1 access contract](rom4_dsp_loader.md#st1-memory-access-constraints).
Generic status writes do not inherit RSBX/SSBX following-instruction
protection. Negative masked cases keep a
valid bounded IDLE continuation: pending sources may wake IDLE without
being serviced. This fixture does not establish arbitrary interrupt arrival
phases or all combinations with status consumers/block repeats.

The arithmetic fixtures do not establish all possible SXM/ASM consumer forms.
SPRU131G table 7-14 lists MVMD among ST1 writers safe directly
before RPTB; a late write must not erase a younger BRAF update. Section
6.10.7's protected-following-instruction rule specifically names RSBX/SSBX
INTM and must not be assumed for every full-ST1 writer. The resident
memory audit remains exact.

The status gates distinguish the following SPRU131G contracts:

- Table 7-16: MVMD has two intervening-cycle latency for CMPT consumers;
  POPM has three or two depending on the consumer category.
- Table 7-19: MVMD has CPL latency two for ordinary direct addressing and
  one for the listed extended-shift/data-copy category. Decode-time CPL
  snapshots must not be replaced indiscriminately with the newly written bit.
- Table 7-20: MVMD is absent from the one-cycle SXM-latency list, hence
  covered consumers have zero latency; POPM and accumulator stores are listed.
- Table 7-22: MVMD is likewise absent from the one-cycle ASM-latency list;
  adjacent ASM-dependent arithmetic must use the new field, while POPM and
  accumulator stores retain their documented consumer latency.
- Table 7-14: an immediately following RPTB must retain its BRAF update.

These are consumer-visible latency requirements, not a claim that every
status field is physically written at the same pipeline stage. Publication,
consumer sampling and save/replay have separate gates. Special INTM
following-instruction protection is not assigned to arbitrary full-ST1 writes.

The peripheral/control/IMR checkpoint passes full CPU conformance (288
required markers), native 5110 save-state restoration, all 33 isolated
profiles in `tools/run_c54x_core.py`, 135 targeted Python tests and evidence
validation. The original audit still matches 48,261 program and 3,656
ordinary data words, with four register-window words excluded. Full
board boot, native speech and the explicitly excluded instruction or
producer/interrupt compositions remain unaccepted; do not
infer full board boot from this initialization frontier.

TI SPRU131G section 6.10.1 explicitly defines writing the current IFR
contents back to IFR as clearing all pending interrupts. The observed
`4801 -> 8801` pair therefore has a documented acknowledgement purpose,
not a request for a fabricated peer response. The existing staged STLM
path samples the accumulator in Execute, rather than using the older
Read-stage operand snapshot; the adjacent LDM fixture confirms that
publication order without an additional forwarding change.
SPRU172C page 4-175 identifies STLM as a one-word Class 10A store with
page-zero addressing independent of DP. Pending/accepted interrupt
ownership and NMI isolation are covered by the composition fixture below.

The isolated `ifr-pair` profile admits direct `8801/8901` stores and
executes adjacent `4801/4901` loads, covering both accumulators, four
pending-bit patterns, with and without a physical IRQ12 pulse after the
read, and three snapshot positions. The pulse is delivered strictly between the
read and store; a newly set bit survives unless the captured mask already
contained that bit. Snapshots after the pulse allow its scheduled delivery
to settle before saving. This proves read-back acknowledgement and
preservation of the unselected accumulator. Additional cases unmask IRQ12
after the read with its flag already set, or pulse independent NMI. Both
finish one vector acknowledgement after the older store and return through
RETE with SP restored. The selected IRQ survives clearance of its own IFR
bit; NMI is not cancelled by IFR clearance. All 96 cases and 96 restores
pass. IMR stays zero in the pending-only and NMI cases. This is executable
composition coverage, not a claim of measured silicon cycle timing.

Mapped BITF admits AL/AH/AG, BL/BH/BG and T/TRN; other CPU control
register sources below `0020` remain excluded. Peripheral-read acceptance
is described above. `tms54test -bios mapped-bitf` requires
13,824 replay cases: eight register sources, four masks including zero,
both initial TC states, direct/absolute forms and 1,536 AL-producer cases.
It captures one source in Read and changes only TC in Execute according
to whether any masked bit is nonzero (SPRU172C 4-28/4-29). Source changes
after Read cannot change the result, and every address/mask extension is
owned once. The producer cases use zero/one values for the status-bit
question without synthesizing a DMA status or completing a transfer.

Mapped memory ADD/SUB now admits AL/AH/AG, BL/BH/BG and T/TRN, using
one retained Read sample through their existing register map. Controls,
peripheral sources and mapped SUBC retain their exclusions. The mandatory
`tms54test -bios mapped-word` fixture has 6,912 save/replay cases covering
eight registers, four add/subtract/destination selectors, direct/absolute
forms, signed/unsigned operands, OVM, accumulator aliases, changed source
registers and two program pages. Its 768 producer cases pin SPRU131G
table 7-28: a short mapped AL consumer sees the previous value after an
adjacent one-word accumulator load; one NOP exposes the new value. The
long-offset consumer has no such gap requirement. Register sampling and
flag publication stay separate; no RAM write or frontend fallback is used.
These are bounded CPU contracts, not measured DA150 bus phases.

The isolated `tms54test -bios negate` fixture requires 13,824 cases with
save/replay, all A/B source/destination combinations, 16 patterns including
32/40-bit extrema, 32 initial status combinations, two pages and 1,536
immediate-load producer cases. It checks carry only for zero, sticky
destination overflow, OVM saturation, unrelated accumulator/status
preservation, one opcode fetch and no data-memory access. The source is
live in Execute, including after a preceding producer. Arithmetic uses
the same one-word helper as the atomic interpreter without changing
frontend mode. Repeated NEG and measured silicon pipeline phases remain
outside this acceptance scope. The instruction contract is TI SPRU172C
pages 4-119/4-120.

The recorder's original `03bdb6..03bdc2` escape prefix also requires
pending double-store forwarding: `DST A,*SP(a)` followed by `DLD *SP(a),A`
must consume the retained pair, not old RAM. A passive native trace read
old low word `0088` immediately before the actual store of `0007`, causing
the bit writer to receive `0088` at width three. Mapped AL/BL reads already
returned seven; changing accumulator latency would address the wrong cause.
The staged bypass now forwards both retained DST words while preserving
the two real EB writes and their order. The exact original prefix passes
128 cases with save/replay; an independent 4,608-case matrix covers A/B,
word/pair consumers, three store addressing forms, saturation, sign extension,
aliases, gaps, pages and two save points. Physical bank timing is unvalidated.
A fresh two-second native recorder observation contains 1,010 bit-writer
inputs with zero overwide values (previously two); the first corrupt escape
is now `0007` at width three. Changed downstream field ordering means later
ordinal numbers are not identities. Full saved-media and native audio
validation remain separate requirements.

The corrected-core full `bootrecac` recording completes successfully and
passes resident-memory comparison (48,261 program and 3,656 data words)
and converted-input checks. Its independently serialized NAND SHA256 is
`6801c3b7ca072cae90c99aa0ec131d0ab9bd6a344dd77294049cc339396e413b`;
independent volume extraction yields 143,680-byte `TEST`, SHA256
`4ba59580f8bf50b8d45d7a62d8ddf42ea94dfb7d884ff05b5092e7c13141470d`.
These identify the post-DST-fix recording, not a decoded-audio oracle.

Fresh read-only transform observations match all 96 source prefixes,
scalar outputs and independently extracted saved chunks (3,072 bytes,
unique file origin 622, one state and cursor 0..3,040 in steps of 32).
Captured plaintext SHA256 is
`5b722a6fc85fb40368b7d4099d4cc7fbaae99e520161fd3875d7952e29295938`.
FFmpeg decodes its first 2,977 bytes as eight packets/eight frames,
8,192 samples and zero decode errors, versus one frame/seven errors before
the DST correction. This is host validation of captured native encoder
output, not native decoder or analog audio acceptance. Reusing a prior
run's transform state fails and is not a valid substitute for fresh capture.
Native cold playback takes the processing body on its first invocation.
The original `0260b3..0260bc` caller loads T from `1170`, initializes A/B
from the first two coefficients at `c22a`, repeats `POLY *AR2+` three times
and stores A to `1174`. Ten source words are pinned by
`mu4_context_contract.py`; mutation tests reject every changed source word.
[TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf), page 4-126,
defines POLY: A receives rounded signed A(32..16) times T plus old B;
B receives the coefficient shifted 16, with SXM extension. T is unchanged.
The staged path captures memory/T in Read and shares arithmetic with the
atomic path. Short direct/indirect forms and pipelined RPT are admitted;
mapped/peripheral sources, extended forms and control-delay composition
remain excluded. SMUL-set behavior and interrupt composition are unvalidated.
The `mac-smem` profile now also requires 4,608 POLY cases/restores over
three addressing forms, eight input patterns, FRCT/SXM/OVM combinations,
no repeat/RPT #0/RPT #2, two pages and four snapshots. Tests preserve T,
unrelated status and exact coefficient-read/pointer counts. POLY must remain
excluded from generic Smem-special T publication: reading its coefficient
does not write T.

`mu4_playback_poly_observe.lua` captures actual reads of `1170=4600` and
`c22a..c22e={1282,be62,76db,3842,0000}`, then actual stores
`1174=68a9,1175=0000`. The source-contract tool independently computes
all three steps and matches `68a90000`. Live PC is context only, not
retained operand ownership or retirement proof.

SQURA/SQURS (TI SPRU172C 4-163/4-164) square the signed memory word,
add/subtract it from A or B with FRCT and final OVM saturation, and load
T from memory. The staged path retains the operand, publishes T in Read
and must not republish it in Execute. Do not substitute the MAC product
clamp: these square operations retain the full product before accumulation.
The atomic and staged paths share one arithmetic helper. The `mac-smem`
profile requires 36,864 square cases/restores over both operations and
destinations, indirect/absolute RAM and mapped AR1 sources, 64 input pairs,
16 FRCT/OVM/SMUL/SXM settings and three snapshots. Tests change RAM/T after
Read and require one captured source read, no data writes, sticky overflow,
the unaffected accumulator/status and preservation of the later T value.
This does not validate all mapped-producer latencies, other extended
address forms, RPT/interrupt composition or measured physical bus phases.

Fourteen original AA99 words at `0260cd..0260da` pin the block-repeat
setup and `0260d8: SQURA *AR2,A`; source mutation tests guard the extent.
Native playback advances past that boundary. The global playback frontier
belongs in Current boundary above; source and dependency acceptance here
must not be mistaken for complete decoder-output validation.

### T-input Product Dependencies

The register forms `MPYA A/B` and `MACA[R]/MASA[R] T,src,dst` use signed
A(32..16) and signed T, with FRCT, final OVM saturation and sticky target
overflow. MACA/MASA additionally use the source accumulator, SMUL product
saturation and optional rounding; MPYA does not apply that intermediate
product clamp. Atomic and staged execution share one arithmetic helper.
No data-memory transaction or T publication belongs to the product itself.

SPRU131G 7.5.6/table 7-13 gives these multipliers zero T-producer latency:
they are not the Category-I shift/bit-test consumers. Table 7-3's generic
recommendation must not be turned into an old-T multiplier oracle. The
partial staged model retains T after older Execute and before younger Read
publication, then samples A and the source accumulator in Execute. Reading
live T in Execute would instead risk consuming a following STM-to-T.
Smem MPY/MPYR/MPYU/MAC/MACR/MAS/MASR and POLY retain dedicated T at the
same older-write boundary. A mapped-T Smem operand defers its single
memory read to that boundary too; other sources keep their existing read
ownership. Immediate-T products retain their operand during the complete
two-word Read, whose extension clocks already separate the tested older
producers. They need no production change in this audit.

Two executable counterexamples establish why early capture is wrong.
Adjacent STLM-to-T followed by `2081` consumes zero rather than `1234`,
producing zero instead of `fff6e61234` with memory `8001`. Adjacent STLM
followed by `3681` uses stale T=1 rather than `8000`: with A=`12340000`
and B=`7fff8000`, it produces `0080000000` rather than `0076e60000`.
Both fail before their capture-order correction and pass afterwards.

The `mac-smem` profile requires these dependency banks, each with eight
numeric patterns, two pages and three save-state snapshots:

| Bank | Cases And Replays | Domain |
| --- | ---: | --- |
| Register products | 69,120 | 18 opcodes, 16 control settings, five layouts |
| Short Smem products | 107,520 | 14 opcodes, RAM/mapped T, 16 settings, five layouts |
| POLY dependencies | 2,304 | Indirect RAM, eight settings, six layouts, SMUL excluded |
| Immediate-T products | 27,648 | Six opcodes, 16 settings, six layouts |
| Absolute Smem products | 64,512 | 14 opcodes, RAM, 16 settings, six layouts |
| EXP producers | 35,328 | 18 register and 28 short RAM/mapped-T forms, 16 settings |

The first five layouts are no producer, adjacent/separated STLM-to-T,
an A producer and a following STM-to-T. The sixth is adjacent EXP A.
An independent scalar model checks signed/unsigned arithmetic, rounding,
FRCT/OVM/SMUL, fresh/sticky overflow, and POLY coefficient extension.
Adversarial T, RAM and extension replacement after Read checks retention.
Each case requires exactly one opcode fetch, the expected zero/one source
and literal reads, and only the expected producer write. The existing
MAC/MAS, square and repeated-POLY arithmetic banks also pass.
Nine AA99 words at `025b0d..025b15` pin the original T-producing caller
and `025b14: MPYA A`, with per-word mutation tests.

This is bounded family/dependency acceptance, not every producer/address
combination: long-offset Smem, absolute mapped-T and other mapped producers,
POLY with SMUL, dependency/repeat/interrupt composition and physical phases
remain unvalidated. Keep Category-I TS/NORM/bit-test restrictions separate.
`c54x_remaining_t_multiplier_category_audit` retains these residual limits.
TS load/arithmetic producer, EXP-to-NORM, EXP-to-register and invalid MOD=3
profiles pass. A fresh 20-second native playback after the POLY correction
passes DMA/source-range and capture-integrity checks; all three complete
captured streams are byte-identical to the preserved baseline. Neither
dependency correction is an observed waveform improvement in this workload.

With these register forms admitted, the preserved recording produces a
complete 20-second legal run, not an unsupported-opcode stop. Observation
checks report 1,631,772 actual channel-3 source reads (1,582,728 nonzero)
within `1000..137f`, 1,661,065 TX words and 1,691,842 DIN words, with
1,597,627 nonzero at each serial boundary. First-session method coverage
has 375 body/publication/return paths and no early returns; parent coverage
completes 11,956 wrapper/body/return paths and 5,978 dispatches. The selected
AA99 range matches original source at 1,170 visited addresses and
6,631,884 Execute entries, with no opcode changes. DMA and source-range
checkers pass; the diagnostic still deliberately exits rejecting audio
completion. Counters alone are not a waveform-quality or complete
transport-pairing oracle; analog conversion and speech remain unproved.

### Playback Word Capture

The bench records complete words at the existing actual channel-3 source
read, McBSP TX and codec DIN callbacks, without extra guest memory reads or
feedback. Files are raw little-endian words, not a claim about stereo frame
phase or analog audio. Output is fresh-run only: file handles are not
save-state artifacts, and restored capture integrity is unsupported.
`tools/mu4_playback_capture_check.py RUN_DIR` verifies byte/word counts,
observed word hashes, nonzero counts and the existing strict DMA observation.
New captures also write `mu4_playback_din.slots`: one packed nine-byte record
per DIN word, little-endian unsigned 64-bit emulated nanoseconds followed by
the callback's actual slot byte (0 or 1). This observes the existing callback,
not extra guest reads or a changed serial clock. `--require-framing` requires
its terminal record, count and raw-byte FNV hash; legacy word-only captures
remain explicitly identified as lacking slot evidence. File save/restore is
unsupported for both outputs.

New captures additionally write `mu4_playback_tx.times`, one little-endian
64-bit emulated-nanosecond timestamp per existing completed-TX callback.
The terminal record binds count, byte extent and raw-byte FNV hash.
`--require-tx-times` requires both TX times and validated DIN slot metadata.
Pairing selects the next chronological DIN callback within one BCLK period
(84 ns at this fixture's 11.9952 MHz codec clock); it never chooses by sample
value, searches for a matching word or fits an offset. The checker reports
unpaired TX and unmatched DIN separately. Legacy captures cannot establish
this timestamp-pairing result; file save/restore remains unsupported.

The post-DAGEN-correction twenty-second replay completes with illegal=0
and completed parent dispatch. Its numerical streams are deliberately
re-banked after the evidenced CPU fix; they are not the pre-fix oracle.
All four word/slot files reproduce the shorter proof run as exact prefixes,
and both selected complete transforms reproduce at every stage.
Its complete stream identities are:

| Boundary | Words | SHA256 |
| --- | ---: | --- |
| DMA source | 1,631,772 | `dc9662f5705b6ac7b1f16dadcab9587ef456e9e4de24a3be5abc48c0a03e43a0` |
| McBSP TX | 1,661,065 | `4e6ddc62d886f62883211216425dc3ba7401b37d05ef0e5a19ff4b64ce0c63d8` |
| Codec DIN | 1,691,842 | `cafaec6eefc94f7b4789db241470858b17209f20176ad8fe30fce81e2193b684` |

Chronological first-nonzero anchors, checked over 64 words, give initial
DMA-to-TX offset 92 and TX-to-DIN offset 367. The first 783,108 and 783,200
overlapping words respectively match exactly, including zero words.
Thereafter a single-offset comparison diverges. The codec sidecar rules
out a DIN slot discontinuity; the first disagreement is the underflow
interval described below, not evidence of a corrupted transmitted word.
Later source/TX reset and direct-write epochs still require alignment.
The checker reports the complete mismatch extent, not a best-fit realignment,
and never promotes integrity or overlap to decoded-audio acceptance.

With verified slot metadata, the checker also reports the actual DIN time
and slot for each retained fixed-offset mismatch. In this capture the first
TX-to-DIN difference is source index 783,200 / sink index 783,567 at
9,702,093,026 ns, slot 1 (`ffbc` versus `0000`). This precedes the next
logged system-clock change at 9.869008605 s; that later change cannot explain
the onset.

The passive transport observation brackets DXR writes at `0023` from
9.698 through 9.876 s and records every McBSP0 indexed configuration write
at `0038/0039` from boot. The last DXR write before the gap is `ffbc` at
9.702069134 s. There are no further DXR writes until the observed SPCR2
reset (`0102`) at 9.871026174 s; the next write is `0000` at 9.874720177 s.
All 14,900 DIN words from the first disagreement until that reset match
the modeled underflow pattern: slot 0 receives repeated `ffbc`, while slot 1
samples the held-low serial pin as `0000`. McBSP completes only the repeated
first word, whereas the codec continues sampling both slots. Comparing
these callbacks at one constant word offset therefore ceases to be valid.
This explains the first divergence without changing clocks or samples; it
does not establish later epoch pairing, analog behavior or decoder accuracy.

Two fresh ten-second runs reproduce the twenty-second capture as exact
prefixes in all four files. The full-interval observation records 147
configuration writes and 474 DXR writes, ending at 10.016666667 s with
illegal=0. `tools/mu4_transport_observe.lua` retains this recording-specific,
read-only observation preset; use it as `-autoboot_script` with the same
`mu4nand -bios bootplaystage` media fixture and a ten-second run. It is not
a generic transport-acceptance gate, a source of MMIO writes or a claim
that file-capture save/restore is supported. Next classify the remaining
reset/reload intervals before asserting complete DMA/TX/DIN pairing.

#### Timestamp-paired serial callbacks

A fresh twenty-second run with the TX observer preserves the three word
streams and DIN sidecar byte-for-byte. All 1,661,065 completed TX callbacks
pair with the immediately following DIN callback: 525,994 at 41 ns and
1,135,071 at 42 ns, with zero value mismatches and zero unpaired TX words.
The TX timestamp file has 13,288,520 bytes, SHA256
`e59e55b476e7f0d280e814f2782c3a3cd6343ac95aeb099b008be56f658e270f`.
This covers the complete observed serial callback stream, including both
later reloads, rather than only its first fixed-offset prefix.

The remaining 30,777 DIN callbacks have no completed TX word in that clock
window. All have value `0000`: 737 in slot 0 and 30,040 in slot 1. Every
one follows a completed TX word and equals the uniform level implied by
that word's last bit. The checker reports this held-level consistency
separately from actual pin ownership, with no exceptions in this capture.
It does not infer the initial pin state when no TX word precedes a sample.
Their existence is not a failed transmitted-word comparison: the codec also
samples idle pins during underflow/reset. Only the first 14,900-word interval
has the independent DXR/configuration-write coverage above; the other 15,877
have value consistency, not a causal reset/pin-ownership proof.
This result does not establish channel-3 DMA-to-DXR
ownership, correct handling of every overwritten/interrupted transmission,
PCM accuracy, analog output or native speech. The original strict playback
completion check still exits rejecting audio acceptance at 20 s, illegal=0.

```sh
python3 tools/mu4_playback_capture_check.py RUN_DIR \
  --require-framing --require-tx-times
```

Unit tests reject malformed/duplicate metadata, changed hashes and extents,
non-increasing TX times and missing required sidecars, and report pairings
outside the clock bound as unpaired.
A wrong first word followed by a matching word stays a mismatch; an
underflow gap cannot shift subsequent pairings by index. Unmatched-sample
tests distinguish initial unknown pin state, held-high/held-low consistency
and contrary samples without dropping exceptions from the report.

The DIN sidecar contains 1,691,842 records (15,226,578 bytes), SHA256
`311d7ef1e63b60307fdbe9846d4220168b443334add930fab29580e905241bfa`.
It begins at slot 0, yields 845,921 complete pairs, and has zero orphan
words or slot discontinuities. Pair spacing is 1,333/1,334 ns and frame
spacing 22,675/22,676 ns, consistent with the modeled 44.1 kHz clock.
CPU clock changes do not interrupt that cadence. Slot metadata remains
byte-identical to the pre-fix baseline; numerical word hashes change as above.

#### DMA source-to-DXR ownership

`mu4_playback_dxr.writes` records every observed full-word DXR bus write
after the AA99 playback selection: little-endian nanoseconds (8 bytes),
channel-3 source-read ordinal (4), word (2), active DMA owner (1), reserved
zero (1). Owner zero means no active DMA destination write, not an inferred
CPU instruction; owners 1 through 6 identify channels 0 through 5. Only
channel 3 carries a source ordinal. Ownership comes from the synchronous
DMA destination-write inspector, not matching sample values or GENPC.
The terminal record binds extent and raw-byte FNV hash. Like the other
capture files, this is fresh-run evidence without save/restore support.

The complete twenty-second observation contains 1,631,772 DXR writes,
all owned by channel 3. Every actual source-read ordinal occurs exactly
once, in order, with its original source value. No other writer appears
within this capture window; this does not assert that CPU code never
writes DXR in other profiles or before selection. A fresh run preserves
all three word streams, DIN slots and TX timestamps byte-for-byte.
The ownership file is 26,108,352 bytes, SHA256
`1da07329b20a74191a13ea61c91655347463e6e5ee29969b2b875e264239e1ad`.

```sh
python3 tools/mu4_playback_dxr_check.py RUN_DIR
```

The checker first requires capture integrity and both serial time sidecars,
then rejects missing, duplicated, skipped or out-of-order source ordinals,
wrong source values, foreign-owner ordinals, reserved bytes and malformed
metadata. Rehashed corruption tests exercise those semantic failures.
This proves the actual source-to-DXR bus transaction, not that the controller
accepted the write, preserved it through overwrite/reset, or generated exactly
one TX callback. Keep `dxr_to_tx_validated` false until buffer loads,
discarded writes and underflow repetition are causally accounted for.

The optional reset-aware sequence check consumes the full passive indexed
McBSP0 configuration observation from `mu4_transport_observe.lua`. It checks
the observation's terminal count, register-index continuity and endpoint,
then merges SPCR2 writes, owned DXR writes and completed-TX times in order.
It does not search by sample value. A clear XRST discards pending words and
clears retained DXR; writes while reset is asserted are ignored. Otherwise
pending writes feed completions in order, and an empty queue predicts the
retained word. More than two pending words stops as an unresolved overwrite
case rather than silently dropping one. Timestamp ties between event kinds
also stop rather than inventing their order.

For the complete capture this model accounts for 1,631,766 fresh-write
completions and 29,206 retained-word repetitions. The first 93 TX words
precede a reset within the capture and remain unattributed: their equality
to zero is not evidence of pre-capture write ownership. Together these
account for all 1,661,065 TX callbacks. Source ordinals `1`, `783109`,
`786383`, `1559251` and `1562525` are pending at resets; all five are zero.
Ordinal `1631772` remains pending at the endpoint. No observed write falls
inside asserted reset, and maximum pending depth is two. Thus all 1,631,772
source writes are accounted for without a fitted offset or omitted zero
samples.

```sh
python3 tools/mu4_playback_dxr_check.py RUN_DIR --check-buffer-sequence
```

This is reset-aware sequence agreement, not direct observation of internal
DXR-to-XSR loads or proof that zero-valued writes cannot have ambiguous
buffer histories. Keep that distinction from the actual bus-ownership and
TX-to-DIN callback proofs above. Tests exercise nonzero reset discards,
ignored writes, repetition, pending endpoints, wrong values, overfull queues,
time ties and incomplete control metadata. A capture with no checked
post-reset TX words fails instead of passing entirely as an unknown prefix.

#### Independent Reference Comparison

The first 2,977 captured plaintext bytes form eight host-valid AAC packets,
SHA256 `3f0a2b7eafb22ede5b3e3f1bb54c83f2f959386bedd897ebebf218e6fb816ff4`.
FFmpeg decodes eight AAC frames at 44.1 kHz without errors, producing
8,192 signed stereo sample frames, SHA256
`440e479020b5e558b9aa792adc14af8bb4974f30898cba57f3189ac1434f8993`.
The regenerated host output matches the earlier reference byte-for-byte.

##### Complete-window host reference

`mu4_recorder_transform_check.py --output-projected-aac PATH` can explicitly
extend the single state from a freshly verified recording prefix across
complete 32-byte saved-file windows. It first requires all captured states
to agree and the inverse to reproduce captured plaintext. It never models
a partial final window or claims that later native state was observed.
Output is a new host-only artifact, not an emulation input; existing files
and input evidence cannot be overwritten. Without this option the checker
remains read-only.

For the post-DST recording identified above, the unique origin is 622.
The projection covers 143,040 bytes, with 18 final bytes left unmodeled.
It finds 384 consecutive complete ADTS header/length records totaling
142,849 bytes, SHA256
`812882ea5da3fb9051a04e421922694366e580e75ae98df469936426eef1a873`.
All headers declare MPEG-2 LC, 44.1 kHz stereo and one raw-data block.
Another 191 projected bytes follow that chain and are not accepted as AAC.
Header syntax alone is not payload validity: FFmpeg decodes 383 frames,
but rejects packet 384 at byte 142,477 (nominal extent 372) with an exhausted
data-stream element. Its permissive whole-input exit zero is not a pass.
Partial-final-packet/container handling and later native state remain
unresolved; do not repair the payload or assume either explanation.

The explicit `--project-packet-limit 383` output contains the first
142,477 bytes, SHA256
`0a3091469463f5937df4be728e6edc0af087c21a06a3f8097713522cc4cb2b8e`.
Its report still inventories all 384 header-delimited packets and labels
the selected output prefix separately. FFmpeg with `-xerror` decodes this
isolated prefix with no diagnostics into 392,192 stereo sample frames
(1,568,768 bytes of signed LE16), SHA256
`000e716c4dbe30e78d137706be5621aefb271059d343bf5ab793f1406706dfab`.
Its first eight frames are byte-identical to the earlier 32,768-byte host
reference. These are independent host-decoder results for a declared model
projection, not native output acceptance beyond the captured prefix.

```sh
python3 tools/mu4_recorder_transform_check.py RECORDING_TRACE EXTRACTED_TEST \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00 \
  --output-projected-aac NEW_PREFIX.aac --project-packet-limit 383
ffmpeg -v error -xerror -i NEW_PREFIX.aac -f s16le -acodec pcm_s16le NEW_REFERENCE.s16le
```

Whole-session return observation below extends the chronological comparison;
its termination decision still needs classification. Do not infer a waveform
offset or gain from first nonzero samples: guest post-conversion processing
remains separate from decoder conversion blocks.
It is analysis output, never media supplied back to the emulation.

```sh
ffmpeg -v error -i captured-eight-packets.aac -map 0:a:0 \
  -c:a pcm_s16le -f s16le host-eight-frames.s16le
python3 tools/mu4_playback_capture_check.py RUN_DIR --require-framing \
  --reference-pcm host-eight-frames.s16le --reference-rate 44100
```

The capture checker's optional first-nonzero-frame comparison is diagnostic
only: it does not establish source-packet/output identity and never selects
a fitted lag, scaling or channel permutation. Use the stronger
[processing proof](#processing-and-conversion) to bind the first eight input
packets and locate the differing values before conversion. The large
processing corruption is corrected; the remaining small fixed-point/float
residual needs broader coverage and independent acceptance criteria, not
a guessed serial phase.

##### Native session input coverage

`mu4_packet_reads_observe.lua` captures actual data-bus reads in the original
bit-reader context over 0.8--10 seconds, bounded to `162e..1e2d` and 131,072
records. `mu4_decoder_call` entries additionally expose the existing input
base snapshot; the eight-call detailed arithmetic budget is unchanged.
`mu4_packet_session_check.py` composes the complete session/capture and
eight-call arithmetic checks with the existing reservoir/bus-byte proof.
It starts at reference offset zero, requires each call's net byte consumption
to equal the next declared ADTS packet length, and requires coverage of every
consumed byte. It never searches for a favorable offset or supplies missing
bus bytes from the reference.

All 375 native calls match the first 139,501 projected reference bytes,
SHA256 `e94a84480d87edbb2a00ed57366cf4a1a6c6ec5dde621a17199933964b46c729`.
The closed capture has 73,381 reads, of which 69,750 are within the checked
calls; the other 3,631 are explicitly reported outside-call observations.
Reservoir prefixes and actual reads cover every byte of the checked packets.
All these headers specify one raw block, and every call produces one complete
stereo conversion block. This closes input-packet correspondence for the
375-call prefix, not retained operand or complete output-store ownership.

The passive linked-buffer instrument `mu4_buffer_flow_observe.lua` observes
three initialized descriptors from 0.825 through ten seconds: producer
`1622` -> transform `1616` -> decoder input `160a`. Their upstream/downstream
links agree with the complete source-pinned refresh routine. One fresh run
records 43772 bus writes and 20766 changed values without caps or CPU faults;
identical full-word writes are counted but deduplicated from the event log.
`mu4_buffer_flow_check.py RUN --source-a00 .../InitData_R060.a00` checks the
scope, topology, domains, write order and complete observation terminal.

There are 376 pending-request changes at `15e0` in context `02068e`, versus
375 clears in context `02088b`. The last request is `00ba` at 9.700572343s.
The preceding source-node advances and consumer compaction remain live
through all 375 decoder calls; this is not a generally absent refill peer.
At the subsequent existing Execute-entry gate capture, decoder input is
`162e..1d46` (1816 words, above the `0400` admission threshold), but both
activity fields are zero. The remaining boundary is software end/drain
ordering, not lack of buffered input. Context PCs on data writes remain
correlations, not retained instruction ownership.

Later writes of `ffff` to `12fb/12fc` at about 9.859s have low-PC contexts
`002faf/002fb2`; do not reinterpret those reused addresses as new AA99
control messages. The observation closes at ten seconds, while the original
twenty-second diagnostic still ends with its mandatory audio-completion
failure. This capture does not extend audio acceptance. Use a unique run
directory and confirm child-process termination before reusing evidence
paths; the checker rejects NUL-corrupted logs and incomplete terminals.

The 142,849-byte reference retains 3,348 unconsumed bytes with nine complete
ADTS headers. Header completeness is not payload validity: the strict host
reference has eight further decoded blocks and rejects its final packet.
The tail remains outside native processing acceptance. The existing maximum
sample difference 13 / RMS 1.24083 is unchanged and is not a selected codec
tolerance. No native whole-file, analog-audio or speech claim follows.

Run the isolated `bootplaystage` diagnostic with
`-autoboot_script <absolute-path>/tools/mu4_packet_reads_observe.lua` through
its twenty-second terminal, then:

```sh
python3 tools/mu4_packet_session_check.py RUN_DIR \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00 \
  --reference-aac PROJECTED_REFERENCE.aac --reference-pcm STRICT_HOST_PREFIX.s16le
```

The diagnostic's deliberate audio-completion failure remains mandatory;
packet-prefix validation is separate. Wrong bytes, missing reads, changed
scope, caps, failed calls and partial terminals fail closed. Retain this
bounded instrument until end-of-input policy is understood; do not expand
the arithmetic snapshot budget or inject a completion to consume the tail.

##### Native session return coverage

The compact observation follows at most 512 processing calls in the first
verified AA99 session, independently of the eight-call detailed arithmetic
budget. It logs each entry/return and completed conversion block, and writes
the returned converter low words as interleaved signed LE16. These are
Execute-entry observations before the caller's store, not proof of retired
stores beyond the separately verified eight-call prefix. The terminal record
binds call/return/block counts, byte extent, raw-byte FNV hash, pending state,
cap status and observed contract errors. Capture-file save/restore is unsupported.

The completed twenty-second run records 375 calls, 375 zero-status returns
and 375 complete 1,024-sample stereo blocks, with no bad/pending/capped state
and illegal=0. `mu4_decoded_returns.le16` contains 768,000 signed words,
1,536,000 bytes, SHA256
`1871f924d9c12dca42bcced964dfe1ee0a42165574f5d4e424fbf284fdfb26dc`.
It reproduces the shorter run exactly. The six existing word, slot, time
and ownership captures remain byte-identical, as do all 918 detailed
arithmetic snapshots; the bounded coefficient/transform checks still pass.

Against the first 375 blocks of the 383-packet host reference, 391,341 of
768,000 signed samples differ, maximum absolute difference 13 and RMS
1.2408299054. The consumed-input proof above now binds this packet prefix;
these remain unfitted numerical diagnostics, not a chosen audio tolerance
or complete later output-store binding. No nonzero processing status
occurs before termination, so the rejected host packet 384 does not directly
explain this session ending at 375 calls. The host reference has eight
additional blocks; do not silently truncate that discrepancy or call it a
decoder failure. The mapped input-length cleanup still needs its legitimate
end-of-input lifecycle classified; whole-file completion, output-store binding
beyond the detailed prefix and independent codec criteria remain open.

```sh
python3 tools/mu4_decoder_session_check.py RUN_DIR \
  --source-a00 roms/5510-mu4-reference/InitData_R060.a00 \
  --reference-pcm STRICT_HOST_PREFIX.s16le
```

The checker requires finalized capture integrity, the preserved eight-call
arithmetic checks, ordered call/return/block identities and matching PCM
extent/hash. Reference comparison stops at the first failed native call;
later successes cannot collapse its missing position. Return status uses
the caller's low word, retaining upper accumulator bits only as context.
Tests reject partial/pending/capped captures, wrong ordering/times/scope,
changed counts/hash and missing successful blocks. Waveform disagreement
remains an explicit diagnostic, never fitted away or labeled conformance.

##### Dispatch and termination boundary

`decoder_dispatch` in `mu4_context_contract.py` pins 99 additional AA99 words
around the original caller and ABI. Receiver reference `15eb` selects table
`c686`; method index 13 uses the far pointer at `c6a0`, `0002,3d5d`.
Wrapper `029879` supplies index 13 to common dispatch `0298b8`; its selected
direct caller is `0293cd`. Body `023d5d` sets up the already verified
processing call/conversion loop. Its post-conversion call at `023e2e`
delegates through `029f2b` to virtual method 12; no hardware meaning is
assigned to that method here.

Before dispatch, `0293a2` refreshes the input object referenced by `15e9`.
Helper `02973c` returns `object[1] - object[0]`; the caller retains its low
word in AR2. The threshold setup at `02936d` selects AR1=0 for nonzero
`1310`, otherwise `0400`; `0293ac` copies AR1 to AR0. Opcode `f5aa` at
`0293ad` is `CMPR 1,AR2`: [TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf),
page 4-34, specifies unsigned AR2<AR0. The TC branch at `0293ae` skips to
`02940b` when that local availability test fails. Independently, the AEQ
branch at `0293b5` skips the same way when `15d9 OR context[0x16]` is zero.
Counter units, complete buffer-refresh effects and runtime branch operands
remain unvalidated; do not translate `0400` into bytes, samples or a physical
FIFO depth without evidence.

The source census covers 49,843 mapped AA99 words and 49,828 adjacent pairs.
It finds one encoded `0002,3d5d` pair (`c6a0`) and one wrapper target among
the recognized direct FCALL/FCALLD/FB forms (`0293cd`). This is quantified
raw-pattern coverage, not code/data classification, all indirect callers or
a whole-ROM absence proof. Tests mutate every pinned word, retain additional
candidate references, and distinguish threshold equality, unsigned high-bit
values, cursor wrap and the separate activity condition.

The bounded Execute observer now covers these caller paths from 9.64 to
9.86 seconds in the first verified AA99 session. A fresh ten-second run
produces 170 events, zero opcode mismatches and no trace cap. Check with
`python3 tools/mu4_decoder_gate_check.py RUN/error.log`. The checker requires
an uncensored window, verified parent terminal, resident-entry bounds and
known executed branch successors; live registers/RAM remain context only.
The `call` field counts underlying processing entries, not virtual-method
attempts. This short run does not replace the finalized twenty-second
capture-integrity or audio checks.

All 18 observed availability branches pass. Activity skips occur seven times
after processing count 373, seven after 374 and twice after 375; the first
two groups each subsequently reach another processing call. Thus inactivity
is normal between blocks, not evidence by itself of a missing event. The
last successful processing return is at 9.700534036 seconds. Two later
evaluations pass availability but skip for inactivity; the last selected
caller event is at 9.700780776. Original loader entry `002080` follows at
9.701774207. No further selected caller events occur through 9.86; the next
resident entry is at 9.868868536.

All 918 detailed snapshots and the complete 375-block return stream remain
byte-identical to the prior capture; all six transport streams match its
prefixes. The next question is the original cleanup/lifecycle decision
(`02956f`, state `373e`, control `3742`) leading to loader re-entry, not
how to force an inactive decoder to run. Neither these observations nor
the pinned cleanup route establish EOF or why only 375 blocks are decoded.

`playback_lifecycle_literals` in the context-contract report enumerates all
mapped literal occurrences of these two fields, retaining unknown patterns.
AA55 covers 10,760 words and one occurrence (the control-zero store pattern
at `0008f2`). AA99 covers 49,843 words and 43 occurrences: 25 immediate-store
patterns, 12 immediate-compare patterns, three A-load patterns and three
unclassified literals. Exactly one AA99 immediate-store pattern writes
state 4 (`020b0e`); the other stores select different states or control
values. This selects a source candidate for runtime observation, not an
all-writer closure: boundaries, indirect stores, aliases and dynamically
constructed addresses are not classified by this raw-word census.

A fresh ten-second run with `mu4_playback_lifecycle_observe.lua` records
state 4 at 0.837819684 seconds (bus-context PC `020b11`), with no subsequent
write to that field until state 5 at 9.700791097. The cleanup-context read
of control zero occurs at 9.700790964, immediately after the last caller
event at 9.700780776 and before loader entry at 9.701774207. The terminal
accounts for all ten lifecycle writes and three control writes, with
`illegal=0`; the bounded gate check also passes unchanged. All 918 snapshots,
six transport streams and decoded returns match the prior ten-second run
byte-for-byte. These are raw bus timings/context PCs, not retained
instruction ownership. State 4 becoming newly set is not the late trigger:
the remaining question is why execution leaves the processing loop and
reaches the already-enabled cleanup at this point.

The older `--playback-lifecycle` checker requires a later state-4 re-entry
as well as cleanup. This ten-second recording ends before that re-entry,
so it does not pass that separate contract; do not weaken the checker or
claim the complete repeating lifecycle from this shorter observation.

The next static boundary is now pinned as `playback_cleanup_ingress` in
the context tool (103 AA99 words). Routine `029422` fills a local frame via
`029264`; the selector at frame word 4 dispatches inputs `11`, `16`, `17`.
Input `17` copies frame word 5 to `aa1a` at `02944b`, writes reply selector
`8017` and takes the ordinary tail. Input `11` enters at `029471`, writes
reply selector `8011`, and has a later compare at `0294fd`: `aa1a == 1`
branches to cleanup `02956f`. The complete interpreter/producer is not yet
modeled. The mapped AA99 image has three literal `aa1a` references, including
an immediate-one store pattern at `027887`; this is not an indirect-writer
closure. Local-frame roles beyond the shown copies remain unassigned.

The passive `mu4_cleanup_control_observe.lua` bus taps now cover a complete
ten-second run. `aa1a` is written zero at 0.007246732 seconds, then one at
0.823387403 (context PC `02788a`). Its only read is one at 9.700790847
(context PC `029503`). The terminal accounts for both writes and the one
read, with no cap and `illegal=0`. Check using
`python3 tools/mu4_cleanup_control_check.py RUN/error.log`; the checker
requires complete full-word writes, chronological matching reads, explicit
passive scope and a non-capped terminal. All 925 output artifacts (918
snapshots, six transport streams and decoded returns) match the previous
ten-second capture exactly; the gate checker still passes.

Thus a late change to `aa1a` is not the trigger on this observed bus path.
The one-valued control was present before decoding began. The input-`11`
length and completion paths are mapped below; the remaining question is
correct end/drain ordering for the selected recording. This is
not proof of EOF, a decoder error, absence of indirect writers, or Execute
ownership at the context PCs. No control word or input selector should be
forced to extend playback.

`mu4_cleanup_ingress_observe.lua` uses bounded debugger breakpoints with
opcode guards. The staged core calls this hook at Execute entry; live
register/frame snapshots remain context only and annulment is unclaimed.
Run with the same fresh NAND fixture plus `-debug -debugger none`, an initial
`g` debugger script, and this observer as the autoboot script. Check with
`python3 tools/mu4_cleanup_ingress_check.py RUN/error.log`. The checker
requires the verified original AA99 image, first-resident-session bounds,
ordered identities/opcodes, a non-capped terminal and explicit branch
successors; it rejects the earlier 512-event prefix as incomplete.

The complete ten-second capture has 1,881 events (budget 4,096), 376 inputs
of `0011`, no selected input-17 store, and one observed cleanup successor.
The first 375 length branches select `0294ca`; the final branch selects
`0294fd`, then `029500 -> 02956f`. The final local frame still has
`f6=f7=00ba`, as do the preceding two transactions. At `0294c3`, live B
changes from `fffffffffa` (-6) on the preceding transaction to `00000000b4`
(180) on the final one. At the join, live AR1 is 6, AR2 is 1 and A is
`0000023140`; no callback site `0294e6` occurs on this final path.

The pinned source derives local full/half doublewords from `371a`, compares
the counter at `a9f6` plus an incoming signed word against the logical half,
and takes `0294ea` on BGEQ. That path computes a low-word remainder into AR1,
sets `aa1c=1`, replaces the counter with the local full value, and joins
the `aa1a` test. These source operations and the observed successors locate
the immediate stop in input-length handling, not a decoder-return failure.
The complete producer contract and whether this represents intended
end-of-input or an emulation defect remain unresolved. The input-word
units are established below; buffered decoder input is not drained by this
selected branch. Do not force a larger
length or suppress cleanup. All 925 output artifacts remain byte-identical
to the earlier ten-second capture, with `illegal=0` and the gate check green.

`playback_final_input` composes 235 original source words to distinguish
the final short-input route from ordinary completion. The BGEQ path reaches
`0294ea`, computes the remaining count in AR1, sets `aa1c=1`, stores the
local full extent into the counter, and joins `0294fb`. With `aa1a==1`,
`029500` branches directly to `02956f`, bypassing both normal reader call
`0294e0` and the ordinary input-12 completion publication `029563..029568`.
Thus the observed last request does not receive that short-read completion.

For active state 4 the cleanup path includes loader call `02959b -> 002080`.
The source after it (`02959d..0295a7`) prepares selector `16`, argument `9`
and a two-word send through `0292a7`. It is not a pre-reload flush request;
its presence after the loader call is not proof of delivery in the original
session. The alternative `aa1a!=1` path restores the counter from `3726`,
clears `aa1c`, calls API `002092`, then calls the reader before its ordinary
completion. That is not an independently established decoder-drain operation
and does not justify changing the control word.

The selected source policy is therefore explicit. Whether its 375-block
stop is correct for the recording's intended audio extent remains separate:
header-shaped buffered bytes alone do not prove that every remaining packet
belongs in the intended output. Resolve recording metadata/encoder extent or
an independent playback contract before treating this policy as a device bug.

The original R061 image does not change this pinned final-input policy.
`mu4_playback_version_check.py` checks both complete source hashes and all
235 mapped policy words: 234 are identical at the same addresses; the sole
difference is the page-3 FCALL at `02958a`, whose target changes from
`039005` to `039019`. The complete 28-word destination body through FRET
is identical after relocation. No other policy-word normalization is allowed.
Mutation tests cover every policy word in both versions and both target bodies.
The images have 5,617 raw destination differences overall, so this is not
whole-image equivalence or proof that their native runtime behavior is equal.
It rules out a direct change to these known short-read/cleanup operations,
not all possible R061 decoder, request-size or lifecycle differences.

```sh
python3 tools/mu4_playback_version_check.py \
  roms/5510-mu4-reference/InitData_R060.a00 \
  roms/5510-mu4-reference/InitData_R061.a00
```

`playback_input_units` pins 116 source words across AA55/AA99.
Descriptor arithmetic is `36b0 + index*002c`; index 2 is `3708`, and its
size pair at offset `0012` is `371a`. The local logical right shift by one
converts an even byte extent to file words. Frame word 7 flows through AR1
and adapter `0292cc` to the count argument of callbacks `0295cd/0295de`,
then APIs `2090/2086`. Their common reader advances the destination by one
16-bit word per iteration and doubles its cursor for the underlying byte
read; `2086` additionally swaps each returned word's byte order. The source
also pins the zero initializer of `a9f6` at `02788a`. This is source argument
flow, not an independent capture of every read/seek or all counter writes.

For an independent file-extent comparison, add
`--input-file tmp/mu4-dst-bypass-recorder/TEST` to the ingress checker.
The extracted file is 143,680 bytes, SHA-256
`4ba59580f8bf50b8d45d7a62d8ddf42ea94dfb7d884ff05b5092e7c13141470d`.
With a zero initial counter, the captured first 375 positive requests total
71,834 words against a limit of 71,840. Six words remain; request 376 asks
for 186, crossing by 180. This source model agrees with all 376 executed
length successors, without fitting a length or changing media. Equality
also selects the boundary. Tests reject odd/zero/oversized extents,
nonpositive/signed-high requests, missing boundaries and requests continuing
after the modeled cleanup. This establishes consistency of the word-count
interpretation, not correctness of decoder draining, the final packet,
waveform conformance or native handset audio completion.

##### Refill Control and Buffered Tail

`playback_refill_control` pins 140 source words. Sender `020643` checks
the high bit of its local reply word, then prioritizes pending `12fb`
(input `16`) over pending `12fc` (input `17`), before ordinary refill
gates. Input `11` is built at `020709`; all three use `02073b -> 029629`.
Receiver `029264` copies 16 words from data `0090..009f` to its local
frame; these mailboxes are RAM in this bench, not modeled external devices.
The input-17 builder forwards the nonzero pending value in AR1. Exhaustive
16-bit tests show this prefix cannot construct a zero-valued input 17;
that is not closure over other producers or pending-word writers.

The alternate `aa1a != 1` path is not an established drain-only mode.
When its AR2 test is nonzero, `02950a` resets the counter from `3726`,
clears `aa1c` and seeks via `2092`, then calls the selected reader. The
shared tail builds input `12` at `029542` and publishes through `029568`.
Do not manufacture a zero control/message to reach this seek/read path
and present the result as organic draining.

`mu4_cleanup_buffer_observe.lua` captures one passive data-RAM snapshot
(`0080..ffff`, 130,816 LE16 bytes) at the first opcode-guarded `02956f`
debugger hook. Its complete ten-second run has `illegal=0` and the same
925 output artifacts as the previous run, byte-for-byte. Check using
`mu4_cleanup_buffer_check.py RUN/error.log RUN/mu4_cleanup_input.le16`.
The checker binds object, cursor, limit and context to the verified first
AA99 session, preserving the observation-only/retirement-unclaimed scope.

Object `160a` has cursor `162e`, limit `1d46`: 1,816 words / 3,632 bytes
remain in the captured contiguous extent. Its high-byte-first view has
eight ADTS-header candidates at offsets 369, 741, 1113, 1485, 1857, 2229,
2601 and 2973; the other byte view has none. Signatures alone do not prove
valid payloads. More decisively, bit-reader `a854` holds 24 valid bits,
reservoir `a858/a859=e0ff/f950`, base `b332=162e`, index `b331=00ba`.
The low 24 reservoir bits supply prefix `ff f9 50`.

With `--reference-aac tmp/mu4-complete-host-reference/projected.aac`, the
checker uses the 375 captured successful processing blocks as a fixed
packet ordinal, not an offset search. Prefix plus high-byte-first buffer
matches all 3,348 projected bytes from packet 375 at offset 139,501 through
the projected end; 287 additional buffer bytes remain uninterpreted. The
reference SHA-256 is
`812882ea5da3fb9051a04e421922694366e580e75ae98df469936426eef1a873`.
Thus this buffer contains bytes matching reference-valid packets 375--382
and the already-rejected final packet 383. This is tail-content equality,
not native decoding of those packets or validation of the malformed final
packet. Prefix searches are ambiguous because the fixture repeats; no
search-selected alignment is accepted. Tests preserve reservoir low-bit
selection and reject a wrong fixed tail, failed ordinal, partial snapshot,
pointer mismatch or unaligned reservoir.

`playback_refill_completion` pins the receiver's 80 source words. Dispatch
`0207b5` selects input `12` at `020862`. A zero pending word `15e0` skips
the buffer update and produces reply `8000`. Otherwise frame word 3 is
copied to `15df`, and `02976d` adds it modulo 16 bits to field 6 of the
object referenced by `15e7`; metadata is updated and `020888` clears the
pending word. The common tail sets the reply high bit and publishes via
`020944 -> 02961e`. The small reference model covers the pending test and
cursor delta only, not all metadata or an EOF/drain protocol.

The existing snapshot has pending `15e0=00ba` and last supplied
`15df=00ba`. File-buffer object `1622` has fields 0/1 both `1e0a` (empty
by the same difference helper), with field 6 at `1d50`; the downstream
buffer still contains the tail described above. Mailbox first words are
`0080=8012`, `0090=8011`. These raw markers do not identify an individual
message or prove its payload/lifetime.

`mu4_refill_completion_observe.lua` captures paired entry/updated debugger
hooks and control-19 contexts. Its complete ten-second run has 752 events
(budget 2,048), 375 active completion pairs and zero inactive entries.
Every cursor delta and pending clear agrees with the source model; supplied
counts total 71,834 words. Control 19 first appears with argument zero and
no pending request, then with argument one and a pending 186-word request
after the final successful completion. This is cleanup-associated control,
not a claim that input 19 means DSP stop, mute or drain. All 925 output
artifacts remain byte-identical and `illegal=0`. Add
`--completion-log COMPLETION_RUN/error.log` to the buffer checker to verify
this separate capture; capped, overlapping, unpaired, wrong-cursor and
out-of-session traces fail rather than become absence evidence.

Thus ordinary completion delivery is functioning on the observed path.
The final request takes the file-length cleanup branch before its completion
publisher, leaving it pending; there is no evidence here for repairing a
missing peripheral interrupt by synthesizing input 12. Next classify the
original controller's cleanup/drain policy and its legitimate producers,
rather than extending decoding with invented completions. The current
one-pass path is explained, but its early cleanup is not yet classified
as an emulation defect or correct handset behavior. Preserve the strict
audio-completion failure and never replay this RAM snapshot.

The `playback_refill_policy_literals` census makes the policy's source
coverage explicit: AA55 has 10,760 mapped words and zero `aa1a/12fc`
literals; AA22 has 51,921 and zero; AA99 has 49,843 and eight. AA99 includes
five immediate-store patterns, one MVDK destination pattern, one compare
and one unclassified literal. Its sole immediate `aa1a` store writes one
at `027887`; its other store is frame-word payload at `02944b`. The known
`12fc` immediate stores write zero or one. The census preserves MVDK and
unknown references rather than silently counting only literal stores.
These are raw mapped-word patterns, not a code/data split or closure over
indirect, bulk-initialization, table-driven or other-image writers.

Together with the two-write/one-read runtime control capture, this classifies
the observed branch as the uploaded controller's default one-pass cleanup
path. It does not establish that this is the complete product playback
lifecycle or justify changing hardware timing to force eight more calls.
The remaining policy question needs the legitimate producer/consumer
contract or an independent handset playback reference, not another guessed
peripheral response.

`playback_pending_control` pins 215 further AA99 words around the controller,
notification and input-16 receiver. Controller frame setup `020769` enables CPL and loads request
frame word 2 into AR1. Its input `17` branch at `0207f5` stores **one** to
`12fc`, independent of the parameter; it is not a zero-valued input-17
producer. Input `16` at `0207fa` instead sets `12fa/15db` and copies frame
word 3 into `12fb`, which the previously mapped sender forwards as input-16
payload. Receiver `029458` replies `8016`, passes frame word 5 through
`0291d3`, then loads immediate `3708` into frame word 0, puts `9` in frame
word 1 and passes the reply frame word-5 pointer in A to `0299cc`.
The complete 50-word copy helper selects forward copying
when source AR4 exceeds destination AR0 and backward copying otherwise,
with a zero-count exit. Destination arrives in A; source and count arrive
in caller frame words 0 and 1. This call copies fixed record `3708..3710`
to reply frame words 5..13, not a parameter-indexed region. The acquired
image's hook at `0291d3` is `NOP; NOP; FRET`; runtime replacement and live argument ownership are not
closed. These source fragments distinguish a reply-copy control from an
established EOF/drain operation. The pending-argument producer census below
maps additional internal routes; their live incoming-command ownership and
the original controller's end-of-input protocol remain to be established
before changing cleanup.
Every pinned word has a mutation test; no message or state is injected.

The verified vector target, not that frame setup, identifies the wrapper
entrance: AA99 vector `022064` saves XPC and branches to `02:0742`, whose
register-save prefix precedes the local frame. Initializer `0278cd` supplies
far callback `02:0981` to setter `0292f0`, which stores it at `aa7e`.
Publisher `0292a7` copies its frame to mailbox `0080`, loads that pointer
and conditionally invokes it. Callback `020981` sets `12f8=1` and executes
`INTR 25`, completing a source-owned software notification chain. The three
literal `aa7e` references in the mapped AA99 image are retained in the report;
computed aliases and runtime replacement are not closed. This is not evidence
that a missing external HPI interrupt explains the final pending request.
Those source fragments do not close live message ownership or the input-16
argument's upstream producer.

`playback_pending16_producers` scans all186290 mapped words: AA55=10760,
AA22=51921, AA99=49843 and AA88=73766. All six exact`12fb` literals are in
AA99, and each is classified:

| Local site | Operation |
| --- | --- |
| `02065e` | Read pending argument into AR1 for the sender. |
| `02072e` / `0278ba` | Clear pending argument. |
| `020800` | Input16 handler copies incoming frame word3. |
| `0208e5` | Input3 handler supplies constant3 if`12fd != 0` and frame word3 is zero. |
| `020915` | Input1 handler supplies constant3 unconditionally. |

The141-word contract includes the controller's input1/2/3/4 dispatch and
both complete local constant-producing branches. Input1 clears`12fc`,
`15e0`, `15e1` and`12fd`, sets`15db=1`, initializes AA4C fields and publishes
a reply. Input3 clears`15da/12f9`; its zero frame-word3 selects`15db=1`,
then the active`12fd` branch substitutes`15de` and sets pending3. Its
frame-word4 equality test separately controls AR7.

Thus pending input16 is not restricted to an externally supplied argument:
the controller has two source-owned constant producers. The known
first-session notification inventory below contains neither input1 nor3,
so it does not exercise those routes. This is a raw mapped-word census plus
pinned local instructions, not a code/data split, computed-alias/bulk-writer
closure or runtime proof that either incoming command is issued at EOF.
The acquired receiver hook remains a no-op; no command, argument, hook or
pending state should be synthesized to turn this source finding into a
playback-completion claim.

The companion`playback_input3_senders` contract extends the source coverage
to285 words. Builder`02905d` writes BA56 as`{03, A, caller-SP[0],
caller-SP[1]}` and sends four words through`0292a7`. Its three direct calls
are`028481`, `0284a4` and`02859a`; the following words contain the target
literal, not another instruction entry.

The AA99 serial dispatcher indexes the table at`023d09` by selector minus
`29`. Selector`30` selects index7/word`023d10=8d03`, reaching`038d03` in
the current program page. This is **selector30, not recorder selector36**.
Parameter03 calls`02846f`, which requires`A825=1`, sets it to2 and supplies
builder arguments`{1,0,1}`; the
controller's frame-word3 is nonzero, so this route does not set pending3.
Parameter04 calls`028498`, whose builder arguments are`{0,0,1}`; with
active`12fd`, this reaches the input03 pending3 producer. The third caller
also supplies`{0,0,1}`, but its upstream owner remains unresolved.

The dispatcher rejects a matching previous class/selector when the selector
is above`28`. A declared external control test must respect this guard
rather than force its previous-command state; the existing status-query
transition supplies the intervening exchange in the explicit control fixture
below. These are source-derived routes, not official command names or an
automatic EOF sequence.

The `bootplaystop` bench profile now qualifies explicit serial control after
native decoder progress. It sends playback `30/02`, then status `49`, waits
for the complete status response, acknowledges it, and sends `30/04` through
the existing serial input. No firmware RAM, CPU PC or internal message is
injected. Request token `02` receives ACK `02`; the DSP status response has
its independent TX token `03`. Equating those tokens incorrectly parks the
peer and must not replace semantic response and framing checks.

The qualified run observes the source-identified route
`038d03 -> 028498 -> 02905d -> 0208ce -> 0208e5 -> 020727`.
The input03 frame contains `{03,0,0,1}`, and the input16 sender sees pending
argument `3`. Original resident AA22 is then reloaded and executes IDLE3
`028922:f5e1`; the staged core previously rejected that instruction. At
20 seconds the CPU is clean, idle at `028923`, with five decoder calls,
29 serial input bytes and six complete native response frames. Sixteen
read-only observations stay below the 1024-event cap. The AA99 `receive16`
and `hook16` sites are **not observed**; the sender alone does not prove
their execution after the overlay transition.

Use `tools/mu4_playback_serial_control_observe.lua` with `bootplaystop` and
the existing finalized recorder NAND/ROM setup, plus `-debug -debugger none
-autoboot_delay 0`. Headless host options `-joystickprovider none
-midiprovider none` avoid unnecessary physical host-device initialization;
they do not alter the emulated serial protocol. The run retains the expected
rejecting endpoint `MU4 playback coverage diagnostic: audio completion is
not validated`. Check it with:

```sh
.venv/bin/python tools/mu4_playback_serial_control_check.py \
  roms/5510-mu4-reference/InitData_R060.a00 RUN_DIR INPUT_NAND --console CONSOLE_LOG
```

The full input NAND SHA256 is
`ca1a96666fb527b41a40796c123bd6a6ec5c486b0d4cbf373d2368d58af75923`.
Under `-nonvram_save`, unchanged on-disk NAND establishes input retention,
not absence of runtime NAND writes or durable persistence. The checker
independently verifies wire bytes/checksums, opposite-direction tokens,
ordered source opcodes, uncapped read-only observations and the clean final
diagnostic. This is an explicit user control, **not automatic EOF draining,
protected-tail recovery, audio-completion acceptance or complete 5510 boot**.

`mu4_playback_serial_restore.lua` adds a separate same-process checkpoint at
1.05 seconds, after the status request but before its response and the next
control. It restores 36 exposed CPU registers and checksums over 65,408
non-MMIO data words and the 16-MiB program backing. The reference and restored
0.15-second windows each contain exactly 14 native TX-register writes: the
complete status response followed by control ACK03, with identical values,
order and timestamps. The restored continuation reaches the same pending3
sender, resident IDLE3 and rejecting terminal diagnostic as the cold control.

The external peer's ACK count belongs to the shared media-command save state,
not only recorder profiles. Binary diagnostic streams require separate host
file handling: before loading, preserve each complete reference as
`.before-load-1`, then rewind its active file and capture counters together.
Nine preserved reference prefixes (3,631,970 bytes) match the replay; all nine
complete restored captures additionally match the independent cold control
byte-for-byte (173,513,990 bytes). This covers TX, codec input, DMA source,
slot/time records, DXR writes/events, decoded-return blocks and the ideal
headphone diagnostic stream, not measured analogue output.

Restored metadata says `save_restore=same_process_rewind`; cold captures retain
`unsupported`. Existing cold-only audio checkers deliberately do not accept the
new scope. Loading into another process, loading after capture closure and
portable capture files remain unsupported; missing, closed or shorter active
captures fail closed. File-position checks are not a process-identity proof.
Files are external
diagnostics, not emulated storage; NVRAM retention under `-nonvram_save` still
does not prove runtime NAND write absence. Reproduce using the restore Lua in
place of the cold observer, then require both independent runs:

```sh
.venv/bin/python tools/mu4_playback_serial_restore_check.py RESTORED_RUN \
  --console RESTORED_CONSOLE --cold-control COLD_RUN --cold-console COLD_CONSOLE \
  --source roms/5510-mu4-reference/InitData_R060.a00 --input-nand INPUT_NAND
```

The checker rejects missing registers/checksums, non-pending checkpoints,
changed replay timing or bytes, altered wire framing, capped observations,
reference-prefix drift, any cold/restored capture difference, incomplete
continuation or unhealthy process exit. This qualifies this native control's
restoration; it does not promote automatic EOF, full audio completion, native
handset speech or complete 5510 support.

The `resident_idle3` source contract pins all 22 words at `028914..028929`
and ten caller words around `03ac0b`. The caller enables IMR bit 2 through
`AR1=0; ORM 4,*AR1`; the routine pushes IMR/IFR, selects `IMR=0004`, writes
`CLKMD=0`, polls CLKMD bit 0 until clear, writes zero again and executes
`028922:f5e1`. Its tail writes the saved IFR word through W1C acknowledgement
semantics, restores IMR and returns. Do not describe POPM IFR as restoring
the old pending-bit set.

All 186,290 mapped words in AA55/AA22/AA99/AA88 are scanned for the adjacent
`f982 8914` far-call pattern; only AA22 `03ac0b` matches. This is not
code/data classification, an indirect-call census or proof of every caller.
The runtime IDLE3 context's return words `0003/ac0d` agree with this caller.
The selected mask excludes this bench's command RX IRQ6; NMI and reset are
separate architectural inputs, not IMR bits. Combined with `UC_DSP_INT` on
the schematic, this identifies a controller interrupt boundary worth testing,
not permission to infer its timing from a serial packet or rename `30/04` as
an official power-off command. A declared logical pin fixture now qualifies
the native wake continuation; the normal MSP430 waveform and ordinary
EOF/draining remain separate unknowns.

#### Declared resident INT2 wake fixture

`mu4_resident_int2_observe.lua` asserts the schematic `UC_DSP_INT` input
through the bench input port, without writing firmware state. Its paired
`mu4_resident_int2_control.lua` observes the same boot without an edge.
Both start at 11 seconds in resident `028923`, IMR `0004`, IFR `0c38`.
The control remains there with zero IMR/IFR writes in the observation window.
The pulse is sampled at 11.016666667 and released at 11.066666667 seconds;
this is a declared 50 ms logical assertion, not a measured MCU waveform.

The positive branch executes the original INT2 handler, restores the model
PLL selection, reaches `028925:8a01` / `028926:8a00`, and returns to
`03ac0d`. The data-write tap observes IFR `0c38` written with saved word
`0438`, then IMR restored to `0ac5`. The next Execute-entry observation
has IFR `0808`: newly pending bits survive, including the still-asserted
INT2 level. A later interrupt runs before the caller observation, so that
observation is not an immediate post-POPM pending-bit snapshot.
The worker reaches IDLE1 at `0298ea` cleanly and later enters resident
IDLE3 again. The bounded register observer records 258 writes below its
1024 cap; path observations record five events below their 32 cap.
Execute-entry context is not a guarantee of pre-instruction SP state.

Check both branches, original-image contract, retained whole input NAND and
the unchanged rejecting audio endpoint with:

```sh
python3 tools/mu4_resident_int2_check.py SOURCE_A00 INPUT_NAND CONTROL_RUN PULSE_RUN
```

Each run's console is `RUN-launch.log`. Unchanged `-nonvram_save` input
files do not prove runtime NAND write absence. This paired fixture proves
the declared wake input's native continuation, not automatic EOF, complete
audio, normal MSP430 control or full 5510 graphical boot.

`mu4_resident_int2_restore.lua` qualifies a same-process checkpoint at
10.95 seconds, before a declared INT2 assertion. It compares 36 exposed
CPU registers, a checksum of all 65,408 non-MMIO data words and the complete
16 MiB program backing before and after load. Both legs assert the logical
pin at 11.000000000 and clear it at 11.050000000 seconds; 282 IMR/IFR
writes match in order, value and absolute time within the 0.25-second
window. Both return to the native worker's clean idle. All nine preserved
diagnostic streams match their restored prefixes (93,569,510 bytes total).
This does not claim independent final-file equality with a differently
timed cold pulse, cross-process capture restoration or normal MCU timing.

The command peer's saved history now holds 4,096 words instead of 64:
late checkpoints include more than one frame's historical output. This
is a bounded diagnostic history, not an emulated serial FIFO. Overflow
still fails with explicit TX/decoded counts; no history is silently truncated.

```sh
python3 tools/mu4_resident_int2_restore_check.py SOURCE_A00 INPUT_NAND RESTORE_RUN
```

The checker requires the original image contract, complete retained input,
ordinary native serial-control acceptance and rejecting audio endpoint,
matching checkpoint state, actual board-pin edges, replay publications,
all nine preserved capture prefixes and an uncapped completion marker.

#### Resident command interface after wake

`mu4_resident_status_observe.lua` adds a declared serial-peer status query
after the paired INT2 fixture, using the existing selector `49` packet with
request token `04`. The bench input `MCU status request` queues this packet
through the ordinary timed command receiver; it does not write DSP memory,
select a PC or claim a recovered MSP430 command policy. Its one-shot request latch is
saved with the other playback peer state. The matched
`mu4_resident_status_control.lua` sends the same query without an INT2 edge.

Both request the packet at 11.2 seconds and begin ingress at 11.201.
After wake, all eight bytes are consumed (37 total fixture bytes), native
firmware emits ACK `7f 04 55` and a complete XOR-checked status frame:
`1e 05 aa 01 71 01 00 00 02 c2 55`. The request token `04` and native
TX token `02` belong to opposite directions; the latter follows the
resident reload and must not be forced to match the request token.
Without wake, only the first additional byte is offered (30 total), no
ACK04 or response follows, and the DSP remains at resident `028923`
through the 20-second diagnostic. This is a bounded negative control,
not a permanent-stall or all-wake-source claim.

```sh
python3 tools/mu4_resident_status_check.py SOURCE_A00 INPUT_NAND CONTROL_RUN WAKE_RUN
```

The checker composes the existing native serial/wake contracts with exact
query byte inventory, acknowledgement ordering, reply framing/payload and
the sleeping control's lack of a response. Both legs retain the strict
rejecting audio endpoint and unchanged complete input NAND. This proves
the original resident command interface is usable after the declared wake,
not normal MCU policy, automatic EOF, complete music or native phone speech.

`mu4_resident_status_restore.lua` separately checkpoints at 11.205 seconds,
after five of the eight status bytes have entered, but before native ACK04
or response6. It composes the same declared INT2/status inputs with the existing
serial restore observer; no DSP memory or counters are changed. All 36 exposed
registers, 65,408 non-MMIO data words and the 16 MiB program backing match
exactly after load. Both branches emit the same fourteen DXR writes and
timestamps over 150 ms: ACK `7f0455`, then native status frame
`1e05aa017101000002c255`. The firmware eventually returns to resident
`028923`, not permanently to worker `0298ea`.

The nine preserved diagnostic prefixes match (95,886,186 bytes), and all nine
final files match the independent cold status run exactly (173,513,990 bytes).
The complete retained input NAND is unchanged. Capture:
`tmp/mu4-resident-status-restore`; cold control: `tmp/mu4-resident-status`.
Use `mu4_playback_serial_restore_check.py --resident-status` with its existing
source/input/cold-control arguments. The original default pre-control restore
contract remains distinct and unchanged. Both paths deliberately retain the
rejecting audio-completion endpoint; this is native serial/device restoration,
not normal MSP430 policy, automatic EOF or complete music acceptance.

The post-wake caller is not another receive wait. The separately pinned
52-word `resident_wake_tail` contract loads `07d0`, calls `03acdf`, then writes
zero to `b558` at `03ac11`. Helper `03acdf..03acf1` passes stack literal
`000a` to resident `00:2098`, retains its result in AR1, reads `b582` into
AR0, and performs unsigned `CMPR GT,AR1`. Its `f820` is **BC NTC**, not
BC TC: it skips the store when AR1 is not greater than the old value.
Otherwise it publishes AR1 to `b582`. There is no backward branch or
receive operation in this helper: the effect is a countdown lower bound,
not a synchronous delay or a cap on the old value.

The separate AA55 bootstrap supplies the nested call's source: `002098`
branches to `00352c`, whose five words clear SXM, load AL into A, repeat
`SUBC SP(2),A` sixteen times and return. The retained low word is the
unsigned low-16-bit argument divided by the caller's ten. Consequently the
bounded software model is `max(old_b582, low16(argument) / 10)`; the native
call's `07d0` produces `00c8`. All 65,536 argument values are checked against
the model at zero, quotient-adjacent and maximum counter values. These
host arithmetic checks do not replace the executable CPU SUBC/repeat
fixtures or measure physical countdown units.
`SP(2)` assumes the CPL-addressed ABI, present in the native wake context's
`ST1=6113`; this is not a context-free interpretation of the direct operand.

The same contract pins the consumer at `03ac03`: `b582 != 0` branches
to `03ac11`, bypassing the resident deep-idle call. Worker prefix
`03a8c9..03a8d7` decrements a nonzero counter, then tests `bb94 & 0002`;
when set it adds one back (including changing zero to one). This is an
asynchronous idle-inhibition countdown, not a receive-completion latch.
The worker's cadence and ownership of the inhibiting flag remain unknown.

A read-only data-tap window after the declared wake observes `b582` being
set to `00c8`, followed by `b558=0`; ordinary activity subsequently
decrements `b582` to `00c7`. This supports asynchronous bookkeeping, not
another missing MSP430 acknowledgement. The countdown's physical units
and its official lifecycle meaning remain
unqualified. Source ownership comes from the pinned upload,
not data-tap context PCs or earlier same-address overlay observations.

The bounded native playback fixture now observes 402 complete software
notification groups: publication `0292c1`, callback `020981`, vector
`022064`, wrapper `020742`, and request-frame read `020780`. All groups
retain the mailbox value and callback `0002:0981`; flag `12f8` follows
`0,0,1,1,0` across those entries. Request IDs are `12` (375), `04/05`
(11 each), `19` (2), and `14/02/15` (one each). Neither request `16/17`
nor their selected handlers occurs in this first-session observation.
This is fixture/window evidence, not an all-producer absence proof.

Use `tools/mu4_notification_observe.lua` with the isolated `bootplaystage`
NAND/ROM setup, `-debug -debugger none`, and a ten-second stop bound; the
observer starts debugger execution itself. Check the log with
`python3 tools/mu4_notification_check.py RUN/error.log`. It rejects censored
chains, changed scope, opcode/address disagreement, truncated captures,
unverified overlays and out-of-session entries. Program-word guards must
retain the upper page above `8000`; only the lower 32K uses OVLY's shared
backing. The observer logs the actual opcode rather than hiding a mismatch
behind a per-site opcode predicate.

The 2,010-event capture completes at 10.016667 seconds, below its 4,096-event
cap, with `illegal=0` and verified first-session completion. Its 1,536,000-byte
returned-sample file retains SHA256
`1871f924d9c12dca42bcced964dfe1ee0a42165574f5d4e424fbf284fdfb26dc`.
The ten-second observation does not satisfy the separate twenty-second
whole-playback terminal or establish retired operand ownership, annulment,
physical interrupt latency or audio completion. Ordinary software
notification is functioning here; resolving end-of-input policy still needs
the legitimate control lifecycle, not a synthetic HPI edge.
Retain this observer while cleanup policy is unresolved; retire its PC hooks
when a smaller accepted boundary regression can replace them.

The directory's adjacent `ADDM #1,*SP(2)` / `LD *SP(2),A` dependency
requires same-address forwarding. [TI SPRU131G](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
section 7.3.2 specifies an internal-bus bypass for an on-chip read from a
location with a pending write. The staged model now forwards retained
RMW results into supported Read-stage RAM consumers without reading stale
RAM, moving the EB write, or publishing ADDM flags before Execute.
Register/peripheral reads are excluded; unrelated addresses and spaced
consumers retain real data-bus reads. Physical bank contention, other
producer families and measured DA150 bus phases remain unvalidated.
The isolated `tms54test -bios rmw-forward` fixture covers 768 cases with
save/replay, boundary values, SXM/OVM combinations, two program pages,
compiler/noncompiler addressing, same/different addresses and zero/one
gap words. Existing PSHD and descriptor-copy expectations now reflect
forwarded data rather than treating raw first-half DB ordering as proof
that consumers must see old RAM.

The defect's read-only witness is stack address `15de`: cycle 192385 read
zero for the increment, while cycle 192386 read zero for the follower
before writing one. It produced `MMCUSI16`, displaced extension bytes
and malformed lengths, then zero-length loader reads and a 37,019,530-read
retry tail. That tail is superseded, not a current startup result.
Program-Fetch A snapshots alone never proved a consumed descriptor index;
real data reads established descriptor zero before later index-128 retries.
Original `0e41..0e48` selects stack `1200`, adds `03ff`, and aligns it;
the loader later selects `3aea`. Immediate
`MPY #lk,A`, `f066` at `00:2eda`, now has retained literal/T,
arithmetic and producer/restoration coverage. STLM A,T,
`880e` at `00:2ed8`, now has publication, saturation and consumer/replay
coverage. Unsupported Decode can stop older Execute; the runtime PC alone
does not prove retirement of instructions still in the pipeline. Repeated
`ST #lk,*AR1+`, `7691` at `00:08ea`, now has short-memory store
retention, two-clock cadence and interrupt/restoration coverage. BANZD,
`6e87` at `00:0a50`, now has short-indirect delayed-branch coverage.
Short-memory repeated
`SUBC Smem,A`, `1e02` behind `RPT #15` at `00:352e`, is now covered.
The runtime reaches a different program range; that is not proof of the
complete loaded image or startup acceptance.
`ADD *(0016),0,A,B` at `00:319c`, encoded
`6ff8 0016 0d00`, now has staged arithmetic coverage; its memory source
is mapped AR6. The preceding
`LD *AR1(0004),T` at `00:3194`, encoded `30e1 0004`, now has
Read-publication and dependent-multiply coverage.
The preceding `STL A,-1,*AR1(0003)` at `00:313e`, encoded
`6fe1 0003 0c9f`, now has shifted-store coverage.
Unsupported Decode can stop older Execute work, so
later PCs alone are not retirement proof. The
initializer loop's complete runtime effects have not yet been accepted.
The terminal bootstrap-entry failure remains coverage evidence,
not a passing recorder gate.

Reproduce in a new disk-backed run directory with a copy of the retained
`mu4nand_13/nand` seed under `nvram/mu4nand_25/nand`, then run:

```sh
/path/to/mame mu4nand -bios bootrecstage -rompath /path/to/seed/roms \
  -nvram_directory "$PWD/nvram" -video none -sound none -nothrottle \
  -debugger none -log -nonvram_save -seconds_to_run 60
```

Do not reuse the source seed as writable NVRAM. The instruction contract at
`00:30c6` now has short-indirect DST address/modification, two-write publication
and replay conformance across all twelve short modes. The next instruction
at `00:30c7` has long-offset LD source/address and replay conformance.
Short-indirect DLD at `00:30d3` has retained paired-read, pointer-modification,
accumulator-publication and replay coverage, including all short modes and
the extended forms. Long-offset BANZ at `00:30df` has retained offset/target,
resolved-address predicate, pointer-modification and replay coverage.
Immediate SUB at `00:3216` has retained-literal, arithmetic-flag and
replay coverage, including shift-16 and preceding-load composition.
Register SUB at `00:321c` has source/destination, signed-shift, arithmetic-flag
and replay coverage, including ASM forms and preceding-load composition.
Register LD at `00:30f7` has shifted-source, publication and replay coverage,
including ASM forms and carry preservation. Long-word DADD at `00:30fa`
has paired-read, source/destination, arithmetic-flag and replay coverage
for ordinary and C16 modes. LDU at `00:3114` has retained-operand,
unsigned-extension, address-ownership and publication/replay coverage.
Memory MPYA at `00:3520` has retained-source, signed A-high multiplier,
Read-owned T, Execute-owned B/flags and replay coverage. Memory LD
shifted by 16 at `00:3521` has retained-source, SXM extension,
accumulator publication and replay coverage. Unsigned memory multiply
at `00:3523` has retained Smem/T, unsigned-product, FRCT/OVM,
destination/flag and replay coverage. The extended Smem instruction
at `00:3291` has checked extension decoding, retained-address/source,
SXM/shift/status and replay coverage. The shifted store at `00:313e`
has destination ownership, Execute-live source, saturation,
low-word publication and replay coverage. The load into T at `00:3194`
has retained address/source, T publication, dependent-multiply
ordering and replay coverage. The ADD at `00:319c` has mapped
source sampling, shifted operand, separate source/destination accumulator,
arithmetic flags and replay coverage. Single-step SUBC has retained
source/address, conditional-subtract/shift, carry and replay coverage.
Its ALU-difference overflow projection is explicitly unvalidated at the
silicon boundary. Repeated SUBC now has sixteen-step quotient/remainder,
repeat ownership, IRQ ordering and restoration coverage. BANZD at `00:0a50`
has retained target/count, pointer-update ownership, delay-slot
execution and restoration coverage. The repeated store at `00:08ea` has
literal/address retention, count+1 writes, per-iteration pointer
ownership, pending-IRQ ordering and restoration coverage. STLM A,T at
`00:2ed8` has source/saturation ownership, T publication,
dependent-instruction ordering and replay coverage. Zero-gap T consumers
are explicitly outside TI's allowed STLM latency; the model does not claim
hardware forwarding. Immediate MPY at `00:2eda` has retained literal/T,
FRCT/OVM, destination/overflow and replay coverage. STLM B,BRC at
`00:32ec` now has 3,072 replay cases covering source/saturation ownership,
Execute BRC publication and retained LDM reads. The context tool pins
`891a f495 f072 32f4 1386 f678 f3e8 f1a0 8196`: the NOP before
RPTB satisfies SPRU131G table 7-3 category 9, and the block spans
`00:32f0`--`00:32f4`. Zero-gap LDM results are a declared pipeline
projection, not a silicon forwarding claim. Straight-line RPTB now has
6,144 count/data/replay cases, 24 IRQ/NMI replay cases and sixteen
endpoint/exclusion cases. Delayed/nested/control-changing blocks and
exact silicon phase/terminal-counter behavior remain unaccepted;
passing the idle-call fixtures alone is insufficient to enable native
staged startup.

The original five-word byte-swap body now has isolated staged execution
and 1,536 save-state replay cases, plus another 1,536 cases for two
consecutive zero-gap bodies, supplemented by the bounded RPTB matrix;
the exact coverage and reproduction command live in
[the native loader contract](rom4_dsp_loader.md).

The context tool also pins the 12-word AA55 reset sequence at
`00:ff80`--`00:ff8b`: PMST=`ffc8`, AR0=`0060`, `RPT #ff20`,
`MVPD 0060,*AR0+`, PMST=`ffe8`, then `FB 00:0e41`. This is a
65,313-word program-to-data copy, not a NOP delay or constant fill.
TI SPRU131G section 6.7 specifies count+1 executions and implicit increment
of the absolute program source; the encoded AR0 postincrement advances the
destination independently. Both ranges are `0060`--`ff80`, ending with
AR0=`ff81`. The original upload supplies 10,749 words in this range;
54,564 source words are not assigned by the upload inventory, whose absence
does not establish their runtime values. Do not replace the copy with
guessed zero-fill or program/data aliasing.

The next staged contract therefore needs both long-immediate repeat and
program-source MVPD, with retained operand words, count+1 retirements,
per-iteration source/destination progression, exact continuation and
save/replay. Interrupt suppression begins when RPT is decoded and includes
NMI until the repeated instruction completes (SPRU131G 6.7). The repeat's
first and steady-state bus phases remain partial. The bounded staged repeat
cases and pending-IRQ/NMI replay are described in the CPU contract.
Observation-only `bootrecstage` taps verify all 65,313 program reads and
data writes, in order with matching values, before the original serial entry
is reached. `mu4_staged_bootstrap_copy: PASS` deliberately reports
`native_startup_complete=0`; this does not close recorder acceptance.
The new diagnostic is intentionally not a passing save acceptance gate.
The independent INTM-change protection fix passes pending-interrupt load,
multiword-store and return fixtures, but the metadata diagnostic still has
cursor 27, RX/TX 36/47 and no metadata delivery. It does not close this
general timer/write/pipeline boundary.

The `bootrecsave` reproduction reaches original resident
startup and control return, but stops with RX-ready set, zero
tracks and PRD `1`; the metadata packet has not been delivered. This remains
a CPU interrupt-recognition/pipeline-drain question, not evidence of metadata
rejection. The independently verified stopped-PSC/read-only-TCR corrections
did not change serial progress: RX/TX remained 36/47, with three responses,
two ACKs and completion `8009`. Sampled handler PC and TIM depend on the
observation phase; the replay below is the authoritative snapshot. The
atomic `bootrecsave` recorder acceptance checker still fails; passing timer-register conformance
must not promote it.
A blanket return-instruction guard is not established by
[SPRU131G](https://www.ti.com/lit/ug/spru131g/spru131g.pdf): section 6.10.7
explicitly protects SSBX/RSBX INTM and their following instruction, while
section 7.2 examples 7-9/7-10 describe RETE/RETED clearing INTM during execute.
Do not transfer that instruction-specific guarantee to RETE, RETF or FRETE,
or choose a fixed post-return delay to make this firmware advance. The next
CPU experiment must distinguish recognition, already-decoded instruction
drain and memory-mapped timer-write visibility using firmware-free fixtures.

The ISR's return path is also checked independently of its endpoint PC.
The vector saves XPC, the handler reserves a frame word, and its stack store
selects the original `FRETE` stub at `02:4044`. A bounded read-only trace
shows the first RET taking that stub, then restoration of XPC `2`, PC
`98de`, and the pre-interrupt SP `1272`. The following TINT repeats the
same frame; no cumulative stack loss or substituted return target is seen.
The temporary stack probe is removed after harvesting this conclusion.
The firmware-free `probe-c54x-timer-boundary` reproduces starvation with
just indirect PRD/TCR stores and a `PSHM XPC; FRETE` ISR; its long-period
control reaches the IMR mask. This excludes NAND, codec and serial work as
necessary causes of the stall, but is not proof of the exact physical
interrupt latency. See [the CPU contract](rom4_dsp_loader.md) for scope.

On the current build the short-period synthetic case stops at `01:0608`
with SP `1000`, IMR `0008`, IFR `0008` and its completion marker clear.
That PC is the `LD #0,AR1` immediately after the indirect TCR reload,
before the indirect IMR mask at `01:060a`; the ISR restores the stack but
the mask's address setup never completes. The PRD `0100` control reaches
`01:060f`, sets the marker and clears IMR bit 3 while IFR bit 3 remains
latched. Thus the discriminator is execution reaching the mask, not
whether the timer flag can latch while masked. These are observations of
the current core, not physical timing acceptance criteria.

The probe also substitutes a direct `ANDM #fff7,*(0000)` at `01:0608`
for the pointer setup and indirect mask, retaining the completion marker's
address. Both short-period cases leave the marker clear; both long-period
controls set it and mask TINT. In the direct short-period observation the
CPU is at vector `01:004c` with SP `0fff`. This excludes the extra pointer
setup instruction as the sole cause, without establishing a physical
interrupt delay or changing the CPU implementation. The diagnostic now
reports four cases and still declares `fidelity_acceptance=0`.

The CPU's MOD4/MOD7 addressing modes require reverse carry/borrow when
subtracting/adding AR0, not linear arithmetic ([TI SPRU131G](https://www.ti.com/lit/ug/spru131g/spru131g.pdf),
section 5.5.3.5 and table 5-4). Linear `*AR+0B` indexing made the codec
buffer-reordering routine's `DST` at `03:c85d` write to address zero,
clobbering IMR and leaving subsequent serial data unread. Correct reverse
carry restores native receive progress without firmware changes.
`check-c54x-core` covers 2,688 MOD4/MOD7 MAR/load/store cases and the
reference guide's 16-address sequence in both directions. The dedicated
mask/idle probes and stale unread-character assertion are retired.

The current control endpoint is `03:bc4d`, IMR `0ac1`, IFR `0438`,
lifecycle 9, mode 3, continuation 8 and zero tracks. The application
dispatcher `02:84cd` is called at `03:94a2`. The ordinary loop can reach
that call directly; the `fd85` completion handshake at `03:9499..94a1`
is not an unconditional prerequisite. The control run observes 40 application
calls, one successful dequeue and zero entries at the two processing waits.
The unresolved contract is the original sender's command sequencing and
the legitimate updates to the previous-command fields, not absent receive
delivery or a demonstrated processing stall. Recover that contract before
changing packet semantics or claiming recording finalization.
A literal-word scan of all 57,875 extended words in the original AA88
upload finds four `3766/3767` operands, all in this dispatcher: two reads
and its two previous-command updates. This is direct-reference coverage,
not proof that no indirect writer or initialization record exists.
Reproduce it with `make check-mu4-bootstrap-original
MU4_BOOTSTRAP_SOURCE_RUN=tmp/native-gates/mu4-retained
MU4_BOOTSTRAP_BIOS=bootreccontrol`.
Its receive PASS label proves native packet receipt and acknowledgement,
not successful control-handler execution or recording.

Isolated replays after the MMR bus, INTR SP ordering and XC protection
corrections distinguish this duplicate filter from the save fixture's CPU
boundary. `tmp/mu4-recorder-control-xc/error.log` receives
selector `36`/parameter `4` at 11.406 seconds with previous class/selector
`1/36`; native receipt/acknowledgement passes in this atomic diagnostic, while recording completion remains
unclaimed.
With input-derived CLKMD rates and the C5416-compatible reset profile,
`bootrecsave` receives control at 10.086 seconds with previous selector `49`.
The resident image returns at approximately 10.718 seconds and completion
reaches `8009`. The 20-second endpoint is `02:3d79`, ST1 `2900`, IMR `0ac9`,
IFR `0c78`, cursor 27/50 and RX-ready set: 611 application calls, two dequeued
messages, 23 processing-wait entries and 611 processing-return observations.
RX/TX are 36/47, with three responses and two ACKs; TIM/PRD/TCR are
`0000/0001/0000`. It exits 3 because metadata has not been delivered.
These counts and endpoint PC are observations, not acceptance constants.
Correcting the reset/PLL execution rates changes progress and phase but does
not remove the warm timer/receive stall. The source and isolated NAND copy
compare byte-for-byte with NVRAM saving disabled and no firmware-state forcing.
The control fixture's working warm dispatch does not close the save fixture's
timer/interrupt recognition and drain boundary.
Packet-delay tuning is not a substitute for that CPU contract.

Resident command and loader addresses are reused by the recorder overlay;
their resident trace counters must stop when that overlay takes ownership.

Create a fresh firmware-owned source first:

```sh
make check-mu4-retained-original MU4_RETAINED_RUN_DIR=tmp/native-gates/mu4-retained
make check-mu4-bootstrap-original \
  MU4_BOOTSTRAP_SOURCE_RUN=tmp/native-gates/mu4-retained \
  MU4_BOOTSTRAP_RUN_DIR=tmp/native-gates/mu4-recorder \
  MU4_BOOTSTRAP_BIOS=bootrecsave
```

Use new output paths for subsequent runs; extraction does not overwrite them.
Without explicit output paths both gates allocate unique directories under
the disk-backed `NATIVE_SCRATCH_ROOT` (default `tmp/native-gates`).
`MU4_STORAGE_SECONDS` defaults to 1,800 emulated seconds: the one-millisecond
upload cadence plus two complete readbacks at the compatible reset divider
outlast the former 1,200-second reset-fixture limit. This is an outer harness
deadline, not a timer/serial/NAND calibration change.
Fresh original provisioning, reset/reload and fresh-process status service
pass with this clock profile. The resulting NAND is byte-identical to the
previous firmware-created source, without donor provisioning or a runtime
snapshot. The independent complete-startup `bootstatus` gate also passes.
The atomic `bootrecsave` metadata diagnostic fails at the stated receive boundary;
this retained negative control does not contradict the staged `bootrecmeta`
metadata/persistence acceptance above. Use `bootrecord` for the initial
recorder-loader diagnostic.
This runs in an isolated NAND copy, forbids NVRAM saving and verifies that
the source disk file remains unchanged. `bootstatus` supplies the independent
complete-startup lifecycle observation.
Use `MU4_BOOTSTRAP_BIOS=bootrecordram` for the provisional shared-RAM probe;
it passes startup, native response and execution checks. It is not a
passing recording acceptance gate.

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
uncompressed terminal save streams match across all registered
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
status and pin-receive checks, and all 1,040 registered emulation-state
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
also checks 42 stereo tone blocks (5,376 buffer samples) and 12 signed
phase wraps against independent table/phase/scaling calculations. Block
counts are observations of this fixture, not a physical timing contract.
The codec input remains a declared converted-sample fixture. These checks
prove native DSP processing and generated tone buffers after original
startup, not accepted media decoding, analog output or native speech.

The same profile continuously compares every McBSP0 transmitter word with
the independently pin-decoded AIC23 DIN word. A four-word bounded comparison
queue rejects overflow, unpaired codec words, channel-order errors and
sample mismatches; it is registered for save states and cleared on reset.
The complete 20-second observation matches 1,744,936 words, including
5,160 nonzero words, with no pending word at its endpoint. Acceptance
requires coverage beyond the old 1,024-word prefix, nonzero output and at
most one final launched-but-not-yet-sampled word; exact volume is not a
physical clock-rate oracle. This verifies sustained digital transport
during native processing; the source-selection proof below connects tone
samples to that transport. It does not model the DAC/analog output.

Original worker `02:955b..9588` writes DMA source-reload index `32`, selects
`ba79` or `baf9`, calls the generator and flips `bb86`. Each bank contains
64 left samples followed by 64 right samples. A passive data-read observer
uses side-effect-free DMA channel-3 **active source-read** inspection to
distinguish DMA reads from CPU operands and other channels, without changing
the DMA register index. The older `source_matches` inspector identifies an
enabled programmed source only; an address match alone cannot attribute a
bus read to that channel. `source_read_matches` is scoped to the actual
synchronous source read and is false during the destination write and
outside the transfer. Device conformance checks cover ordinary reads while
enabled, wrong channel/space/address, cancellation and a pending-transfer
save/restore. The symmetric `destination_write_matches` inspector is true
only inside the actual destination bus write, after source-read ownership
has ended. Executable data/program-write checks reject wrong channel,
space and address, exclude ordinary fixture writes, require matching
source/destination transaction counts, and cover cancellation and pending
save/restore. Neither inspector changes the DMA index or persists a bus
owner into a save state. I/O-space destination ownership is not independently
exercised by this fixture. Use this explicit write owner when binding
playback source reads to DXR; matching a word or timestamp alone is not
ownership evidence. The complete playback capture now validates this
source-to-DXR leg. Bounded internal load/completion attribution is covered by the
[serial-epoch contract](aac_validation.md#actual-shift-buffer-and-din-epochs);
full-session DXR-to-TX accounting remains separate.
Every uncancelled generated word matches the independent
tone calculation in alternating left/right source order. Bounded queues
then require every observed DMA source word to match McBSP TX, followed by
the existing independent codec-DIN comparison.

The command's original cleanup matters: `03:b056/b058` zero both tone
banks while the last DMA block is in flight, before `03:b05e` writes mode
zero. The observer recognizes only those exact CPU store boundaries and
zero values, updates its expected source image and accounts for samples
cancelled before DMA read. This fixture generates 5,376 words: 5,180 are
independently verified through DMA and 196 are cancelled by original
cleanup. The complete source stream has 1,744,937 reads, 1,744,936 completed
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

SPRU302B section 2.3.7.3 additionally defines transmit overwrite: a new DXR
write before DXR-to-XSR handoff replaces the buffered word rather than
stopping the device. The supported external-frame implementation preserves
any word already in XSR and does not invent another readiness edge. Three
pin-driven cases exercise overwrite before the first frame and overwrite
of the next word while XSR is active, checking exact serial bits, completed
words and IRQ counts at 8/12/16 bits. The old abort is reproduced in
`tmp/mcbsp-overwrite-red/output.log`; the full original-storage gate requires
`mu4_mcbsp_overwrite: PASS` alongside its existing underflow/overrun and
save-state gates. The complete gate passes with
`MU4_STORAGE_RUN_DIR=tmp/mcbsp-overwrite-green`; its isolated `error.log`
retains the original storage, native entry, serial/DMA and error-state
results. This extends generic serial error behavior, not native
recording completion or speech.

SPRU302B sections 2.3.6.2/2.3.7.2 define unexpected receive-frame sync:
with RFIG clear, discard the partial word and pending RBR, retain unread DRR,
restart reception at the new frame, and set sticky RSYNCERR (SPCR1 bit 3).
With RFIG set, ignore the early frame and finish the original word. RSYNCERR
is software-writable, clears on receiver reset, and drives the receive IRQ
in RINTM mode 3; DRR reads do not clear it. Six external-pin cases cover both
behaviors at 8/12/16 bits, retained DRR, software set/clear and reset. A separate
save/replay case restores a half-received replacement word with RSYNCERR and
its IRQ held, checking exact payload replay and no duplicate IRQ edge.
The full original-storage gate requires `mu4_mcbsp_receive_sync: PASS` and
`mu4_mcbsp_receive_sync_restore: PASS`; both pass in
`MU4_STORAGE_RUN_DIR=tmp/mcbsp-rsync-restore`. These mid-word tests do not
establish terminal frame packing, which has separate coverage below. This
is generic McBSP recovery evidence, not recorder completion or native speech.

Receive frame packing follows SPRU302B section 2.3.5.3 and figures 2-23/2-24:
the minimum frame period is the number of bits in the frame. With data delay
`D`, an expected FSR can arrive while the previous final word has `B <= D`
bits left. The receiver finishes that tail and retains the next frame with
`D-B` delay clocks remaining, without RSYNCERR or discarding either word.
RFIG ignores premature sync, not this legal overlap. An FSR one clock before
the minimum period instead uses the documented recovery/ignore contract.
The pin fixture covers 36 legal three-word streams (8/12/16 bits, delays
0/1/2, gaps 0/1, both RFIG values) and 18 premature-sync controls, checking
exact words, ready edges and error status. Save/replay retains a pending
next frame while the previous word's last bit is still unreceived.
The original-storage gate requires `mu4_mcbsp_receive_packing: PASS` and
`mu4_mcbsp_receive_packing_restore: PASS`. Cases yield between scheduler
callbacks so CPU interrupt synchronization drains normally; interrupt delivery
is not suppressed. Multiple pending receive frames remain unsupported.
The full gate passes with `MU4_STORAGE_RUN_DIR=tmp/mcbsp-rxpack-yield`,
retaining the original storage, native-entry and DMA checks. This validates
generic serial reception, not native recorder completion or speech.

The TX sync-recovery contract is distinct: SPRU302B section
2.3.7.5 and figure 2-36 show an early FSX with XFIG clear restarting the
current XSR word from its first bit, while a newer word already queued in
DXR remains queued. It sets sticky, software-writable XSYNCERR (SPCR2 bit 3),
cleared by software zero or transmitter reset; XINTM mode 3 selects its IRQ.
XFIG set ignores the pulse. The figure explicitly shows partial B followed
by complete B, then queued C, without an extra XRDY transition at restart.
`tx_frame_w` restarts the preserved XSR directly, not via `load_shift`, which
consumes DXR and would replace B with C. Six pin-driven cases check
partial-B/full-B/C ordering at 8/12/16 bits, frame-ignore as a negative control,
sticky status/software/reset behavior and readiness-edge counts. A save/replay
case retains a half-shifted restarted XSR, queued DXR and held error IRQ,
checking the remaining physical bits and no duplicate IRQ edge. The full
original-storage gate requires `mu4_mcbsp_transmit_sync: PASS` and
`mu4_mcbsp_transmit_sync_restore: PASS`. Terminal frame packing has separate
pin-level coverage below; XINTM modes 1/2 and
internal-clock sync recovery remain unvalidated.
The complete gate passes with
`MU4_STORAGE_RUN_DIR=tmp/mcbsp-txsync-restore-final`, retaining the original
storage, loader/native-entry, DMA and existing serial error-state results.

Transmit frame packing uses the same minimum frame period from SPRU302B
section 2.3.5.3, but not the receiver's delay counter convention. Zero-delay
TX emits its first bit asynchronously at FSX; nonzero delay includes the
first shifting edge. An expected next-frame sync that overlaps the final XSR
tail retains both that tail and the queued DXR word, then starts the next
frame with the unelapsed delay. It does not restart XSR or assert XSYNCERR,
even with XFIG set. The fixture drives FSX before the selected shift edge and
checks the bit count after every clock as well as complete payloads and
ready IRQ edges: 36 cases at 8/12/16 bits, delays 0/1/2, gaps 0/1 and both
XFIG values. A save/replay test retains the final old bit, queued DXR and
pending next frame, checking every remaining output bit and one new ready
edge. The full original-storage gate requires
`mu4_mcbsp_transmit_packing: PASS` and
`mu4_mcbsp_transmit_packing_restore: PASS`. Multiple pending transmit frames,
sync between words of an active multiword frame, and other sub-clock FSX
phases remain unsupported or unvalidated. This is controller evidence, not
native recorder or speech completion.
The old controller's terminal-overlap abort is reproduced in
`tmp/mcbsp-txpack-red.log`; the full original-storage gate, including overlap
save/replay and per-edge assertions, passes with
`MU4_STORAGE_RUN_DIR=tmp/mcbsp-txpack-final`.

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

The outgoing route is also MCU-derived. Constructor `335a50` selects
transport `1e`, destination `28`, class `42` and wrapper `d2`, then calls
generic sender `399f12`. For this nonzero class and destination, the sender
tests the node-availability bitmap before copying or posting: an unavailable
node returns 4. An accepted remote object is marked with source sentinel
`f0` and queued to task 7 through `2ce88c`. Task 7 replaces the sentinel
with its local node and calls router `362958`; remote `1c/1e` objects go
through `2f5dc0`, which copies and posts them to task 8. This is distinct
from the local class-`d2` route to task 29.

The own-image aligned Thumb census covers all 1736703 halfword pairs and
finds only two direct callers of constructor `335a50`: `335adc`, in the
source-pinned `335aae` builder of self-test `47/02`, and `335d78`, which
forwards an existing message after matching its routing context. The builder
has one direct caller, `335b0c`, and neither constructor has a recovered odd
Thumb pointer. These are not two independent music-control producers.
Computed calls, ARM instructions, unaligned/even pointers and code/data
classification remain outside the scan's closure. The package checker
retains these bounds rather than asserting no other producer exists.

In particular, this MA4 envelope does not recover the MSP430-to-music-DSP
stop sequence. The immediate music-DSP peer is the separate MU4 controller,
not the ARM directly. The native `mu4nand` bench's legal receiver requests
prove DSP-side behavior, not that a normal handset uses that complete
sequence. Resolving its intended drain/finalization policy requires the
missing controller program or a normal-operation transcript/specification;
do not infer another packet from the service envelope or its shared `1e`
prefix. The acquisition exclusions in Software-accessible MU4 references
apply to this boundary too.

Task 8's `2f52aa` fragments the frame into type-`05` DSP objects, appends
count/sequence bytes and queues them through `3849ee` at `125ae0`.
Drain `2f51fc` posts copies to task 3 through `2ce7a4`. These code extents
are pinned by the package checker. This establishes the MCU-side outbound
boundary, not the resident DSP's UART implementation or electrical timing.
The power-up-triggered request `47` therefore does not authorize inventing
an initial node-`28` greeting to satisfy the availability gate.

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
The MCU-side sequenced-ingress boundary is the external sender and startup
exchange, not an inactive generic receive queue. This is separate from
the standalone recorder's CPU execution limit.

The package checker pins sequenced receiver `2f5760..2f59db` and task-loop
code `2f5c8c..2f5d71`. Descriptor index 8 in the 30-entry task table selects
`2f5c8d`. Its receive loop stores the object at context offset `14`, tests
object byte 3, and calls `2f5760` at `2f5caa` for internal selector `8e`.
Objects whose first byte is `1e` or `1c` instead call control handler
`2f52aa`; these are not interchangeable internal and wire namespaces.

Across all 3,473,408 image bytes, the checker scans 1,736,703 aligned
halfword pairs and finds 64,693 plausible Thumb BL encodings. The receiver has
one matching direct reference, `2f5caa`; ingress `399fbc` has the three
references already listed above. Neither target has an aligned odd Thumb
pointer in this image. This quantifies encoded-reference coverage only:
computed calls, ARM instructions, unaligned pointers and candidate code/data
classification remain excluded. It is not a proof of absent external input.

The sequenced receiver reads its trailing count and sequence bytes at
`object + payload_length + 8/9`, uses the low three sequence bits and bit-6/7
branches, and queues only after the selected completion branch. This is
upstream of the MU4-specific `d2/42` consumer.

The internal producer is task 4, not a literal `8e` constructor. Its own
descriptor selects `3a52b5`. Ring header reader `318ee6` reads a halfword
from the shared window at `10000`, allocates `length + 5` bytes and builds
`{18, 02, length, type, payload...}`. Reader `318f5c` copies the packed
halfword payload to object offset 4 and advances the ring consumer. Task
loop `3a52b4` selects the type from object byte 3. The 13-entry big-endian
jump table at `3a5304` covers `83..8f`; entries `8d` and `8e` both select
`3a53b2`, whose explicit range check forwards through `2f5f8a`. That wrapper
posts the unchanged object to task 8 via `2ce88c`. The package checker pins
these code bodies, task descriptor, shared-window literal and table entries.
This is a static transport contract, not a runtime MU4 acceptance result.

The positive runtime control is now executable through
`noki5510_bootstrap_check.py --runtime --serial-readiness --input-lifecycle
--dsp-service-route <error.log>` using `noki5510_input_observe.lua` and
verbose logging. A fresh sixteen-second hybrid run observes task-4 caller
`3a53f7` forwarding two type-`8e`, length-`0a`, source-`02` D0 frames,
followed by sequenced ingress from `2f58b1` with classes `01/04`.
The checker requires that order and exact route fields. Node-`28` sends
and availability events remain absent in this bounded run. This validates
the service transport with declared HLE traffic, not a physical MU4 backend.

The remaining question is which physical receive mechanism delivers MU4's
node-`28` bytes to this DSP-carried service ring, including its activation
and framing rules. Existing node-`02` HLE replies demonstrate the route but
cannot establish that physical source. No key mapping or MU4 self-test reply
is enabled by this result.

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
