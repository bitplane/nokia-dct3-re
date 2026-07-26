# DCT3 model coverage

This matrix records demonstrated product coverage, not family resemblance. A
cell is promoted only by a named reproducible gate or reviewed hardware or
firmware evidence. `Partial` means that the preceding acceptance level works
but a material hardware contract remains calibrated, opaque, or unverified.

| Product / tested firmware | Booting | Interactive | Registered | Call control | Internal media | Physical duplex | Hardware-faithful | Principal evidence or next boundary |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Nokia 3210 NSE-8 v6.00 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Radio lifecycle, paired GSM-FR/FACCH/degraded-media and isolated physical audio gates pass. The cross-model type-`1f` gate independently finds four NSE-8 constructors for the shared status-2, four-byte task-3 envelope; its flag/value policy remains product-owned. Real COBBA DSP-controlled mux/gain semantics remain opaque. |
| Nokia 3210 NSE-8 v5.01 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Independent ROM gates cover the call lifecycle and isolated physical duplex. The same COBBA/DSP limitation applies. |
| Nokia 3310 NHM-5 v6.39 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | `verify-dsp-bootstrap-3310`, `verify-3310-frontier`, `verify-3310-navigation`, `verify-3310-radio-registration`, `verify-3310-radio-paging`, `verify-3310-radio-incoming-call-boundary`, `verify-3310-radio-incoming-call-ui`, `verify-3310-radio-incoming-call-lifecycle`, `verify-3310-radio-media-resilience`, `verify-3310-radio-physical-duplex`, and `verify-dct3-type-1f-static`. Its sole type-`1f` constructor is a parameterized four-bit builder for the shared status-2, four-byte task-3 envelope, rather than inherited NSE-8 policy. The frontier/navigation fixtures now resolve BIOS `639` to MAME namespace `noki3310_3`, reconstruct mutable flash/EEPROM/SIM state for every non-persistent run, reproduce clean idle hash `5871dd93…` in independent directories and wait for the firmware redraw before asserting the exact return-to-idle frame. Its typed `0x56` candidate search selects and camps on ARFCN `0x0058`; SI1--SI4, Random Access and Immediate Assignment remain firmware-owned. A correctly addressed page organically produces RACH and Paging Response. On the dedicated link v6.39 accepts Cipher Mode Command, publishes its distinct DSP type-`0x14` body and emits Cipher Mode Complete. Product-specific pacing after acknowledged MM Information lets its firmware-owned time-update transaction finish before SETUP. Continuing the assigned-channel receive schedule after the SETUP acknowledgement lets its queued LAPDm messages emerge organically as Call Confirmed and Alerting. Its TCH/F channel-change context `0x0402/00/01` independently requires confirmation bit zero, unlike the assigned-SDCCH context `0x0402/01/01`; satisfying that typed contract produces the new-main-link SABM and Assignment Complete. It presents the incoming caller, physical Navi emits one Connect and changes the UI from Answer to End, and the network's Connect Acknowledge reaches the handset as a good FACCH block. A queued protocol response pre-empts stale idle-poll backoff while preserving the evidenced NHM-5 MM-information settling interval. A second physical Navi organically drives Disconnect, network Release, Release Complete and traffic-link release; its independently observed release CHANNEL_CONFIGURE carries product byte `0x14`, receives the zero-body confirmation and returns to the idle PCH schedule. The same lifecycle independently publishes DSP command `0x08/0x060b` at Answer and `0x08/0x040a` at End. Nokia's NHM-5 schematic establishes built-in MIC2 and differential EAR endpoints without NSE-8 gains; system-module pages 27--28 establish the distinct 1 MHz/125-clock, 8 kHz, 13-bit-in-16 PCM profile. The organic media gate carries 150/146 encoded-uplink/decoded-downlink frames and 147 non-silent COBBA blocks before clean End. Its resilience gate resumes active speech through exact save-state replay, recovers from 56 impaired bursts per direction and resulting BFIs, proves bidirectional FACCH substitution/recovery, and observes 33 correctly phased SACCH/TF reservations while speech advances. The isolated physical gate routes a host source only through MIC2 and captures only EAR: all 250 microphone blocks are non-silent, the network decodes non-silent unclipped uplink, and playback contains a 5.36-second 1 kHz run. Product-specific analogue gain programming remains unproved. |
| Nokia 3330 NHM-6 v4.50E | Yes | Yes | No | No | No | No | No | `verify-3330-frontier` and `verify-3330-navigation`; later product contracts are not established. Its lower external-service transport remains available, but the typed application profile is `none`; the frontier gate still passes without inheriting NHM-5 registration delay, status body or channel bitmap. |
| Nokia 3410 NHM-2 v5.46E | Yes | Yes | No | No | No | No | No | `verify-3410-frontier` and navigation/menu gates; later product contracts are not established. It inherits the same conservative `none` external-service application profile, and its frontier gate still passes without the 3210/3310 script. |
| Nokia 6110 NSE-3 v4.06 PPM B (ROM3 candidate) | No | No | No | No | No | No | No | The declared `noki6110` machine expresses the manual-backed 1 MiB flash extent as the independently reported Intel 28F800B3-T (`0089:8892`), plus a 64 KiB SRAM map, serial 8 KiB 24C64, 84 x 48 display, firmware-verified UE4 five-by-five keypad, MIC2/EAR routes and 1 MHz/8 kHz 13-in-16 PCM profile. Exact-image static gates now cover reset/vector/SRAM, direct MAD2 extent, keypad, GenIO bit-2 EEPROM clock/two-byte address framing, the generic MAD2 SIMI controller surface, its higher ATR/interface-byte parser, PPS selection and T=0 status-family classifier, generic DSPIF ring geometry, independently routed external-service framing and discovery, the MCU's 64-block bootstrap transfer, and the radio packet envelope. NSE-3 independently constructs two type-`1a`/length-68 forms. Constructor `20faec` builds and submits a populated bitmap, maps ARFCN 1 to payload byte 65 bit 0 and arms post-submission timer `1b` with raw duration `09cd`; task 4 is the shared DSP/radio dispatcher and arms the same timer at entry. Event `db` rearms and clears a task-wide processed-input counter when activity occurred, or enters separately gated system handling when idle; the timer is not search-specific. Constructor `216f72` instead zero-fills the packet, sets control byte `13` and is reached from non-radio status `0445` after private-buffer reset; it is not treated as a second organic search. Its bounded inbound dispatcher routes the established `80`, `83/84`, `86..8c` and `8f` radio-report vocabulary; type `8b` preserves the 166-byte, forty-record result geometry and posts task-12 status `13b8`, whose distinct ingestion case copies the 160-byte body. The `13b8` direct call graph selects product-local processors `2143f4`, `214494` and optional forwarder `214670` by runtime mode/state, but constructs no direct DSP request. The state-4/6/7 measurement consumer belongs to later status `13aa`; the external image does not prove a direct `13b8 -> 13aa` edge. Timer table `2b75d0` maps code `6b` with flags `03` and owner task 12 to fixed status object `13aa`; together with the `13aa` entry and consumer rearm, this proves the recurrence loop and its raw durations `0eb4` in state 4 and `04e6` otherwise. Type `80` is a discriminator multiplexer, and only report byte-4 value `40` reaches task-12 status `139e`; its object preserves a 24-bit value, two 16-bit values, two controls and 24 further report bytes. Task 12 selects either a state-2 helper path or, only in mode 2, generic candidate updater `2124a8`. That updater has two callers, uses `0x44`-byte records at `106d3c`, owns the only non-consumer timer-`6b` arm, and directly calls no radio constructor or task-3 submit API. The other discriminator arms are independently bounded: `50` uses controller helper `28000c` with possible task-13 status `13c8`; `60` validates timing and can post task-14 status `040b`; `70` posts compact task-12 status `13d0`; its unique producer carries the shared 24-bit value at +4, and the explicit task-12 case adds `68`, stores the result in candidate controller `106aa4` + `20`, and conditionally enters the timer-`6e` recheck helper. Its unique original-report follow-up can publish numerically bounded objects to tasks 3, 13 or 14. The task-3 status-2 queue proves a four-byte `1f 00 flags value` byte-stream profile. The cross-model gate independently establishes the common `1f 00 flags value` envelope in 3210, 3310 and 6110 while keeping policy product-specific. The NSE-3 register-independent census finds five constructors: argument-1 branch flags `04`, status-`03f3` flags `01`, status-`03f0` flags `0d`, status-`03ee` flags `0c/0d`, and this report-driven flags `01/06/07`; byte 7 is local metadata outside the payload. Task-17 states 21, 20 and 22 publish `03ee`, `03f3` and computed `03f0` through one task-11 builder/dispatcher route; a gated fall-through can order them but intervening event reads make that order non-mandatory. Adjacent argument `1e08` belongs only to a separate nine-caller filtered-copy family and is not on that wire. Neither the complete follow-up nor the direct handler calls a known search or channel constructor. `80` posts `040b`; `a0` passes status `043b` to service `13`; and `b0/b1` either release or share `040b` according to report byte 6. None directly calls CHANNEL_CONFIGURE or either search builder. The DSP-side emission conditions and timer unit remain unproved. NSE-3's private `0x44` stride differs from NSE-8's `0x48` and remains outside the wire peer. A separate type-`02`/length-20 constructor has exactly five direct callers: literal contexts `10`, two independent `1a` paths, a table-derived context and `50`. Only the `10` path selects operations 4/6/7; the other four retain operation 4, and all submit to task 3. The type-`89` handler posts fixed status `1393` and advances without inspecting a confirmation body byte; its task case follows independent controller state rather than the outbound operation, while type `84`/RA_INFO remains distinct status `1394`. Its handler preserves object bytes 2..7 after replacing the leading status; task 11 suppresses it only at controller substate 1, while the unique consumer reads byte 4 and a big-endian 24-bit value from bytes 5..7. It publishes derived fields through a fixed eight-byte task-17 object and selects either unique helper `20d8d6` or timer-`71` control by controller state. Task 17 is independently bounded as a thirty-state controller at entry `24ce99`; across its complete code extent it directly calls neither CHANNEL_CONFIGURE, either search builder nor the task-3 submission API. Its status-`0400` behavior remains state-dependent, so DSP-side ordering and transitive radio effects are not inferred. The alternate timer-`71` path is task-11 status `138c`, with exactly two runtime-duration arms, three cancellations and no query; expiry returns through the same controller consumer, requires two pending counters to agree, and neither rearms directly nor calls a radio constructor. The report-to-task map now also fixes task-11 routes `87 -> 138f` and `8a -> 1390`, task-12 routes `88 -> 13ac`, `8b -> 13b8` and `8f -> 13b7`, and the controller-byte-selected type-`83` split between task-11 `139f` and task-12 `13a0`, with their exact object sizes. Controller value 1 discards type `83`, value 3 stores only signed report byte 6 through task 11, and all other values feed task 12's five-sample scalar aggregator. Timer `6c` is now separately bounded: task-12 expiry status `13a6`, one arm with raw duration `0273`, three queries and three cancellations; its gated expiry consumes a negative scalar and does not rearm, so it is distinct from timer `6b` recurrence. Neither type-`83` arm directly constructs bitmap search or CHANNEL_CONFIGURE. Type `88` is independently repacked into status `13ac` with a 24-bit value, two 16-bit values and two controls; state 3/mode 2 derives a scalar for a gated timer-`6e` helper, while other states can update matching 24-byte candidate records. It likewise has no direct search or CHANNEL_CONFIGURE constructor. Type `8f` is a body-independent trigger for a distinct type-`09`/declared-length-`50` candidate-list builder: selected 24-byte records contribute up to forty two-byte keys, unused pairs are `ff ff`, and the request posts to task 3; no submission instead arms timer `6e` for raw duration 8. Timer `6e` is a shared task-12 controller recheck with expiry status `13b0`: exactly three duration-8 arms join the candidate-list fallback and two derived-timing paths, with one cancellation and no query. Expiry accepts states 6/7 and state 4 outside context `1a`, then conditionally re-enters helper `2142b2`; it does not make the type-`88` and type-`8f` paths one report stage. This later candidate-list stage remains separate from the type-`1a` bitmap. Adjacent type `8c` discards its report body and posts fixed status `1395` only outside controller value 1 and context `1a`; task 11 either forwards a four-byte object to task 17 or converts substate 2 into task-12 status `13a5`. It has no direct edge to either search builder or CHANNEL_CONFIGURE, so a type-`09 -> 8c` acknowledgement correlation is not claimed. Type `8a` reads no report-body field, is rejected only at controller value 1, otherwise clears shared flag `02`, and shares `211e48` preprocessing with status `138f`. The central event decoder then keeps them distinct: `138f` can call populated-bitmap constructor `20faec(2)` when either pending-object slot is occupied, while `1390` only advances a threshold-gated counter before shared control. Type `87` also cancels timer `1b`. One populated type-`1a` request path reads the type-`8a` controller byte for request fields, but no direct request-to-`8a` edge or acquisition-terminal role is established. Task 11 recognizes the wider numeric controller family `1389`, `138a`, `138c`, `138e..1390`, `1392..1397`, `139f` and `13bb`; unidentified statuses remain deliberately unnamed. Task 12 now has an exact `13aa..13ba` jump table, preserving result ingestion and measurement completion as separate controller events. Type `86` independently dispatches body discriminators `70`, `80`, `b0` and `b1` principally toward task 3, not as an acquisition-terminal shortcut. Product configuration therefore selects the shared `bitmap_search` wire profile independently, while NSE-3 acquisition policy remains `none` and the peer stays disabled. DSP parameter selector 8 is now bounded separately: it publishes `0x8000 | value[11:0]` at shared `0x100a8`; one service-mode table supplies `0x0600`, and the normal delta updater maps state slot zero to selector 8. That updater constructs the live slot at `0x10c020` through two mask/indexed-value table families, compares it against shadow `0x10c008`, and copies it only after successful publication. Its exact ten-entry upstream dispatcher maps messages `0x076f..0x0778`; three set/clear pairs toggle controller masks `0x02`, `0x04` and `0x10` and immediately run the updater, while two entries are no-ops. A whole-image census of the two recovered packed-event APIs resolves 938 of 946 direct callsites; none of the resolved calls produces this event range. Seven of the eight runtime-built calls are independently bounded by their complete stored-event constructor inputs, fixed local values, or exhaustive inspection of all 512 entries addressable by a byte-indexed mode table. The last call, `0x25a87e`, reads a runtime object or an 80-entry SRAM table. Startup proves that table is zero-initialized: none of its 109 counted copy records covers the table. Its sole registration constructor has 116 direct callers; all 81 fixed-ROM and five startup-SRAM descriptors exclude the target event range. Exact construction analysis also bounds 29 of the 30 runtime descriptors away from the target. The last call has two explicit events outside the range but one branch leaves its event halfword unwritten, and its free-list allocator demonstrably does not clear reused payload. A whole-image allocator census finds nine exact 28-byte owners; one reads all 28 bytes from serial EEPROM address `0x40` and then frees the payload, making the stale-halfword risk concrete. The paired field writer has one direct caller in the image: a central inbound type-`ca`/`cb` message handler that passes its byte-9 selector directly; selector 4 replaces EEPROM range offsets `0x11..0x14`, including the stale halfword at `0x12..0x13`, before writing the complete block back. The bytes are therefore runtime-request-writable rather than immutable factory state. A 44-call serial-write census finds this is the only statically resolved write range overlapping those bytes, but 12 runtime-address writes prevent a sole-writer or untouched-startup claim. Missing initial EEPROM contents, message history, runtime class thresholds and allocation order leave that branch unresolved. The alternative object boundary is now exact—zero-initialized group table, 24-byte records and event at +0x0e. Its sole constructor has 45 callers; 35 unique fixed catalogues provide 124 events, all outside the target range. The only runtime constructor is dispatched by packed event `0x0389`, whose argument copier supplies zero-initialized SRAM cell `0x10b284`. Exactly three direct producers exist: one supplies `(0, 0)`; the other two share the current record value, paired respectively with bounded `0xff` and zero. The valid non-null path is now exact: registration `0x278792` builds value `0x2b01d8` with flag `0x40`; producer `0x25b044` emits event `0x0389`, and the dispatcher installs that nine-record ROM catalogue. Its events exclude `0x076f..0x0778`. The final parser-built value at `0x28b628` is bounded by replaying the exact PPM root, top-level `TEXT` search and aligned child walk: its eight registered values are all small non-address words. All runtime object values are classified; only the independent reused-descriptor event halfword remains unresolved, so producer absence is still not claimed. Product configuration declares the proven selector decoder independently from the still-empty speech-request policy. No organic Answer/End transition or speech meaning is connected to those message pairs, so observing selector 8 cannot activate media. Full radio-report ordering and DSP startup remain unproved. NSE-3 independently routes DSP reports 0x8d/0x8e to task 9, recognizes link families 0x1e/0x1c and constructs the canonical seven-byte class-0xd0 discovery frame; the generic external-service transport and class-`0x40` frame helper are therefore shared. NSE-3 independently accepts command `0x70`, returns its one-byte map result and uses empty command `0x71` for disable. Its map consumer independently takes 64 bytes from object byte 9 after requiring the low length byte to exceed `0x42`. NSE-3 constructs command `0x64` with a nine-byte body whose fixed bytes `30 08 01 01 01 1f 20` differ from NSE-8. Its result byte comes from runtime flag bit 6, independently of the publisher control argument: incoming command `0x64` supplies body byte 0, while an internal event-`0xd3` path can supply value 2 under a runtime flag or counter-at-15 condition; those meanings remain unknown. The DSP-owned startup delay, advertised channel bitmap and services, and complete ordering remain unproved, so the peer stays disabled and the status body remains typed product configuration. The product selects external-service application profile `none`; lower transport evidence therefore cannot activate an inherited NSE-8/NHM-5 script. Task 9 independently dispatches events `0x011c..0x0120` to five fixed handlers; type-`0x8d` flags feed three counters with family-specific aggregate reset thresholds `0x5a` for `0x1e` and `0xe6` for `0x1c`, while a separate retry path continues while its pre-increment byte is at most ten and a progress path advances at two/ten by family. Event meanings, timer units and the missing DSP trigger policy remain unknown, so this controller evidence does not enable the peer. The removable lab card's `3b 10 05` ATR takes the firmware's ordinary `ff 00 ff` PPS path, and its normal status families have explicit firmware paths, so it is composed without treating its synthetic subscriber/filesystem data as NSE-3 identity. The exact class-`a0` constructor layer establishes 19 command builders covering 18 unique GSM SIM instructions and one common submit boundary. The shared lab card implements thirteen of those instructions generically, including persistent GSM 11.11 CHV state and cyclic EF_ACM/INCREASE with its immediate GET RESPONSE contract; RUN GSM ALGORITHM and four SIM Toolkit commands remain unsupported, and the first organic NSE-3 sequence remains unobserved. Service selector 0x03 independently identifies DSP external software revision 25.3.531, dated 17-Dec-97 for NSE-3Nx, through an exact flash pointer and NUL-terminated string; this remains distinct from COBBA B06 and handset build v4.06. Selector 0x09 instead reads an initially empty runtime buffer whose sole setter converts byte 11 of inbound type-0x0a or type-0xc8 reports to one ASCII digit; task 2 uniquely dispatches object family 0x74 to that handler and releases it without an acknowledgement. The external image contains no fixed internal-DSP revision or DSP-side producer logic. DSPIF receive materializes the family byte from the DSP-to-MCU shared ring in an envelope addressed to task 2, and the radio/DSPIF owner routes that object unchanged; the bootstrap responder and external-service peer are therefore excluded as sources. The DSP transfer writes a fixed seven-halfword shared header, asserts MAD2 byte 0x20002 bit 0, stages a fingerprinted 64 KiB stream while alternating two synchronization cells, captures both final publications and then releases the same MAD2 bit. Firmware captures final shared words `0x10000/0x10002`; selector pass-through plus the adjacent system-ASIC query and the documented Nokia service protocol establish that the first supplies COBBA identification `B06`, which one later path requires as `0x0b06`. The generic HLE ready word `1` is incompatible. A complete census of the thirteen literal roots of the DSP state object finds that the second captured result is written once and never directly read by the external MCU image; the bootstrap wait proves only that it is non-zero. The sole wrapper does not test a return code or either capture before unconditional continuation; its only conditional synchronizes EEPROM word 0x74 to zero from pre-transfer MAD2 byte 0x20001 bit 0. This still does not establish that the stream is DSP code, its DSP destination, DSP-side publication semantics or a service grammar. The v4.06 image (`5025a6ac…`) remains a labelled ROM3 candidate; matching F711604 boot/DSP ROMs and EEPROM remain `NO_DUMP`; and every firmware-derived DSP/external-service/radio peer is disabled. Direct-to-flash reset bypass remains disabled, so this is not a boot promotion. See `docs/6110_bringup.md`. |
| Other declared DCT3 products | No | No | No | No | No | No | No | Driver ROM declarations are not executable evidence: required local images and product-specific contracts are absent. |

The Nokia 6110 remains unpromoted after an independent ROM4-HLE comparison:
the exact v4.06 candidate receives all 64 alternating transfer
acknowledgements but still waits at `0x2859e0` for the missing non-zero
`0x10002` publication. Its product profile therefore marks bootstrap
completion `unresolved` independently from the evidenced transfer count. See
the reproducible comparison and provenance in `docs/6110_bringup.md`.

The same comparison suggested an outbound type `0x03` control publication.
Exact v4.06 analysis independently proves its fixed, zero-body task-3 object
and its task-11 status-`0x03f1` route. A controller-context/helper guard
selects it as the alternative to a context-`0x1a`
`CHANNEL_CONFIGURE`; both branches publish task-12 status `0x13a3`.
Because six task-17 producer sites feed `0x03f1`, neither the external
`DEACTIVATE` label nor a single acquisition ordering is accepted. This adds
a checked protocol boundary without enabling the NSE-3 radio peer or
promoting coverage.

The comparison also identifies SRAM `0x10fde1` as a possible v5.48 DSP
verdict location. Exact v4.06 analysis rejects treating it as a portable
Boolean hook: direct and derived references use bits 2, 4, 5 and 6 in
different external-service controller paths, and command `0x64` exposes only
bit 6 as its result. The bit meanings and initial state remain unassigned.
No firmware state is forced, no DSP peer is enabled and coverage remains
unpromoted.

The exact image now also proves one organic transition through that bitfield:
with bit 2 set, incoming command `0x64`/body byte 0 equal to 2 sets bit 4 and
arms shared timer `0x13` for raw duration `0x19`. Its task-2 expiry event
`0xd3` clears bit 4 and publishes the typed status with control argument 2.
The timer has three other arms and the initiating DSP condition remains
unknown, so this bounds a response contract without enabling or scripting the
peer.

The bit-2 prerequisite is now bounded too. DSPIF initialization writes mode
cell `0x10b970` to zero; a uniquely dispatched handler can promote it to one
only while shared halfword `0x100e4` is zero. Task-2 initialization maps mode
one to controller bit 2 and uniquely submits fixed empty task-3 type `0x70`.
No equivalence is claimed between that publication and class-`0x40` command
`0x70`, and the missing DSP-side condition still prevents peer enablement or
coverage promotion.

## Promotion rules

The levels are cumulative:

- **Booting**: the handset completes its evidenced flash, MAD2, CCONT, SIM and
  DSP bootstrap path without firmware hooks.
- **Interactive**: physical matrix inputs reproducibly drive normal firmware UI.
- **Registered**: the product's own radio packet grammar camps and completes
  Location Updating against the independent network peer.
- **Call control**: paging, ringing, physical Answer/End and clean teardown pass.
- **Internal media**: bidirectional GSM-FR crosses the firmware/DSP boundary,
  including FACCH substitution, degraded frames, SACCH coexistence and
  save-state replay.
- **Physical duplex**: isolated host microphone and playback paths pass without
  loopback, feedback, or UI-level injection.
- **Hardware-faithful**: all material product flash, DSP, PCM, COBBA and analogue
  topology claims are backed by hardware documentation, firmware analysis or
  reproducible traces. A working calibrated compatibility response is
  insufficient.

For NHM-5, packet length similarity is not semantic evidence. In particular its
68-byte type `0x20` must not be reinterpreted as NSE-8 type `0x1a` merely because
both are 68 bytes. The current radio peer therefore remains disabled until the
NHM-5 command/report lifecycle is recovered.

## NHM-5 v6.39 radio configuration boundary

The first three previously unclassified startup families are firmware-built
configuration publications:

| TX type | Payload | Firmware constructor | Proven source |
| ---: | ---: | --- | --- |
| `0x22` | 32 bytes | `0x2c29f8` | The selected payload is an exact copy of the ROM table at `0x325e28` in the observed product/band branch. |
| `0x20` | 68 bytes | `0x2c2a54` | The selected payload is an exact copy of the ROM table at `0x325ef0` in the observed product/band branch. |
| `0x21` | 32 bytes | `0x2c2aac` | Firmware composes it from two 16-byte tables; a firmware/NV branch selects each source. |

The common initializer at `0x2c2c26` invokes the `0x3c`, `0x21`, `0x22`, and
`0x20` constructors twice. Each object carries its actual DSP type and length
in the ordinary queue object, and the task-side transport reaches the shared
ring through `0x2bc6e0`/commit store `0x2bc6c4`. This proves configuration-table
ownership and packet identity, but not individual RF field meanings or a reply
contract.

The later type `0x56` is separately constructed at `0x2a7dd8`. Firmware
allocates and clears a 164-byte queue object, declares a 160-byte payload and
fills that payload with `0xff`. The producer at `0x28a0d0` walks 16-byte
firmware-owned channel records, copies the two channel bytes from offsets 6/7
into consecutive payload entries, and stops after 80 entries. Both observed
call sites pass their selected record through this producer before publication.
The runtime packet therefore represents one big-endian candidate `0x0058`
followed by 79 erased `0xffff` entries. This establishes a bounded
candidate-channel-list command. The peer places its laboratory cell on the
requested channel rather than substituting NSE-8's ARFCN 1.

The inbound result layout is independently established in NHM-5, not borrowed
from NSE-8. Its consumer at `0x28aa0e` skips a two-byte header, reads the
big-endian channel from object offsets 6/7 and signed RSSI from offset 9,
advances four bytes and bounds the loop at 40 records. Thus the shared
`0x8b/166` encoder is a genuine protocol-layer component used behind two typed
command profiles: NSE-8 bitmap search and NHM-5 candidate-list search.

Type `0x56` publication opens the firmware's candidate-synchronisation window:
producer `0x28a158` selects the candidate record, writes the channel into the
command and sets acquisition-active byte `0x00110e40`. A channel-`0x40`
`RECEIVED_BLOCK` for that ARFCN while this byte is active reaches `0x232258`;
zero error changes the selected record to synchronised state 4. The ROM then
organically constructs type `0x02/20` through
`0x28a842 -> 0x2d5ea6 -> 0x2a874c`, preserving SCH, BSIC and ARFCN fields.
The peer completes the ordinary `NO_PSW_LEFT`, `CHANNEL_CHANGED_CNF` and
`RA_INFO` transaction, after which v6.39 consumes sustained serving BCCH and
RSSI reports and issues its own later serving-channel configuration. The
generic laboratory network encodes SI1 bitmap-0 from the selected serving
ARFCN, so NHM-5 receives a cell allocation containing `0x0058`; it does not
inherit NSE-8's ARFCN-1 cell allocation. This removes a real cross-product
contradiction, but does not promote registration.

An opt-in v6.39 firmware-boundary analysis closes the internal selection
frontier without assigning new protocol meanings. The SI parser accepts SI1
through SI4, reaches its complete `0x0f` set, and drives the selected-cell
machine from state 8 through state 4 to state 6 with a successful return.
Independently, the SIM task reads `EF_LOCI`, `EF_IMSI`, `EF_ACC`, the PLMN
selector and the remaining enabled files through SIMI/T=0. The default card
still contains `EF_LOCI` status “location not updated”; a retained cached-LAI
explanation for the missing update is therefore false. Primitive `0x09c8` is
emitted even when radio acquisition is deliberately held before SCH or SI
delivery, so it is a SIM-side publication rather than evidence that SIM and
selected-cell state have already been combined.

NHM-5 acquisition is no longer collapsed into adjacent TDMA ticks: its active
candidate waits through the same complete eight-multiframe validation interval
used by the selected-cell path. Once the firmware has accepted SI1 through
SI4, it organically configures idle common control, issues Random Access and
accepts Immediate Assignment. The network now encodes the live serving ARFCN
in that assignment instead of inheriting NSE-8's ARFCN 1; v6.39 consequently
publishes an assigned-SDCCH configuration on `0x0058`.

The previously empty assigned-channel uplink was traced to a rejected
confirmation, not a missing MM payload. NHM-5 handler `0x2c4c28` correlates bit
0 of the `CHANNEL_CHANGED_CNF` body with byte 2 of its pending channel-change
context. That context requests success value one for the assigned SDCCH; the
all-zero body accepted by NSE-8 is discarded before NHM-5 can arm LAPDm. The
typed NHM-5 radio profile now returns that recovered success value. Firmware
then organically constructs SABM with its Location Updating Request, consumes
contention UA and Location Updating Accept, acknowledges N(R)=1, consumes RR
Channel Release, acknowledges N(R)=2, updates `EF_LOCI`, deconfigures channel
`0x60` and resumes serving BCCH/PCH. `verify-3310-radio-registration` checks
that complete ordered lifecycle, exactly one request, SIM persistence and
steady post-release camp. No firmware state, internal message or synthetic
uplink payload is injected.

The release transaction has recovered context `0x0409/01/00` and the modeled
DSP returns the observed zero body. Static decode and runtime branch tracing
correct an earlier stronger claim: handler `0x2c4c28` recognizes context
`0x0409` before its bit comparison and takes the release-completion path for
either body value. Only the assigned-channel context proves a correlated
value-one requirement.

## NHM-5 v6.39 paging boundary

The first post-registration page is no longer an unexplained transport loss.
The peer emits channel-`0x60` `RECEIVED_BLOCK` with a 22-octet Paging Request
Type 1 carrying the registered IMSI. Firmware routine `0x260994` accepts the
complete block as result `0x061a`, copies the L3 body into its channel state,
and `0x2607b2` constructs received-block event `0x05eb`. The event is posted to
NHM-5 task 13 and received by the loop at `0x2a75a0`; its payload retains
protocol discriminator `0x06`, message type `0x21`, channel-needed SDCCH and
the exact mobile identity.

The task passes that object to parser `0x280664`, which classifies the page as
result `0x08a2`. Ordinary no-identity PCH fill blocks on the same transport
produce zero, so the page is distinguished and parsed rather than discarded
as fill. The active result branch is `0x2573c0`: it sets parser-completion byte
`0x0010dc5f`, releases the received object and resumes the parser wait loop.
This result is stronger than a message-type decode. The Paging Request path
calls `0x28056a` for mobile-identity IE `0x17`; that helper recovers the SIM
identity through `0x290462`, compares its length and bytes with `0x2f0dd0`,
and returns one only on an exact match. The addressed request reaches
`0x08a2`; a request carrying another valid IMSI does not. Thus the unresolved
boundary is after subscriber identity matching, not in Paging Request
construction or SIM identity availability.

The frame number in the received-block report is also intentionally a
scheduled future paging frame. A quarantined experiment delayed each PCH
report until that numbered TDMA frame. It made the NHM-5 report timestamp and
frame number coincide, but the proved NSE-8 reference then accepted no page
and emitted no RACH in a 40-second run. Restoring advance delivery restores
the established NSE-8 page-to-RACH lifecycle. The frame number is therefore
not a stale label to be corrected at delivery time: it gives Layer 1 advance
notice of the subscriber's CCCH paging opportunity. Both product profiles
already receive that generic scheduling contract.

The separate RR admission path at `0x255ff8` checks that generic completion
byte before selecting a decoder. Under the former aliased BCCH cadence it was
not reached after the page. Delaying the page until well after SIM file
activity had settled did not change that outcome. Repeating the correctly
addressed request at every subsequent 102-frame paging opportunity made the
parser repeat the same successful completion without waking the idle-RR
admission path or producing RACH, which excluded a missing network retry
before the scheduling cause was found.

The short nine-second trace was not by itself a sufficient latency bound:
the proven NSE-8 path does not emit its paged RACH until 13.64 seconds after
the page, with serving-cell revalidation in between. A 32-second NHM-5 run
covered that complete comparison window. NHM-5 received its page at
7.731663 seconds, performs two correlated channel changes after outbound DSP
type `0x46`, emits outbound type `0x57/03050000` at 10.120796 seconds, and
continued receiving BCCH/PCH reports through the end of the run without
another random-access request. The corresponding NSE-8 trace receives its
page at 16.980246 seconds and emits the paged RACH at 30.620707 seconds.
This proved that the then-missing NHM-5 transition was not an artefact of
stopping before the established NSE-8 admission latency.

The reconfiguration following the page is temporal, not causal. A matched
15-second idle control with no network event emits the same
`0x46 -> 0x02 -> 0x89 -> 0x02 -> 0x89 -> 0x57` sequence at
10.079243..10.096980 seconds, versus 10.102477..10.120796 seconds in the
paging fixture. It is periodic serving-cell maintenance. In the aliased
cadence the addressed page had no downstream effect beyond its
identity-matched `0x08a2` parser completion, and the maintenance terminal
could not stand in for page admission.

Complete receive-event traces also prevent transferring NSE-8 event numbers
by resemblance. NSE-8 receives page event `0x0811`, then the later SI3-driven
serving reconfiguration produces `0x03fc`, `0x07f9` and `0x0835`; `0x0835`
immediately publishes internal event `0x0440` and RACH follows. NHM-5 receives
its `0x08a2` page while its outer RR dispatcher is in idle state 7. Periodic
serving reconfiguration instead produces `0x0801`, `0x07fb`, `0x07d1`,
`0x07dd`, `0x05e8`, `0x08d1` and `0x07d4`, all while that dispatcher remains
in state 7. The product event grammars are structurally different; injecting
NSE-8's `0x0835` into NHM-5 would not model a shared hardware response.

The two post-page `CHANNEL_CHANGED_CNF` packets also do not fail NHM-5's
product-specific body-bit correlation. Runtime at handler `0x2c4c28` shows
that the pending-context pointer is null for both transactions, so the handler
takes its context-null completion path before the comparison at
`0x2c4c48..0x2c4c52`. The value-one body remains required only for the proved
assigned-SDCCH context `0x0402/01/01`; changing the generic confirmation body
could not supply the then-missing paging handoff.

Address-scoped writes to the RR global refine the post-release control flow.
The `0x256f1a` release branch arms timer `0x9f`, waits for the real release
events, cancels `0x9f`, sets RR state byte `r4+7` to one at `0x2570b8`, clears
`r4+0x0d` and `r4+0x0f`, and branches directly to the parser loop at
`0x257376`. The addressed page then sets exactly `r4+0x0b`
(`0x0010dc5f`) at `0x2573c8`. It does not write the A2-active byte
`r4+0x14` (`0x0010dc68`); the nearby generic-queue write at `0x2a6e72`
targets the distinct absolute byte `0x0010dc69`. This excludes both an
incomplete release-state change and accidental aliasing of the queue flag
with A2 state.

The foreground RR loop at `0x255fbe` does not enter admission merely because
the parser completion byte is set. It first waits for internal event
`0x08cb`; only that event calls `0x255ff8`, which consumes the saved parser
result and can enter the organic random-access setup at `0x256014`. Static
decode corrects the earlier task-12 attribution: ROM objects
`0x3303a0..0x3303b0` carry `0x08ca`, `0x08cc`, `0x08cd`, `0x08cb` and
`0x08c9`. The task-13 dispatcher maps `0x08ca..0x08cd` to RR timer IDs
`0x9f`, `0xa2`, `0xa0` and `0xa1`; therefore `0x08cb` is specifically timer
A2's expiry event.

RR initialization cancels timers `0x9f..0xa2`. During registration release it
later arms only `0x9f`, for 80 ticks at `0x256f50`; its `0x08ca` expiry is
received before the final channel-change request. The sole A2 arm site is
`0x255ec0`, for 501 ticks, and the aliased-cadence paging trace never reached
it.
Task 13 still sends task 12 request `0x0a05` with selector one, which task 12
stores at `0x0010e4bd`, but no evidence makes that request the producer of
`0x08cb`.

A standards-shaped, nonmatching IMSI page in place of the no-identity filler
also fails to arm A2 or admit the addressed page. Repeated nonmatching pages
eventually make firmware revalidate its serving-channel configuration and
publish its existing type-`0x57` terminal, so treating unrelated subscriber
traffic as the missing wake would be both ineffective and architecturally
wrong. Reversing the release ordering so the real `0x89` confirmation is
accepted before result `0x05e8` likewise does not change paging.

The missing admission event was caused by report scheduling rather than an
unimplemented DSP packet. While registered, the old serving cycle delayed a
whole multiframe after both decoded BCCH and advance-delivered PCH reports.
That made successive BCCH reports advance two 51-frame multiframes at a time,
aliasing the eight-multiframe schedule to SI2 and SI4 after channel release.
NHM-5 therefore never rebuilt a complete SI1--SI4 serving set around the
addressed page. PCH is now kept in its own advance-notification slot without
consuming another BCCH interval. The firmware consequently sees SI2, SI3,
SI4 and SI1 in its own schedule, enters the existing `0x0802/0x0804` handoff,
arms A2, emits paged RACH and completes Paging Response and release. No timer,
event, parser result or firmware state is synthesized.

The intervening type `0x57` is an outbound MCU-to-DSP publication, not an
inbound `NO_PSW_FOUND` report, and its four-byte body is retained verbatim in
the trace. Its firmware lifecycle is now bounded independently of the DSP.
RR result `0x07d4` selects internal event `0x03ec` at `0x2557d6..0x2557ea`;
the generic event builder `0x2a6d68` posts that event to the radio task's
third event slot. The active radio-task branch receives an object of type
`0xc8`, calls the type-`0x57` constructor at `0x2a7fa0` from `0x232144` with
arguments `(3, 0)`, then branches directly to ordinary object release and the
next event receive at `0x2324f0`. The constructor forms the exact body
`03 05 00 00`: `0x2a7b4a` encodes selector three and table index zero into
the first two bytes.

There is no type-`0x57`-specific acknowledgement state after publication.
Moreover RR produces the same internal `0x03ec` event at 5.632798 seconds,
before the addressed page, and again at 10.119838 seconds; only the later
radio-task state selects the observed terminal publication. Thus `0x57` is
not a page-specific request/reply boundary and must not gain a synthetic DSP
response. The paging failure remains the earlier missing firmware event that
would transfer the saved parser result through `0x255ff8`.

The alternative `0x8b` measurement terminal still has its independently
recovered 40-record layout. That path organically constructs type `0x55/4` at
`0x2a7baa` with payload `03 05 00 00`; it is a terminal/control publication,
not the start of the active candidate window. Firmware-named type `0x8a`
`NO_PSW_FOUND` reads its channel from object offsets 4/5 and is a failure
result, not an acknowledgement.
