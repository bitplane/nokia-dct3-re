# Nokia 5510 NPM-5 bring-up

## Contract references

- [MU4 hardware, storage, clocks, DMA and serial consumers](mu4_hardware.md).
- [Recording, metadata and cold-catalogue maps](mu4_recording.md).
- [Playback, serial provenance, refill and pending tails](mu4_playback.md).
- [CPU composition and atomic negative controls](c54x_mu4_observations.md).
- [Independent audio validation and current limits](aac_validation.md).

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
and [transport provenance](mu4_playback.md#dma-source-to-dxr-ownership).
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
[refill and tail contracts](mu4_playback.md#refill-control-and-buffered-tail).

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
separately from this handset/loader boundary. Detailed music-DSP maps have
their own owners in the contract references below; this document owns the
MA4 handset's inputs, startup, provisioning and acceptance limits.

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
