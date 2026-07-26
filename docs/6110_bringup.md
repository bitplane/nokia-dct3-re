# Nokia 6110 NSE-3 bring-up

This is the evidence boundary for the first 6110-family profile. The driver now
declares the hardware facts established by Nokia documentation, separately from
contracts that can only be recovered from an identified firmware image. Until
both sides are available, the coverage matrix remains `Unsupported`; the
declared machine intentionally contains `NO_DUMP` execution inputs.

## Primary hardware evidence

Nokia's *NSE-3 Series Transceivers, Chapter 3 System Module*, Original
11/97, and *UI Module UE4*, Original 11/97, establish:

| Boundary | NSE-3 contract | Emulator consequence |
| --- | --- | --- |
| MAD | MAD2 with ARM and TI Lead DSP; the parts list identifies ROM3 variant `F711604` | Do not assume a later product's boot ROM or bootstrap exchange count. ROM3 and ROM4 firmware pairs must be labelled separately. |
| Program flash | Parts list: Intel `TE28F800`, 512K x 16, 120 ns; handset flashing log: `28F800B3-T`, ID `0089:8892` | `noki6110` composes an explicit 1 MiB Intel 28F800B3-T top-boot component. It does not use the later 2 MiB TE28F160 compatibility device. |
| Work RAM | 64K x 8 (512 Kbit) SRAM | The NSE-3 address map exposes only 64 KiB of work RAM. Unproved upper aliases remain unmapped pending firmware analysis. |
| EEPROM | 8 KiB serial EEPROM | `noki6110` composes a 24C64-class device and a separate 8 KiB `NO_DUMP` region on PUP's serial signals. It does not truncate a 3210 image or invent a parallel alias. |
| Display | UE4 GD40 COG module with 84 x 48 one-bit display RAM and a serial interface | The existing 84 x 48 serial LCD component is structurally applicable; command compatibility still needs a boot trace. |
| Keypad | Five rows by five columns; documented Send, End/Mode, softkeys, Up/Down, digits, `*`, `#`, side keys and flip input | A dedicated `noki6110` input map follows the UE4 matrix and does not inherit either later handset's map. |
| Internal audio | Internal microphone on differential MIC2; internal dynamic receiver on differential EAR | The machine installs neutral physical MIC2/EAR routes. Accessory MIC1/MIC3 and HF remain separate, and no product gain is guessed. |
| PCM serial bus | COBBA-GJ generates 1.000 MHz `PCMDClk` from 13 MHz / 13 and 8.0 kHz `PCMSClk` by /125; 16-bit word contains a sign-extended 13-bit sample | Reuse the generic typed 1 MHz/125-clock, 13-in-16 PCM bus shape, but do not inherit NHM-5 speech-control values or analogue gains. |
| Alert path | Dynamic buzzer driven by MAD `BuzzerPWM`; vibra is in a special battery | Keep alert audio outside the speech path and do not add an internal vibrator route. |

Primary source:
[Nokia NSE-3 service-manual archive and extracted text](https://files.elektroda.pl/55977%2Cnokia%2B6110%2Bservice%2Bmanual.html).
The standalone system-module copy is
[NSE-3 Chapter 3](https://electronicsandbooks.com/edt/manual/Hardware/N/Nokia/Phone/6110/03SYS%20%5B73%5D.pdf).
An independent handset flashing report identifies ROM 3 and the fitted flash
as `Int 28F800B3-T`, manufacturer/device ID `0089:8892`:
[NSE-3 v5.48 flashing log](https://gsmforum.ru/threads/ishchu-proshivku-na-nse-3-6110-5-47.136332/).

## Firmware baseline

Public historical indexes consistently label the final GSM NSE-3 release as
v5.48 and describe an MCU + PPM B archive of about 1.07 MiB. The currently
indexed firmware.center NSE-3 directory is empty and the old free-host links
are dead, but Internet Archive item `Nokia_DCT3_firmwares` preserves the
original Wintesla payload in three independently hashed wrappers.

The package manifest directly distinguishes `ImageFile=nse3nx_5.480` from
`Rom4ImageFile=nse3nx05.480`, with corresponding ordinary and ROM4 PPM members
for every language pack. PPM B normalization produces separate, exact 1 MiB
ROM3 and ROM4 images. ROM3's embedded `V  5.48`, `08-09-99`, DSP software
`40.3.617` and `14-Dec-98` strings corroborate the independent real-ROM3
handset log. ROM4 instead embeds `V 05.48`, dated `03-09-99`. Complete source,
member and normalized hashes are recorded in `roms/README.md`; neither image
inherits the other's internal-ROM or bootstrap identity.

The same collection supplies the earlier original `Nse-3_v4.06.exe` service
package. Its independently hashed Wintesla members normalize reproducibly to
exactly 1 MiB: MCU
`0x200000..0x2bebff`, an erased gap through `0x2bffff`, and PPM B
`0x2c0000..0x2fffff`. The normalized image has SHA-1
`5025a6ac3b4a13714211fde903f27f92cbb7c9b6`; full source/member hashes and the
reproduction command are recorded in `roms/README.md`.

This is a real firmware-analysis baseline, but not a boot promotion. Its
unprefixed `V 4.06` version spelling is consistent with contemporary ROM3
version tables, so the BIOS is explicitly labelled “ROM3 candidate”. The
matching F711604 internal boot/DSP ROMs and 24C64 contents remain `NO_DUMP`.

### v5.48 ROM3/ROM4 bootstrap split

`make verify-6110-v548-static` pins both normalized images independently.
Their homologous loaders are at `0x2833f4` (ROM3) and `0x285010` (ROM4), a
consistent `0x1c1c` relocation, but they are not compatible with the v4.06
completion profile. Both initialize shared halfwords `0x10000` to zero and
`0x10002`, `0x10004`, `0x10006` to `0xffff`. Before uploading, they wait for
the DSP to alter `0x10004` and require it to agree with `0x10006`. Only then
do they stage the same 64-block, 32-byte-stride projection of their respective
flash image through `0x10200`.

After the final block they wait for `0x10002` to change away from `0xffff`,
then capture the final `0x10000`/`0x10002` publications in product state.
ROM3 and ROM4 stage different 65,536-byte streams, with SHA-1
`73ddf5f79e421fdcfff7742e238fb24ea5f1fcfa` and
`94e447d8386e010326fdfb261e247d6c0ac4d97a` respectively. The first final
value is constrained independently: ROM3 and ROM4 each render it as a
three-character `Bxx` string and later accept only `0x0b06`. Their sole direct
loader calls, capture locations, formatter roots and comparisons are pinned by
the gate. This does not make the two bootstrap protocols interchangeable:
v5.48 cannot reach that final publication without first satisfying its
pre-upload exchange, and generic ready words remain incompatible.

The pre-upload value also has an exact external-MCU role. Both images permit a
bounded wait for shared `0x10004` to leave `0xffff`, store the observed
halfword, and proceed only when it equals shared `0x10006`. They then expose
values below ten as a one-digit selector-`0x09` string and insert the same
digit into literal diagnostic text `DSP ROM  `. Whole-image literal censuses
find exactly those two consumers in each image. The delay helper receives raw
value 10 and the retry counter is bounded at 20; its time unit remains
unproved.

The independently recorded matching v5.48 ROM3 handset reports
`DSP ISw : ROM 3` in the same service namespace
([GSMForum handset record](https://gsmforum.ru/threads/ishchu-proshivku-na-nse-3-6110-5-47.136332/)).
Because that running handset necessarily passed the loader's equality gate,
the ROM3 pair is constrained to `3/3`. The driver types that pre-upload
publication separately and emits it only after firmware initializes both
cells to `0xffff`. ROM4 has no matching handset record and does not inherit a
speculative `4/4` pair from its package label.

The real ROM3 handset log reports physical COBBA `B07` while this same v5.48
external firmware requires the captured result `0x0b06`. That contradiction
is useful negative evidence: the result may participate in a service
projection, but it cannot safely be named as the fitted COBBA revision. The
static JSON therefore records the firmware-required value and explicitly
marks the physical-silicon semantic unproved.

The driver now declares the manifest-proven ROM3 and ROM4 external images as
separate BIOS choices and keeps their absent internal MAD2/DSP ROM inputs
separate. This is an identity and architecture promotion, not a booting
promotion.

### Reproducible static boundary

`make verify-6110-static` checks the exact normalized v4.06 image before making
any structural claim. The big-endian ARM entry at `0x200040` reads and
byte-reorders the flash header word, writes it to the MCUIF window at
`0x040000`, establishes CPU-mode stacks, copies eight vector words from
`0x200180` to address zero, clears interrupt masks, and changes to Thumb state
at `0x2000e4`. Its stack literals (`0x100020`, `0x10c508`, and `0x10f9d8`)
all lie inside the documented 64 KiB NSE-3 SRAM. This independently supports
the declared flash and SRAM boundaries; it does not establish what consumes
the MCUIF header word.

The same checker byte-swaps the identified image only for the existing
conservative Thumb literal analysis. It resolves 548 direct MAD2 accesses from
225 literal seeds at 41 distinct byte offsets, with a maximum offset of
`0x3f`. This is an address-surface census, not a register map: dynamic and
table-driven accesses remain outside it, and no offset is assigned a meaning
without separate evidence. The checker pins these results to the exact
firmware hash and emits `run_census/nse3_v406_static_boundary.json`.

The v4.06 image also contains a 25-byte five-by-five keycode table at
`0x2be8bc`, followed by its special-key table at `0x2be8d8`. The numeric,
softkey, Send/End and navigation positions agree with the UE4 matrix. More
importantly, the firmware distinguishes side-key code `0x11` at drive row 1
from code `0x10` at drive row 4; those decode as Volume Down and Volume Up
respectively. This corrected the initially reversed side-key labels in the
driver. Power is code `0x0d` in the separate special table. The verifier checks
the source bytes rather than accepting the input declaration as its own proof.

The external EEPROM boundary is now firmware-checked as well. The v4.06
transaction routine at `0x29cd88` emits the high and low halves of a 16-bit
word address for the large-device mode required by the documented 8 KiB
serial EEPROM. Its byte sender at `0x29e8bc` uses MAD2 GenIO signal register
`0x20020`: bit 0 is SDA, with its open-drain direction at `0x20024` bit 0,
while bit 2 is SCL. This exposed a genuine profile error—the generic PUP
default used bit 3 for SCL. The PUP device now keeps that compatibility default
for other products and accepts a typed per-product SCL bit; NSE-3 selects bit
2. `make verify-6110-static` pins the instruction anchors that support this
configuration.

The v4.06 SIM driver is a contiguous firmware-owned surface at
`0x28ff84..0x2905f4`. It gates the standard MAD2 SIM clock through
`0x2000d.bit5`, initializes and activates the `0x20036..0x2003f` SIMI window,
stages TX bytes through `0x36/0x3e/0x3f`, drains RX through `0x37/0x3c`, and
dispatches/acknowledges the interrupt-identification register at `0x38`.
`make verify-6110-static` checks representative initialization, activation,
FIFO, RX and IIR instruction anchors. This establishes that the generic MAD2
SIMI controller is physically applicable to NSE-3.

The higher v4.06 SIM manager independently establishes the removable-card
opening contract. It accepts both direct (`3b`) and inverse (`3f`) convention,
walks the interface-byte chain from the T0/TDn presence bits, and maps ordinary
TA1 values—including the lab card's `05`—to PPS `ff 00 ff`. TA1 `94` instead
selects the separately checksummed high-speed request `ff 10 94 7b`.
`make verify-6110-static` pins that parser and its organic call from the SIM
manager loop.

NSE-3 therefore composes the existing standards-shaped removable lab card:
its `3b 10 05` ATR is demonstrably accepted and retains the controller's
default serial rate. The card's synthetic subscriber identity and filesystem
remain fixture policy, not Nokia 6110 product identity or an NSE-3 ROM special
case. The same static gate pins the T=0 status classifier: the card's normal
`67`, `90`, `94`, `98` and `9f` families all have explicit firmware paths,
while its `6d 00` unsupported-instruction response reaches the generic command
error path. That status classifier alone does not prove an instruction set or
transaction order. The firmware's first APDU sequence still requires an
organic boot trace; SIM and registration coverage remain unpromoted.

The command-construction layer is now bounded independently of that missing
ordering. The contiguous routines at `0x286c40..0x2875a8` construct class
`a0` APDUs and submit every command object through `0x286b6a`. Their 18
unique instructions are:

| Instruction | Firmware constructor meaning |
| --- | --- |
| `10`, `12`, `14`, `c2` | TERMINAL RESPONSE, FETCH, TERMINAL PROFILE, ENVELOPE |
| `20`, `24`, `26`, `28`, `2c` | VERIFY, CHANGE, DISABLE, ENABLE and UNBLOCK CHV |
| `32`, `88` | INCREASE and RUN GSM ALGORITHM |
| `a4`, `b0`, `b2`, `c0` | SELECT, READ BINARY, READ RECORD and GET RESPONSE |
| `d6`, `dc`, `f2` | UPDATE BINARY, UPDATE RECORD and STATUS |

There are two separately stateful SELECT constructors; this is 19
constructors, not a duplicate-byte search presented as 19 instructions. The
exact verifier pins each CLA/instruction authoring site plus the first and
last common-submit calls.

This vocabulary also makes the shared lab-card boundary explicit. Its
standards-shaped core implements SELECT, READ BINARY, READ RECORD, GET
RESPONSE, UPDATE BINARY, UPDATE RECORD and STATUS. The recovered command set
motivated a generic GSM 11.11 CHV component rather than an NSE-3 handler:
VERIFY, CHANGE, DISABLE, ENABLE and UNBLOCK now share persistent credentials,
three-attempt CHV and ten-attempt unblock counters, blocked/enabled state and
reset-scoped verification. Mutable-file access follows CHV1 disabled or
verified state, and directory status reports the live counters. Existing
pre-CHV card NVRAM is migrated by initializing only the appended security
state. The laboratory credentials remain synthetic card provisioning, not
Nokia product identity. Command coding, status words and retry semantics follow
[ETSI GSM 11.11 v5.0.0](https://www.etsi.org/deliver/etsi_gts/11/1111/05.00.00_60/gsmts_1111v050000p.pdf),
clauses 8.9--8.13, 9.2.9--9.2.13 and 9.4.

INCREASE is now implemented as a generic GSM 11.11 card operation against the
laboratory profile's real `EF_ACM`: a persistent three-byte cyclic record,
CHV1 READ/UPDATE/INCREASE conditions, checked 24-bit addition, `98 50`
overflow, and the specified six-byte result through the immediately following
GET RESPONSE. This removes a genuine shared-card gap, but does not prove that
NSE-3 reaches its constructor or promote 6110 runtime coverage.

The remaining unsupported constructors are RUN GSM ALGORITHM and the four SIM
Toolkit commands; they continue to return `6d 00`. This is not permission to
pre-implement an assumed 6110 boot script: an organic trace must establish
which commands are actually issued, their parameters and status progression.
Authentication also needs an explicit laboratory A3/A8 and key profile, and
Toolkit needs a proactive-service profile. Missing commands should be
implemented generically from the applicable GSM SIM contract rather than
special-cased for NSE-3.

The same exact-image gate now establishes the firmware side of the generic
DSPIF transport. The MCU-to-DSP ring occupies shared byte offsets
`0x000..0x0a3`, with word cursors at `0x0a4/0x0a6`; the DSP-to-MCU ring
occupies `0x100..0x1c7`, with word cursors at `0x1c8/0x1ca`. Firmware
initializes the first pair to zero and the second pair to word offset `0x80`,
then applies the corresponding wrap limits while producing and consuming
records. The verifier checks both the Thumb operations and their effective
shared-memory literals, including the `0x30000` DSPIF doorbell. These are the
same transport boundaries expressed by the reusable `nokia_dspif_device`, so
NSE-3 does not require a copied or product-specific ring implementation.

The exact-image gate also closes the MCU-side radio packet envelope without
enabling a radio peer. Two independent v4.06 control paths allocate
`0x48`-byte queue objects and construct DSP packet type `0x1a` with wire
length 68, but they are not equivalent searches. The ordinary constructor at
`0x20faec` derives and packs its search set, publishes it through task 3 and
arms numeric timer `0x1b` with raw duration `0x09cd`; the timer unit remains
unknown. The timer table gives code `0x1b` flags
`0x03`, owner task 4 and configured value `0x000000db`, so it is specifically
not evidence for a direct task-11/task-12 search-completion event.

Task 4 is the shared DSP/radio dispatcher at `0x2a20dc`, and it arms the same
timer at task entry before any search submission. Every processed input
increments a shared counter at `0x10c4ce`. When value `0xdb` arrives with a
nonzero count, the task rearms `0x1b` and clears the counter; when the count
is zero it calls `0x2962c2` with argument 1. That routine either rearms the
same timer or, under a separate set of system-state gates, calls
`0x2974ae`/`0x297504`; the latter feeds `(1, 0x1b)` to `0x260568` and arms
another timer. Those gates are not radio-search state. The search constructor
therefore refreshes task-wide activity machinery, not a search-specific
completion timeout.

Its gated task-11 caller supplies
constructor argument 1, while the constructor can internally select argument
6 when it obtains a bounded candidate source.

The second form at `0x216f72` first zero-fills its entire `0x48`-byte object,
then writes the type/length envelope and sets object byte 5 to `0x13`. It is
reached from task-12 status `0x0445` after `0x216f10` clears two private
forty-record buffers and after the case updates runtime mode byte `0x106b0d`.
The fixed `0x0445` producer is outside the radio-report dispatcher. This
proves a zero-bitmap control form, not a second populated cell search or an
organic acquisition phase.

Task-side code reads the object length at `+2`, passes
`object + 3` (type followed by body) to the generic DSPIF writer at
`0x285746`, and frees the object only after submission. This proves a genuine
NSE-3 bitmap-shaped `SEARCH_LIST` command, rather than compatibility inferred
only from NSE-8 packet sizes.

The receive side is independently bounded. DSPIF parser `0x285794` preserves
the received length at queue-object byte `+2` and packet type at `+3`.
Dispatcher `0x2a20dc` routes type `0x80` directly and uses a bounded table for
radio reports `0x83`, `0x84`, `0x86..0x8c` and `0x8f`. Its handler topology
matches the established MDI vocabulary: received block, RSSI/RA information,
block request, search terminals, timing offset, channel-change confirmation
and random-access completion. In particular, type `0x8b` is copied as a
`0xa8`-byte queue object: the four-byte queue envelope plus a 166-byte DSP
packet containing forty four-byte result records. Types `0x8d`, `0x8e`,
`0x95`, `0x9b` and the `0x70..0x7f` family are classified separately and are
not folded into the radio report table.

The search bitmap boundary is now exact rather than merely shape-compatible.
The ordinary constructor subtracts one from the ARFCN, divides by eight,
addresses backwards from queue-object byte 69 and uses the low three bits as
the bit index. Since the DSP wire begins at object byte 3 and the peer payload
excludes the type byte, ARFCN 1 is bit 0 of payload byte 65. This is exactly the
numbering currently decoded by the NSE-8 bitmap-search path.

The task boundary is more staged than the packet geometry alone suggested.
Type `0x8b` posts task-12 status `0x13b8`; its case at `0x21718c` copies the
160-byte result body from object byte 6 and enters product-local result
processing. A separate task-12 status, `0x13aa`, reaches case `0x21732e`.
Only that later controller case accepts state 4 when the active command is
type `0x1a`, also accepts states 6 and 7, and enters measurement consumer
`0x213fbc`. The external image does not yet establish a direct
`0x13b8 -> 0x13aa` edge, so receipt of the DSP result packet must not be
treated as immediate search completion.

The direct post-ingestion call graph is bounded as well. Runtime mode byte
`0x106b0d` selects no call to `0x2143f4` for value 1, calls it with argument
1 for value 2, or calls it with argument 2 followed by `0x214494` otherwise.
When state byte `0x106b08` is not 3 and no saved result buffer exists, the
case can also call result forwarder `0x214670`. None of these direct branches
constructs a DSP request. A later transitive request sequence remains
possible, but is not established by the external image; result ingestion is
therefore not evidence for synthesizing the next DSP report or command.

Status `0x13aa` is nevertheless a recurring measurement-controller boundary,
not an unstructured queue value. Its entry calls numeric controller operation
`(0x6b, 2)` before the measurement consumer. The consumer rearms code `0x6b`
with operation argument zero and selects raw duration `0x0eb4` in controller
state 4 or `0x04e6` otherwise.

The expiry producer is now exact. Generic timer initialization consumes
eight-byte records from table `0x2b75d0`; timer `0x6b` has flags `0x03`, owner
task 12 and event-object pointer `0x2be7b0`. That fixed object begins with
status `0x13aa`. The whole-image direct-arm census finds only three sites:
candidate-update arm `0x212558` plus the two state-selected rearm paths inside
the measurement consumer. This proves the recurring
`timer 0x6b -> task 12/status 0x13aa -> measurement consumer -> timer 0x6b`
loop. The timer unit and the trigger that first reaches the candidate-update
arm were previously unproved.

The MCU-side initial-arm path is now bounded too. Handler `0x2800b0` is a
report multiplexer: object byte 4 admits distinct values `0x40`, `0x50`,
`0x60`, `0x70`, `0x80`, `0xa0`, `0xb0` and `0xb1`, and one gated
`0x60` form is rewritten to `0x50`. It always combines report bytes 7..9
as a big-endian 24-bit value at `0x109078`, but only discriminator `0x40`
reaches the status-`0x139e` controller branch.

The complete direct route table is:

| Byte 4 | Direct handler route |
|---:|---|
| `0x40` | Controller flags select release, timer-`0x1b` cancellation, task-11 status `0x138e`, or task-12 status `0x139e`. |
| `0x50` | Calls controller helper `0x28000c`; later mismatch handling can post status `0x13c8` to task 13. |
| `0x60` | Runs timing validation at `0x2801a0`; an accepted path posts status `0x040b` to task 14, while one gated form is first rewritten to `0x50`. |
| `0x70` | Builds an eight-byte status-`0x13d0` object for task 12, then calls follow-up `0x27fdc4`. |
| `0x80` | Preserves a `0x28`-byte object as status `0x040b` for task 14. |
| `0xa0` | Preserves a `0x28`-byte status-`0x043b` object and passes it to service helper `0x279b72`, which selects service `0x13`. |
| `0xb0`, `0xb1` | Report byte 6 equal to 1 is released; other values share the status-`0x040b` task-14 route. |

The `0x70` arm is now bounded beyond that first publication. Status
`0x13d0` has exactly one literal producer in the image, at `0x280174`.
The handler stores the shared big-endian 24-bit value at offset +4 in its
eight-byte task-12 object. Task 12 recognizes `0x13d0` explicitly after
arithmetic dispatch from base status `0x13a3`: case `0x21715c` adds
`0x68` to the received value and stores it at controller
`0x106aa4 + 0x20`. If byte `0x106b0b` equals one, the case also calls
the already bounded timer-`0x6e` controller-recheck helper `0x2142b2`.
This makes the compact report a genuine candidate/timing-controller
input rather than a terminal notification.

The subsequent call to `0x27fdc4` is also unique in the image. It receives
the original report, not the compact task-12 copy, and tests fields at
offsets 6, `0x0e`, `0x0f`, `0x10`, `0x12`, `0x13` and `0x14`. Depending
on those fields and controller state, it may publish an eight-byte
status-2 object to task 3; preserve a `0x28`-byte status-`0x13c8` object
for task 13; or preserve a `0x28`-byte status-`0x040b` object for task 14.

The task-3 form is now separated from its adjacent instrumentation. Its
object byte 2 is four and task 3's status-2 queue eventually passes exactly
four bytes starting at object +3 to byte-stream helper `0x285746`. The
wire-side bytes are therefore `1f 00`, a controller-flags byte, and one
value byte. Object byte 7 is local metadata beyond that declared payload.

An exhaustive register-independent constructor census finds five direct
stores of type `0x1f`, not just this report-driven one:

| Constructor context | Flags byte | Value byte | Local byte 7 |
|---|---:|---|---|
| Argument-1 branch `0x20d6bc -> 0x20d78e` | `04` | Table `0x2bcfdc`, indexed by the low five bits of cell `0x10acf4` byte 8 | Zero |
| Status `0x03f3`, routine `0x20f0e0` | `01` | Zero | Input object byte 2 |
| Status `0x03f0`, routine `0x20f128` | `0d` | Cell `0x108f82` byte 1 | Byte selected from cell `0x108f50` |
| Status `0x03ee`, routine `0x20f3ec` | `0c` or `0d` | Table `0x2bcfdc`, indexed by input byte 9 | Optional input byte `0x0b` |
| Type-`0x80`/discriminator-`0x70` follow-up | `01`, `06` or `07` | Derived timing or zero | A report-derived value |

The `0x03f3`, `0x03f0` and `0x03ee` forms are distinct cases of the same
status dispatcher and have one direct constructor call each. The flags-`04`
form is not standalone: routine `0x20d6bc` branches to it when argument 0
equals one. Its three direct callers supply values `0`, `1` and `1`, proving
two ordinary entry paths to the form. All five share the same
status-2 task-3 framing and four-byte type-`0x1f` wire profile. This supports
a shared typed transport object with product/controller-specific construction
policy; it does not yet establish the DSP-side meaning of individual flag
bits or the value byte.

The controller-status route is now exact too. Common builder `0x27d5c0`
posts all three objects to task 11, whose sole call to dispatcher `0x20f8e4`
selects the constructors arithmetically from base status `0x03ee`:

| Task-17 state case | Published status | Selected form |
|---:|---:|---|
| 21 (`0x24e0a6`) | `0x03ee` at `0x24e0ba` | flags `0c/0d` |
| 20 (`0x24e136`) | `0x03f3` at `0x24e17e` | flags `01` |
| 22 (`0x24e222`) | computed `0x03f0` at `0x24e2cc` | flags `0d` |

For `0x03f3`, the builder copies cell `0x10a4e4` into status-object byte 2;
the selected constructor retains that only as local object byte 7, outside
the four-byte payload. One gated fall-through path can publish
`0x03ee -> 0x03f3 -> 0x03f0`, but calls to event reader `0x27daf4` and
state-dependent branches intervene, and all three labels remain independent
task-17 jump-table entries. The sequence is therefore possible, not an
unconditional DSP lifecycle script.

The separate cross-model gate independently finds the same status-2,
four-byte, task-3 envelope in Nokia 3210 v6.00 and Nokia 3310 v6.39. Their
policies are not interchangeable: NSE-8 has four specialized constructors,
NHM-5 has one parameterized four-bit builder, and NSE-3 has the five forms
above. The generic boundary is therefore only `1f 00 flags value`; flag/value
construction stays in product/controller policy. See
`docs/dct3_type_1f.md`.

Before task submission, `0x2768b4` receives the same object with argument
`0x1e08`. An exact census finds nine callers with arguments
`1e00..1e05` and `1e08` (not every value occurs). The helper applies the
shared `0x1c00` filter, derives a secondary identifier from object byte 3,
and copies eight bytes into a separately allocated envelope through
`0x29fe98`. It contains no task-3 submission. Thus `0x1e08` belongs to the
filtered-copy family and is not a task-3 wire field. The numeric
type-`0x1f` payload remains unnamed until its DSP-side consumer or an
independent protocol source supplies its meaning.

Across the complete handler extent, none of these arms directly calls
`CHANNEL_CONFIGURE`, the type-`0x1a` bitmap builder or the type-`0x09`
candidate-list builder. The complete `0x27fdc4..0x27ffa6` follow-up also
calls none of them. Their numeric statuses and first consumers are
therefore preserved without treating them as parallel acquisition
terminals or inferring a transitive radio request.

That branch first applies flags in controller object `0x1090fc`, clearing
mask `0x0a000000`. Depending on the remaining flags and controller state it
can release the report, cancel timer `0x1b`, or forward status `0x138e` to
task 11. Only the remaining path allocates a `0x40`-byte task-12 object with
status `0x139e`. It stores the 24-bit value at +4, report words
`0x0a..0x0b` and `0x0c..0x0d` in big-endian form at +8 and +`0x0a`,
copies report bytes 5 and 6 to +`0x0c` and +`0x0d`, and preserves 24
further report bytes from +`0x0e` at the same object offset.

Task-12 case `0x217418` has two separate continuations. State byte
`0x106b08 == 2` uses helpers `0x216e84` and `0x2141bc`. Other states reach
candidate updater `0x2124a8` only when mode byte `0x106af6 == 2`; all
remaining modes skip candidate update. The updater has exactly two direct
callers: this received-report path and internal synthetic-object builder
`0x21269c`. It derives an index from object +8, requires object +`0x0d` to
be zero, consumes copied byte +`0x0e`, and updates `0x44`-byte records based
at `0x106d3c` under controller `0x106aa4`. Its code extent directly calls
neither `CHANNEL_CONFIGURE`, either search builder nor the task-3 submit API.

The updater does contain the sole non-consumer timer-`0x6b` arm at
`0x212558`, further gated by a nonzero byte at `0x106afd`. This closes the
external-MCU path from one specific received-report discriminator into the
recurring measurement timer, but not the missing DSP-side conditions that
emit the type-`0x80` report or the meaning and unit of its timing. The raw
durations therefore remain exact-image policy evidence rather than enabled
peer timing.

The later state/type topology matches the NSE-8 acquisition family, but its
consumer exposes a real product boundary: NSE-3 advances through internal
candidates in `0x44`-byte strides, whereas NSE-8 uses `0x48`. Those are
firmware-private controller objects, not the shared four-byte DSP result
records, and must not become a radio-peer wire setting.

Together these findings establish that bitmap packing and the principal
measurement terminal belong in the shared radio layer while firmware-private
candidate storage does not. The 6110 still selects no radio peer: its missing
internal boot/DSP image prevents organic transport startup, and the remaining
report sequence and retry/completion rules have not all been closed. Enabling
the existing NSE-8 lifecycle wholesale would therefore still overstate the
evidence.

The next acquisition transaction is also independently constructed. Routine
`0x20cffa` allocates a `0x18`-byte queue object and emits
`CHANNEL_CONFIGURE` type `0x02`, wire length 20. Its first body byte is a
controller operation: the constructor seeds value 4 and evidenced callers
overwrite it with 6 or 7. These values remain typed protocol data; no register
or channel-mode meaning is assigned merely from their numeric form.

An exhaustive direct-call census now bounds the request side rather than
sampling those overwrite sites. The exact image has five calls to
`0x20cffa`: `0x20d1c2`, `0x20da60`, `0x20ea06`, `0x210a0e` and
`0x210caa`. Their second constructor arguments are respectively `0x10`,
`0x1a`, `0x1a`, a byte selected from table `0x2bd710`, and `0x50`. Only
the `0x10` caller can replace operation 4 with 6 or 7; the other four retain
4. Each path finishes its own body fields and submits the resulting object to
task 3. The repeated `0x1a` argument therefore represents two independent
request-building paths, not duplicate callsite noise, while the dynamic path
prevents the set of request contexts from being reduced to four literals.

Inbound type `0x89` reaches handler `0x2804f4`. It gates acceptance using
controller state, converts the packet into a fixed eight-byte task object,
posts status `0x1393`, and advances its controller state to 3. It does not
read a confirmation body byte before that transition.

Type `0x84` is materially different rather than an empty second
acknowledgement. Handler `0x280398` discards the object only when controller
byte `0x1090ff` equals 1. Otherwise it replaces only the leading halfword
with task-11 status `0x1394`, preserving object bytes 2 through 7 in the
fixed eight-byte object. Task case `0x211d70` suppresses further processing
only at controller-object byte-3 value 1; all other values enter
`0x20db1c`, which has exactly that one direct caller.

The `0x1394` branch at `0x20db80` proves that the preserved report body is
meaningful. It first requires controller-object byte `0x12` to be zero,
copies input byte 4 into a newly allocated eight-byte status-`0x0400`
object, and combines bytes 5, 6 and 7 as one big-endian 24-bit value. Fixed
divisors `0x052e`, `0x33` and `0x1a` derive three output fields; another
field masks input byte 4 using a runtime shift, and the final field mirrors
a controller flag. The result is posted to task 17. When controller-object
byte `0x18` is not 1, the same 24-bit value is passed to the uniquely called
helper `0x20d8d6` and its result is stored back at byte `0x18`; value 1
instead enters a timer-`0x71` scheduling path.

The task-17 destination is now bounded independently. Task table entry
`0x2b757c` contains Thumb entry `0x24ce99`. Its dispatcher reads signed
controller state at `0x10a3b8`, accepts exactly states `0x00..0x1d`, and
uses the exact thirty-entry jump table at `0x24ea6c`. Status `0x0400` must
therefore be interpreted within the active task-17 controller state rather
than through one global RA_INFO callback. Across the complete task extent
`0x24ce98..0x24f1c4`, a direct-call census finds no call to
`CHANNEL_CONFIGURE`, the type-`0x1a` bitmap builder, the type-`0x09`
candidate-list builder or the task-3 submission API. Transitive effects of
individual task-17 state paths remain to be recovered, but this fixed
boundary rules out treating the derived object itself as a direct radio
request.

The alternative timer path is exact too. Timer-table record `0x2b7958` is
`01 0b 00 00 00 2b d7 0c`: flags 1, task-11 ownership and fixed expiry
status `0x138c`. A whole-image census finds two arms, at `0x20dc2c` from
the RA_INFO byte-`0x18 == 1` path and `0x20dcc6` from a separate controller
event. Both obtain a runtime duration through `0x2a4ac4`. There are three
cancellations (`0x20dd24`, `0x20dd6c`, `0x20f0b0`) and no query.

Task-11 expiry case `0x211ea6` publishes numeric control `(0x71, 2)`,
suppresses further work at controller substate 1, and otherwise returns to
the uniquely called general consumer `0x20db1c`. Its status-`0x138c` branch
at `0x20dc40` clears a controller flag, requires the two pending counters in
`0x108ed4` to agree, prepares a four-byte internal status object if needed,
and resets controller bookkeeping through `0x296008` and `0x20d1a4`. It
does not directly rearm timer `0x71` or call a radio request constructor.
The timer unit and the meaning of its runtime duration remain unknown.

Thus the NSE-3 `RA_INFO` boundary carries compact control/timing data into
the controller; it is not a zero-body completion like the direct type-`0x89`
handler. The exact path calls neither the type-`0x1a` bitmap builder nor
`CHANNEL_CONFIGURE`, so static adjacency does not establish a direct
confirmation-to-RA_INFO edge or the DSP-side emission order. The independently
bounded `CHANNEL_CONFIGURE -> CHANNEL_CHANGED_CNF` envelope shows that the
direct NSE-3 confirmation handler does not require the product-specific
body-bit correlation recovered for NHM-5. It does not, by itself, prove
which DSP report follows next in every controller state.

The task-11 `0x1393` case is correspondingly state-driven rather than
operation-driven. After accepting the eight-byte object it suppresses its
`0x211550` continuation for controller-byte-3 values 1 and 2 and takes that
continuation for other values. Neither the direct handler nor this task case
reads the outbound operation value. Static evidence therefore does not
justify assigning separate confirmation packets or follow-up reports to
operations 4, 6 and 7; that correlation still requires the missing DSP-side
ordering or an organic trace.

The surrounding report-to-task boundary is now exact enough to prevent an
NSE-8 sequence from being inferred from that single confirmation. Type
`0x84` posts status `0x1394` to task 11; types `0x87`, `0x89` and `0x8a`
post `0x138f`, `0x1393` and `0x1390` there as fixed eight-byte objects.
Type `0x83` selects between task-11 status `0x139f` and task-12 status
`0x13a0` using runtime state, preserving a `0x2c`-byte object. Task 12
separately receives type `0x88` as status `0x13ac` in 16 bytes, type `0x8b`
as status `0x13b8` in `0xa8` bytes, and type `0x8f` as status `0x13b7` in
eight bytes.

The type-`0x83` split is now bounded rather than merely described as
runtime-selected. Handler `0x27fd40` reads controller byte `0x1090ff`.
Value 1 releases the report without posting it. Value 3 preserves the
`0x2c`-byte object as task-11 status `0x139f`; its consumer stores only
signed report byte 6 in shared cell `0x10918d`. Every other value posts
task-12 status `0x13a0`.

The `0x13a0` consumer at `0x2173b0` passes report bytes 4, 6, 7, 8 and 9
through `0x21630c`. That routine maintains a five-sample rolling scalar
state and conditionally arms or cancels timer `0x6c`. The timer is now
independently bounded. Its configuration record at `0x2b7930` is
`01 0c 0000 002be7a8`: flags 1, owner task 12, and expiry object
`0x2be7a8` carrying status `0x13a6`. A whole-image direct-call census finds
one arm at `0x2163de` with raw duration `0x0273`, queries at `0x2156b0`,
`0x2163d2` and `0x2163e8`, and cancellations at `0x2156bc`,
`0x21594c` and `0x2163f2`.

Task 12 initially shares case `0x2172b2` for statuses `0x13a5..0x13a7`,
but decoder `0x2155b8` sends `0x13a6` to its own `0x2156e4` branch. That
branch issues numeric control `(0x6c, 2)` and continues only when signed
scalar `0x106cea` is negative, controller byte `0x1090ff` equals 4, and an
additional gate in object `0x106aa4` is clear. It does not rearm timer
`0x6c`. This makes `0x6c` a distinct one-shot scalar-control timer, not the
timer-`0x6b` measurement recurrence or timer-`0x1b` activity machinery.
Its raw duration unit and semantic name remain unassigned.

The type-`0x83` consumer then selects product-local helpers `0x216424` or
`0x216cb4` from mode
`0x106af6` and controller states 6/7. Neither the task-11 nor task-12 arm
directly constructs a bitmap search or `CHANNEL_CONFIGURE` request. Type
`0x83` is therefore established as controller-routed scalar measurement
input, but not as the missing acquisition terminal; the timer unit and DSP
emission conditions remain unknown.

Type `0x8a` does not close the missing search-result sequence. Its direct
handler `0x2803f8` reads no report-body field. It discards the object only
when controller byte `0x1090ff` equals 1; for every other value it clears
bit `0x02` in shared word `0x109178` and posts the unchanged eight-byte
object as `0x1390`. Task 11 deliberately merges statuses `0x138f` and
`0x1390` at case `0x211e48`, where shared flag gates and controller state
determine further processing through `0x210da8`.

That common case is not the end of the distinction. `0x210da8` is a central
task-11 event decoder with exactly six direct callers, all inside the same
dispatcher. Its exact ten-entry table covers statuses `0x138e..0x1397` and
routes `0x138f` to `0x2112a4` but `0x1390` to `0x211288`.

The type-`0x87`/status-`0x138f` branch writes value `0x0d` to controller byte
`0x109107`, then tests the two pending-object slots at
`0x108ed4 + 0x20` and `+ 0x30`. With neither slot populated it calls shared
control routine `0x20fa18`; with either populated it calls the populated
bitmap constructor `0x20faec` with argument 2. The type-`0x87` direct handler
also clears shared flag `0x02` unconditionally and cancels timer `0x1b`
after posting `0x138f`. This is a proven report-to-search-resubmission edge,
although the report's semantic name and the DSP conditions that emit it
remain unassigned.

The type-`0x8a`/status-`0x1390` branch instead increments counter
`0x109184`, compares it with runtime threshold provider `0x28f2ae`, and
can reach `0x20fa18` only after the threshold and an additional shared-flag
gate. It contains no direct bitmap-constructor call. The two reports
therefore share preprocessing infrastructure but implement distinct
controller transitions; treating either as a generic search-complete reply
would lose evidenced NSE-3 policy.

Type `0x88` is also bounded as controller input rather than a search
terminal. Handler `0x280464` discards it when controller byte `0x1090ff`
equals 1. Otherwise it allocates a fresh 16-byte object with status
`0x13ac` and repacks incoming object bytes `5..7` as a big-endian 24-bit
value at +4, bytes `8..9` and `0x0a..0x0b` as 16-bit values at +8 and
+`0x0a`, and control bytes 4 and `0x0d` at +`0x0c` and +`0x0d`.

The task-12 consumer at `0x2172d2` has two bounded paths. With state
`0x106b08 == 3` and mode `0x106af6 == 2`, it derives a scalar from the
24-bit field plus control/state offsets and passes that value to
`0x2142b2`; that helper can arm timer `0x6e` only behind further
controller gates. Outside that state/mode, a zero first control byte permits
`0x2159b0` to match the report's first 16-bit value against +`0x0a` of
24-byte candidate records and update record offsets +8, +`0x0e` and
+`0x0f`. Neither branch directly constructs a bitmap search or
`CHANNEL_CONFIGURE` packet. The exact image therefore establishes a
timing/candidate-update shape without assigning a DSP report name.

Type `0x8f` exposes a later and materially different request stage. Handler
`0x2803c8` reads no report-body field, discards only when controller byte
`0x1090ff` equals 1, and otherwise posts the unchanged eight-byte object as
task-12 status `0x13b7`. Its consumer at `0x217208` likewise reads no
report-body field. Unless task state `0x106b08` equals 3 it calls shared
builder `0x214788`; the whole image has exactly one other direct caller of
that builder, at `0x216192`.

Builder `0x214788` does not emit another bitmap. It allocates `0x54` bytes,
declares length `0x50` and type `0x09`, and scans 24-byte records in
candidate state `0x106aa4`. Records whose byte +`0x0f` is zero contribute
their two-byte key at +`0x0a..0x0b`; up to forty pairs are placed from
request-object byte 4 onward, with every unused pair filled `ff ff`. A
non-empty selection is submitted to task 3, advances controller byte
`0x109107` to `0x0f`, and returns 1. Its no-submission path returns zero;
the type-`0x8f` consumer then arms timer `0x6e` with raw duration 8 and
numeric control argument zero.

Timer `0x6e` is a shared delayed controller recheck, not a type-`0x8f`
protocol acknowledgement. Its table record at `0x2b7940` is
`01 0c 00 00 00 2b e7 b4`: flags 1, task-12 ownership and fixed expiry
status `0x13b0`. A whole-image census finds exactly three arms, all with raw
duration 8: the type-`0x8f` no-submission path at `0x21721c` and two gated
derived-timing paths at `0x2141e6` and `0x214320`. There is one cancellation
at `0x212092` and no query.

The expiry case at `0x217260` publishes numeric control argument 2 and
continues only in controller states 6 or 7, or state 4 outside context
`0x1a`. The accepted path calls `0x212088(1)`, then re-enters helper
`0x2142b2` with fixed value `0x297001`. The expiry case does not directly
rearm the timer, although the helper can do so after applying its controller
gates. This establishes conditional re-evaluation shared by distinct arm
contexts; it does not merge the type-`0x88` timing input and type-`0x8f`
candidate-list trigger into one report stage. The timer unit and the
semantic names of those reports remain unknown.

This proves that NSE-3 acquisition uses at least two distinct outbound radio
representations: the type-`0x1a` populated bitmap and a later type-`0x09`
candidate-key list. They are separate protocol stages sharing firmware
candidate state, not alternative product-wide wire profiles. The semantic
name of type `0x8f`, the exact declared-length/pair-padding interpretation,
and the DSP conditions that emit the trigger remain unassigned.

### Empty type-`0x03` controller publication

An independent ROM4 HLE labels an empty outbound type `0x03` as
`DEACTIVATE`. That label is useful as a search hypothesis, but the exact
v4.06 ROM3 candidate does not yet prove the protocol meaning or its position
in an acquisition sequence. It does independently prove the wire object and
the controller choice that emits it.

The fixed task-3 object at `0x2bd6fc` has normalized storage bytes
`02 00 03 00`. The status halfword is 2; after the MAD2 byte-lane crossing,
task 3 reads byte 2 as declared length zero and byte 3 as type `0x03`. Its
unique fixed-object submission is at `0x20eac6`. Task-11 status `0x03f1`
reaches dispatcher
`0x20f014`; its `0x20f06c` arm is the sole caller of route `0x20ea98`.
That route submits the empty object only when controller context
`0x108f18` equals `0x1f` and helper `0x27a174` returns zero.

The alternate branch uses the same task-12 status builder `0x20e920`, then
builder `0x20e9cc` supplies context `0x1a` to the established
`CHANNEL_CONFIGURE` constructor at `0x20cffa` and submits the resulting
type-`0x02` object. Both choices therefore publish task-12 status `0x13a3`;
the firmware evidence makes type `0x03` a context-gated alternative to the
context-`0x1a` channel request, not a second spelling of that request.

Status `0x03f1` itself has six calls to the common status builder, arising
from task-17 states 15, 17, 6, 7, 7 and 24. Those independent producers rule
out inferring one mandatory lifecycle or a sequence such as
type `0x1a` -> `0x03` -> `0x1a`. Until a DSP-side consumer, protocol document
or organic trace supplies stronger evidence, the checker deliberately
records no semantic name and the disabled NSE-3 radio peer emits no synthetic
reply.

Adjacent type `0x8c` does not establish the missing acknowledgement edge.
Handler `0x2804c0` releases the received object before reading any body
field. It suppresses its fixed notification when controller byte
`0x1090ff` equals 1 or controller context `0x1090fa` equals `0x1a`;
otherwise it posts the four-byte ROM object at `0x2bcfa8`, containing status
`0x1395`, to task 11. Those gates distinguish bitmap context from other
controller contexts, but do not test type `0x09` or the candidate-list
builder's success state.

Task-11 case `0x211d44` sets flag `0x20` in its controller object. When
controller substate is not 2 it forwards a separate four-byte object to task
17. At substate 2 it passes `0x1395` through central decoder `0x210da8`;
the decoder's `0x211346` arm immediately sees that same substate and emits a
four-byte task-12 status-`0x13a5` object. The exact type-`0x8c` path reaches
neither the bitmap builder, the type-`0x09` candidate-list builder nor
`CHANNEL_CONFIGURE`. It remains a bodyless non-bitmap-context notification,
not a claimed candidate-list acknowledgement.

There is a narrower relationship to one populated type-`0x1a` request path:
builder `0x20e9cc` reads the same byte `0x1090ff`, distinguishes values 6
and 7, and uses that state to populate request-object bytes `0x12..0x13`
before its `0x20ea06` constructor call. The other type-`0x1a` builder is a
separate path. Shared state therefore relates one request form to type
`0x8a` acceptance, but the exact MCU image supplies no direct
request-to-report edge and no payload discriminator that would make
`0x8a` a search terminal.

Task 11's controller dispatcher recognizes the numeric status set
`0x1389`, `0x138a`, `0x138c`, `0x138e..0x1390`, `0x1392..0x1397`,
`0x139f` and `0x13bb`. Its branches establish fixed object sizes and four
numeric control calls: `(0x67, 2)`, `(0x72, 2)`, `(0x71, 2)` and
`(0x68, 2)` for statuses `0x1389`, `0x138a`, `0x138c` and `0x1397`
respectively. The exact-image output records those numbers and state gates,
but deliberately assigns no protocol names to the otherwise unidentified
statuses. This closes more of the MCU-side lifecycle graph while leaving DSP
initiation and complete report ordering unproved.

The type-`0x86` path is independently unsuitable as a shortcut to that
acquisition terminal. Handler `0x27fa34` dispatches on its first report-body
byte with evidenced values `0x70`, `0x80`, `0xb0` and `0xb1`; its principal
outputs are task-3 objects and numeric internal notifications. Their meanings
are not assigned by the exact image, and this handler does not establish the
missing `0x13b8 -> 0x13aa` controller edge.

The emulator configuration now reflects this evidence directly. Radio
`wire_profile` selects only packet representation (`bitmap_search` or
`candidate_list`), while `acquisition_profile` selects the product-specific
sequencing and timing policy (`nse8` or `nhm5`). The 3210 and 3310 select both
halves of their proven contracts. The 6110 selects the proven shared
`bitmap_search` wire format but leaves acquisition policy `none` and keeps the
peer disabled. This prevents a future caller from treating identical bitmap
packing as proof of the entire NSE-8 search lifecycle, while avoiding a copied
NSE-3 packet codec.

### External-service transport and application boundary

NSE-3 also independently establishes the reusable external-service transport
boundary without establishing a complete peer lifecycle. DSPIF report
types `0x8d` and `0x8e` stay outside the radio-report jump table and pass
through wrapper `0x273e3e` to task 9. The exact task-table entry at
`0x2b751c` points to `0x273b2d`; its loop recognizes link-family bytes `0x1e`
and `0x1c`, and routes type `0x8e` to service-frame parser `0x273368`.

Startup independently authors a seven-byte discovery frame. Constructor
`0x273878` writes caller-supplied family `0x1e`, destination `0xff`, source
`0x00`, class `0xd0`, control word `1`, and byte 6 value `1`, then submits it
through the ordinary service transport. This is the same transport/discovery
shape implemented by the generic `nokia_external_service_peer_device`; NSE-3
does not need a copied framing component.

The application-frame boundary is independently shared too. Constructor
`0x237a12` allocates payload length plus nine bytes, writes the learned
destination and source nodes, zero at byte 2, class `0x40` at byte 3,
big-endian length at bytes 4--5, the learned control byte at byte 6, value 1 at
byte 7 and the application command at byte 8. Handler `0x2398d6` accepts
channel-map command `0x70`, applies its map through `0x2398a0`, and returns a
one-byte `0x70` acknowledgement containing that result. Its alternative path
disables the map and returns an empty command-`0x71` acknowledgement.

The map consumer independently fixes the reusable data boundary. It rejects a
frame whose low length byte is at most `0x42`, addresses the map at queue
object byte 9, and passes exactly `0x40` bytes to store `0x293a40`. It then
feeds the stored map and two associated runtime bytes into activation call
`0x29ffc2`, returning success. This proves a shared 64-byte map parser; it does
not prove that NSE-3's peer advertises the NSE-8 startup bits for service
channels `0x5f` and `0x62`.

NSE-3 also constructs command `0x64` with a nine-byte body at `0x239cfc`.
The result field at body offset 1 is dynamically selected as zero or one from
bit 6 of a runtime flag byte, while the seven fixed product bytes are
`30 08 01 01 01 1f 20`. NSE-8 uses `45 0d 01 01 01 1b 58` in the
corresponding frame. The command vocabulary and frame helper can therefore be
shared, but the status body is genuine typed product configuration rather
than a portable 3210 fixture.

The caller graph separates that result from the publisher's control argument.
Class-`0x40` dispatcher `0x239ef4` reads the command at byte 8. Command
`0x64` passes incoming body byte 0 (object byte 9) to the status publisher,
while commands `0x70` and `0x71` alone route to the map enable/disable
handler. A separate dispatcher maps internal event `0xd3` to a path that can
publish status with control argument 2: once when a runtime flag bit is set
(clearing that bit), and again when a separate byte counter reaches 15. The
external image establishes these numeric conditions but not the meaning of
the control argument, flag, counter or event.

The result-source byte is now bounded more precisely. Exact v4.06 code reaches
SRAM `0x10fde1` both through direct literal roots and as offset `0x69` from
controller root `0x10fd78`. It is not used as one Boolean verdict:

- two paths test bit 2 and can clear it;
- internal event `0xd3` tests and clears bit 4 before one status publication;
- two independent external-service gates test bit 5; and
- the command-`0x64` result derives from bit 6, which three paths can clear.

An independent v5.48 HLE profile calls this numeric address its DSP
self-test/verdict cell. The address overlap is useful corroboration, but the
v4.06 consumers prove a multi-bit controller contract and do not establish
those bit meanings, their initial value, or portable revision semantics.
Consequently the driver does not acquire a writable `verdict` hook and must
not force this byte to pass startup. Any future peer must cause the
firmware-observed transitions through the recovered DSP/service protocol.

One such transition is now exact. In task 2, controller bit 2 enables a
special intercept for command `0x64` whose first body byte is 2. The intercept
sets controller bit 4, arms timer `0x13` with raw duration `0x19`, and skips
the ordinary command dispatcher. Timer-table record `0x2b7668` is
`03 02 00 00 00 00 00 d3`: flags 3, task-2 ownership and expiry event
`0xd3`. When that event arrives with bit 4 still set, the established
consumer clears the bit and publishes the typed command-`0x64` status with
control argument 2.

Timer `0x13` is not dedicated to this exchange. A complete arm census finds
four callers with raw durations `0x036e`, `0x01f5`, `0x0019` and `0x007d`;
the command intercept is only the `0x0019` arm. This proves an organic,
timer-mediated response path without proving the timer unit, meanings of
controller bits 2/4, or which missing DSP condition sends the initiating
command. A future service peer can implement that initiator only when its
trigger is independently evidenced; it need not and must not poke the
controller byte.

The prerequisite bit-2 path is also firmware-owned. DSPIF setup writes mode
byte `0x10b970` to zero. Handler `0x285e7c`, reached from its sole direct
dispatch call at `0x29f31a`, promotes that byte to one only when its current
value and shared halfword `0x100e4` are both zero. Getter `0x297104` returns
the byte unchanged and has exactly two direct callers.

Task-2 initialization is the only caller of initializer `0x237a7a`. When the
getter returns one, it sets controller bit 2 and uniquely submits the fixed
task-3 object at `0x2b9be8`. Its first six normalized storage bytes are
`02 00 70 02 00 0d`: status halfword 2, while lane-correct MCU byte reads give
declared stream length 2 and stream `70 0d`. It therefore publishes type
`0x70` with one-byte body `0x0d`. Task 3 applies no type-specific branch:
status 2 queues the object and the common output loop passes byte 2 as length
and object +3 as source to DSPIF TX writer `0x285746`. The request therefore
reaches the DSP byte-stream boundary organically.

This supplies an upstream condition for the delayed command-`0x64` response.
It does not prove what the mode represents, who causes the shared `0x100e4`
condition, which DSP report follows `70 0d`, or that this raw DSPIF type
`0x70` is semantically the same as the later class-`0x40` application command
`0x70`. They remain separately typed transports until a trace or DSP-side
implementation connects them.

The independent ROM4 HLE calls `70 0d` a self-test request and proposes a raw
type-`0x74` completion carrying `0d 00`. The exact v4.06 image supports a
narrower controller relationship, not that complete label or raw layout.
Task 2's sole family-`0x74` handler call reaches `0x237d60`, which dispatches
on queue-object byte 8. Selector values `0x0d` and `0xd0` share one arm; it
runs only while controller bit 2 is set, then clears that bit and consumes
bits 0 and 1 from object byte 9 into separate controller fields. Both status
arms also clear controller bit 6.

The response arm invokes one private helper and uniquely submits another fixed
task-3 object at `0x2b9bcc`. Lane-correct fields give declared stream length 2
and stream `70 0a`, so the generic task-3 path returns raw DSPIF type `0x70`
with one-byte body `0x0a`. This establishes a controller-gated inbound
type-`0x74` arm and organic DSPIF follow-up. It does not establish that raw
ring payload `0d 00` answers the earlier `70 0d`, or that the operation is a
self-test.

The exact DSPIF RX envelope in fact rules out that compact reply for v4.06 as
written. RX allocates raw payload length plus four, stores the ring type at
queue-object byte 3 and copies raw payload from byte 4. A two-byte raw payload
`0d 00` therefore reaches bytes 4--5, not the selector/status bytes 8--9 used
by `0x237d60`.

The missing transformation is independently visible in the proven NSE-8
v6.00 image. Its type-`0x70..0x7f` decoder at `0x29bc00` preserves the raw
type, inserts frame bytes `00 (compact_length + 2) 01 00` at queue-object
bytes 4--7, and copies the compact DSP payload to byte 8. For compact payload
`0d 00`, that produces the six-byte expanded payload
`00 04 01 00 0d 00`. Supplying those six bytes directly through NSE-3's
untranslated DSPIF receive path places selector `0x0d` and status `0x00` at
the exact offsets consumed by `0x237d60`.

The NSE-3 controller proves the request/completion correlation independently.
In the same initializer, the mode-one arm clears pending byte `0x10fcae`, sets
controller bit 2, submits `70 0d`, and later arms timer `0x14` for raw duration
`0xc8`. The timer table assigns that timer to task 2 event `0xd4`. The inbound
selector-`0x0d`/`0xd0` arm requires bit 2, cancels timer `0x14`, clears the bit
and emits `70 0a`; event `0xd4` is the alternative timeout path and clears the
same controller bits. This makes the expanded type-`0x74` message a
request-derived completion rather than an unrelated layout match.

The independent HLE may describe a revision whose DSP receive path still
contains the NSE-8-style decoder, while v4.06 expects the internal DSP to
provide the expanded frame. The generic HLE therefore expresses compact and
framed service-control completions as separate typed profiles. NSE-3 selects
the framed profile, but it remains dormant while the product's unresolved DSP
bootstrap keeps DSP service disabled. The firmware does not name the
transaction as a self-test, and this evidence does not establish the missing
DSP bootstrap publications, service discovery triggers or channel map.

This MCU-side grammar still does not establish who initiates the exchange,
NSE-3's registration/start delay, the advertised channel bitmap and services,
or the complete ordering around discovery and acknowledgements. Those facts
belong to the unavailable internal DSP side. The product therefore continues
to leave the external-service peer disabled: receiving and acknowledging
commands is not evidence for synthesizing their triggers or contents.

Task 9 nevertheless constrains the MCU controller around that boundary. Its
main loop dispatches the contiguous event values `0x011c..0x0120` to
`0x273ab4`, `0x273a48`, `0x273a00`, `0x27395a` and `0x273256`,
respectively. The external image does not name those events or establish their
timer units, so the verifier records the numeric mapping rather than assigning
service meanings.

Type-`0x8d` report flags increment three independent controller bytes at
offsets 3, 5 and 10. Their aggregate reaches a reset/rediscovery path at
`0x2738b2` when the selected family is `0x1e` and the sum is at least
`0x5a`, or when it is `0x1c` and the sum is at least `0xe6`. The reset clears
all three counters and can emit the already proven `0x1e` discovery frame.
The `0x011d` path has a separate retry byte at offset 1, continues while its
pre-increment value is at most ten, and can emit `0x1c` discovery. The
`0x011c` path increments a progress byte at offset 4 until family-specific
thresholds of two for `0x1e` or ten for `0x1c`.

These are product controller policy, not portable peer delays. They further
bound how NSE-3 reacts after task events and DSP reports, but do not prove when
the missing DSP produces either. Accordingly they remain exact-image
verification data rather than a scripted HLE lifecycle.

### DSP parameter selector 8

The v4.06 external firmware also closes the encoding boundary for DSP
parameter selector `0x08`, but not yet its organic call-state lifecycle.
Generic writer `0x285b7c` bounds selectors to `0x00..0x2e` and dispatches them
through an exact jump table. Selector 8 preserves the low twelve input bits,
sets bit 15, mirrors the encoded word at SRAM `0x10b972`, and publishes it to
shared cell `0x100a8`:

```
shared[0x0a8] = 0x8000 | (value & 0x0fff)
```

One service-controlled mode routine at `0x2391bc` submits selector-8/selector-9
pairs from a nine-entry table. Every selector-8 input in that table is
`0x0600`, producing shared word `0x8600`; the paired selector-9 values vary.
This proves a real command codec and one service-mode caller, not that
`0x8600` means Answer, speech enable, or any particular audio route.

The ordinary DSP-parameter updater supplies a second, non-service path. It
constructs a live eleven-halfword parameter block at SRAM `0x10c020`, compares
it with the shadow block at `0x10c008`, then maps changed slots through the
exact selector sequence `08,09,1b,25,20,21,22,23,24,28,2d`; selector 8 is
slot zero. Within that controller updater, two branches construct the first
live halfword as `(previous & selected_mask) | indexed_value`, using separate
tables at `0x2b8608`/`0x2b85f8` and `0x2b861c`/`0x2b860c`. Successful
publication copies the live halfword into its shadow. This proves that command
8 participates in product-specific controller-state publication; it does not
prove which higher call-control transition selects those table entries or
what speech meaning, if any, belongs to their bits.

The upstream dispatcher is now bounded as well. An exact ten-entry table at
`0x257f40` covers messages `0x076f..0x0778`. Within it,
`0x076f/0x0770`, `0x0773/0x0774`, and `0x0777/0x0778` respectively
set/clear masks `0x02`, `0x04`, and `0x10` in controller flag cell
`0x10ae9f`, then immediately call the parameter-state updater. Messages
`0x0775/0x0776` are explicit no-ops in this table; the other two entries
route to separate handlers. This establishes paired event lifecycles on the
MCU side without naming any pair as call Answer, End, or a DSP speech route.

The producer side is bounded separately. Exact relocated signatures identify
NSE-3's task-5 render post at `0x29e556` and generic packed-event generator at
`0x29e604`. A whole-image call census recovers 946 direct calls to those APIs
and resolves the packed event at 938 of them. None of those 938 publishes any
event in `0x076f..0x0778`. Eight callsites are runtime-built:
`0x2524ce`, `0x252c3e`, `0x252e7c`, `0x252f76`, `0x253552`,
`0x25a1f8`, `0x25a87e`, and `0x2a3472`. Seven are now bounded without
assuming a runtime state. The first four forward one common stored event
field: all direct constructor inputs are enumerated, its self-reload does not
enlarge the value set, its extended wrapper supplies `0x0578`, and the only
otherwise forwarded input comes from nine fixed dispatcher calls
(`0x0b55..0x0b61`). The fifth forwards the extended wrapper's sole completion
event, `0x0268`. `0x25a1f8` locally chooses only `0x038c` or `0x038e`.
Finally, `0x2a3472` uses a byte selector plus a binary input to address two
16-byte records per selector; exhaustive inspection of all 512 addressable
event halfwords finds no member of `0x076f..0x0778`.

Only `0x25a87e` remains unresolved. It selects an event from either a
runtime-owned object record or the SRAM table rooted at `0x1061a4`. Startup
first clears `0x100020..0x10c507`, then applies the 109-entry counted copy
table at `0x2a5008`; no copy record covers `0x1061a4`, so the event table is
proven zero-initialized rather than treated as unknowable initial SRAM.

The table's population boundary is also recovered. The sole constructor at
`0x25a4f0` has capacity 80 and copies event halfword `descriptor[0x12]` into
the selected 28-byte SRAM record. A whole-image census finds 116 direct calls:
81 use fixed ROM descriptors, five use descriptors reconstructed from the
startup SRAM copy image, and 30 construct or select descriptors at runtime.
None of the 86 fixed descriptors supplies `0x076f..0x0778`. Exact local
construction and forwarding analysis now bounds 29 of the 30 runtime
descriptors as well; their complete value sets also exclude the target range.

The last descriptor is registered at `0x256e2e`. Two branches explicitly
store events `0x13cf` and `0x13ce`, but the branch at `0x256e0c` leaves
`descriptor[0x12]` unwritten. Its 28-byte allocation comes from the general
free-list allocator at `0x260abc`; that routine returns the selected block's
payload at header plus eight without clearing it. The missing store therefore
cannot honestly be promoted to event zero: a reused block may retain an older
halfword. This uncertainty is field-specific: every loop iteration explicitly
stores the zero-extended byte index `r7` into value field `+0x0c`, so this
descriptor cannot retain or introduce a ROM catalogue address there. The
remaining evidence frontier is the unwritten event halfword plus the other
runtime-object record populations. Both require further producer tracing or a
bounded runtime trace. The exact verifier still does not claim producer
absence or that the paired events are dormant.

The alternative representation is no longer structurally opaque. Its
six-entry group table begins at `0x106a64` and is also zero-initialized by
startup: none of the same 109 copy records supplies it. Group byte `+9`
selects the representation. Zero follows the group's runtime pointer, indexes
a 24-byte object record with group byte `+6`, and reads the event halfword at
record `+0x0e`; nonzero follows the registered 28-byte record chain described
above and reads `+0x12`. This establishes the exact object/event boundary, but
not by itself the values later installed through the mode-zero runtime
pointer.

The mode-zero constructor at `0x25b0cc` is now censused independently. Its 45
direct callers include 44 fixed-ROM object inputs representing 35 unique
catalogues. Parsing each catalogue's count and 24-byte record array enumerates
124 addressable event halfwords; none is in `0x076f..0x0778`. The sole
runtime-object call is `0x27c17c`, which loads its object pointer from SRAM
cell `0x10b284`. That cell is also absent from the startup copy table and
therefore begins at zero, but later firmware message handling writes it.

The writer is now bounded to packed event `0x0389`. Its dispatcher case loads
cell `0x10b284[0]` and invokes the constructor, while the common packed-event
argument copier stores the event's arguments into that cell. A whole-image
call census finds exactly three direct producers of `0x0389`: callsite
`0x231660` supplies fixed arguments `(0, 0)`, `0x25a8fa` supplies two
initially runtime-derived values, and `0x25b044` supplies one runtime value
followed by zero. Backward entry analysis further bounds `0x25a8fa`'s second
argument: its enclosing emitter preserves the entry argument in `fp`, all
four direct entries supply `0xff`, and the image contains no stored Thumb
pointer to that entry. Its arguments are therefore `(record[+8], 0xff)`.
The first producer therefore cannot introduce a non-null object pointer; the
other two producers share one genuinely dynamic input shape, record field
`+8`, and it is not reinterpreted as a fixed catalogue address. This reduces
the alternative-object frontier from an unspecified population to one
identified record field reached through two producers. With the internal boot
ROM still missing, a passive MAME trace cannot reach this firmware boundary
organically, so that value is not fabricated through a direct-to-flash reset
bypass. The exact verifier still does not claim producer absence.

The fixed value population is now checked rather than inferred from that
field's apparent use. The 124 fixed 24-byte records contain 32 distinct
`+8` values, including eight values that are addresses of other ROM
catalogues. Thirty-one records satisfy the static flag half of
`0x25a8fa`'s producer condition, but none of those 31 contains one of the
eight ROM catalogue addresses. No fixed record sets the `0x40` flag that
gates producer `0x25b044`. Independently, none of the 81 fixed-ROM
registered descriptors contains a ROM address in its corresponding `+0x0c`
field. Thus a valid non-null catalogue input reaching event `0x0389` is not
proven by any fixed record; it would have to come from a runtime-built record.
Small fixed values are retained as opaque values rather than labelled as
pointers or forced through the constructor.

The two runtime descriptors that explicitly carry stored event `0x0389` are
also complete stack templates, not unknown heaps. Registration call
`0x22d686` supplies value field `+0x0c = 0x13`; call `0x22d6a0` supplies
`+0x0c = 0x10`; both clear flag byte `+0x18`. Neither value is a catalogue
address. This excludes those conspicuous event-labelled descriptors as a
valid non-null constructor source, but does not exclude another runtime-built
descriptor becoming the current record before producer `0x25a8fa` runs.

The valid non-null constructor source is now recovered independently.
Registration call `0x278792` builds a 28-byte descriptor with stored event
`0x0387`, value field `+0x0c = 0x2b01d8`, and flag byte `+0x18 = 0x40`.
That flag is the exact gate at producer `0x25b044`; the producer emits packed
event `0x0389` with `(0x2b01d8, 0)`, and dispatcher call `0x27c17c`
passes the first argument to constructor `0x25b0cc`. This is an organic
firmware data path, not a forced SRAM value.

Catalogue `0x2b01d8` is itself bounded: it addresses nine 24-byte records at
`0x2b00e8`. Their event sequence is `00dc, 05e0, 05e0, 05e0, 0387,
05e0, 05e0, 0387, 01f4`; none lies in `0x076f..0x0778`. Two records
refer to nested ROM catalogues `0x2b00dc` and `0x2b043c`, but neither the
catalogue header nor their record flags satisfies an `0x0389` producer gate.
The exact verifier parses this catalogue rather than accepting the addresses
as labels.

A field-specific pass over the other runtime descriptor constructors bounds
their `+0x0c` values to small integers, SRAM/heap pointers, or ROM values
whose producer flags are clear. The final parser-built descriptor at
`0x28b628` is now bounded by reproducing its PPM traversal. Firmware root cell
`0x2beae8` names the identified PPM at `0x2c0000`; its top-level walk reaches
`TEXT` at `0x2c2cd0`, then advances through aligned child lengths. The nine
non-terminal child values are `33, 01, 02, 0d, 0f, 18, 1a, 1b, 13`.
Firmware skips `0x33`, registers the remaining eight and stops at the zero
child at `0x2f8658`. None can name a ROM catalogue. The exact verifier
replays the root, top-level and child walks and no runtime descriptor value
source remains unclassified.

This closes the alternative-object value provenance, not the independent
event provenance. The reused descriptor at `0x256e2e` can still retain an
unwritten event halfword, so producer absence for `0x076f..0x0778` is not
claimed.

The allocator reuse boundary is now measured directly. A whole-image census
finds 1,105 direct calls to allocator `0x260abc`; 924 sizes are statically
resolved, 181 are runtime values, and nine resolved calls request exactly
28 bytes. The allocator compares the request against eight class thresholds
through a runtime-installed table, so equal request size is useful ownership
evidence but static firmware cannot prove that other request sizes never
share the class.

One exact-size owner makes the stale-halfword risk concrete. Call `0x28edfa`
allocates 28 bytes, `0x28ee0c` reads 28 bytes beginning at serial EEPROM
address `0x40`, and `0x28ef0a` returns the payload to the general allocator.
The callee is the same GenIO-backed serial-memory reader whose two-byte
address framing is already pinned above; it is not the separate class-`0x40`
external-service protocol. Consequently payload offset `+0x12` is EEPROM
data and can survive into the unwritten branch of descriptor `0x256e2e` if
the runtime class and allocation order permit reuse.

Those bytes are not immutable factory data. Message handler `0x238218`
recognizes request type `0xcb`, passes byte 9 directly to the field writer as
its selector and passes the payload beginning at byte 10. The writer
`0x28ecec` first reads the complete EEPROM range `0x40..0x5b`. Selector 4
decodes four payload bytes into range offsets `0x11..0x14`, which includes
the possible stale event at offsets `0x12..0x13` (EEPROM `0x52..0x53`), then
writes the complete 28-byte range back. Selectors 1 through 5 are accepted;
other values return status 3. The exact verifier pins the request boundary,
selector branch and paired serial-memory transactions. A whole-image direct
caller census finds only call `0x23822a`, reached from the central inbound
dispatcher for message types `0xca/0xcb`; there is no separate direct
startup invocation of the field helper.

The whole-image direct-call census finds 44 calls to the serial EEPROM writer
`0x29ce12`. Thirty-two have statically resolved addresses and 12 retain
runtime-derived addresses. Of the resolved calls, `0x28edcc` is the only one
whose range overlaps EEPROM `0x52..0x53`. The runtime-address calls include
generic storage operations, so their presence prevents a stronger static
claim that selector 4 is the only possible writer or that no earlier
lifecycle operation touches the bytes.

The NSE-3 EEPROM remains `NO_DUMP`, so neither the initial nor the
request-updated value is known. Static firmware also cannot establish the
allocator class/order at the later descriptor allocation. Therefore no
fixed value, zero-fill, or exclusion from `0x076f..0x0778` is claimed.
Recovering an EEPROM image would establish initial state, but a faithful
runtime conclusion additionally needs the ordinary message history and
allocation order that precede the descriptor branch.

No organic Answer or End transition has yet been connected to that state
slot. Product configuration therefore declares selector 8's proven
high-nibble-command/low-twelve-value decoder, but leaves the independent
speech-request mask/value policy empty. The HLE can observe an NSE-3
selector-8 publication without treating it as a request for media. In
particular, neither NSE-8's `0x0201` mask nor NHM-5's observed
`0x060b -> 0x040a` lifecycle may be inherited merely because all three
products use numeric selector 8.

The external image also proves the MCU side of a much larger bootstrap
transfer. Routine `0x2858fc..0x2859ff` samples 32,766 halfwords from its own
flash, beginning at reset entry `0x200040`, advancing by `0x20` bytes and
ending at `0x2fffe0`. It stages 63 complete 512-word blocks at shared
`0x10200..0x105ff`, followed by 510 more sampled words and two explicit
`0xffff` terminators in a final block. The resulting 65,536-byte staged stream
has SHA-1 `f708ffd71e430f41c47f12e18128cf4deffb5845`.

After each of these 64 transfer blocks, the MCU selects shared cell `0x100fe`
or `0x10100` from the low bit of the block index, writes zero to the selected
cell and waits for the opposite cell to become non-zero. After the final
exchange it additionally waits for `0x10002` to become non-zero. The verifier
pins the loop bounds, source/destination strides, literals, alternating
accesses, terminators and derived stream fingerprint. This establishes the
external firmware's transfer schedule without requiring a running emulator or
guessing through a missing peer.

Immediately before staging, the MCU writes a fixed seven-halfword shared
header:

| Shared address | Initial halfword |
| --- | --- |
| `0x100f6` | `0x0100` |
| `0x100f8` | `0x0300` |
| `0x100fa` | `0x0000` |
| `0x100fc` | `0x8000` |
| `0x100fe` | `0x0001` |
| `0x10100` | `0x0001` |
| `0x10102` | `0x0200` |

The two alternating synchronization cells are therefore initialized as part
of the same header rather than appearing as uninitialized RAM. Firmware also
sets bit 0 of MAD2 byte `0x20002` before the first staged block and clears that
same bit only after both final publications have been captured. The exact gate
pins both read/modify/write sequences and their bracketing helper calls. The
physical name of this bit and the meanings of the other header words remain
unestablished; they are a required wire contract, not inferred register
semantics.

The post-transfer results are no longer completely unconstrained. After the
final non-zero wait, the routine copies shared `0x10000` to SRAM `0x10b97a`
and shared `0x10002` to `0x10b97c`. The service-response handler passes request
byte 9 unchanged to its information formatter. Selector `0x0d` formats the
first captured word nibblewise as `B06`; adjacent selector `0x0c` reads MAD2's
ASIC-version byte at `0x20000`. This pairing independently agrees with the
documented Nokia 6110 service commands `0xc8/0x0d Get COBBA` and
`0xc8/0x0c Get system ASIC` in the
[Gammu Nokia 6110 protocol reference](https://docs.gammu.org/protocol/n6110.html).
This proves that selector `0x0d` exposes the captured result as `B06`, and that
the generic “DSP ready” value is wrong. It does not prove that `B06` is the
fitted COBBA revision: both v5.48 images retain the same required result while
an independently logged v5.48 ROM3 handset reports physical COBBA `B07`.
The protocol-name correlation is evidence for the service projection, not
enough to collapse bootstrap result, service label and silicon identity into
one typed value.

A separate later path compares that captured first result against exactly
`0x0b06` and branches away when it differs. The verifier pins the sole direct
bootstrap call at `0x2973f0`, both result captures, selector pass-through,
adjacent ASIC query, formatter and exact comparison. It also exhaustively
censuses direct literals: `0x10b97a` has only the formatter and comparison
references, while `0x10b97c` has none.

The second word is an offset inside the larger DSP state object at `0x10b970`,
so the verifier also enumerates all thirteen literal roots of that object.
Inspection of every directly rooted access finds one write to offset `+0x0c`,
the capture at `0x2859e8`, and no read. The external MCU image therefore has
no direct consumer of the second publication after capture. Indirect or
table-mediated addressing remains outside that negative result, and absence
of a consumer does not constrain the exact DSP-published value. It is,
however, required to be non-zero: `0x2859de..0x2859e2` spins on the same
shared `0x10002` halfword before capturing it.
Publishing the generic HLE ready word `0x0001` cannot satisfy all NSE-3
firmware paths.

The sole bootstrap wrapper at `0x2973c6` also rules out a hidden immediate
result check. It preserves a MAD2 byte read from `0x20001` before transfer,
calls the complete 64-block routine, then branches only on bit 0 of that
preserved byte. When clear, helper `0x28ef38` compares EEPROM word `0x74`
against zero and writes zero only when it differs. In both cases firmware
unconditionally continues through `0x260252`; it neither tests the bootstrap
return register nor reads either captured word first. Whole-image direct-call
censuses give each wrapper/helper/continuation edge exactly one caller. The
EEPROM update is therefore pre-existing MAD2 configuration housekeeping, not
a DSP completion-status response.

The surrounding service selectors also preserve the revision namespaces in
firmware rather than merely in our documentation:

- `0x03` (DSP external software) dereferences the flash pointer at `0x2ab52c`
  to the NUL-terminated identity at `0x286098`: revision `25.3.531`, date
  `17-Dec-97`, product line `NSE-3Nx`, and copyright marker `(c) NMP.`;
- `0x09` (DSP internal software) reads runtime buffer `0x10bcf0`. Startup
  clears it; the sole setter call accepts inbound report type `0x0a` or
  `0xc8`, converts message byte 11 to one ASCII digit and stores it under
  selector 9;
- `0x0c` (system ASIC) reads MAD2 register `0x20000`; and
- `0x0d` (COBBA) projects the bootstrap-captured `0x10000` word as `B06`.

The exact-image gate pins all four dispatch/source paths. A product profile
must therefore never use COBBA `B06` as a DSP software revision, MAD mask
revision or ROM3/ROM4 flash selector. Conversely, learning one of those other
identities cannot supply the missing DSP-side rule that publishes the COBBA
word.

The external-software string is stronger than the previously reported source
root: the verifier now checks the pointer value and every byte through its NUL
terminator. It identifies the DSP external-software namespace carried by this
v4.06 image; it does not by itself prove that the sparsely sampled 64 KiB
bootstrap stream is executable DSP code or that all of revision `25.3.531`
resides in that stream. Notably, the identity string address is not one of the
halfwords selected by the stream's `0x20`-byte sampling stride, so the string
is metadata rather than a byte-for-byte member of the derived stream.

The internal-software namespace has the opposite provenance. Exact anchors
pin startup's empty string, the report-type branches, byte extraction and
ASCII conversion, generic setter geometry and formatter read. A whole-image
direct-call census gives setter `0x28ead2` exactly one caller, `0x237dce`.
The external image therefore establishes the report grammar but contains no
fixed internal revision value. The service presentation may eventually render
the received digit as a ROM label, but neither `ROM3` nor any other digit may
be provisioned from the NSE-3 product name, MAD assembly, external revision
`25.3.531`, or the v5.48 handset log. A matching runtime report remains
required.

The report belongs to task 2 rather than the bootstrap routine. The exact task
table begins at `0x2b74b0` with 12-byte records; its third entry at `0x2b74c8`
is Thumb address `0x23a5cf`. That task's object dispatcher routes family
`0x74` from object byte 3 to `0x237d60`, except subcommand `0x32` at byte 8,
which has a separate branch. The accepted identity reports update the runtime
buffer, retain the ordinary success result and release the received object;
they construct no acknowledgement. Direct-call censuses pin the dispatcher
and setter edges as unique.

The MCU-side physical boundary below task 2 is also recoverable. DSPIF receive
routine `0x285794` consumes one DSP-to-MCU shared-ring record and constructs
the queue object in place. It writes fixed source byte `0x18` at object byte 0
and destination task `2` at byte 1, then copies the ring length to byte 2 and
the ring type to byte 3. The radio/DSPIF owner at `0x2a20dc` alternates its
ordinary task receive with DSPIF poll `0x28580a`; a returned ring object is
passed unchanged at `0x2a2224` through router wrapper `0x2a1f52`. Family
`0x74` therefore reaches task 2 across the DSP-to-MCU shared ring. It is not
emitted by the bootstrap responder or by the class-`0x40` external-service
peer.

This closes the MCU transport route, not the missing DSP implementation. The
external image contains no proof of the internal DSP routine that produces
type `0x74`, when it sends report type `0x0a` versus `0xc8`, or which internal
ROM digit it would report on the fitted handset. Consequently an NSE-3 DSP
peer may eventually publish this report through generic DSPIF, but it must not
invent its value or trigger policy. A matching internal image or a runtime
ring trace is still required for those DSP-side facts.

This remains deliberately MCU-side evidence. We do not yet know whether the
staged stream is DSP code, its DSP-side destination, how the DSP derives or
publishes the first bootstrap result, what the write-only second captured word
represents, or
what the intermediate non-zero acknowledgements mean.
The matching internal DSP image is absent. In particular, “64 transfer blocks”
is not interchangeable with the existing HLE peer's product-configured
completion counter: that counter embeds response policy, while this gate
establishes transfer geometry, non-zero waits and one captured-word
constraint. Consequently the NSE-3 profile still has no DSP peer, no guessed
ready value and no inherited NSE-8 or NHM-5 service grammar. The static JSON
records these unknowns so later work cannot silently promote the shared layout
into a working-handshake claim.

An independent implementation now provides a useful negative comparison.
[`djr-747/nokia-dct3-emulator`](https://github.com/djr-747/nokia-dct3-emulator)
commit `c50f7e272bf10c37a40c57174dbde84d9717b7e3` reports NSE-3 v5.48 booting
with a ROM4 HLE profile. Its profile deliberately does not run the recovered
5110 DSP image on the 6110, but supplies ROM4 ready value `4` and a prepared
NSE-3 external EEPROM. Running this repository's exact v4.06 candidate in that
revision's unmodified headless harness gives a narrower result:

```
ROM4_LOG=1 ./build/dct3_boot_trace \
  6110_nse3_v406_rom3_candidate.fls 100000000
```

The harness recognizes NSE-3 and services all alternating transfers, reporting
66 DSP acknowledgements. It nevertheless stops in the firmware's final wait at
PC `0x2859e0`, where `shared[0x002]` (`0x10002`) is still zero; no LCD command
or data has been written. This independently reproduces the precise frontier
of the static gate: transport acknowledgements alone do not supply v4.06's
post-transfer publications. It does **not** establish that the other
implementation is incorrect for its v5.48 image, or that v4.06 and v5.48 use
the same internal ROM.

A follow-up comparison used current upstream commit
`31c06f4954ed78abfdfb7c87a1e1ae9703ba9cab` (26 July 2026) and this
repository's exact recovered v5.48 images, without modifying the upstream
profile:

```
./build/dct3_boot_trace 6110_nse3_v548_rom3_ppmb.fls 100000000
./build/dct3_boot_trace 6110_nse3_v548_rom4_ppmb.fls 100000000
```

Both runs consumed the full 100,000,000-instruction budget without a wild-PC
or assertion stop. ROM3 reached 69 DSP acknowledgements and 3,588 LCD data
writes; ROM4 reached 69 acknowledgements and 5,160 LCD data writes. This is
reproducibility evidence for transport and for both external images'
ability to progress under that implementation. It is **not** an identity
observation: the upstream NSE-3 product profile supplies the same hard-coded
ready value `4` to both files. Local static analysis proves that each firmware
accepts any equal, non-sentinel `0x10004/0x10006` pair. The ROM3 run therefore
demonstrates that boot progress cannot distinguish `3/3` from `4/4`; it does
not overturn the matching real-handset `DSP ISw : ROM 3` record. Conversely,
the package label plus a ROM4 HLE value is insufficient to promote ROM4's
pair to `4/4` without a matching handset service report, DSP trace, or
internal-ROM analysis.

The same upstream HLE returns zero when the MCU parks final shared `0x10002`
at `0xffff`. Its hardware-bridge path describes that cell as a one-shot
fingerprint-verification verdict and permits non-zero results on other
hardware. Zero is therefore an implementation convention, not evidence for
NSE-3 v5.48's exact publication. The local completion profile continues to
leave the second word unresolved.

The exact-image census now closes the MCU side of that question. Each v5.48
image has fourteen literal roots for its product-local bootstrap state
(`0x10ba28` in ROM3 and `0x10ba38` in ROM4). Inspection of every root-owning
routine finds no state pointer passed to another routine and no read of
state offset `0x0e`; the loader's anchored `strh` capture is its sole MCU
access. Thus the external firmware cares that shared `0x10002` changes away
from the parked `0xffff`, but does not subsequently consume its numeric
value. This narrows the missing contract to a DSP-owned completion event. It
still does not justify publishing zero: zero would not satisfy the firmware's
wait, and the exact non-sentinel hardware verdict remains unknown.

The collaborator's prepared NokiX EEPROM is also ROM4-specific evidence, not
a generic NSE-3 factory profile. The exact flash record directories encode
eight-byte descriptors `{u32 id, u16 EEPROM offset, u16 length}`. ROM3's
directory at `0x2b8524` maps security state record `0x0701` to
`0x0358`/length 8 and settings record `0x0702` to `0x0360`/length `0x2c`.
ROM4's directory at `0x2b9838` instead maps them to `0x0380` and `0x0388`
with the same lengths. The `0x28` relocation is in EEPROM data layout, not
merely flash code.

The nested descriptors are relocated by the same amount: ROM3 maps
`0x070a` to `0x035c`/length 1 and `0x070b` to `0x035e`/length 2, while ROM4
maps them to `0x0384` and `0x0386`. The homologous validators at `0x29df76`
and `0x29fba2` independently sum sixteen identity bytes, add the byte in
shared security-level state `0x10fcf5`, truncate to sixteen bits and compare
against the stored halfword at revision-local state `0x10c62e`/`0x10c646`.
This proves the checksum relationship and its per-ROM storage without
assuming the EEPROM template's contents.

Current upstream provisions its shared NSE-3 blob using the ROM4 offsets
`0x0380` and `0x0388 + 0x1f`. Those offsets are now independently corroborated
for this repository's exact ROM4 image, but applying them to ROM3 would edit
different records. No upstream bytes are imported, and the setting-byte index
inside record `0x0702` remains runtime-sweep evidence rather than a
flash-derived fact. Local provisioning must therefore be selected by exact
firmware layout and must begin from a legally obtained matching 24C64 image;
an erased device cannot be turned into calibrated factory state by copying
only these security records.

The driver therefore types bootstrap completion separately from exchange
count, ping-pong transport and parked-loader status. Proven 3210/3310 profiles
retain their three ready words of `1`; NSE-3 v4.06 selects
`nse3_final_b06_second_unknown`. That profile publishes only the evidenced
`0x0b06` result at shared `0x10000` and deliberately leaves `0x10002` zero.
Enabling its DSP service can therefore neither turn the observed 64-block
geometry into the legacy ready-word publication nor invent a successful boot
before the second word's DSP-side semantics are recovered.

Both v5.48 images independently require the same final first result. ROM3
combines this partial completion with its separately evidenced pre-upload
`3/3` pair, but still halts at the unknown second final publication. ROM4
halts earlier because its pre-upload value remains unresolved. A regression
gate explicitly rejects both a BIOS-3 publication branch and an invented
`nse3_dsp_rom4_pair` profile. Conversely,
the physical `B07` report must not be substituted into the firmware
comparison. Product configuration keeps bootstrap publications,
external-firmware build, internal ROM identity and physical COBBA identity
separate until matching traces establish their relationships.

These findings do **not** prove that v4.06 matches F711604 ROM3, recover the
internal boot/DSP handshake, or promote `noki6110` to booting. The machine
therefore retains `NO_DUMP` internal ROMs and firmware-derived peers remain
disabled.

### Revision namespace guard

Four revision labels must remain separate during bring-up:

1. the MAD2 assembly/mask identity—Nokia's NSE-3 parts list calls F711604
   “MAD2 ROM3”;
2. the external flash build and its ROM3/ROM4 compatibility variant;
3. DSP external/internal software, mask-ROM and boot-protocol identities,
   retaining their separately evidenced sources; and
4. the physical COBBA identification, independently reported as `B07` on the
   real v5.48 ROM3 handset; and
5. the bootstrap-captured first result, which all three recovered external
   images require to be `0x0b06` and render as `B06`.

The real v5.48 handset log reports both `DSP SW 40.3.617` and `DSP ISw ROM3`,
plus COBBA `B07`, but does not establish that every numeric `3` or `4` in the
boot exchange names the MAD mask revision. In particular, an HLE responder
returning a ready value of four is not evidence that an NSE-3 contains “ROM4”,
and physical `B07` must not replace the exact `B06` firmware comparison. Product
configuration must eventually type these identities separately; until a trace
or matching internal dump connects them, `noki6110` declares none of the
firmware-derived values.

Any later v5.48 replacement or comparison baseline must:

1. be identified as Nokia 6110 NSE-3, not the later 6110 Navigator;
2. retain the original archive and member names outside the repository;
3. record byte size and SHA-256 for every source member and normalized image;
4. distinguish ROM3 and ROM4 compatibility;
5. account for the documented 1 MiB program-flash extent;
6. keep EEPROM content separate from MCU/PPM code;
7. pass static vector/reset inspection before it is declared as a MAME BIOS.

The firmware and any extracted proprietary data remain git-ignored.

## Staged executable acceptance

Once a baseline is present, promotion proceeds in this order:

1. **Declared hardware (implemented):** TE28F800, 64 KiB SRAM, 8 KiB EEPROM,
   UE4 display and keypad, MIC2/EAR topology, and the documented PCM shape.
2. **Boot/DSP boundary:** the generic shared-ring transport is established;
   next recover the organic reset path, ROM3/ROM4 identity, DSP upload and
   handshake census. Any HLE service remains disabled until its packet grammar
   is observed.
3. **Interactive:** firmware-rendered idle UI plus physical softkey,
   Send/End, navigation and digit acceptance.
4. **SIM/radio:** recover the NSE-3 SIM transaction and radio packet profiles;
   do not select either the NSE-8 bitmap search or NHM-5 candidate-list grammar
   by resemblance.
5. **Call control:** registration, paging, ringing, physical Send/End and clean
   teardown, with firmware-owned RR/MM/CC state.
6. **Media:** recover the NSE-3 speech-control lifecycle, then enable its
   independently documented PCM profile and prove bidirectional GSM-FR,
   FACCH/BFI/SACCH coexistence and save-state replay.
7. **Physical duplex:** connect only MIC2 and EAR, with neutral host scale until
   product gain programming is recovered. Hardware-faithful status remains
   `Partial` while analogue gain/mux control is opaque.

This order permits shared DCT3 devices and GSM protocol layers to be reused,
while ensuring that no 3210 or 3310 firmware contract is silently inherited.
