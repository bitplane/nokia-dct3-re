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
v5.48 and describe an MCU + PPM B archive of about 1.07 MiB. They are discovery
leads, not identity evidence. The currently indexed firmware.center NSE-3
directory is empty, and surviving free links terminate at obsolete hosts.

Internet Archive's `Nokia_DCT3_firmwares` collection supplies an earlier,
original `Nse-3_v4.06.exe` service package. Its independently hashed Wintesla
members normalize reproducibly to exactly 1 MiB: MCU
`0x200000..0x2bebff`, an erased gap through `0x2bffff`, and PPM B
`0x2c0000..0x2fffff`. The normalized image has SHA-1
`5025a6ac3b4a13714211fde903f27f92cbb7c9b6`; full source/member hashes and the
reproduction command are recorded in `roms/README.md`.

This is a real firmware-analysis baseline, but not a boot promotion. Its
unprefixed `V 4.06` version spelling is consistent with contemporary ROM3
version tables, so the BIOS is explicitly labelled “ROM3 candidate”. The
matching F711604 internal boot/DSP ROMs and 24C64 contents remain `NO_DUMP`.

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
length 68. The ordinary constructor at `0x20faec` derives and packs its search
set; the second at `0x216f72` emits the same type and length from a separate
controller path. Task-side code reads the object length at `+2`, passes
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

The MCU-side initial-arm path is now bounded too. A conditional type-`0x80`
report branch constructs a `0x40`-byte task-12 object with status `0x139e`
and preserves 24 report bytes. Task-12 case `0x217418` sends it to candidate
updater `0x2124a8` when state byte `0x106b08` is not 2 and mode byte
`0x106af6` is 2. That updater contains the sole non-consumer timer-`0x6b`
arm at `0x212558`, further gated by a nonzero byte at `0x106afd`. This closes
the external-MCU path from a received report into the recurring measurement
timer, but not the missing DSP-side conditions that emit the type-`0x80`
report or the meaning and unit of its timing. The raw durations therefore
remain exact-image policy evidence rather than enabled peer timing.

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

Inbound type `0x89` reaches handler `0x2804f4`. It gates acceptance using
controller state, converts the packet into a fixed eight-byte task object,
posts status `0x1393`, and advances its controller state to 3. It does not
read a confirmation body byte before that transition. Type `0x84` remains a
separate fixed-size `RA_INFO` event with task status `0x1394`; the task
dispatcher keeps the two paths distinct. This establishes the
`CHANNEL_CONFIGURE -> CHANNEL_CHANGED_CNF` envelope and shows that the direct
NSE-3 confirmation handler does not require the product-specific body-bit
correlation recovered for NHM-5. It does not, by itself, prove which DSP
report follows next in every controller state.

The surrounding report-to-task boundary is now exact enough to prevent an
NSE-8 sequence from being inferred from that single confirmation. Type
`0x84` posts status `0x1394` to task 11; types `0x87`, `0x89` and `0x8a`
post `0x138f`, `0x1393` and `0x1390` there as fixed eight-byte objects.
Type `0x83` selects between task-11 status `0x139f` and task-12 status
`0x13a0` using runtime state, preserving a `0x2c`-byte object. Task 12
separately receives type `0x88` as status `0x13ac` in 16 bytes, type `0x8b`
as status `0x13b8` in `0xa8` bytes, and type `0x8f` as status `0x13b7` in
eight bytes.

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
The defensible MCU-side meaning is therefore **COBBA identification B06**, not
“DSP ready” or “DSP software B06”.

A separate later path compares that captured COBBA word against exactly
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
publishes the COBBA identity, what the write-only second captured word
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
4. the independently queried COBBA identification, which v4.06 renders as
   `B06`.

The real v5.48 handset log reports both `DSP SW 40.3.617` and `DSP ISw ROM3`,
but does not establish that every numeric `3` or `4` in the boot exchange names
the MAD mask revision. In particular, an HLE responder returning a ready value
of four is not evidence that an NSE-3 contains “ROM4”. Product configuration
must eventually type these identities separately; until a trace or matching
internal dump connects them, `noki6110` declares none of the firmware-derived
values.

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
