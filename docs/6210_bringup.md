# Nokia 6210 bring-up

## Current boundary

NPE-3 v5.56 PPM C reaches the final DSP verification wait at
`0x426cc2..0x426cc8` after 232 alternating buffer handoffs. The product-local
profile models the independently decoded MAD2 DSP release/ready and GENSIO
contracts. Final DSP completion is not published. Boot/UI, SIM, network,
calls/audio and handset save/load are not yet validated on the normal machine.

Separate research compositions establish the own-upload/runtime boundary:
`npe3stage` executes the acquired verifier/loaders and retains native ownership
at absent mask routine `2c75`; `npe3hle` explicitly hands transport to HLE there.
The latter uses the unchanged acquired PMM, request-correlated D0 discovery,
the own compact self-test consumer and a declared nominal battery sample.
Acceptance gates prove graphical Menu/SIM PIN interaction, Calculator,
persistent SIM contacts with cold readback, laboratory registration/operator
presentation, physical incoming/outgoing call signaling and incoming/outgoing
SMS. Physical Menu decodes as `19` on the 96x60 display. Native resident DSP
execution, measured final silicon verdict, speech and full hardware fidelity
are not claimed. The research phone-service milestone is complete; the normal
machine remains fail-closed pending authentic native DSP completion.

`npe3mbus` separately enables the firmware-requested MBUSTIM clock on the
same own-upload/HLE composition. It qualifies read-only version service,
but exposes an unresolved clock-stop/wake input failure during calls.
`npe3hle` retains its established timer-disabled scope; this split contains
the regression, not a completed hardware fix. Merge the compositions only
after service and complete call/sleep gates pass together. See
`mbus_controller.md`; `--trace-sleep` selects the clock-enabled instrument.

Physical `*123#` followed by Send also completes a laboratory USSD transaction
on `npe3hle`: the exact processUnstructuredSS request receives a correlated
successful response, the firmware renders `Service reply / Nokia test network`,
and physical Back returns to the registered idle frame. The isolated runner's
`--scenario ussd` checks ordered handset inputs, protocol release, both reviewed
96x60 frames and persisted EF_LOCI. This is HLE network service acceptance,
not native DSP or speech evidence.

`verify-6210-ussd` repeats that acceptance. `verify-6210-call-divert` separately
drives physical `*#21#` and Send: an exact InterrogateSS request for service
`0x21` receives an inactive result, renders `Service not active`, releases RR
and returns to registered idle after Back. These checks do not establish
divert registration, activation, deactivation or persistence.

The separate runner scenario `divert-lifecycle` extends coverage within one
coherent session: physical `*21*5551234#`, `*#21#`, `#21#`, `*#21#` produce
register/active-query/deactivate/inactive-query responses in order. Reviewed
frames show `Divert activated`, `Done`, the inactive service summary and
`Service not active`, followed by registered idle. The active query's frame
does not establish displayed forwarding-number content. Save/load and
cold-process forwarding persistence are not covered by this scenario.

`state-divert` saves after physical forwarding activation, restores exact
CPU/RAM/time and compares the one-second observed protocol replay. Only after
load does a fresh physical query establish retained active forwarding; physical
deactivation and a final inactive query complete normally with the same four
reviewed result frames. This scenario establishes save/load, independently of
cold-process persistence.

`verify-6210-call-divert-cold` runs three independent processes with unchanged
own provisioning: physical registration, retained-storage active interrogation
and external incoming-call forwarding without handset paging, then a separate
fresh-storage inactive interrogation. The network-owned subscription file stays
byte-identical during the retained query. Result and idle frames are reviewed
6210 oracles; the fixture does not import another product's provisioning or
claim native speech.
The gate also copies only its own retained handset state into an isolated
fourth process and damages the network subscription checksum. MAME must report
the failed NVRAM read; a physical query must then return inactive with the
reviewed result and idle frames. The original retained evidence is unchanged.

## Inputs

`roms/noki6210/6210_556c.fls` is 0x3a0000 bytes with SHA-1
`3d9ea319503e78ec69b60d72cda23e461e118ea9`. The product-local PMM tail is
0x6000 bytes with SHA-1 `b3a527ede1be87bd715fb3741a81eef5bd422efa`, mapped
at region offset `0x3fa000`. Sources and archive hashes are in
`roms/README.md`. Uniform-fill DSP audit files are not executable ROM6 evidence.

## Hardware contracts

- At `0x4dc0e4`, firmware writes CTSI offset 2 with `0x40`, then sets bit 2
  and polls bit 4. `LSRS #5` tests original bit 4 through carry, not bit 5.
  The profile exposes that ready bit only when the release line is asserted.
- Verifier start `426c36..426c3e` separately sets CTSI+2 bit 0 after writing
  the uploaded descriptor. Execution therefore uses bit 0, not the bit-2
  early readiness handshake; the original frontier gate still reproduces.
- At `0x4ec7b0`, GENSIO control `0x22` selects CCONT; a write to offset
  `0x2c` precedes polling status offset `0x6d` bit 2 at `0x4ec7bc` and
  reading offset `0x6c`. Control bit 2 stays clear, so receive-ready follows
  the byte write rather than a control-bit trigger. The LCD uses `0x2e/0x6e`.
- The 6250 has a separate decoded NHM-3 configuration and initial-record
  research-PMM comparison; its phone-service gates are documented in
  [6250 bring-up](6250_bringup.md). Neither its native boundary nor its
  provisioning is inherited from the 6210 configuration.

## Keypad

The NPE-3 v5.56 scanner `4f84c8` masks `6b`, drives `a8/28` and samples
`2a`. Its ordinary scan forms `drive * 5 + sense` at `4f8550..4f8556`.
Decoder `4facfa` uses the normal table at `2869b8` and the special table
at `2869d4`. The normal table is:

```text
5a 5a 5a 5a 5a
11 19 01 02 03
0e 17 04 05 06
0f 18 07 08 09
10 1a 0c 0a 0b
```

Special input bit 4 maps to Power (`0d`); the other four special entries
are `5a`. These tables independently select the same logical input layout
as the 5210, so the 6210 reuses its port declarations with a product-local
five-line controller contract and Power mask `10`, not its hardware profile.
The previous inherited 3310 input declarations and four-line default were
not NPE-3 evidence.

`make verify-6210-keypad-controller` checks both pinned ROM tables, all 20
matrix keys across all five driven lines (100 reads), and held-Power
press/release through the actual MAME controller. It is an MMIO conformance
fixture: Lua drives controller registers and host inputs, never firmware RAM
or messages. It restores the row/direction/mask registers. This proves the
configured matrix, not firmware decoding, debounce or usable menus. The
separate research-HLE menu gate below proves physical firmware decoding and
menu interaction; the normal machine still retains the native DSP boundary.
The ordinary input exerciser selects this product's logical key layout too.

Nokia's [NPE-3 board schematics](https://altehandys.de/downloads/ser-no-6210-schematics.pdf)
(Version 1.0, 09.02.2001) show the fitted matrix switches on page 5 and
identify the MAD2WD1 ROM6 V16 part on page 4. The physical sheet's ROW/COL
names are not the input fixture's abstract drive/sense names. The archived
PDF is `roms/research/npe3/npe3-schematics-v1.pdf`, 2,656,135 bytes,
SHA-256 `559e9718a9dad703694f17d349383f75af4237b1c26cf94ecdb4aafe641f1e43`.
It does not specify the missing DSP PROM word or final HPI publication.

## Display

The same Nokia sheet identifies H400, LPH7690-1, as a GD45 **96x60** LCD
(page 5). This is primary evidence; the 96x65 claim in the reference
emulator and 95x60 secondary descriptions are not the selected geometry.
Firmware `4e656c..4e659c` clears banks 0..7, writing 96 zero bytes per
bank: command `24`, then `40|bank`, `80`, 96 data bytes, and finally `20`.
The profile therefore uses 96x64 controller storage with a 96x60 visible
area. The capture harness uses the same storage/visible dimensions.

The bootstrap gate requires all eight ordered bank clears (768 bytes),
and a 96x60 blank capture, not merely a blank final picture of any size.
This validates the boot transfer extent,
not ordinary rendering or the complete controller command vocabulary.
The PCD8544-compatible backend remains provisional, as do orientation and
the isolated unused commands observed outside this clear routine.

## DSP upload

The pointer at RAM `0x170070`, read without modifying firmware state, selects
flash descriptor `0x225c2c`: `0f00 0000 00df 0f00 00dc 0000`.
The 223-word program following it has SHA-1
`6646da3c5be9c70deda7e0b5b9f257d5d2ace815`, identical to the stock NSM-3
staged verifier. Identical code does not establish identical final results.

The MCU initializes DSP buffer descriptors with remaining count `0001:d000`
and block size `0200`. The loop at `0x426c58` sends 231 full blocks, followed
by 510 samples and two `ffff` terminators. Flash source advances by `0x20`
per halfword. Shared offsets `0fe/100` alternate ownership, with 116 writes
of zero to each. The HLE acknowledges transport ownership but supplies no
final verdict. At `0x426cc2`, firmware waits for shared offset 2 to leave
`ffff`, then stores the result pair in its own bootstrap state.

The next question is the staged program's final publication under NPE-3's
ROM6 memory/peripheral contract and larger input stream. The NSM-3 fixture's
explicit ROM-version and COBBA assumptions must not be promoted as measured
6210 values. The collaborator's self-test responder is a later request/reply
lead, not evidence for this earlier bootstrap completion.

### Executable calculation and remaining inputs

`make verify-6210-verifier` runs the actual 223-word program with NPE-3's
own flash samples, remaining count `0001:d000` and 232 handoffs. It first
fails closed at the unsupported peripheral read (`port 002d`, PC `0f9f`).
Three explicit sensitivity configurations then attach the existing COBBA
register model and supply PROM word `ff87` as either 6 or 4. All calculate
fingerprint `f65a:0d46` at DSP data `04f7:04f8`, with PMST `ffa8`.
The companion 8210 fixture still calculates `c2e0:6006` over 116 blocks.

These are core-fixture calculations, not measured silicon results. At the
final publication, shared word 0 follows the supplied COBBA register-F value
(0 or `0016`), while words 1/2 follow the supplied immutable PROM value
(6 or 4). Neither is selected by the calculated fingerprint. Thus the
larger NPE-3 stream does not itself resolve the missing publication input;
ROM6 PROM mapping/content and the peripheral contract remain unvalidated.
The handset profile does not receive any of these sensitivity values.

At MCU `426cca..426cd2`, the loader stores shared word 1 at `16fff0`
and word 0 at `16ffee`, relative to bootstrap structure `16ffe4`.
The base pointer comes from literal `426ddc`. This identifies the result's
firmware-owned destination without claiming the later self-test responder
or its IRQ4 upload-completion flag supplies the earlier verifier result.

## Acceptance

`make verify-6210-bootstrap` performs a fresh isolated run and checks CCONT
receive-ready, all 232 ordered handoffs, no reset and the uncompleted final
wait. It is a frontier gate, not usable-phone acceptance. Generic harness
task/mode RAM addresses still describe the 3210 and are not NPE-3 semantics.

## Research-HLE acceptance

Own descriptor `224a44` uploads 104 program words from file `24a50`, SHA-1
`440bf49f1eba4cadb12f7f7581c992b0025807d6`. Descriptor `224b70` uploads
**629** second-loader words from `24b7c`, SHA-1
`734491708f00ca396b422ec4d489d0d4c491d0c9`. Native execution publishes
`0000/0006` under the explicitly fragment-derived PROM/COBBA inputs, requests
selector `14` once and selector `01` 156 times, verifies the whole second
loader and installs 422 words at `0590..0735`. The next call is missing
resident `2c75`. This executes own acquired uploads, not a ROM6 mask dump.

The installed `0590..0735` bytes occur exactly at own MCU file offset
`1eac4`. A native program-space capture matches that source, and the
`0a00` loader and `ff80` fragment captures match their own payloads above.
`tools/noki6210_installed_program.py` pins the image and optionally verifies
all three captures (`--captures DIR`, files `npe3_{bootstrap,loader2,fragment}.bin`).
Its aligned census covers all 211 word pairs: 143 `RET/NOP` pairs, 64
two-word unconditional branches and four other pairs. Six branch entries
target the available second-loader interval; 58 target outside these three
owned intervals. This is not an all-code vector-table claim: the exceptional
pairs include words that must not be silently decoded as vectors. Nor is it
a CFG or proof that no uploaded codec configuration exists. The native
loader's first unresolved call remains `0a40 -> 2c75`; later linear-decode
port operations cannot be treated as executed initialization or PCM evidence.

The same checker inventories a **candidate** 28-entry upload pointer table
at file offset `315994`. Every pointer resolves into this own image with a
six-halfword descriptor, zero final header word and a complete payload that
does not overlap the next descriptor. Entries `00`, `0f`, `14` and `1b`
point respectively to descriptors `21eab8`, `224a44`, `224b70` and `225c2c`;
their declared lengths are 638, 104, 629 and 223 words. The 422 installed
words are a prefix of entry `00`, not its whole 638-word payload. Empty
entries remain represented rather than being dropped and renumbered.
These are table positions, not a universal runtime request-selector ABI.
The presence of other payloads does not supply missing resident `2c75`.

Passive native `npe3stage` observation establishes startup copying all 28
pointers from MCU `515994..515a03` into RAM `170004..170073` at PC `200194`
(0.050300..0.050362 s). The next observed RAM reads in the four-second run are:

| Table entry | MCU reader | Time (s) | Own descriptor |
| --- | --- | --- | --- |
| `1b` | `426be8` | 0.131315 | `225c2c` |
| `00` | `426e1a` | 1.924214 | `21eab8` |
| `14` | `427396` | 1.925512 | `224b70` |
| `01` | `427396` | 1.925662 | `21efc4` |
| `15` | `420356` | 3.878971 | `22507c` |

The indexed reader `427392..4273b4` scales the index by four, reads the
descriptor pointer and copies five header halfwords; `42034e..420366`
obtains a byte-selected index and reads the selected descriptor's size.
The verifier and entry-zero readers independently consume size at `+4`
and payload at `+12`. Four additional flash-table reads at `426c5c` are
strided verifier input, not additional upload selections. Native DSP halt
does not halt the MCU: later MCU selection is not proof of DSP execution.
No later physical-call selection coverage is claimed by this short run.

Use the capture command below with script
`../tools/noki6210_upload_catalogue_observe.lua` and a fresh run directory,
then add `--catalogue-log DIR/error.log` to the source checker. Its acceptance
requires all 28 own pointer copies, the five ordered RAM reads and uncapped
flash/RAM totals (32/5); missing, misrouted or truncated evidence fails.

The coherent 48-second physical outgoing-call fixture independently has the
same five RAM reads and 32/5 totals, with no additional catalogue reads during
Send, laboratory connection or End. Run
`tools/run_noki6210_acceptance.py DIR --scenario outgoing-call --coherent-cell --trace-dsp-control`
with the project Python environment. Its ordinary call acceptance and positive
parameter-compiler observations are retained, and its result additionally
records `upload_catalogue_observation`. This bounds the negative result to
the research-HLE composition, duration and this table's observed reads;
it does not show what missing resident DSP code would request, or exclude
other upload paths. Do not synthesize later DSP requests to make the table
yield a codec-clock answer.

Reproduce the bounded capture without an HLE handoff:

```sh
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir tmp/npe3-upload-capture -- npe3stage -rompath ../roms \
  -video none -sound none -nothrottle -seconds_to_run 4 -log \
  -debug -debugger none -autoboot_delay 0 \
  -autoboot_script ../tools/noki6210_upload_capture.lua
.venv/bin/python tools/noki6210_installed_program.py \
  roms/noki6210/6210_556c.fls --captures tmp/npe3-upload-capture
```

The runtime comparison's type-05 discovery request is
`1eff00d000030101e000`; response `1e0002d000030401c100` is acknowledged by
the MCU with `1e0200d0000305014100`. Own class-74 dispatcher `305a0c..305a5e`
calls `3029d4`; command `0d` selects `302a52`, checks flag `17fd99` bit 2,
cancels timer `1b` and consumes fault bits 0/1 from byte 9. The declared
compact peer clears fields `17fbef/17fbf0/17fbf1`; identity/record replies
are not enabled.

Independent PMM replay finds 295 records, a 2500-byte initial cache record,
and journal end `1bb0`. Both base and replayed application checksums compute
and store `6d85`. No journal deletion, donor storage or checksum edit is used.
This static grammar assessment is not a full runtime NV-reader census.

Analogue routine `3d71f8` reads source 7 through `4f3962`; table `2869dc`
maps it to selector 2. Calibration gain/offset are at `17fce0/17fce4`, followed
by integer scale 1500/232. Both samples must be in `0708..157c`. Conservative
full scale `3ff` produces rejected `19f0`; the declared nominal input `230`
produces accepted `0e31`, with acquired gain `3f809bca` and zero offset.
This is a nominal battery fixture, not a measured electrical transfer curve.

`make verify-6210-stage`, `verify-6210-runtime` and `verify-6210-menu` invoke
`run_noki6210_acceptance.py` with a new isolated `RUN_DIR`. The runner checks
pinned own inputs, refuses an existing directory and writes `acceptance.json`.
Menu acceptance requires all 50 ADN reads, physical Menu followed by own
decoder `4fad30` reporting `19`, and the reviewed Messages frame SHA-256
`8c7650fdb0514ec34c85b89795e529de062e6f141268a507bafc7eb77370df65`.
Neither a host press alone nor a blank framebuffer establishes interactivity.

### Applications and persistent phonebook

`verify-6210-calculator` navigates the product's physical menu keys and pins
the reviewed `12 - 3 = 9` Calculator frame (SHA-256
`2c5e99fd98ab56d41574c613021a7ed5270fe7d39e94ec57a1f52b9f732199fc`).
Menu position is harness policy, not a hardware contract.

`verify-6210-phonebook` enters `A` / `123` through physical keys, requires
the EF_ADN record-1 UPDATE RECORD body and `9000` response, then starts a
second isolated process with the first process's persisted NVRAM. The cold
process must read record 1, select the contact physically, issue no update,
and display the reviewed contact frame (SHA-256
`39ca7b13f4afdc8c6e3ca553d7fd0bafcdd7dd3de42c054edf0f445713dd09bc`).
The storage validator independently checks the saved name and number.
Neither process patches phone memory or seeds a contact directly.

The handset bank is independently exercised by
`.venv/bin/python tools/run_noki6210_phone_memory.py RUN_DIR`.
This compatibility entry point delegates to `run_dct3_phone_memory.py` with
6210 as the default product. The shared physical sequence is not shared
provisioning: each product retains its own observer, inputs and frame oracles.
Two fresh own-PMM processes physically select Names / Options / Memory in use /
Phone. The control searches the empty bank and must render **Selected memory
empty**. The writer enters A / 123 and must render **Saved to phone**. A third
process inherits the writer's complete NVRAM and physically searches/selects
the persisted contact, rendering the same exact A / 123 detail frame as the
SIM-contact gate. Both bank-selection confirmations, empty control, save and
cold detail have exact grayscale-pixel hashes. All three complete SIM files
must match, and no SIM UPDATE RECORD may occur. The writer's complete 4MiB
flash must differ from the independent empty-bank control; the changed-byte
range is reported, not interpreted as a decoded phonebook-record format.
Cold boot may update flash metadata, so byte-identical whole-flash readback
is not required. This qualifies one handset contact and persisted memory-bank
selection, not capacity, SIM-to-phone copying, native DSP, network registration
or electrical flash timing.

Adding `--lifecycle` extends this to nine isolated processes. After the initial
save/read, physical Options / Edit number changes123 to124. A cold process
must render A / 124. Physical Names / Erase / One by one then opens the
**Erase all details?** dialog for A; Back cancels, and another cold process
must retain A / 124. The same dialog is later confirmed, producing **Phone
book erased**; the final cold search must render **Selected memory empty**.
Both confirmation frames, edit-save, erased, edited/cancelled cold details and
final empty search are exact pixel oracles. Complete SIM storage remains
identical across all nine runs, and no UPDATE RECORD is allowed in any trace.
Each successor inherits the complete previous process's NVRAM; no contact
record is seeded or changed outside firmware. The fixture's prior search
means One by one opens the existing contact directly, not an extra search
editor; the dialog oracle prevents an extra OK from silently confirming
instead of cancelling. Capture: `tmp/npe3-phone-memory-lifecycle-banked`.
This qualifies one numeric edit, erase cancellation and complete-contact erase,
not multiple numbers, bulk erasure, capacity or SIM/phone copying.

The independent `--rename` variant uses five fresh processes: empty control,
A / 123 save, cold retrieval, physical Options / Edit name changing A to D,
and cold retrieval of D / 123. The name editor, edit-save confirmation and
cold detail are exact pixel oracles; the latter checks both the changed name
and retained number. Complete SIM files remain identical, no UPDATE RECORD
is allowed, and the rename changes persistent handset flash. Successors
inherit complete predecessor NVRAM. This is one single-letter rename, not
arbitrary character encoding, multiple contacts or a decoded flash-record
grammar. The rename and numeric/erase variants are deliberately separate
fresh fixtures. Capture: `tmp/npe3-phone-rename-final`.

The separate `--numbers` variant uses seven isolated processes. After the
empty control, fresh A / 123 save and cold retrieval, physical Options /
Add number / General adds789. A cold process must display the ordered detail
fields123,789,A. The next process selects789, opens its **Erase? 789**
confirmation and confirms it. Final cold navigation must display123,A,123,
showing the original contact/number retained and wraparound without789.
All seven complete SIM files remain identical, no UPDATE RECORD is allowed,
and both field mutations change handset flash. Exact detail/confirmation
pixel oracles reject skipped addition, unconfirmed deletion and whole-contact
erasure. This qualifies two General numbers on one contact and selective
field deletion, not arbitrary field types, capacity or record encoding.
Capture: `tmp/npe3-phone-numbers-qualified`.

The independent `tools/run_noki6210_phone_memory_copy.py DIRECTORY` gate
qualifies one **Phone to SIM card / One by one / Keep original** transfer.
Three fresh baseline processes establish an empty Phone-bank control,
firmware-saved A / 123 and cold Phone readback. A fourth process physically
copies that contact, renders **Copied** and completes the absolute EF_ADN
record-1 UPDATE with status9000. Exact whole-card comparison permits only
the expected 32-byte A / 123 record to change; every other record, security
field and settings byte must remain unchanged.

Two independent cold branches inherit the complete copied NVRAM. Each
explicitly selects its bank before searching; distinct **Phone memory
selected** and **SIM card memory selected** pixel oracles prevent reading
the destination twice from masquerading as source retention. Both branches
must render the exact A / 123 detail frame, preserve the complete copied
card and issue no UPDATE RECORD. The SIM branch must also read EF_ADN
record1. Menu positions are harness policy: the Phone bank inserts **Add
number** before Options, and Memory in use initially highlights the current
Phone choice. These observations do not decode flash records or qualify
reverse copying, Erase original, bulk copying, capacity or native DSP.
Capture: `tmp/npe3-phone-to-sim-final`.

The independent `--move` variant selects **Erase original** after the same
Phone-to-SIM / One by one route. It requires the distinct **Moved** frame,
the same exact EF_ADN record and complete-card preservation outside that
record. Independent cold branches explicitly select their banks: Phone must
render **Selected memory empty**, while SIM must retain A / 123. Neither
cold branch may write the card. A Keep-original capture cannot satisfy this
variant because the destructive choice, completion frame and cold source
oracle differ. This qualifies one firmware-owned cross-storage move, not
reverse/bulk copying or capacity. Capture: `tmp/npe3-phone-to-sim-move`.

`tools/run_noki6210_phone_memory_roundtrip.py DIRECTORY` extends the fresh
move gate with **SIM card to phone / One by one / Keep original**. The Phone
bank has already been proved empty in an independent cold process; the SIM
contact was created by the firmware's preceding move, not by a fixture write.
The reverse process must read EF_ADN, render **Copied**, change persistent
handset flash and issue no UPDATE RECORD. Two further cold branches inherit
its complete NVRAM, explicitly select distinct Phone/SIM banks and render
A / 123 from each. The entire source card must remain identical throughout
the reverse copy and both readbacks. This nine-process gate qualifies one
contact's physical move-and-copy roundtrip, not bulk transfer, merging,
capacity, field-type conversion or decoded flash-record semantics.
Capture: `tmp/npe3-phone-sim-roundtrip`.

### Laboratory registration

The runtime emits an organic type-`0x56` acquisition request with a 160-byte
body, starting `0023` followed by erased candidate entries. Own RX dispatcher
`4f604a` indexes the thirteen-entry table at `4f6078` for types `83..8f`.
Type `8b` calls `45835c`, which posts to **task 14**, not the NSM-3 task 12.
Type `89` calls `4580e8`; `458106..458110` compares body bit 0 against pending
context byte 2. `noki6210_radio_contract.py` pins these product-local facts.

The NPE-3 candidate-window peer completes organic acquisition, Location
Updating and acknowledged release. `verify-6210-registration` requires the
ordered exchange, persisted laboratory LAI `00f1100001` and updated EF_LOCI
status, plus the reviewed `DCT3 LAB` idle frame (SHA-256
`1138954cc94944c83019823ea500fa9ea9f8857c76e3d8ba929cdc40db4c0b74`).
The unattached-input accessory contract is described below. Neighbour/handover
and speech are not accepted; unrecovered handover fields remain unset.

### Phone-service acceptance

`verify-6210-sms-status-persistence` sends a physical `A`/5551234 SMS through
the host backend with PIN enabled, then starts a separate process using the
retained card and sends another. Each process requires PIN verification, own
registration, CP/RP/RR closure, and the sent-message frame. Firmware reads
EF_SMSS before submission and writes it afterward; its saved TP-MR must equal
the transmitted reference, its capacity flag must remain unset, and the second
reference must advance modulo 256. This proves handset-managed SMS reference
persistence, not native DSP or speech.

`verify-6210-home-plmn-reject` supplies GSM 04.08 cause 11 through the
ordinary downlink and requires rejection/channel-release acknowledgements,
EF_LOCI status 3, no acceptance, and unchanged erased EF_FPLMN. This is a
negative control: GSM 03.22 forbids adding the subscriber's home PLMN to
the forbidden list ([specification](https://www.etsi.org/deliver/etsi_ts/100900_100999/100930/07.04.00_60/ts_100930v070400p.pdf)).
The network outcome and visited-PLMN inputs are external laboratory policy;
they do not alter firmware state or subscriber identity.

`verify-6210-forbidden-plmn-persistence` uses visited test PLMN 001-02,
preferred by the external laboratory card without changing its IMSI/home
identity. Firmware performs Location Updating, receives cause 11, releases
the channel and writes the 12-byte EF_FPLMN. Two separate cold boots reuse
that resulting card and the acquired product PMM, with the network changed
to acceptance. The retained-list boot reads FPLMN and receives visited SI3
without issuing random access during 30 seconds; its entire card record
is unchanged. The control clears only FPLMN before boot: the same cell now
registers, releases and persists LAI `00f1200001`/status 0. Thus the negative
result is tied to retained FPLMN rather than merely an unresponsive peer.

International roaming is verified on the two-digit-MNC laboratory cell
234-15 by `verify-6210-foreign-registration`, including retained EF_LOCI
after a cold restart. The three-digit-MNC cell 310-260 is a negative
compatibility case: selection now completes, but the own mandatory LAI
parser rejects its non-`F` MNC filler. The contracts and checks below
distinguish selection completion from Layer-3 registration.

The pre-follow-up-model comparison establishes the original selection
failure boundary. Paired fresh 90-second research-HLE controls used the acquired NPE-3 PMM,
the same unchanged home IMSI, no PIN, coherent ARFCNs 35/36 and acceptance
policy. Only the external preferred PLMN and serving PLMN differ:

| Visited PLMN | Preferred-list reads | SI3 deliveries | Random-access-phase TX packets | Location Updating acceptance acknowledgements |
| --- | ---: | ---: | ---: | ---: |
| 001-02 (`00f120`) | 1 | 65 | 3 | 1 |
| 310-260 (`130062`) | 1 | 57 | 0 | 0 |

The captured SI3 PLMN bytes match each configured network, so the difference
is not an untested selector or absent broadcast. Evidence is retained under
`tmp/6210-plmn-current-national/` and
`tmp/6210-plmn-current-international/` (raw `error.log.gz`, console and input
invocation). This is a bounded no-random-access observation, not proof that
the phone cannot roam. The exact follow-up measurement contract below
resolves this pre-model stall; the subsequent mandatory LAI check separately
establishes the three-digit-MNC restriction. Neither observation implies
roaming coverage for every PLMN.

Own BCCH consumer boundary: type `80` decoder `4f611e -> 457f80`
routes channel `50` through `457f28`. Its admitted path changes the original
object's status to `0800` and posts it to task 18 at `457f70`. Both fresh
controls observe object `105cec`, caller `457f75`, at 6.773790308 seconds;
the same object's `0800` header is read by `3b9302` at trace PC `3b9308`.
That helper caches the object at context offset `1c`. Its caller `3ba706`
has a state-2/channel-50 path into BCCH decoder `3b9e34`; SI3 chooses
`3ba0c0`, then operation `19` in `3b9598` reaches parser `43f074`.
These static branches are pinned by `noki6210_radio_contract.py`, not all
asserted as executed by the header-read probe. Object reuse after reception
must not be interpreted as continued ownership by the original message.
Evidence: `tmp/6210-plmn-consumer-{national,international}/error.log.gz`.
In paired fresh 25-second observations, the first SI3 reaches `3ba0c0` in context
state 2 with flags `0e`. Both return 1 from `43f074` at `3b9a72`, take
the success branch `3b9a82`, and return 1 from the enclosing operation at
`3ba0d6`. The national control subsequently gets Location Updating acceptance;
the international control does not emit random access. Thus rejection at
these sampled parser return boundaries is excluded, not all PLMN validation
or all state-machine branches. Evidence:
`tmp/6210-plmn-parser-{national,international}/error.log.gz`.
The compact passive probe is `tools/noki6210_plmn_observe.lua`, scoped to
the own v5.56 research-HLE composition; its addresses are checked by the
radio contract tool. The next discriminator is downstream selection/admission
of the parsed SI3, not another synthesized transport message.

SI3 publication also survives the paired controls: caller `3ba2a5` posts
status `03f8`, word `+4 == 0` and completion byte `+8 == 1` to task 14.
Both use object `102a4c`; task 14 reads its original `03f8` header at
`2d3da4` before the object is later reused. National post/read times are
7.970027308/7.970089538 seconds, international 7.965625846/7.965688077.
This excludes an absent initial publication, not all later delivery failures.
Evidence: `tmp/6210-plmn-publication-{national,international}/error.log.gz`.

The first completed-SI3 classifier sequence also agrees: `4b2dd4` returns
0 at `4b2e0c`, `3c911c` observes that value at `3c915a`, and task 14
receives classification 1 at `2d3e3c`, with context state 2 and flags `0f`
in both runs. Earlier partial-SI publications return 1/1/0 at those same
boundaries. These are observed return values, not recovered semantic enum
names. Evidence: `tmp/6210-plmn-classifier-{national,international}/error.log.gz`.
The initial SI3 parser, publication and classification therefore do not
explain the national/international difference.

The first classification-1 continuation also agrees in the paired controls:
both reach `2d3ec0`, `2d3f26`, `2d3f64` and `2d3f6e`. At `2d3f26`,
R5 points to `172518` and its byte at offset 8 is `50` in both runs.
The decoded inclusive `10..18` test therefore takes the outside-range path
in both, followed by calls to `48a004(1, 3)` and `38b420`.
This excludes the first continuation's observed selection-byte fork as the
national/international discriminator, not later selection or admission.
Evidence: `tmp/6210-plmn-continuation-{national,international}/error.log.gz`;
the observation addresses are pinned by `noki6210_radio_contract.py`.

A bounded task-14 publication census locates the first message-stream
difference after this shared SI3 path. The first fourteen `(status, caller)`
pairs agree. The next publication is `03ec` in the national control at
10.136467923 seconds, versus `03ea` in the international control at
10.085341846 seconds; both return through publisher wrapper `3cc9fd`.
The national stream later reaches `03ed`, `03fe` and `0402`; the foreign
stream instead reaches `03f5` and repeated `03f9` publications.
These are numeric observations, not yet decoded selection outcomes.
Evidence: fresh 25-second isolated controls
in `tmp/6210-plmn-task14-census-{national,international}/error.log.gz`.
`tools/noki6210_plmn_task14_observe.lua` retains the read-only census;
its 160-publication limit is a diagnostic bound, not a topology closure.

The own-ROM switch at `3151da` selects `315234 -> 03ec` for saved
stack-offset-8 input `07d4`, and `315256 -> 03ea` for input `07d2`.
Both publish through `31525e -> 3cc660`, which copies R0 into the message
status and posts to task 14. Runtime confirms the national/foreign saved
inputs with the same R4 context `169be8`; their semantic names are unknown.
Instructions and pool literals are pinned by the radio contract checker.
`315070..315078` is a static candidate source that reads a signed R6
message header into the saved slot; execution for these inputs is unverified.
Evidence: `tmp/6210-plmn-{publisher,switch}-{national,international}/error.log.gz`.

The divergent inputs are themselves published to task 18: `07d4` in the
national control at 10.136268846 seconds, `07d2` in the international
control at 10.085155923 seconds. Both use object address `103544` in their
separate cold boots and return through `2a0615` (post call `2a0610`).
This publisher stores R4 into the object's halfword header and posts with
R0 = 18; the instruction contract is pinned. The result reproduces in two
fresh pairs under `tmp/6210-plmn-input-source{-branch,}-{national,international}/`.
The original headers are consumed at `3ccef0`, immediately after receive
call `3ccee8`: national `07d4` at 10.136331077 seconds, international
`07d2` at 10.085218154 seconds. These first reads are correlated with the
posted object; later accesses belong to reuse and confer no message ownership.
Evidence: `tmp/6210-plmn-input-consumer-{national,international}/error.log.gz`.
The candidate source path `31506a` remains unobserved (not disproven).
The upstream chain is now observed across fresh paired controls:

| Boundary | National | International |
| --- | --- | --- |
| Original post `2e2efa`, task 17 | `09c8` | `09c6` |
| Translator `2a086c`, caller `2a7d16` | `09c8` | `09c6` |
| Dispatcher `29ffb8`, caller `2a08c0` | `07d4` | `07d2` |
| Post `2a0610`, task 18 | `07d4` | `07d2` |
| Saved switch input, then task-14 publication | `07d4 -> 03ec` | `07d2 -> 03ea` |

`2a086c` translates against base literal `09c6`; its base and base+2
branches load `07d2` and `07d4`, respectively. The mapping and calls are
pinned, and runtime confirms the arguments, not just the resulting statuses.
The original task-17 posts occur at 10.134729462/10.083616538 seconds.
Evidence: `tmp/6210-plmn-{dispatcher,translator,upstream-post}-{national,international}/error.log.gz`.
The dispatcher observer's 9–11-second window is bounded research instrumentation.
Constructor `2e2e60` copies its R0 argument into the allocated header.
Its observed callers are national `2e6ee4` (`09c8`) and foreign `2e6e30`
(`09c6`). The earlier helper `2e36fc` returns 0/1 respectively at
`2e6da6`; the foreign result takes `2e6e18`, while national reaches
`2e6ec0`. The helper compares stored bytes at `16fe90` against two pairs
returned by `414a58` and `4148ac`:

| Control | Stored Pair | First Pair | Second Pair | Helper Result |
| --- | --- | --- | --- | ---: |
| National | `00f1` | `00f1` | `00f1` | 0 |
| Foreign | `13f0` | `00f1` | `00f1` | 1 |

The foreign mismatch branch `2e374a` and national return `2e374c` are
observed. This supports an identity/country comparison interpretation,
but the fields' complete semantic contract is not yet recovered. The
different status is therefore explained by an actual pair mismatch, not
an absent parser result or guessed DSP reply.
Evidence: `tmp/6210-plmn-{constructor,decision,compare-pairs}-{national,international}/error.log.gz`.
The foreign mismatch continuation is not waiting indefinitely at its first
follow-up receive. `2e321c` calls `3c2470`, dispatches/free-loops over
messages, and returns a surviving numeric status. Foreign return at
`2e6e38` is `1583` (10.614096923 seconds); national's corresponding
return at `2e6eec` is `1584` (10.619503462 seconds).
At foreign branch `2e6e44`, context `16fe18` has words `+60 = 1`,
`+38 = 2`, `+50 = 0`. Its shifted-word test takes the zero branch;
the later `1589` comparison fails and enters `2e6046`. Runtime observes
R0 = 2 at `2e60a2` (10.616668231 seconds), whose pinned instruction
writes that value to controller halfword `+22`. This is a numeric lifecycle
transition, not a recovered enum name or proof of cell admission.
Evidence: `tmp/6210-plmn-followup-{receive,context,transition}-{national,international}/error.log.gz`.
The state table at `2e4e78` selects `2e6e68` for controller state 2.
That handler compares R6 with `1589`; a mismatch enters `2e6046`, which
retains state 2 except for its separate `1580` branch. The foreign run
observes R6 = `1583` at `2e6e68` before its state-2 write, then enters
another `2e321c` receive. No later receive return or `1589` RAM-object post
through the observed RTOS primitive is captured during the 25-second run;
this is bounded non-observation, not proof of producer absence.
The candidate source is in ROM: `2a394e` loads literal `1589`, calls
`2a7f74`, and that entry forwards it to dispatcher `29ffb8` with R1 = 0.
Instructions, pool literal and state-table entry are pinned by the contract
checker. Evidence: `tmp/6210-plmn-state2-dispatch-{national,international}/error.log.gz`.
The dispatch condition for this candidate is explicit: saved stack-offset-4
status `09fd` at `2a31ca` matches literal `2a3534`, then branches through
`2a3948` to the `1589` publisher. Neither `09fd/1589` RAM-object posts nor
the completion-dispatch entry is observed in either 25-second control;
the passive taps do not establish a global absence proof.
A static predecessor exists: `31321e` loads `09fd`, branches to `313156`,
and calls publisher `3ccbec` with R1/R3 zero and the existing stack+12
argument in R2. That publisher copies the status into its message header.
The local selector compares saved stack+8 with `07f4` at `312e1e..312e24`
(literal `313184`) and branches from `312e26` to `313218` on equality.
For status `09fd`, publisher `3ccbec` subtracts `080b`, `60`, `ff` and
`8d`, selecting index 6 of its twelve-entry table. Entry `3ccc84` points
to `3cccf0`: it frees the temporary message, writes the status into the
pre-existing object at `[172fa4+10]`, and posts that object to task 17
through `3ccd3c/3ccd44`. This is a static destination contract, not an
observed completion or proof that ordinary selection produces `07f4`.
No synthetic completion is justified.
Evidence: `tmp/6210-plmn-completion-dispatch-{national,international}/error.log.gz`.
An aligned 32-bit literal scan of the complete acquired image finds two
`07f4` literals: selector `313184` and publisher pool `4cffb4`. The latter
has a Thumb literal-load reference at `4cfdde` in routine `4cfdd4`, which
allocates a 12-byte object, writes its header and payload, and posts to
task 18 at `4cfe1e`. An image-wide direct Thumb BL candidate scan finds
one call to that entry, at `2d1126`. This does not cover indirect calls,
arithmetic status construction or data-driven publication.
Three nearby direct branch candidates reach this publisher's caller:
`2d0d5a` reaches `2d111a` when query `3c1ad0(75)` returns zero;
`2d0f1e` reaches `2d1118` when R1 equals literal `0419`; and `2d1062`
reaches `2d1118` when byte zero of the object referenced by `175458`
equals 5. These local comparisons are pinned to the own image, but their
enclosing entry conditions and subsystem semantics are not yet recovered.
They do not justify treating `07f4` as an ordinary roaming completion.
The `0419` comparison follows receive/dispatch entry `2d2952`, not a
normal getter: it frees `[R4+8]`, receives through `3c2470`, replaces
`[R4+8]`, and copies the signed header to context+4 and message byte 3
to context+0. It reloads the saved header into R1 at `2d2978`; the
unmatched-status branch redirects through `2d2528`, which can transfer
to `2d0f18` and `2d0ee4`. These BL transfers participate in the controller
dispatch; do not infer ordinary call/return ownership from their opcodes.
The image-wide aligned `0419` literal scan finds three pools (`26dd14`,
`29e588`, `2d1214`), with Thumb literal-load candidates at `29e1e0`
and consumer `2d0f18`. The former is not a recovered message publisher:
it passes `0419` in R1 into local dispatcher `295f2c`, which preserves
the supplied context in R10 and selects `296bec` by subtracting literal
`0412` and then 7. Do not count that literal use as an RTOS send site.
The remaining occurrence `26dd14` is a key in a 64-record, eight-byte
lookup table at `26dbac`. Reader `46a53c` obtains that base through
literal `46a8c4`, compares each record's word with the constructed key,
copies record byte+4 to the supplied output pointer on a match, and
returns 8 (4 on exhaustion). Its loop advances eight bytes and is bounded
at `40` records. This use is not a message publication either; no semantic
meaning is assigned to its metadata byte here. Thus all three aligned
literal occurrences are classified, but computed, indirect and other
table-driven message producers remain outside this scan's coverage.
Fresh 25-second national and foreign controls reproduce the original
`09c8/07d4` versus `09c6/07d2` split with clean observer health. Passive
taps at `2d1118`, `2d111a`, `4cfdd4`, `313218`, `2d0d50` and `2d1048`
record no hits in either control. This is bounded non-observation, not a
proof that the code never executes (instruction-fetch tap granularity
still applies). Evidence:
`tmp/6210-plmn-completion-entry-{national,international}/error.log.gz`.
Additional fresh paired controls show that the controller itself is active:
foreign boot reaches receive entry `2d2952` at 10.616510385 s with R4
`174550`, then repeatedly reaches `2d2528`; subsequent receive entries
are approximately 305 ms apart. The observer records its twelve-hit cap
at both sites in the foreign run and no hits at either in the national
run. Thus absent publisher-entry hits must not be generalized to a dormant
controller. R0 is `03fe` at the observed `2d2528` entries, but that is not
the saved status in R1 and must not be labeled as its incoming message.
Evidence: `tmp/6210-plmn-completion-controller-{national,international}/error.log.gz`.
Direct R1/header observation resolves those foreign dispatcher inputs:
`03f9` at 10.902136692 s, then `1802` (message byte 3 = `83`) at
11.201837538 s, then `03f9` at 11.207263615 s. This alternation repeats
through the twelve-hit cap at 12.726838000 s; `1802` arrivals are about
305 ms apart. R1 agrees with context+4 in every recorded entry. There
are no such dispatcher observations in the national control. Do not
assign timer semantics solely from cadence or byte `83`; identify the
actual publishers first. Evidence:
`tmp/6210-plmn-controller-input-{national,international}/error.log.gz`.
The local consumer at `2d0f28` recognizes `1802` but its routes toward
`2d1048`/`2d1038` require kind `8f`/`8b`, respectively. Observed kind
`83` satisfies neither. The following comparison at `2d0f5a` separately
recognizes `03f9`; its continuation additionally checks the referenced
state at `[1711dc]`. These static guards are pinned, but the runtime
branch and referenced state's semantic ownership still need observation.
Source/destination observation identifies the repeated foreign inputs as
task-14 posts: `1802`/kind `83` has caller LR `457a1b` (post at `457a16`),
and `03f9` has caller LR `3ba705` (post at `3ba700`). The national run
also receives `1802` kinds `89`, `8b`, `84` and later `83`, without the
mapped controller-dispatch hits. Therefore `1802` is not a uniquely
missing foreign peer message; its MCU routing/context lifecycle matters.
No `2d0f6e` continuation hits occur in either 25-second control. This
remains a tap-bounded observation, not a proof that its state check is
the sole reason the continuation does not run. Evidence:
`tmp/6210-plmn-controller-post-{national,international}/error.log.gz`.
Sampling the referenced guard at the active `2d2528` entry shows
`[1711dc] = 102754` and `[102754] = 1` at all twelve foreign observations
(10.902136692..12.726838000 s). Thus these samples do not satisfy the
`03f9` continuation's own state-3/state-4 comparison at `2d0f60..2d0f6c`.
This establishes a concrete unsatisfied MCU state predicate, not its
meaning or a justified state replacement; observe its actual writers
and lifecycle next. Evidence:
`tmp/6210-plmn-controller-guard-{national,international}/error.log.gz`.
An ownership-following write watch observes allocation/publication of
object `102754` at 3.687860846 s in both controls, then its zero
initialization. Both subsequently reach value 4: national at
10.328670769 s, foreign at 10.323930000 s (reported callback PC `38b790`).
Foreign then resets it to zero at 10.614832385 and 10.615582846 s
(reported PC `38b514`), and writes 1 at 10.616167615 s (reported PC
`2d1074`). These are firmware writes, not a missing initial state-4
publication. Own-ROM stores are pinned at `38b516` (zero initialization),
`38b792` (computed state) and `2d1074` (literal 1); callback PC is not
assumed identical to the store's address. A later national zero write
is reported inside `3c2aca`; its allocator/lifetime meaning is not yet
classified and must not be treated as a controller transition by default.
Evidence: `tmp/6210-plmn-controller-writers-{national,international}/error.log.gz`.
Reset wrapper `38b5c6` calls zero-initializer `38b502`, then `38b588`
with its saved input. Paired entry observations show input 3 from caller
LR `2d42d9` at 3.687833846 s in both controls. Only foreign boot calls
it again: input 3 from LR `2d0b3b` at 10.614830077 s and LR `2d0cd7`
at 10.615580538 s, immediately preceding the two observed zero writes.
The corresponding own-ROM BL sites are `2d42d4`, `2d0b36` and `2d0cd2`.
Trace the latter two entry conditions rather than treating state 1 as
a missing DSP publication. Evidence:
`tmp/6210-plmn-controller-reset-call-{national,international}/error.log.gz`.
The first foreign reset block is selected by incoming `03f5`: comparison
`2d0902..2d0906` uses literal `2d0c7c`, then branches to `2d0b1c`.
That block calls initializer `4cfe3a`, copies incoming object fields via
`4cfdb2`, and passes configuration byte `[172518+34]` to `38b5c6`.
Fresh paired observation confirms publisher `3cc660` receives `03f5`
from caller LR `315565` at 10.614435077 s in foreign boot, followed by
selector R1 `03f5` at 10.614610846 s. National selector observations
instead include `03f4`, `03ed` and `03fe`, with no observed `03f5`
publication. Thus the reset is attached to a specific firmware-message
branch, not an unexplained peer-state loss. Evidence:
`tmp/6210-plmn-controller-entry-status-{national,international}/error.log.gz`.
Instruction breakpoints confirm `315560` executes with R0 `03f5` and
LR `3131fd` in foreign boot, then `3cc660` receives R0 `03f5` and LR
`315565`. The actual task-14 post independently has header `03f5` and
saved return `315565`. This is a shared publication point: direct branches
from multiple status-preparation blocks reach `315560`. Its physically
preceding literal load (`31555e`, value `03fa`) must not be assumed to
have executed on every incoming path. There is no ROM-byte discrepancy.
Recover the specific `03f5` preparation branch before assigning its
subsystem semantics. Evidence:
`tmp/6210-plmn-publisher-instruction-{national,international}/error.log.gz`;
`tmp/6210-plmn-publisher-return-{national,international}/error.log.gz`.
The concrete `03f5` preparation is `3131f6` (literal `313574`), followed
by BL `3131f8` to the shared `315560` post. Selector `312e34..312e3c`
enters that block when saved stack+8 equals `07d7` (literal `313188`).
Fresh instruction observation confirms selector input `07d7` in foreign
boot. Its preceding task-18 post is observed at 10.614313769 s, caller
LR `2a062b`, object `1038d4`; own post site `2a0626` sends the existing
object from context+68. Do not assume the earlier `09c6/09c8` translator
owns this publication: a `09cb` constructor/post is observed in the
national control instead, and no such foreign post is recorded by this
bounded observer. Recover the condition selecting `2a061a` next.
Evidence: `tmp/6210-plmn-03f5-selector-{national,international}/error.log.gz`.
That run also records dispatcher `29ffb8` input `07d7` from LR `2a7f7b`
at 10.614281231 s. The ROM's matching preparation `2a3942` loads `07d7`
from `2a3ca4` and calls `2a7f74`. Dispatcher table entry `2a015c` points
to `2a0598`, which loads `07d7` from `2a0934` into R2 and branches to
`2a061c`; that block writes the status into context+68's existing object
before posting it to task 18. Adjacent `2a061a` loads a different status
(`07d1`) and must not be mistaken for this path's predecessor.
Foreign boot constructs `09c9` at
10.614109385 s, caller LR `2e6e69` (own call `2e6e64`), posts it to task
17 at 10.614156000 s, and instruction observation at `2a393c` reads
saved input `09c9`. Selector `2a31d4..2a31dc` compares stack+4 against
literal `2a3538 = 09c9` and enters that preparation. National records no
such constructor/post/selector in this bounded control. The connected
organic chain is therefore `2e6e50` continuation -> `09c9` -> `07d7` ->
`03f5` -> controller reset/state-one lifecycle. Its higher-level semantics
and the condition for completing that lifecycle remain unresolved; no
additional peer completion is justified solely by this mapping.
Evidence: `tmp/6210-plmn-07d7-selector-{national,international}/error.log.gz`.
Search selector `4cfc34` uses a stored halfword at `175454`. Instruction
observations show two input-1 calls in each control retaining stage 1
and returning 5. Only foreign boot then calls with input 0, enters with
stage 1, returns 0 and leaves stage 5. The own stage-1 path at
`4cfd6a..4cfd84` sets result 0 and writes stage 5 via `4cfcd6`; do not
conflate that stage with the separate state word referenced by `1711dc`.
The following task-14 zero-result branch writes that separate word to 1
and calls `3f2684` with R1=1. Recover the issued lower-layer request and
its completion contract next; input/stage numbers alone do not name
manual selection, full-band search or a GSM service. Evidence:
`tmp/6210-plmn-search-stage-{national,international}/error.log.gz`.
The static request constructor `3f2684` allocates and zeroes eight bytes,
sets header 2, byte+2 to 4 and byte+3 to `57`, populates bytes+4/+5
through `3f2226`, and posts to task 3 at `3f26c6`. These are own-ROM
message fields, not yet an identified over-the-ring DSP primitive.
Paired runtime capture records request bytes `0002045703050000` in both
boots (foreign 10.323085154 s, national 10.327683077 s). Only foreign
then sends `0002045703030000` at 10.616327769 s. Each is first read at
`4ea0dc` after task-3 receive `4ea0d6`; own header-2 routing queues its
pointer at `4ea1d0`. This establishes the actual bytes and queue ingress,
not evidence that the queued object has already reached the DSP.
The own-ROM queue drain loads the object at `4ea266`, checks ring credit
through `426b9a`, and calls `426a3c(length, object+3)` at `4ea27a`.
That writer combines length and kind into the first halfword, then calls
`4269c0` to copy byte pairs in big-endian order into the ring based at
`10000`, wrapping before `100a4` and publishing its halfword write index
at `100a4`. Thus the captured common request encodes as
`0457 0305 0000`; the foreign follow-up encodes as `0457 0303 0000`.
The same paired runtime logs independently confirm HLE TX delivery:
common type `57`, payload `03050000` at 10.323286 s (foreign) and
10.327884 s (national); foreign-only payload `03030000` at 10.616529 s.
The word packing itself is derived statically, while these decoded
transport observations establish that the queued messages actually drain.
The differing byte is preserved by transport, not interpreted by this
writer. Next verify ring publication and the peer's type-`57` handling;
neither its mode semantics nor its expected completion is established.
The NPE-3 peer accepts the exact configured background request `03050000`
and the product-owned follow-up `03030000`. Other products do not inherit
the follow-up, and arbitrary type-`57` modes remain unsupported. Both use
the ordinary `8b` measurement consumer described below; this is an HLE
response contract, not native DSP execution.
The consumer has a viable ordinary measurement entrance: `1802` kind `8b`
branches at `2d0f56` to `2d1038`, which passes the referenced payload to
`38b6ae`; unlike kind `8f` and status `3f9`, this dispatch does not require
the controller state to be 3/4. `38b6ae` consumes at most forty four-byte
records per response. Completion is cumulative and configuration-dependent:
`38b5e8(3)` requires count halfword +4 >= 30 **and** +6 >= 40. A single
forty-record response is therefore not generally a complete scan. The count
classification and field ownership still need decoding; do not label the
two count fields as physical bands solely from these thresholds.
The request mapper `3f2226` independently distinguishes configuration from
its secondary selector: configuration 3 selects bytes `05 03 03` at
`2819a9`, indexes them by R1, and writes the result to message byte+5.
Thus the foreign constructor's secondary input 1 maps to wire byte `03`,
where input 0 maps to `05`. These are not two configuration IDs inferred
directly from byte+5. Next capture the configuration and cumulative counts
around the existing common `8b` response, then decode which records advance
each count before implementing the unanswered request.
Own classifier `43e9be` returns class 1 for ARFCNs 0..124 and
975..1023, class 2 for 512..885, otherwise zero (for the unsigned
halfword inputs used here). `38b2ec` increments count +4 for class 1
and +6 for class 2, bounded to 30/40 when its second argument is one.
`38b6ae` reads each record's signed RSSI and stops before accepting a
record below -104 dBm. The existing background peer reports receivable
cells followed by `ffff`/-127 padding, so forty slots do not mean forty
accepted measurements.
Fresh paired read-only runs confirm the consequence in both boots:
`38b6ae` receives configuration 3 with state/counts `0 / 0,0,0`; at
`38b79c` the state is 4 and counts are `1,0,0`. Only one class-1 record
was accepted, not the required 30+40. Foreign selection later resets
this state and issues the unanswered `03030000` request. This rules out
describing the common response as a complete multiband scan; it does not
justify filling nonexistent strong carriers or duplicating the one cell
to satisfy counters. Recover the mode's weak-result/scan-end behavior
and retry budget as well as its band selector before changing the peer.
Evidence: `tmp/6210-plmn-measurement-counts-{national,international}/error.log.gz`.
The next own-ROM continuation is now statically bounded. Stage 5 with
input zero checks the referenced measurement state: values 3 or 4 take
`4cfc92` to `4cfcb0`, then call `38a80a`. The intermediate `38b27a`
is an empty return in this image, not an unimplemented hardware command.
`38a80a` collects up to forty ARFCNs from accepted records whose low
flag nibble is 2. If none exist it returns 3; otherwise it posts a
type-`09`, 80-byte ARFCN array to task 3 through `3f22ce`, with unused
bytes initialized to `ff` by `3f22e6`. This is a concrete downstream
request family, not immediate completion publication. Its response
contract remains to be recovered before claiming selection completion.
For configuration 3, `38b662` tests the next response count against 14;
below that it returns zero, at/above it returns one. A weak record can
select state 4 earlier, as the measured common response demonstrates.
Do not interpret this bound as a requirement to send fourteen identical
responses or fabricate enough strong carriers to fill the count quotas.
The own controller's later response entrances are explicit:
kind `88` at `2d0ee4` requires referenced state 3/4, calls `38afea`,
and returns to receive rather than immediately advancing the selection
stage. The RX table maps `88 -> 45808c -> task 14`.
`1802` kind `8f` at `2d0f30` requires referenced state 3/4 and joins
stage advancement at `2d1048`. The RX table routes `8f` through
`4f60cc -> 458054`, posting to task 14 while that handler is enabled.
The separate positive-content entrance at `2d0fc8` requires kind `80`,
decoded object channel `40`, and the same state 3/4. It calls `38a27c`
before joining `2d1048`. That parser extracts an ARFCN from object bytes
+10/+11, looks up an existing record, and returns without record mutation
if lookup fails; arbitrary channel-40 bytes cannot establish a candidate.
The parser then delegates content handling to `38a054`. These are bounded
consumer contracts, not proof that a particular type-`09` request has
received or completed either response. In particular, a measurement `8b`
can establish the state guard, but does not supply decoded synchronization
content or automatically publish the previously mapped `7f4` completion.
The distinct `88` result supplies the missing record-state transition.
`38afea` extracts ARFCN from object bytes +8/+9 (payload +4/+5), looks
up an existing measured record, and calls `38afa8`. Success requires
object byte+4 (payload byte 0) to be zero; `38af66` parses its remaining
fields, then the firmware writes low flag nibble 3 at record+13.
Nonzero status increments record+11 and, unless that byte wraps, sets
low nibble 1 instead. An unmatched ARFCN is ignored, not allocated.
`38a954` searches for nibble 3 and selects the corresponding record.
By contrast, successful type-`80` channel-`40` content through `38a054`
writes nibble 4. This establishes distinct selection phases, not
interchangeable positive-reply shapes.
The `88` field decoder `38af66` has an executable contract in
`tools/noki6210_selection_result_check.py`: the first eight payload bytes are
status, a big-endian 24-bit field F, ARFCN, and a big-endian 16-bit field P.
It stores `(F + floor(P/5000) + 52) mod 51` at record+8 and `P mod 5000`
at record+4. The division helpers return remainder in R0 and quotient
in R1; reversing that convention would corrupt both results.
Isolated own-ROM Thumb execution verifies 48 field-boundary cases and
nine success/failure/retry-wrap transitions, including preserved unrelated
record bytes. This identifies the modulo arithmetic and record layout,
not the DSP's clock origin, timestamp capture point or physical phase units.
Zero-filling a success payload merely to obtain nibble 3 is not a validated
peer contract. Derive those source values and transaction ordering before
implementing an operational selection peer.
The archived Black Sphere primary vocabulary independently names TX `09`
as `PSW_SEARCH_LIST` (forty halfwords), RX `88` as
`NEIGHBOUR_TIMING_OFFSET` (ten-byte body), and RX `8f` as `NO_PSW_LEFT`.
Those names corroborate the own-ROM list/result/completion distinction;
they do not specify this product's timing origin or ordering. Its TX `57`
entry is explicitly unnamed, so that source does not settle the `03`/`05`
selector semantics. The executable decoder check now uses a ten-byte
body with nonzero trailing bytes, but validates only the consumed
eight-byte prefix. Do not present that check as a complete type-`88`
encoder or assign meanings to its unconsumed final two bytes.
The recovered source fields have a concrete downstream use:
selected record nibble 3 makes `38ae9a` call `38ae5c`, which builds
mode 2 and copies record+4 phase, record+6 ARFCN, and record+8 frame
into `3f2500`. That constructor publishes type `11`, length 12:
payload byte 0 is mode, bytes 2/3 are phase, 4/5 are the frame value,
and 6/7 are ARFCN. This is a source-to-request contract, not proof
that the existing neighbour peer's mode interpretation is valid here.
The executable selection-result check now also runs the real `3f2500`
field-copy block for 96 decoded-field/flag combinations, with both a
low-band ARFCN and ARFCN 512. It stops after construction, before posting;
it does not pretend an isolated scheduler stub is an organic transaction.
Both phase/frame fields are preserved in the outgoing request. The
constructor maps source byte+8 bit 5 to payload byte 9 values 1/6; the
physical meaning of that mode-specific choice remains unvalidated.
The public [NMEAS field notes](https://web.archive.org/web/20160119230146/http://nokix.sourceforge.net/help/blacksphere/sub_100hardware/sub_dsp/sub_mdi/sub_typestxd/sub_11.htm)
corroborate 1/5000-frame phase units, modulo-51 frame, and ARFCN positions,
but explicitly leave the 1..5 mode selector unnamed. That confirms the
field units without supplying an authoritative mode-two response sequence.
The current peer requires membership in its independently received
neighbour list; the type-`09` PSW-search list is not yet modeled as
that list. Preserve this distinction when adding selection support.
The phase dispatcher is executable, not merely inferred from nearby code.
`38ae9a` reads the selected-record pointer at `1711d8` (distinct from
the last lookup pointer `1711d4`) and dispatches on the record's low
flag nibble. Exhaustive isolated execution of all sixteen values proves:
nibble 2 takes the direct type-`11` mode-4 constructor, nibble 3 takes
`38ae5c` (mode 2), and nibble 4 takes `38ae22` (mode 3); other values
select none of those constructors. Mode 3 also copies record+10 into
the outgoing BSIC byte. Thus the evidence supports phase-specific
handling rather than one generic neighbour request: measured record,
timing-result record, and decoded-result record are distinct prerequisites.
These are descriptive aliases, not recovered Nokia names for the modes.
No live mode-two request/response or foreign registration is established
by the isolated dispatch checks.
Primary references:
[TX vocabulary](https://web.archive.org/web/20160119223852/http://nokix.sourceforge.net/help/blacksphere/sub_100hardware/sub_dsp/sub_mdi/sub_typestxd.htm),
[RX vocabulary](https://web.archive.org/web/20160119223857/http://nokix.sourceforge.net/help/blacksphere/sub_100hardware/sub_dsp/sub_mdi/sub_typesrxd.htm).
Evidence: `tmp/6210-plmn-search-request-{national,international}/error.log.gz`.
Live follow-up validation: the exact `03030000` request receives the
topology-derived `8b` response. The referenced controller moves from state
1 to 4 with counts `1,0,0`, then takes the no-eligible-candidate completion
branch (stage return 5, reset to stage 1). It does not issue type `09` in
this fixture. Firmware organically publishes `07f4` to task 18 at
10.618152692 s, `09fd` to task 17 at 10.618819769 s and `1589` to task 19
at 10.619167462 s. Random access follows, then a Location Updating Request
and acknowledgement of Location Updating Accept at 11.243672 s.

Three-digit-MNC negative case, downstream of that acknowledgement:
the 25-second international fixture retains invalid EF_LOCI
(`ffffffffffffffffffff01`), with no location-update SIM writes and no
observed Channel Release acknowledgement. Reaching the network acceptance
is not proof of completed registration, persistent roaming state or operator
presentation. Inspect the firmware's acceptance/release consumers before
adding another peer response. The alternative candidate-search/type-`88`
contracts above remain isolated execution evidence, not exercised live
contracts in this run. Evidence:
`tmp/6210-plmn-followup-measurements-international/error.log` and its saved
`nvram/npe3hle/sim_card`. Home-network registration remains gate-green with
the same peer correction.

The release-window uplink is not the ordinary supervisory acknowledgement:
its LAPDm information body is `057160` (MM STATUS, sequence bit masked from
message type `71`, cause `60`). Cause 96 is invalid mandatory information
under [GSM 04.08 section 8.5](https://www.etsi.org/deliver/etsi_gts/04/0408/05.03.00_60/gsmts_0408v050300p.pdf).
This supplies a concrete next target: identify the rejected mandatory field
in the received `0502` Location Updating Accept, particularly the foreign
LAI `1300620001`, rather than treating the LAPDm acknowledgement as Layer-3
acceptance. Home registration, physical outgoing-call signaling and incoming
SMS acceptance all pass with this same build.

Passive byte-lane-aware observation locates the first construction of the
MM STATUS cause at `29fb70`, not the later copies at `50698c`. The
constructor at `29fb68` writes message type `31` and loads its cause from
`16a814+8` via the literal at `29fd48`. A write-watch on that cause byte
observes `60` being stored at `2a343c` at 11.234343692 s, then cleared by
the constructor at `29fb78`. The own-ROM dispatcher compares its incoming
input with `66` at `2a340a`; that branch reaches `2a344c`, loads cause `60`
at `2a3452`, and joins the observed store. Thus the next census target is
the producer of decoder-error input `66`, not the later release handling.
These instruction sites are pinned by `noki6210_radio_contract.py`.
Evidence: `tmp/6210-foreign-mm-cause-writer/error.log.gz`; the watch and
transmit observation change no firmware state.

The mandatory-field failure is now identified, not an unknown response
shape. Location Updating Accept enters `2a12b8`, calls `2a0b70` with IE
selector `13`, then checks the parse-status byte. The LAI branch at
`2a0d5a` requires at least five bytes and requires the high nibble of
LAI byte 1 to equal `F` (`2a0d5e..2a0d64`). The foreign cell's
`1300620001` has nibble 0 there, denoting a three-digit MNC. A fresh passive
run observes that exact LAI and nibble 0 at the comparison, then message
type 2 reaching the `2a1332` error return. This is an own-ROM mandatory LAI
parser restriction, not an unanswered DSP packet or missing SIM write.
`noki6210_lai_check.py` executes the complete parser for all sixteen
nibbles and lengths 0..6: all 112 cases return, and only filler `F` with
length >= 5 is accepted. This isolated test does not modify a live phone.
Runtime evidence: `tmp/6210-mm-lai-mnc-gate/error.log.gz`.

Do not patch this validator or relabel `310-260` as a two-digit-MNC cell.
The supported roaming fixture uses a separately configured,
standards-valid foreign two-digit-MNC laboratory cell and requires both
Layer-3 acceptance and persistent EF_LOCI. Retain `310-260` as a negative
compatibility case. Three-digit-MNC support on this ROM remains unavailable
through this decoded acceptance path; broader product support is a separate
cross-ROM question.

`verify-6210-foreign-registration` selects a laboratory cell with identity
234-15 (`AUTHCFG` serving-PLMN value `08`) on ARFCNs 35/36. This is a
synthetic network, not a connection to that commercial operator. The SIM
keeps its original laboratory IMSI/home identity and prefers the visited
PLMN; the acquired product PMM is unchanged. The initial run requires
random access, Location Updating Accept delivery acknowledgement, Channel
Release acknowledgement and firmware SIM writes yielding EF_LOCI
`32f4510001` with valid status. A second independent process copies only
the same handset's retained NVRAM/configuration, requires EF_LOCI to be read
before the retained-LAI Location Updating Request, and verifies acceptance,
release and continued valid foreign location without a redundant status
rewrite. Both processes pass. Home registration remains independently
gate-green. Both boots also require the reviewed 96x60 idle frame showing
`VODAFONE` and laboratory SPN `DCT3 LAB` (SHA256
`3a130f147f3fb7c3083c9508bf6a3a4f11115a6fa42b0806bfbb7b1d39c48ac3`),
then physical Menu decoding and the existing Messages-menu oracle.
This proves research-HLE foreign registration, presentation and storage,
not native RF or speech.
Evidence: `tmp/6210-foreign-two-digit-cold-persistence/acceptance.json`, its
`error.log.gz`, and `cold-registration/error.log.gz`.

`verify-6210-foreign-outgoing-call` and
`verify-6210-foreign-incoming-sms` select the same foreign laboratory cell
through the runner's explicit `--foreign-cell` option. Both require valid
foreign EF_LOCI after the complete registration/release exchange before
their service checks. The call gate physically dials `1234567`, presses
Send/End and verifies the existing CC/RR establishment, Connect and release
contract on coherent ARFCNs 35/36. The SMS gate verifies one IMSI page,
CP/RP acknowledgements, SIM delivery/read-status writes, physical Read keys
and the exact existing `hello` SMS screen. Both pass; speech remains
unproved. The option is rejected for unsupported scenarios rather than
silently altering other acceptance profiles. Runtime evidence:
`tmp/6210-foreign-outgoing-call/acceptance.json` and
`tmp/6210-foreign-incoming-sms/acceptance.json`. Presentation evidence:
`tmp/6210-foreign-presentation-gate/` and its `cold-registration/` captures.

`verify-6210-outgoing-call` physically dials `1234567` and presses Send/End.
`verify-6210-incoming-call` queues one laboratory network call, captures
ringing and connected screens and physically answers/ends it. Both require
ordered CC/RR establishment, traffic assignment, Connect acknowledgement,
Disconnect/release and return to paging. Own traffic/release configurations
are 24 bytes, not the NSM-3 20-byte forms. Physical End emits release
parameter `14`, now selected by the NPE-3 peer. Incoming Call Confirmed is
`8308040460020081150101`. These prove signaling, **not speech**.

The physical NPE-3 board now attaches COBBA EAR to the speaker and the
internal microphone to MIC2, as specified by Nokia System Module issue 1
(09/00), table 8. Unity MAME routes declare connectivity, not analog gain or
the runtime codec mux. The PCM profile remains unspecified and the HLE
voice selector remains disconnected; these routes do not enable fabricated
call audio. The independently mapped command-8 speech-request field and
exact missing clock/framing evidence are owned by
[`external_call_bridge.md`](external_call_bridge.md).

### Paired DSP-control mode boundary

The own hash-pinned control checker additionally recovers the positive
mode1..9 table at`0x262444`. Publisher`0x303c30` reads two big-endian
halfwords per record and calls compiler`0x426eb4` with selectors8/9:

| Mode | Command8 parameter | Command9 parameter |
| --- | --- | --- |
| 1 | `0702` | `0505` |
| 2 | `0702` | `0604` |
| 3 | `0702` | `0700` |
| 4 | `0603` | `09a5` |
| 5 | `0603` | `0aa4` |
| 6 | `0603` | `0ba0` |
| 7 | `0702` | `0d05` |
| 8 | `0702` | `0e04` |
| 9 | `0702` | `0f00` |

These are compiler inputs, not decoded COBBA registers or PCM rates.
The halfword-aligned direct-BL census finds one candidate caller,
`0x30566a`; its preceding`0x305668` loads unsigned message byte9.
Consumer`0x305258` dispatches message byte8. Its subtract cascade uses
base`0x73+3`; all55 table records at`0x3053b0` are checked, and index10
alone selects this caller, hence command`0x80`. Mode0 takes a separate
branch; positive values>=10 are rejected by the signed compare. The
publisher alone has no negative-input guard, but this mapped consumer
supplies an unsigned byte. Indirect calls and command producers remain
outside the direct-call census coverage.

The parent consumer at`0x305ab8` is reached through class`0x40` at message
byte3, not DSP packet type80. The paired publisher subsequently constructs
class40/command80 at`0x303cbe`, echoes the selected mode into byte9 and
sends it through`0x3026d8`. This is a reply constructor, not evidence of an
initiating command producer. The checker pins the parent subtract cascade,
builder class/command stores and mode echo. It does not close every possible
class40 producer or indirect call.

`verify-6210-dsp-control-observation` retains the complete physical outgoing
call acceptance and adds read-only debugger observations. Its own coherent
run`tmp/npe3-normal-control-gate-final` passes with182 compiler observations,
including9 selector8 entries, but zero paired-mode or class40-consumer hits.
Positive initialization0102 and physical Send/End070b/050a entries all have
R14=`0x420c0f`, identifying the normal publish call`0x420c0a`. Its changed-
parameter comparison and11-selector table at`0x263884` are source-pinned:
`08 09 1b 25 20 21 22 23 24 28 2d`. Null compiler instrumentation cannot
qualify this observation. This is one coherent call, not universal absence
of a service route or a claim that every paired mode is unreachable.

#### Normal-call parameter context

The first parameter's bit0100 is paired with bit0020 in two source-pinned
branches, using context block`0x16ed50`, context byte`+0c` and state byte`+0a`:

| Builder state | Context selecting0020 | Other contexts |
| --- | --- | --- |
| 0 | 2 | Select0100 |
| 1 | 1 | Select0100 |

The selected branch ORs0020 and keeps`feff`; the alternative keeps`fedf`
then ORs0100. Other bits are preserved. Stores are`0x420a48` for state0
and`0x4209de` for state1. These are intermediate firmware parameters, not
decoded COBBA registers. `parameter0_pair()` desk-checks only these two
branches; it rejects other states rather than inventing their behavior.

The extended coherent call gate`tmp/npe3-parameter-context-gate-final`
records9 command8 publications with context00 throughout. State00 accompanies
0102; state01 accompanies050a/070b. All use source`0x172d80` and
cache`0x172d68`, non-overlapping11-halfword parameter blocks. The checker
requires all three phases, stable distinct RAM blocks and agreement with the
decoded pair rule, independently of the physical call/writer checks.
Other contexts have not been exercised physically; their table branches are
static evidence only. In particular, bit0100 is not assigned a clock rate,
microphone route or gain from this observation.
The context setter is linked to accessory lifecycle callers, not a newly
identified clock-control API; the [accessory context contract](#accessory-to-parameter-context)
owns its caller inventory and observed initialization.

The board evidence provides a narrower shared-part hypothesis: own NPE-3
schematic sheet3 names N240`COBBA_GJP_V4.1`, while the independent NSB-7
manual printed65/PDF79 identifies N250 as part4370643,
`Cobba_gjp v4.1 v257bg64t/8`. Its printed27/PDF41 supplies512kHz/8kHz
PCM timing. But the own NSM-3 manual lists the same part4370643/v4.1 while
documenting1MHz/8kHz, not512kHz/8kHz. These conflicting documented profiles
make part identity alone insufficient; see the
[shared-part timing boundary](hardware_atlas.md#cobba-part-identity-does-not-select-pcm-timing).
The difference may involve operating configuration, revision detail or a
documentation error; no one explanation is established. NPE-3's observed
870b/850a words retain bit0100 absent from NSB-7's860b/840a; its intermediate
firmware selection rule is mapped above, but DSP-side meaning remains open.
No shared PCM profile, codec gain or MCU-to-COBBA register translation is
enabled from this comparison. The normal-call evidence shifts the immediate
target to context selection and DSP-side interpretation of the parameter
block feeding`0x420c0a`, plus independent clock evidence. Do not pursue command80
as a mandatory ordinary-call prerequisite without new evidence connecting it.

```sh
.venv/bin/python tools/noki6210_dsp_control_contract.py \
  roms/noki6210/6210_556c.fls > tmp/npe3-paired-mode-contract.json
make verify-6210-dsp-control-observation RUN_DIR=tmp/npe3-control-fresh
```

`verify-6210-incoming-sms` requires segmented GSM delivery, CP/RP acknowledgments,
EF_SMS delivery/read-status writes and persistent `hello`, plus its graphical
read frame. `verify-6210-outgoing-sms` physically composes `A` for `5551234`,
requires the exact own SMS-SUBMIT and network acceptance/CP/RP/RR closure,
then pins `Message sent`. NPE-3 selects relative TP-VP `ff` (63 weeks), not
the sibling fixture's `a7`; its message reference remains handset-managed.
The composer is the first Messages submenu; menu positions are harness policy.

`verify-6210-security` changes only the external laboratory SIM to a
PIN-enabled profile. Physical `1234` and OK must produce VERIFY CHV1 and
`9000`, then open the reviewed Messages menu. The acquired phone PMM is still
unchanged and does not request a phone-lock code on this boot. This gate
proves SIM PIN interaction, not a recovered phone-lock EEPROM contract.

Delayed-PIN registration requires coherent laboratory topology: acquisition
uses ARFCN 35, whereas the default network configures ARFCNs 1/2. With physical
PIN entry starting at eight seconds, the `57/03050000` background measurement
request receives an HLE `8b` response through the firmware-owned task-14 route
and completion parser. An isolated ARFCN 35/36 configuration reports the
receivable serving cell at -60 dBm and permits Location Updating after CHV1
acceptance, including persisted EF_LOCI LAI and status. The exact observed
candidate/release channel header is `12 02`, rather than the no-PIN fixture's
`00 00`. `verify-6210-slow-pin-registration` requires the ordered background
request, serving-cell measurement, enabled task-14 route/completion, physical
PIN acceptance, full registration sequence and persistent EF_LOCI. The ordinary
security gate remains PIN/menu-only; no-PIN registration is checked separately.

`verify-6210-pin-host-incoming-call` composes the PIN-enabled card and coherent
carrier topology with host-originated paging, physical Answer/End, the own
NPE-3 CC/RR grammar, reviewed caller pixels and exact registered-idle recovery.
It requires PIN acceptance and location persistence before validating the call.
This proves host call signaling, not speech or native DSP execution.

`verify-6210-pin-host-incoming-sms` independently composes the same physical
authentication and registration with host SMS delivery, CP/RP acknowledgments,
physical Read, the existing exact message-body image and persistent read-status
SIM record. The host checker is explicitly bound to ARFCN 35; the original
no-PIN host SMS fixture remains on its default topology.

`verify-6210-pin-host-outgoing-call` verifies physical `1234567`/Send/End,
the host connect decision and complete own CC/RR lifecycle after PIN-enabled
registration. `verify-6210-pin-host-outgoing-sms` verifies physical `A` to
`5551234`, the host submit decision, CP/RP completion and reviewed Message sent
text. Both retain the unchanged acquired PMM and explicit HLE boundary.

`verify-6210-pin-host-rejected-sms` and `verify-6210-pin-host-silent-sms`
independently verify authenticated host RP error and full firmware-timeout
recovery on the coherent ARFCN35/36 network. They require physical submission,
one successful PIN VERIFY, own registration/EF_LOCI, correlated host outcome,
the respective protocol closure, reviewed failure-screen pixels and physical
End/Menu recovery. Fresh evidence is `run_6210_pin_host_rejected_sms_01`
and `run_6210_pin_host_silent_sms_01`; neither requires timing overrides or
firmware-state writes.

`verify-6210-pin-phonebook` saves `A / 123` through physical keys, starts a
new MAME process with the saved card and identical laboratory topology, enters
PIN again, and verifies exact contact readback. It checks retained-location
registration, one successful VERIFY per process, enabled CHV1/PIN/retry state
and byte-identical card storage across readback. No session authorization is
transferred between processes.

The `verify-6210-pin-state-idle`, `-call` and `-sms` gates save the
authenticated runtime, then require exact CPU/RAM/emulated-time restoration
and a matched protocol replay window. Physical PIN confirmation, successful
VERIFY and completed registration are required in order before saving, not
merely somewhere in the whole run. The call fixture physically releases
the restored call; the SMS fixture physically reads the restored delivery and
checks the exact message image and persistent read-status record. Each run
retains enabled CHV1, PIN `1234` and a single successful startup VERIFY.

All service runners start with new working directories; incoming network
events are selected through MAME configuration, not firmware injection.
No donor PMM, forced phone state or borrowed DSP verdict is used. The staged
verifier's declared PROM/COBBA inputs and runtime HLE substitution at missing
mask routine `2c75` remain research assumptions. Native mask execution,
measured DSP self-test values, speech/audio parity and electrical ADC units
remain separate work, not implied by the phone-service gates.

## SIM PIN rejection and recovery

`make verify-6210-pin-retry` uses a fresh PIN-enabled laboratory card, the
unchanged acquired product PMM and the coherent ARFCN35/36 network. Physical
9999/OK at8..12s emits VERIFY CHV1 failure9804 and consumes exactly one retry
(3 to2). The reviewed14s frame says Code error. Physical1234/OK at16..20s
then returns9000, resets retries to3 and permits Location Updating at20.980s.
The39s frame shows registered DCT3 LAB idle; physical Menu40 reaches the
existing exact Messages frame. No driver or device behavior is changed.

`noki6210_security_retry_check.py` requires every press/release, the exact two
VERIFY outcomes and their ordering before registration/Menu, complete card
equality against the original deterministic profile plus the documented LOCI
update, and reviewed prompt/error/idle pixels. The generic acceptance runner
also checks own-upload/HLE provenance, launch health, coherent registration and
the Messages-menu pixels. The fixture is `noki6210_security_retry_input.lua`;
qualified capture: `tmp/npe3-pin-retry-final`.

This establishes same-process wrong-PIN recovery and delayed registration.
PIN exhaustion/PUK recovery has its separate cold-process gate below; native
DSP/radio support remains unproved.

`make verify-6210-pin-retry-cold` separately stops after9999/OK and copies
only that run's own NVRAM/configuration into a new process. The full3570-byte
predecessor card equals the deterministic initial profile except PIN1 retries
3->2; it has never authenticated or registered. The copied input must equal
that predecessor byte-for-byte. A separate physical1234/OK process then
returns9000, resets retries to3, registers on the coherent cell and reaches
the exact Messages-menu frame. Its full card may differ only by the expected
retry reset and documented LOCI update; the predecessor remains unchanged.
Prompt/error frames are checked independently using their RGB hashes.

The cold launch has its own invocation ID, script hash, command, complete
initial-storage hash map and predecessor-card hash in `cold-recovery/launch.json`
and the acceptance report. No CPU/RAM/save-state snapshot crosses processes.
Fixture: `noki6210_security_retry_cold_input.lua`; scenario:
`security-retry-cold --coherent-cell`. This qualifies failed-retry durability
and recovery, not arbitrary restart states.

`make verify-6210-puk-recovery` extends the same own-PMM/coherent-cell setup
through three independently launched processes:

1. Physical9999/OK at8..12,16..20 and24..28s produces9804/retries2,
   9804/retries1 and9840/retries0. The complete blocked card differs from the
   deterministic original only at the PIN1 retry byte. The exact display is
   SIM blocked; no authentication, UNBLOCK or registration is admitted.
2. Cold `cold-recovery` starts from that exact own card/NVRAM. Physical
   PUK12345678/OK, new PIN2468/OK and repeated2468/OK produce one successful
   UNBLOCK CHV1, no VERIFY shortcut, and laboratory registration at33.233s.
   The complete card differs only by the new PIN, restored retries and LOCI.
   Blocked/new-PIN/confirmation/idle and Messages frames are checked.
3. A separate `cold-new-pin` process copies the recovered own persistent
   storage, enters2468/OK physically, VERIFYs once and completes retained-LOCI
   registration. Exact idle/Messages and whole-card equality demonstrate
   that the installed PIN works across restart, without a second UNBLOCK.

Both cold launches record independent invocation IDs, commands, script and
initial-storage hashes, plus the exact predecessor-card hash. Predecessors
are rechecked unchanged. No RAM or save state is copied. Ten reviewed frames
and all physical press/release sequences are acceptance predicates. Scripts
are `noki6210_security_block_input.lua`, `noki6210_puk_input.lua` and
`noki6210_puk_reauth_input.lua`; card/protocol checks live in
`noki6210_puk_check.py`. This proves ordinary PIN1 exhaustion and successful
PUK1 recovery, not permanent blocking, PIN2/FDN or native radio.

`make verify-6210-puk-retry-cold` adds a separate wrong-PUK process between
the blocked predecessor and successful recovery. Its physical99999999 PUK,
new PIN2468 and repeated confirmation return9804, with PIN1 still blocked and
PUK1 retries10->9. The complete card differs from the blocked predecessor at
exactly that retry byte: no new PIN is installed on rejection. Four reviewed
frames cover blocked presentation, new-PIN input/confirmation and return to
SIM blocked. No registration or VERIFY shortcut is accepted.

Only this own failed-PUK NVRAM/configuration is copied into `cold-recovery`.
The correct physical PUK then resets retries, installs2468 and registers;
`cold-new-pin` authenticates the installed PIN in a fourth process and keeps
the complete recovered card unchanged. All predecessor cards are preserved.
The acceptance report links the wrong-PUK launch identity, input hashes and
source-card hash alongside the other three launches. This proves a consumed
PUK retry survives cold restart and rejection is atomic with respect to
credentials. The additional fixture is
`noki6210_puk_wrong_input.lua`; the runner option is `--wrong-puk-first`.

`make verify-6210-puk-permanent` covers permanent PUK1 loss through thirteen
independent processes. Its first process physically exhausts PIN1. Each of
the next ten cold processes inherits the exact previous NVRAM/configuration
and enters one wrong PUK/new-PIN/confirmation sequence. Each card transition
changes exactly the PUK1 retry byte; credentials and unrelated bytes remain
unchanged. Attempts1..9 return9804, while attempt10 returns9840 and displays
the exact SIM card rejected frame.

Cold presentation is different: the twelfth process displays SIM blocked
with the same exhausted card, no credential APDUs, no registration and no
card mutation. A thirteenth process provides an active negative control:
the physical key sequence that normally enters the correct PUK/new PIN now
goes into the numeric dialling UI, not a credential editor. It produces no
VERIFY/UNBLOCK or call/mobility service, and End returns to exact SIM blocked
pixels. The entire zero-PUK card remains unchanged. The two banner strings
must not be used interchangeably as a permanent-state oracle.

The chain records distinct launch IDs, binary/script identities and complete
initial-storage hashes, checks every predecessor unchanged, and requires47
reviewed frame comparisons. No RAM, save state or fabricated retry count is
used. `run_noki6210_puk_exhaustion.py OUTPUT --check-only` revalidates retained
evidence without running MAME; `--resume` validates complete predecessors
before starting a missing continuation and never overwrites a partial run.
The shared command builder preserves the existing 6210 launch arguments.
This qualifies permanent PIN1/PUK1 card rejection and responsive noncredential
input, not PIN2/FDN or native radio/speech.

## SIM PIN2 settings

`make verify-6210-pin2-change` authenticates the PIN-enabled laboratory card,
registers on the coherent cell and physically changes PIN2 from5678 to8765.
The [Nokia-authored user guide, mirrored PDF](https://manuals.plus/m/6bb77733be85dbf473e9cdb264eefd193dbc61e6ef4bd6382a5fef5d30ca7144.pdf)
lists Change of access codes under Security settings. On the tested card,
Fixed dialling is absent, so Access codes is the fifth security item rather
than the guide's sixth. The fixture explicitly scrolls and selects through
Settings, Security settings, Access codes and Change PIN2. These conditional
menu positions belong only to the input fixture, not the driver.

Physical current-PIN2 entry50..54s, new8765 entry58..62s and confirmation66..70s
produce exactly one CHANGE CHV2 (`A0 24 00 02 10`) and9000 at70.034s. Firmware
displays PIN2 code changed. End76/Menu78 recover the reviewed Messages screen.
The complete3570-byte card equals the original defaults plus the documented
location update and new PIN2; PIN1, both PUKs, retry counters and all other
records are unchanged.

`noki6210_pin2_change_check.py` checks exact contacts, CHV2 selection and
protocol order, whole-card equality and six reviewed access-code/current/new/
confirmation/success/Menu frames. This fixture has its own exact Messages
animation-phase hash; older menu oracles are unchanged. The scenario is
`pin2-change --coherent-cell`, with `noki6210_pin2_change_input.lua`.
`make verify-6210-pin2-change-cold` additionally copies only that process's
own NVRAM/config into a separate cold process. Physical current-PIN2 entry
8765 then changes it back to5678 through the same settings route. Each process
checks all six frames, coherent registration and complete expected card bytes;
the cold process retains LOCI. Independent launch IDs, script hashes and full
initial NVRAM hashes identify the continuation, and the predecessor's complete
NVRAM must remain unchanged. The verified capture is
`tmp/npe3-pin2-cold-final` (regenerable local evidence, not a shipped fixture).
No RAM or save state is transferred.

`make verify-6210-pin2-retry-cold` instead enters wrong current PIN2 `9999`
and requests8765. CHANGE CHV2 returns9804 and the phone displays Code error.
The complete card changes only the normal LOCI update and PIN2 retry byte
(3 to2); neither credential nor any PUK counter changes. A separate cold
process receives exactly that storage, authenticates the unchanged5678 and
installs8765 successfully, restoring PIN2 retries to3. Both six-frame sequences,
registration, source preservation and launch provenance are checked. Capture:
`tmp/npe3-pin2-retry-banked`. The status parser explicitly selects CHV2's
counters; default CHV1 consumers retain their previous behavior.
`make verify-6210-pin2-exhaustion` runs three physical wrong-current-PIN2
changes through separate predecessor-linked cold processes. Retry counts
3->2->1->0 produce9804,9804,9840; the final screen is PIN2 code blocked.
Every full card equals the registered laboratory profile except the PIN2 retry
byte. PIN1 still authenticates, coherent registration succeeds and Menu remains
usable. All18 reviewed frames pass, with distinct launch IDs and exact copied
storage/source preservation. Capture: `tmp/npe3-pin2-exhaustion-banked`.
`make verify-6210-puk2-recovery` extends this to five processes. The fourth
cold boot exposes Enter PUK2 code at the same settings item. Physical PUK2
87654321, replacement2468 and confirmation produce UNBLOCK CHV2
(`A0 2C 00 02 10`)/9000 at74.033s, followed by firmware-owned VERIFY CHV2
(`A0 20 00 02 08`)/9000 at74.041s. The reviewed result is Code accepted.
The full card changes only PIN2 to2468 and its retry allowance back to3;
PIN1, both PUKs and all other card records remain intact.

The fifth cold process physically uses2468 to change PIN2 back to5678.
Both continuations retain location and require coherent registration, exact
contacts/protocol ordering, independent launches, copied-storage hashes,
unchanged predecessor NVRAM and six reviewed frames each. The complete chain
checks30 frames. Capture: `tmp/npe3-puk2-recovery-banked`.
No RAM/save transfer or credential-state forcing is used.

`make verify-6210-puk2-retry-cold` inserts a separate cold wrong-PUK2 attempt
before recovery. Physical99999999/new2468/confirmation returns UNBLOCK
CHV2/9804 and Code error; PIN2 remains blocked with its original5678 value,
and the complete card differs only at PUK2 retries10->9. No automatic PIN2
VERIFY follows the rejection. The exact failed-PUK card feeds the next cold
process, which correct-PUK2 recovers and restores both CHV2/PUK2 allowances;
a sixth process then cold-uses2468 and changes back to5678. All36 frames,
full-card transitions, registration, independent launches and unchanged
predecessor storage are checked. Capture: `tmp/npe3-puk2-retry-banked`.
`make verify-6210-puk2-permanent` exhausts PUK2 through14 independent processes:
three physical PIN2 failures, ten cold-linked wrong-PUK2 attempts and a cold
correct-PUK2-pattern control. The first nine UNBLOCK failures return9804 and
Code error; the tenth returns9840 and PIN2 code rejected. Complete cards differ
only by the PUK2 retry byte on each failure. PIN1 remains authenticated and
laboratory registration continues throughout.

On the final cold boot, selecting Change PIN2 immediately displays PIN2 code
rejected. The correct-PUK2-shaped physical input reaches ordinary dialing/options
UI rather than a recovery editor; no PIN2 VERIFY/UNBLOCK or call/SMS service
occurs. End/Menu returns to the reviewed Messages frame. The complete zero-PUK2
card is unchanged. All84 frame comparisons and predecessor storage hashes pass.
Capture: `tmp/npe3-puk2-permanent-banked`. `--resume` validates completed stages'
commands, script hashes, complete predecessor NVRAM, distinct launch IDs,
protocol/card transitions and frames before continuing missing stages; partial
or changed evidence is rejected.

## SIM Fixed Dialling

`make verify-6210-fdn-cycle` uses an explicit extended laboratory card with
EF_FDN present, ten empty32-byte records and normal PIN1/PIN2 credentials.
The acquired phone PMM is unchanged. The firmware reads all ten records and
exposes Fixed dialling under Security settings; this extra card-dependent menu
item is fixture knowledge, not driver behavior.

Four own-storage processes prove On, cold On, Off and cold Off. Physical On
with PIN2 verifies CHV2, invalidates EF_ADN and rehabilitates EF_LOCI/EF_IMSI;
the screen confirms Fixed dialling on. Cold On retains invalid ADN while
rehabilitating identity files, without another PIN2 verification. Physical Off
verifies PIN2 and rehabilitates ADN, displaying Fixed dialling off. Cold Off
has no validity transaction. Each complete3898-byte card matches the registered
profile with only the expected ADN-validity flag changed; credentials, retry
counts and all records are preserved.

The gate checks exact contacts, APDU selectors/order/results, record census,
coherent registration,12 reviewed frame comparisons, independent launch IDs,
copied-storage provenance and unchanged predecessors. Capture:
`tmp/npe3-fdn-cycle-banked`. No RAM/save state or forced handset event is used.
`make verify-6210-fdn-retry-cold` first enters wrong PIN2 `5675`. VERIFY CHV2
returns9804 with Code error and no validity-changing APDU. The complete card
retains valid ADN and unchanged credentials/records; only PIN2 retries3->2
and the normal location update differ from the fresh input. The exact retained
card feeds a cold correct-PIN2 activation, which restores retries to3 and
invalidates ADN. Cold On, physical Off and cold Off then pass as above.
All five processes,16 reviewed frames, exact card/protocol transitions,
registration, launch identities and unchanged predecessors are checked.
Capture: `tmp/npe3-fdn-retry-banked`.

`make verify-6210-fdn-records` freshly activates FDN, cold-starts into Number
list/Add and physically saves `A / 12345` after PIN2 authentication. This
6210 editor requests number before name. Firmware sends exactly one successful
UPDATE RECORD to EF_FDN record1 (`A0 DC 01 04 20`), then rereads record1;
the complete card differs only in that32-byte record. A third cold process
reads all ten EF_FDN records,
retrieves the saved entry through Search/Details and renders A/12345 without
another PIN2 verification or any record write. The whole saved card remains
unchanged. Registration, exact contacts/APDU order/results,12 reviewed frames,
independent launches and unchanged predecessor NVRAM pass. Capture:
`tmp/npe3-fdn-records-qualified`.

`make verify-6210-fdn-record-edit` extends that chain with protected Edit and
another cold Details read. Unlike Add, Edit asks for the existing name before
the number. Physical input retains A, removes the last digit of12345 and saves
12346 after PIN2 authentication. The sole UPDATE RECORD and confirming reread
target EF_FDN record1; whole-card equality permits only the final digit's BCD
nibble to change. Cold Details renders A/12346 without writing or verifying
PIN2. All five processes and20 reviewed frame comparisons pass, with the same
registration and predecessor-preservation checks. Capture:
`tmp/npe3-fdn-edit-banked`.

`make verify-6210-fdn-record-erase` separately follows the saved entry through
PIN2-protected Erase cancellation, confirmed Erase and cold empty-list display.
The prompt is Erase? A. Physical Back preserves the entire card with no UPDATE;
physical OK sends one successful UPDATE RECORD replacing only record 1 with
32 `FF` bytes and displays Erased. This path does not issue the confirming
record reread used by Save/Edit. The final cold process reads all ten empty
FDN records, displays Fixed dialling list empty, performs no PIN2 verification
or record write, and preserves the complete card. All six own-storage processes,
22 reviewed frames, protocol/card transitions, registration and unchanged
predecessors pass. Capture: `tmp/npe3-fdn-erase-banked`.

`make verify-6210-fdn-record-retry` denies Number list/Add with wrong PIN2
5675. VERIFY CHV2 returns9804 and Code error; no UPDATE is attempted. The entire
FDN-On card remains unchanged except PIN2 retries3->2. A separate cold process
must observe that exact two-attempt predecessor, authenticate5678, restore the
allowance and save A/12345. Another cold process renders the saved number with
the full card unchanged. All four processes,15 reviewed frames, complete-card
checks, APDU order/results, registration and independent preserved predecessors
pass. Capture: `tmp/npe3-fdn-record-retry-banked`. This is Add authorization
denial, not a SIM UPDATE command being rejected after submission.

`make verify-6210-fdn-dial-controls` freshly prepares that physical A/12345
record, then tests three cold dial controls. With FDN On, physical12345/Send
completes the existing 6210 laboratory outgoing signaling lifecycle through
Connect/Connect Acknowledge and physical End/release. Physical12346/Send from
an independent copy of the same saved state displays Call not allowed, then
returns to the dialed number; no CM Service Request, SETUP, traffic assignment
or call-control message is emitted. Physical PIN2-authenticated Off preserves
the record while rehabilitating ADN. The same12346 then completes and releases
the laboratory call lifecycle.

All seven processes,23 reviewed frames, decoded physical contacts, exact called
digits, whole-card preservation, identity/ADN validity transactions, registration,
independent launches and unchanged predecessors pass. Capture:
`tmp/npe3-fdn-dial-banked`. This proves these exact-number permission controls
and connected HLE laboratory signaling, not native DSP/RF, speech or an external
end-to-end call. Emergency exceptions, prefix matching and special dial strings
are outside this gate's scope.

## SIM Toolkit

`make verify-6210-sim-toolkit RUN_DIR=run_6210_toolkit` uses unchanged
acquired phone PMM and a Phase-2+ laboratory SIM selected through SATCFG.
The own firmware reads EF_PHASE `03`, sends a nine-byte TERMINAL PROFILE,
receives normal completion, then discovers a card-owned proactive command
through STATUS `91 16`. It performs FETCH `A0 12 ... 16`, renders `DCT3 SAT`,
and after physical OK sends the exact successful DISPLAY TEXT TERMINAL
RESPONSE. The gate requires ordered APDUs, both reviewed 96x60 frames,
registered-idle recovery and persisted EF_LOCI. It does not inject any latch,
message, firmware state or proactive object into the handset.

`verify-6210-sim-toolkit-interactive` independently extends the own profile
to GET INKEY and GET INPUT. Physical `5` and `42` return exact successful
terminal-response text TLVs `0d020435` and `0d03043432`. Ordered FETCH,
TERMINAL RESPONSE and pending-status lengths are shared protocol checks;
five reviewed NPE-3 frames and its own registration/EF_LOCI remain separate.
The final idle hash is product-local, not inherited from 6250.

`verify-6210-sim-toolkit-menu` additionally accepts card-owned SET UP MENU:
the preceding GET INPUT response returns `9128`, FETCH length `28`, and the
handset returns successful command-4 response `810304250002028281030100`.
Physical Menu/Scroll Up opens `DCT3 menu`; selecting Continue sends menu-item
1 ENVELOPE `d30702020181100101` through `A0 C2` and receives `9000`.
Physical End returns to the reviewed registered idle. Four extra own frames
cover the menu entry, items, selection result and idle, independently of the
existing five interactive frames. No callbacks or menu objects are injected.
The second item's behavior remains unvalidated.

`verify-6210-sim-toolkit-sms` independently uses laboratory card profile 5.
Physical Continue selection returns `9124`; firmware FETCHes the card's SEND
SHORT MESSAGE and sends exactly one SMS-SUBMIT to `5551234` with 8-bit text
`SAT`. Own observed CP identifier `39` and TP message reference `01` are
checked explicitly. Network CP/RP success precedes successful card response
`810305130002028281030100`, CP acknowledgement and RR channel release.
The physical End returns to reviewed idle only after the network wait.
All earlier interactive/menu frames, the completion frame, physical sequence,
own upload/registration and persisted location are checked independently.
This is laboratory research-HLE signaling, not external SMS delivery or
native-DSP execution.

`verify-6210-sim-toolkit-call` independently uses card profile 6: physical
menu selection yields `911c` and FETCH length `1c`. The firmware presents
`SAT call`; physical Call consent precedes the sole outgoing request to
`5551234`. The gate requires SETUP, proceeding, assignment, CONNECT/ACK,
successful card response `810306100002028281030100`, physical End,
DISCONNECT/RELEASE/RELEASE COMPLETE and RR release. Own prompt, connected
call and idle frames are checked alongside the earlier interactive/menu
frames and unchanged-PMM upload/registration/storage checks. This proves
laboratory signaling and consent, not native speech or an external call.

`verify-6210-sim-toolkit-call-decline` uses the same card request but physically
presses Cancel. The successful APDU carries terminal result `22` (user
rejection), not call success. The checker forbids an outgoing request,
SETUP, traffic assignment or CONNECT anywhere in the run. Reviewed prompt,
recovered menu and post-End idle frames establish physical recovery;
own upload/registration/storage checks remain independent.

This establishes these command lifecycles, not all Toolkit commands or native
DSP execution. The completion trace records the status bytes actually appended
to data-bearing card replies; ordinary `9000` and proactive `9116` responses
use the same observation-only path.

## Host incoming call

`tools/run_noki6210_acceptance.py RUN --scenario host-incoming-call` enables
only `CALLHOST` in private configuration, leaving acquired PMM unchanged.
An idle frame at 32 seconds releases a host request for caller `5551234`;
the runner requires correlated queued/paging/alerting/connected/ended phases.
Physical Send and End must satisfy NPE-3's own Call Confirmed and 24-byte
traffic/release contracts. Reviewed caller text and exact registered idle
before/after the call are required. `--port` selects the unused HTTP port
(default 16210). This is HLE signaling acceptance, not speech or native DSP.

## SIP caller cancellation

Pending outgoing restoration is also covered by
`verify-6210-sip-pending-outgoing-restore`: physical `1234567`/Send, SIP 180,
exact architectural save/load, external CANCEL/487, a single cause-41
termination, complete CC/RR release and exact registered idle. The host
dialog is cleared rather than saved. No CONNECT, media or redial is allowed;
this does not establish answered calls or native speech.

`verify-6210-sip-alerting-incoming-restore` saves during an organically
ringing incoming call and restores exact PC, SP, RAM checksum and time.
The outstanding SIP INVITE is rejected rather than replayed; one cause-41
clear completes CC/RR release under the new epoch. Physical Exit dismisses
the single missed-call notification and returns to exact registered idle.
Network clearing must complete **before** physical Exit; the key dismisses
the notification, not the call. A fresh run against the shared Disconnect
scheduler correction accepts termination at40.090s and reaches ended40.110s,
before the fixture's later Exit. Missing Exit evidence or clearing after it
is rejected. The independent3310 SIP CANCEL gate also retains ordinary
unanswered clearing under the same radio scheduler.
Answer, CONNECT and accepted media are forbidden in this gate; connected-call
SIP restoration has separate gates below, and speech remains unproved.

`make verify-6210-sip-cancel RUN_DIR=run_6210_sip_cancel` runs the unchanged
acquired PMM in `npe3hle`, with fresh private NVRAM/configuration and the
optional PJSIP 2.16 stack. A reviewed registered-idle snapshot at 32 seconds
releases the real SIP INVITE. The endpoint cancels only after the handset
reaches alerting; the checker requires CANCEL/487, exactly one accepted
termination, CC release, LAPDm channel release and the ended phase. Answer,
CONNECT and all bridge media counters are rejected.

The firmware renders `1 missed call`. A physical right-softkey Exit then
returns to the exact registered-idle frame. Product-owned bootstrap,
registration and persisted EF_LOCI are checked as well as SIP signaling.
The runner refuses an existing run directory or stale readiness artifact.
The generic SIP runner permits this unanswered scenario and the outgoing
busy/unavailable fixtures below; answered SIP, speech and native DSP are not promoted.

`make verify-6210-sip-idle-restore RUN_DIR=run_6210_sip_idle_restore` first
performs the existing exact idle architecture/time and protocol-replay check.
The fresh incoming call must use host epoch 2 and start after restoration;
the same missed-call frame, physical Exit and registered-idle recovery are
required. The save contains no SIP dialog and no media is admitted.

## Host outgoing call

`host-outgoing-call` runs the ordinary physical `1234567` dial/Send/End
fixture with only `CALLHOST` enabled. The external runner verifies those exact
digits and submits a correlated connect decision; wrong-ID and duplicate
decisions must be rejected. The own product checker requires traffic assignment,
physical clearing and registered paging. An independent host checker requires
one queued/accepted connect and connected/ended phases. The shared queue log
is not itself evidence of fallback: host decisions enter that same saved queue.
This verifies outgoing host signaling, not audio or a SIP call.

`make verify-6210-sip-outgoing-busy RUN_DIR=run_6210_sip_outgoing_busy`
independently verifies physical `1234567`/Send against a real local PJSIP
486 response. The checker decodes the handset's SETUP digits, requires the
correlated busy decision and complete CC/RR release, and rejects CONNECT
and bridge media. The final frame must match registered idle exactly.
The runner retains unchanged acquired PMM, private fresh storage, own upload
and self-test checks, registration and persisted EF_LOCI checks. This is
research-HLE failure signaling and recovery, not answered-call or speech
acceptance.

`make verify-6210-sip-outgoing-unavailable RUN_DIR=run_6210_sip_outgoing_unavailable`
uses the same physical dial and isolated own-PMM preparation against real
PJSIP 480. The firmware consumes the no-answer termination with cause 18,
completes CC/RR release and returns to the exact registered-idle frame.
CONNECT and bridge media are rejected, as in the busy fixture.

## Host USSD

`make verify-6210-host-ussd-notify` cold-authenticates the ordinary PIN-enabled
laboratory card, registers on the coherent ARFCN35 cell and admits one external
notification after the fixture's idle-ready marker at20 seconds. The shared
WebSocket runner sends packed GSM7 `Host notice`, not a firmware message or
handset-memory edit. Own firmware consumes the network REGISTER, returns the
correlated result and releases RR before host state becomes delivered.

The display automatically opens Service reply / Host notice. Physical Back
returns to registered idle; Menu opens the reviewed Messages frame. All three
96x60 frames, exact contacts, host identity/epoch/phases, notification/result/
release ordering and complete expected card bytes pass. The only SIM changes
from fresh input are the ordinary registration/location update. Capture:
`tmp/npe3-host-ussd-notify-banked`. The event filter retains adapter lifecycle
lines explicitly; the existing PIN-authenticated host incoming-SMS gate also
passes after that tooling change.

`make verify-6210-host-ussd-request` separately admits network request
`Choose 2` after the same physical authentication and idle-ready boundary.
Answer opens the numeric Message editor directly. Physical `2` and Send return
the exact 21-byte USSD ReturnResult with DCS `0f` and packed payload `32`; firmware
releases RR before the adapter publishes that reply and state answered.
The external WebSocket runner independently records exactly one received
`incoming_ussd_response` with epoch 1/request 1, integer DCS 15, data `32` and
`empty=false`. Adapter logs alone are insufficient for this host-delivery check.

Six reviewed frames pin the prompt, blank editor, entered2, Done, registered
idle and Messages. Exact contacts, single admission/request, response and
release ordering, host JSON types/identity/data and complete expected card
bytes pass. Capture: `tmp/npe3-host-ussd-request-banked`. The ordinary
notification gate independently passes after the shared fixture/checker changes.
`make verify-6210-host-ussd-restore` saves the unanswered blank editor at28s,
loads it after one second and checks exact emulated time, all captured ARM
registers and the complete backed-RAM checksum. The original single request
is republished as queued under host epoch2; physical2/Send then returns the
same exact payload once under epoch2. No second admission or network REGISTER
is permitted. RR release, host-received JSON, all six frame hashes and the
complete registered card match the ordinary request contract. The save file
must exist. Capture: `tmp/npe3-host-ussd-restore-banked`.

These are HLE supplementary-service contracts. Empty reply, arbitrary multi-turn
dialogue, native DSP and speech remain unqualified. This pending-editor test
does not establish external network process restoration or arbitrary snapshot
timing; the adapter deliberately gives the restored handset a new epoch.

Dismissal is not an empty-reply proof on this product. In bounded 42-second
captures of the same host `Choose 2` request, Answer26 followed by End30
returns to idle without a supplementary-service component. Separately,
Answer26/Back30 returns from the blank editor to the prompt, and Back34 returns
to idle. Neither run sends a ReturnResult or RELEASE COMPLETE, releases the
dedicated channel or advances host request1 beyond queued through the observed
end. The physical Menu remains responsive and card contents remain normal.
Captures: `tmp/npe3-host-ussd-empty-probe` and
`tmp/npe3-host-ussd-empty-back-probe`. This disproves immediate empty-reply
equivalence with NSB-7; it does not prove permanent deadlock or classify a later
firmware/network timeout. The selected transaction's legitimate termination
after UI dismissal remains open. No empty reply or host answered state is
synthesized. The unsuccessful fixture/scenario is not retained as acceptance.

A fresh own-PMM/PIN-enabled capture extends Answer26/End30 observation to
180s (`tmp/npe3-ussd-late-dismiss-180`). The host records only queued for
request1; the session trace has the request at20.949387s and no handset SS
response or subsequent release. Physical Menu at170s renders Messages, and
the Lua observation completes at175s. This is bounded trace evidence, not an
acceptance gate: the exploratory host observer reports a WebSocket-close race
at normal MAME shutdown. It does not establish permanent deadlock.

The current model supplies no active network-USSD response timer: the adapter's
deadline applies only while the radio request is queued, and
`awaiting_network_ussd_response` handles a correlated FACILITY result or handset
RELEASE COMPLETE without a timer. Thus silence cannot exercise a modeled
network-originated termination. Explicit network termination is now exposed
separately through the correlated host command `incoming_ussd_terminate`; no
automatic timeout is inferred.

`.venv/bin/python tools/run_noki6210_ussd_termination.py RUN_DIR` qualifies
that separate application action on fresh own-PMM/PIN-enabled storage.
Answer26/End30 dismisses the editor; after40s the host sends a stale-epoch
termination, a wrong-request termination, the valid identity and a duplicate.
Only the valid command is accepted. SS RELEASE COMPLETE (`0b2a`) goes through
the existing downlink path, followed by RR CHANNEL RELEASE; only after ordinary
release does host request1 become cancelled, without an incoming-USSD response.
Menu at56s renders the existing byte-exact Messages oracle. Capture:
`tmp/npe3-ussd-termination-exact`. The fixture checks ordered PIN acceptance,
registration, admission and protocol/host release, every expected contact
release and complete registered SIM contents. It does not qualify arbitrary
termination/save-state timing. Console checks reject MAME driver-validity and
script/execution failures as well as the protocol checker rejecting missing
release or fabricated replies.

The same runner's `--restore` variant saves the dismissed but unanswered
transaction at34s and reloads after one emulated second. The pre-save and
post-load time, all37 ARM/banked registers and backed-RAM checksum must match.
Raw snapshot records are independently checked by the shared
`arm_architecture_snapshot_check.py`, including PC/SP register consistency,
exact event order and matching roundtrip-marker times. Authentication and
request/editor dismissal must precede the save boundary. Host request1
is republished queued under epoch2 without a second admission. An epoch1
termination is rejected; epoch2/request1 sends the same SS/RR release and
publishes cancellation once without a reply. Wrong identity and duplicate
controls remain active, Menu56 still renders Messages, and the checkpoint
artifact must exist; exact Messages recovery and complete SIM equality remain
mandatory. Capture: `tmp/npe3-ussd-termination-exact-restore`. This covers
one unanswered post-dismissal checkpoint, not termination in flight or external
network process restoration.

### Continued laboratory USSD

`make verify-6210-ussd-continued` separately qualifies one continued exchange,
not repeated independent host admissions. Following physical PIN1 authentication
and laboratory registration, physical `*123#`/Send starts invoke1. The existing
network conformance policy (`USSDCFG=4`) sends a correlated continued FACILITY;
Answer opens the numeric editor, physical2/Send returns invoke2 with DCS0f and
payload32. The peer parses that response and returns its final37-byte RELEASE
COMPLETE, followed by ordinary RR release. Physical End/Menu recover the same
registered idle and Messages frames. The six exact images cover Requesting,
blank editor, entered2, Nokia test network result, idle and Messages. Complete
card contents match normal registration. Capture: `tmp/npe3-ussd-continued-banked`.
This is one firmware-owned continued reply using the declared HLE network
policy; arbitrary dialogue length, host-controlled continuation and native
speech are not established.

## Host SMS acceptance

Named gates use the same isolated own-PMM runners and require new run
directories. Execute them sequentially:

```sh
make verify-6210-host-incoming-sms RUN_DIR=run_6210_host_sms_incoming
make verify-6210-host-outgoing-sms RUN_DIR=run_6210_host_sms_outgoing
make verify-6210-host-rejected-sms RUN_DIR=run_6210_host_sms_rejected
make verify-6210-host-silent-sms RUN_DIR=run_6210_host_sms_silent
make verify-6210-host-silent-sms-realtime RUN_DIR=run_6210_host_sms_silent_realtime
```

The real-time silent-SMS gate uses `--throttle`, retaining the same own-PMM,
protocol, failure-pixel and physical End/Menu recovery checks as the fast
fixture. Its accepted host decision at 34.09 seconds completes at 101.11
seconds, within the host runner's explicit 180-second total wall-clock
completion budget. This budget is not an emulated firmware timer; unrelated
notifications do not extend it. The generic runner accepts a finite positive
`--completion-timeout` override for slower host environments.

The isolated runner's `host-incoming-sms` scenario enables only `CALLHOST`,
waits for current-epoch adapter readiness and laboratory registration, and
delivers host `hello` from `5551234`. Independent host correlation and NPE-3
handset checks require CP/RP/RR closure, exactly one page, delivery/read-status
writes, persistent read content and the reviewed full message screen.

`host-outgoing-sms` uses the ordinary physical composer to submit `A` to
`5551234`. The host runner requires exactly GSM7 `41`/one septet, rejects an
unrelated request ID and rejects a duplicate success decision. The handset
must independently satisfy its own relative-validity `ff` SMS-SUBMIT grammar,
channel release, paging recovery and reviewed Message sent text (excluding
the animated envelope, unlike the unchanged ordinary full-frame gate). No automatic
SMS network fixture is enabled in either host scenario. The unchanged acquired
PMM and research HLE/native distinction apply; these do not prove native DSP.

`host-rejected-sms` uses the same physical `A` submission but requests an
explicit RP error. The own NPE-3 checker requires CP acknowledgment, RP error,
handset CP acknowledgment, release and resumed paging, forbidding a success
RP-ACK. Correlated error acceptance, wrong-ID/duplicate rejection, physical
End/End/Menu decoding and reviewed `Message not sent this time`/Messages
frames are also required. The independently reviewed 96x60 failure/menu text
regions share the 6250 frame checker; packet and input contracts remain NPE-3.
`host-silent-sms` separately supplies CP-ACK without an RP result. Firmware
paints `Message sending failed`, issues main-link DISC, receives UA and
deconfigures traffic before closing the host request (observed near 101.1
seconds). The checker requires no RP error/success, resumed paging and
physical End/End/Menu recovery. Its shared release grammar receives the
independently decoded NPE-3 `ff` validity value, not the sibling `a7` value.
The 135-second fixture does not shorten firmware timers.

## Save-state acceptance

`verify-6210-state-idle`, `verify-6210-state-call` and
`verify-6210-state-sms` save and restore emulated time, all 37 exported ARM
registers (including CPSR and banked state), and a digest of the complete
handset RAM. The shared read-only `arm_architecture_snapshot.lua` supplies
the same register order and RAM interval as the 6210/8890 alarm fixtures.
The checker rejects any register mismatch or incomplete snapshot.
Each requires a nonempty, identical
ordered protocol replay interval after restoration. Location Updating Accept
and its subsequent Channel Release must occur before the saved snapshot;
post-load registration cannot satisfy this prerequisite. The call fixture then
presses physical End and verifies release and resumed paging; the SMS fixture
opens the delivered message and verifies its persistent read status and frame.
These remain signaling/storage checks, not speech validation.

Each state scenario requires a distinct evidence directory; the runner refuses
an existing directory rather than overwriting a previous run. Per-run
`acceptance.json` records the launch directory/identity, primary input hashes
and initial persistent-storage inventory. The independent accessory cold-repeat
gate requires separate launches as well as matching protocol, CCONT/flash/SIM
storage and idle/Menu pixels; its provenance requirements are specified below.
The acceptance covers
exact saved architectural state/time, selected protocol replay and the
scenario-specific physical-input continuation; it does not establish
cross-host determinism, connected SIP restoration or native speech.
Use the project's Capstone-enabled Python environment. For Make invocations,
pass an absolute `PYTHON` path: the nested MAME build changes working
directory. These gates select the research-HLE composition, not normal
NPE-3 native verification.

MAME cancels pending Lua waits when loading a state. The host fixture therefore
explicitly resumes its physical input schedule after the replay interval;
it does not restore or inject handset state. ARM7's named `PC` is a debugger
cache: the snapshot reads the saved architectural `R15` instead.

## Physical power lifecycle

`verify-6210-power-cycle` starts a fresh private `npe3hle` process with its
unchanged acquired PMM. Registered idle at 20 s is followed by physical
Power at 25 s, held for four seconds. Firmware commands rail-off at about
31.68 s; the display is blank. At least fifty seconds of continuous CCONT
RTC ticks are required with no DSP RX publication, peer shared-RAM writes,
FIQ0 notifications, native port activity or radio LAPDm output. This exceeds
the last 49-second watchdog reload and prevents an old counter from silently
rebooting the supposedly off domain. RTC progression is anchored to the last
pre-shutdown tick, including minute rollover, not an assumed clock origin.
The same continuity is required for three ticks after wake. The independent
address07 acquisition-control initialization must not reset RTC seconds.

A separate physical Power press at 90 s restores the digital domain through
PWRONX cause `02`, not a charger event. Firmware consumes ready/PWRONX
status low bits `03` (observed full status `33` with retained RTC sources),
executes its own native verifier/loaders before the explicit
missing-mask HLE handoff, and completes its own compact self-test again.
Both boot stages are checked independently; passive debugger log caps are
re-armed without changing firmware registers or RAM.

The warm Location Updating request carries retained laboratory LAI:
`0080013f4905087200f110000133080910101032547698`. Organic EF_LOCI reading
must precede it. Unlike the NSB-6 warm lifecycle, NPE-3 rewrites the LAI
after accept but does not redundantly write the already-valid location
status byte. The cold registration grammar remains unchanged. Exact
registered-idle frames before and after restart and physical Menu decoding
as `19` into the reviewed Messages frame prove UI recovery without security
code injection or manual phone-lock entry.

NPE-3 firmware explicitly writes RTC seconds `07=00` after PWRONX wake.
Retained device counters while off do not imply untouched software time,
cold-process calendar persistence, measured wake latency, complete
peripheral power gating or native speech.

## Physical Calendar entry

`tools/noki6210_calendar_input.lua` navigates the own research-HLE handset
through physical Menu, seven Down presses and Select. Fresh storage produces
the Time prompt rather than Calendar immediately. Physical `13:47` and OK
write CCONT's shared latch registers `0b=2f`, `0c=8d`, then expose the Date
prompt. Physical `07-10-2026` and OK render `7 October 2026`, `Wednesday`
and the ordinary Options/Back Calendar window. No calendar RAM, internal UI
events, clock registers or foreign provisioning are written by the fixture.

`make verify-6210-calendar-cold RUN_DIR=run_6210_calendar JOBS=4` runs the
entry fixture, copies only that process's own NVRAM into a separate cold
process, and executes `noki6210_calendar_cold_input.lua`. The second fixture
only navigates to Calendar; it never enters replacement time/date. Both
processes must pass the own upload/self-test and registration checks; the
cold process must consume retained EF_LOCI. Both retain CCONT hour/minute
`13:47` and render the reviewed Calendar hash
`fcf0327cc0cc4edb952d0dc37621e467c0810b867738de6c91f39354a9c3a3b3`.
The earlier seconds-reset interpretation of address07 was invalid: its
write side is acquisition control, independent of the RTC seconds read.
Retained minute/hour and calendar claims do not establish elapsed
offline time or boundary-date arithmetic. Eight checker tests cover
missing/reordered inputs, missing time-register programming, cold replacement,
bad RTC data and midnight/day-consumption ordering.

`make verify-6210-calendar-midnight RUN_DIR=run_6210_midnight JOBS=4` enters
`23:59` and the same date physically, then waits for ordinary emulated time.
CCONT reaches `00:00:00`, day counter 1. Physical Back triggers firmware
reads of day register `0a=01` followed by its rebase to zero; Select then
reopens Calendar with `8 October 2026`, `Thursday`, hash
`a04551d523cb8e0adf4efe4da7e6bdcc6ec683dc264a8e791aed57ae798d4d01`.
A separate retained cold process reproduces the next-day pixels and
registered SIM location without new date entry. The already-open Calendar
remains on its selected date until navigation; this gate proves the
leave/reopen path, not automatic redraw of that window. No host-clock
advance, RTC register fixture or Gregorian date state is injected.

Three date-only variants reuse that same physical clock/day-consumption/cold
runner and the product's own flash journal:

| Gate | Physical date at 23:59 | Reopened and cold-retained Calendar |
| --- | --- | --- |
| `verify-6210-calendar-leap-day` | 28 February 2024, Wednesday | 29 February 2024, Thursday |
| `verify-6210-calendar-nonleap` | 28 February 2023, Tuesday | 1 March 2023, Wednesday |
| `verify-6210-calendar-year-end` | 31 December 2026, Thursday | 1 January 2027, Friday |

Each checks independently reviewed pixels both before and after midnight,
hardware day increment/firmware rebase, retained RTC hour/minute, own staged
verification and fresh/retained-location registration. The original and
next-day hashes are profile data in `run_noki6210_calendar.py`, not borrowed
from another handset. Non-leap February additionally passes in
`run_6210_calendar_nonleap_acceptance` after independent review of both date
frames. These cases do not establish century rules, every month boundary,
offline elapsed time, alarms or native speech.

## Physical alarm acceptance

`noki6210_alarm_input.lua` physically follows Settings -> Alarm clock
(Menu 4-1 in the [Nokia-authored user guide](https://manuals.plus/m/6bb77733be85dbf473e9cdb264eefd193dbc61e6ef4bd6382a5fef5d30ca7144.pdf)).
On retained storage created by this product's physical Calendar entry at
13:47, keypad `13:48` and OK program CCONT registers `0b/0c` to `30/0d`.
Natural RTC advance to 13:48 produces the firmware's `Alarm! 13:48` screen
with Stop/Snooze softkeys and PUP buzzer programming. Physical Stop returns
to `DCT3 LAB` idle and disables the buzzer. Own staged/self-test and
retained-location registration checks pass independently.

`verify-6210-alarm` creates that clock through a fresh independently checked
Calendar fixture, then runs alarm entry/expiry/Stop in a separate process
retaining only its own generated NVRAM. It requires ordered physical input,
CCONT programming, natural 13:48:00, alarm-cause read/acknowledgement, buzzer
enable/disable, all three reviewed 96x60 frames and retained-location
registration. Negative tests reject missing/reordered evidence, wrong time
or alarm cause, clock replacement and a buzzer left enabled after Stop.
`verify-6210-alarm-cold` adds an independent armed cold boot. Physical input
sets 13:48, then exits before expiry; a new process retains only that run's
NVRAM and supplies no clock/alarm entry. Startup reads CCONT `0b/0c=30/0d`,
the first natural tick is 13:47:01, and expiry at 13:48 produces the same
reviewed alarm frame. Physical Stop restores the reviewed operator frame
and disables the buzzer. The alarm capture is at 77 seconds in both gates;
an earlier 70-second capture differs and is not used to re-bank the oracle.
Cold restoration retains hour/minute with seconds starting at zero; no
offline elapsed time is claimed. Negative tests also reject missing retained
register reads, cold observation, clock restoration, or replacement alarm entry.
Neither gate establishes audible output, Snooze, powered-off wake or native
DSP behavior.

### Snooze delivery contract

`tools/run_noki6210_alarm.py --power-off-snooze` validates fresh own-model
physical Calendar provisioning, alarm entry, long power-off, Snooze,
natural five-minute recurrence, Stop and No. The acceptance run
`run_6210_off_snooze_acceptance_20261009` passes two independently checked
alarm-only uploads, both silent off-rail intervals, own RTC
acknowledgement/recurrence and exact reviewed frames. Snooze powers the
phone off; the repeated alarm displays `Alarm! 13:53`, Stop presents
`Switch the phone on?`, and No returns to a blank powered-off display.
No 6250 clock/status/loader or pixel contract is inherited. This is
research-HLE lifecycle acceptance, not native DSP or audible-output proof.

`verify-6210-alarm-off-snooze-yes` independently validates physical Yes
after the recurrence (`run_6210_off_snooze_yes_20261009`). Both alarm-only
upload phases are checked separately from the subsequent firmware-owned
software restart, which must pass runtime/self-test upload validation,
retained-location registration and the exact registered-idle frame. No
extra rail wake or post-decision rail drop is allowed.

`verify-6210-alarm-snooze-countdown-restore` saves at second 100, after
physical Snooze has rearmed the alarm and removed the rails. Exact
37-register ARM/banked state, SRAM digest and emulated time must restore;
the 1.25-second reference and restored windows must reproduce the same RTC
tick and blank frame with no endpoint activity or early wake. The restored
timeline then passes natural recurrence, separately checked alarm uploads,
physical Stop and No (`run_6210_snooze_countdown_restore_20261009`). This
does not substitute the earlier pre-first-alarm countdown checkpoint.

The repeated alarm must finish booting before Stop is pressed. An earlier
fixture pressed Stop before the second RTC acknowledgement: its labelled
action did not dismiss the alarm, so the subsequent right softkey could
not establish No. Physical-action labels alone do not prove UI consumption;
the recurrence and activation-question frames guard this distinction.

`verify-6210-alarm-snooze` uses the same fresh physical Calendar seed and
alarm entry. Right Softkey / C at 77 seconds makes firmware program CCONT
`35/0d` (13:53): this ROM selects a five-minute interval. The gate requires
natural 13:53:00, status `b1`, acknowledgement `a1` (minute plus alarm),
renewed PUP buzzer programming and physical Stop. Exact frames cover the
original alarm, `Snooze active`, repeated `Alarm! 13:53` and final idle.
The repeated capture is at 361 seconds, in its independently observed visible
phase. Audible output and powered-off wake remain outside this acceptance.

Alarm title/time pixels are phase-dependent. The passive matched-phase run
`run_6210_snooze_render` observes a blank text region at 61..73 seconds and
visible `Alarm! 13:48` at 74..76; the repeated alarm shows `Alarm! 13:53`
at 361..373 and blanks at 374..376. Therefore a blank 377-second repeated
capture is not evidence of a missing repaint. No display state or rendering
behavior is changed to obtain the visible capture.

CCONT's held source must survive a MAD2 acknowledgement until firmware
clears it over GENSIO. NPE-3's mask write can expose the minute IRQ before
its register helper updates the RAM mask shadow. Passive reads at
`0x4f2766/0x4f2774` in `run_6210_snooze_reader_valid` establish mask/status
returns `30/31`; at `0x4f277a` the shadow at `0x1742fd` has become `10`, but
the already-sampled mask yields active `01`. With held-input retention,
firmware receives the source again, selects `21`, clears it and lowers the
line. Minute events then drain normally and the second alarm is delivered
(`run_6210_snooze_level`, `run_6210_snooze_acceptance`). The old rising-edge
bridge instead lost the second alarm behind the uncleared minute source
(`run_6210_snooze_probe`). This establishes a source-level integration
defect, not a CPU sleep or serial-latency defect; physical serial timing
remains unmeasured.

### Powered-off alarm boundary

The Nokia-authored user guide specifies alarm operation while switched off
and a Stop/activation choice. `verify-6210-alarm-off-no` and
`verify-6210-alarm-off-yes` physically set 13:48, shut down before expiry,
and verify autonomous RTC wake at the natural deadline. CCONT retains alarm
cause `80` and presents the held source after digital-domain reset. Firmware
acknowledges it, programs the buzzer, paints the reviewed alarm frame and
responds to physical Stop with "Switch the phone on?". No returns to rail-off
with silent powered endpoints and a blank frame. Yes requests an MCU reset
and returns to the exact registered DCT3 LAB idle frame. The gates verify
each own-upload phase independently, including normal self-test/analogue
acceptance after activation. This is research-HLE control/presentation
acceptance, not audible-output or native-DSP acceptance.

`verify-6210-alarm-off-restore` additionally saves the powered-off countdown
at emulated second 45 and compares all 37 exported ARM registers (including
CPSR and banked state), full SRAM checksum and emulated time after restoration.
Reference and restored windows span
45..46.25 seconds, deliberately avoiding an endpoint at the RTC callback's
timestamp. Both contain the same single RTC tick, blank reviewed frames,
no premature wake and no powered DSP/radio activity. Only the restored
timeline continues to the natural 13:48 deadline; held alarm delivery,
buzzer programming and physical Stop/No must still pass the ordinary
powered-off acceptance. The fixture changes emulator save-state only, not
firmware, MMIO, clock or alarm registers. Fresh physical clock-entry and
own provisioning seed the run (`run_6210_alarm_off_restore_quarter`).
`verify-6210-alarm-off-restore-yes` uses the same architecture/RTC replay
contract but physically chooses Yes. It independently verifies the subsequent
software-reset request, restored-countdown alarm boot and normal post-restart
upload/self-test phases, forbids a new rail drop after Yes, and requires the
unchanged exact registered idle frame (`run_6210_alarm_restore_yes`). This
closes both activation choices after a loaded countdown; neither substitutes
for native DSP or audible-output evidence.

Warm MCU reset preserves SRAM. NPE-3 startup reads the reason at `0x17fe48`
before clearing ordinary workspace `0x100020..0x175668`. Physical Yes stores
reason `0c`; it survives reset and is consumed by startup. Clearing all SRAM
in the board reset helper previously destroyed that firmware-owned reason.
Software reset publishes MAD2 status `04`, distinct from cold rail-on `01`:
readers `0x2b7a1a/0x4b0272` match the 3210 readers `0x230608/0x2a922e`.
Bit 1 set selects cold reason `6b`; with bit 1 clear, bit 2 selects the saved
software reason. The second reader separately tests cold-power bit 0 before
software bit 2. Consequently `05` selects conflicting startup lifecycles;
`07` is also wrong, not an internal-reset fix (`BHS` takes carry set).

Acceptance evidence: `run_6210_alarm_yes_rearmed` and
`run_6210_alarm_off_no_warm_regression`. Debugger-only log caps are re-armed
before Yes so the second boot is observed, without changing firmware/MMIO.
Do not substitute a power key, watchdog reset or injected event for alarm
wake, or infer missing firmware work from exhausted trace caps. Exact silicon
reset timing/encoding and off-process elapsed time remain unmeasured.

## Unattached accessory input

The NPE-3 schematic sheets 2/3 connect `HEADDET` to CCONT's EAD pin A2
and the microphone bias network. With no external microphone fitted, that
network provides a pull-up path. The sheet names pins, not ADC selector
numbers; the numbered contract below comes from the product's firmware.

Reader `46b3d6` selects ADC input 0 via `504100`. Consumer
`39d678/39d686/39d692` stores the result and evaluates accessory state byte
`173a97`. Initialization `46b982` writes `0f`. At a constant zero input,
`39ccc8` advances that state to `10`; `39d784` calls parameter-context setter `41a944`
with argument 1, and the idle frame displays `Headset`. A nominal high input
`03ff` leaves the state at `0f` and the reviewed idle frame has no accessory
label. This controlled input/state/frame correspondence supports the
product-local unattached default; it is not measured voltage calibration.

`verify-6210-accessory` requires a real decision at `39d6b2`, high sample
and state `0f`, the reviewed registered unattached idle frame, and physical
Menu decoding with the reviewed Messages frame. It does not inject firmware
state, change the PMM, or synthesize an accessory event. The idle oracle was
deliberately re-banked to remove the false attachment indication; menu and
application oracles remain unchanged. Call/SMS save-load continuation has
independent acceptance under this profile.

Two independently fresh accessory scenarios also reproduce identical selected
protocol traces, CCONT/flash/SIM persisted bytes and registered-idle/Menu pixels.
`make verify-6210-cold-repeat RUN_DIR=RUN` creates both fresh scenarios
sequentially under `RUN/first` and `RUN/second` and requires their comparison.
Compare accepted runs with
`tools/noki6210_cold_repeat_check.py FIRST_RUN SECOND_RUN` using the project
Python environment. The checker rejects a shared directory, unaccepted
scenario, missing/empty artifacts and differing fingerprints; PNG metadata
is excluded by hashing grayscale pixels. This establishes repeatability of
the measured composition on this build, not cross-host determinism or native
DSP behavior. Generate each input with `run_noki6210_acceptance.py --scenario
accessory` in a distinct directory; copying a completed run is not a cold-boot
experiment.

Accepted runs bind their metadata to the canonical launch directory and a fresh
UUID. The cold comparator requires distinct launch identities, identical SHA-256
identities for the executable, MCU, PMM and top-level input script, and empty
initial persistent storage. Copied results are rejected even when their output
bytes match; editing only a copied directory field does not establish another
launch. A changed primary input also prevents a repeatability claim. These
records guard accidental artifact reuse, not cryptographic execution attestation
or a hash closure over every transitive Lua dependency. Older captures without
this metadata cannot satisfy the current cold-repeat gate; generate a fresh pair.

Do not import the selector-7/EAD interpretation from the NSE-8 documentation:
raising NPE-3 selector 7 does not remove `Headset`. The ADC audit observed
179 paired reads through 19 caller addresses over a 24-second old-profile
boot; selectors 1/6 were not observed, not proved unused. Static direct-BL
scanning found 39 ADC-reader candidates and excludes indirect calls. Full
accessory identification, button/hook behavior, electrical thresholds and
cross-product mux identity remain outside this unattached-input contract.

The decision entrance is independently pinned by
`tools/noki6210_accessory_contract.py`, also executed by the accessory
acceptance runner. At state `0f`, a second ADC read must be at most 500
and a third at least 300 to enter branch `39d7d8`, additionally requiring
the tested flag's bit 2 clear. These are distinct reads, not one captured
sample or a proved debounce rule. A separate byte equal to one also selects
that branch. Otherwise state bit 7 selects `39d796`; with that bit clear,
the initial stored sample at `173aa2` is compared against literal `0312`
(786). Values at least 786 call `39cc5c`, whose next signed comparison
uses `0390` (912). The CCONT's 10-bit unsigned sample domain is below
the signed boundary, so these signed comparisons do not invert the ranges.
This explains why a nominal 1023 input exercises the high/cleanup path,
not the 300..500 path. It does not identify every accessory type, recover
electrical units, establish the complete downstream lifecycle, or justify
adding guessed headset/hook inputs. The low-input state-`10` write remains
anchored at `39ccc8`; accessory/context selection remains firmware-owned.

### Accessory-to-parameter context

Setter`41a944` stores its argument in`16ed5c`, calls`41a6b0` and requests
parameter reconstruction through`4202a0`. Its positive argument guard is
`<5`. All8 halfword-aligned direct-BL candidates decode to immediate values
in0..4; indirect calls and other aliases/writers are not closed:

| Context | Adjacent accessory classification / role | Direct call sites |
| --- | --- | --- |
| 0 | `0f` unattached transitions or reset | `39cdba`, `39d58a`, `39dc9e`, `4ba860` |
| 1 | `10` Headset classification | `39d786` |
| 2 | `20`, physical accessory identity unproved | `39db7a` |
| 3 | `80`, physical accessory identity unproved | `39d566` |
| 4 | `84`, physical accessory identity unproved | `39d528` |

These are adjacent lifecycle transitions, not an atomic invariant between
the two bytes. The four reset/unattached sites are not all ADC decisions.
Only known positive direct arguments are covered; the signed guard does not
independently exclude negative inputs from unclosed callers.

Initializer`41ac32` separately stores zero. Extended coherent physical call
`tmp/npe3-context-owner-gate-final` passes with a positive one-byte write
watch: CPU startup clear`200168` and initializer`41ac32` both write zero,
with no setter invocation in that run. Nine command8 observations retain
context0 and source/cache blocks`172d80/172d68`. The tested parameter
publications therefore proceed with initialized context0 and no setter
event; this does not prove native or hardware audio readiness.
Alternate physical accessory transitions and their PCM/DSP meaning
remain unqualified. The two decoded parameter-builder branches select
bits0020/0100 from this context as described above; neither bit is assigned
electrical units, codec routing or a PCM rate.

The static map is verified by`noki6210_accessory_contract.py`; the read-only
setter/write observations are part of`verify-6210-dsp-control-observation`.
No driver, ADC input, PMM, RAM state or service message was changed.

Registration verification permits SIM EF_LOCI writes after Location Updating
Accept either before or after radio release. The network and SIM consumers
are independently scheduled; each ordered chain and final persistent state
is required, not the accidental interleaving of a prior trace.

## Unclocked research-HLE speech boundary

`verify-6210-sip-connected-signaling` independently connects a physically
dialed `1234567` call to a real loopback PJSIP endpoint, then physically ends
it and requires complete CC/RR release and registered-idle recovery. It
preserves own PMM and product configuration. Bridge uplink must remain zero
and no HLE speech tick may execute. This is connected signaling acceptance,
not a speech/media promotion; the product-owned PCM prerequisites below
remain unchanged.

`verify-6210-sip-incoming-connected-signaling` additionally admits a real
INVITE only after the fresh registered-idle capture. The own paging and
assignment lifecycle leads to physical Answer, SIP confirmation, physical
End and complete release, with exact idle recovery. Zero bridge uplink and
no HLE speech ticks remain required; incoming signaling does not establish
PCM timing, audible playback or native DSP speech.

`verify-6210-sip-connected-outgoing-restore` saves a physically dialed,
connected SIP call and checks an exact handset architectural round trip.
The external dialog is cleared with BYE under the new epoch, not restored
or redialed. One cause-41 termination, full CC/RR release and exact idle
recovery are required. Pre-reset bridge uplink must be zero and no HLE codec
tick may run. This gate does not change the PCM or native-speech boundary.

`verify-6210-sip-connected-incoming-restore` separately covers a fresh
incoming dialog answered by physical Send. It saves at 41 seconds after
connection, checks exact CPU/RAM/time restoration, and requires correlated
SIP BYE, one cause-41 clear, full CC/RR release and exact registered idle.
The original identity's Answer and confirmation must precede epoch change.
Zero pre-reset uplink and absence of codec ticks remain mandatory: external
dialog restoration and speech are not established.

`verify-6210-pcm-missing` independently answers and ends an incoming call
using physical keys, unchanged own PMM and the existing NPE-3 upload,
registration and call-signaling checks. Only external network configuration
is added: an independent laboratory 1 kHz voice source. The gate requires
the applied command-8 wire `870b` / speech control `070b`, continued network
downlink exchanges, zero good network-decoded uplink and no HLE codec ticks,
followed by organic release. It does not prove handset playback or speech.

The product's PCM clocks remain unspecified. With a zero frame clock,
the speech timer is never scheduled, so its unsupported-link diagnostic
cannot run either. The gate checks the observed doorbell and absence of
media directly; it does not create a diagnostic clock, attach sibling PCM
timings or treat a missing timer message as a firmware failure. Physical
MIC2/EAR wiring is known, but product-owned clock/framing evidence remains
required before enabling the media path.

The product's physical topology is independently anchored by the acquired
NPE-3/SD4 V19 schematic, version 1.0, sheet 3 (COBBA, 09 February 2001).
N240 is marked `COBBA_GJP_V4.1`; its separate PCM bus has PCMTx at B5,
PCMDClk at C6, PCMSClk at C5 and PCMRx at D5. The net names place PCMDClk,
PCMSClk, PCMTx and PCMRx at PCM(1), PCM(2), PCM(0) and PCM(3), respectively.
This establishes connectivity, not clock direction, divider, sync width,
sample packing or operative mux/gain values. Sheet 3's RF serial interface
is separate from PCM; its RF I/Q lines must not be used as an audio framing
specification.

System Module issue 1 (September 2000), page 19, table 3 specifies the
13 MHz `RFIclk` into COBBA-GJP and the serial RF I/Q interface. That is
the RF/reference clock, not a stated PCMDClk frequency. Pages 8 and 24
establish MAD2-controlled audio and MIC2/EAR connectivity but do not supply
the missing PCM timing. The acquired Troubleshooting issue 1, page 15,
lists analogue audio checks, not PCM timing. The independently inspected
6210 emulator profile supplies no product-owned PCM clock evidence either.
Other products' 1 MHz/8 kHz descriptions are comparisons only, not NPE-3
configuration authority.

The public [NPE-3 service-bulletin archive index](https://files.elektroda.pl/44690,nokia%2B6210-6220.html)
lists 29 NPE-3 PDFs and an index document, mixed with unrelated RH-20
material. Its indexed text has no `PCM` or `COBBA` match, but this is only
a partial text preview, not inspection of the listed PDFs. The linked
[attachment post](https://www.elektroda.pl/rtvforum/topic154278.html)
requires login to download the archive. No archive or member hashes have
been acquired; this remains an unqualified source lead, not evidence that
the bulletins lack PCM timing. Do not use it to justify a sibling clock or
claim the primary-document search is exhaustive.

Reproducible source identities (SHA-256):

- `roms/research/npe3/npe3-schematics-v1.pdf`:
  `559e9718a9dad703694f17d349383f75af4237b1c26cf94ecdb4aafe641f1e43`.
- `roms/research/npe3/npe3-system-module.pdf`:
  `acaf85590cee30a1c8640f7411adf41f14fadf3de58cadb74e92be4d8d506d2e`.
- `roms/research/npe3/npe3-troubleshooting.pdf`:
  `f2371b4067111a0e9311e68fc5dead843e415290f87c0afd562e505c9b42a4cb`.

Next evidence is an NPE-3 PCM timing description/capture or execution of its
matched DSP initialization that establishes the clock/framing registers.
The MCU command-8 `870b` observation establishes speech-control intent, not
those downstream clocks. Do not attach a guessed diagnostic clock merely
to make the HLE speech timer run.

The own System Module issue 1 (09/00), printed page 24, table 8 also supplies
analog reference conditions, separate from the missing digital framing.
EARP/EARN typical output is 14 mV at network level -16 dBm0 with nominal
RLR +2 dB and volume eight dB below maximum; the listed maximum is 220 mV
at 0 dBm0 with maximum volume and codec gain -11 dB. MIC2P/MIC2N typical
input is 2 mV, maximum 12.5 mV; the maximum corresponds to a 1 kHz, 0 dBm0
network signal with input amplifier gain 32 dB. These are electrical test
conditions, not PCM sample amplitudes or recovered runtime register values.
Do not turn them directly into MAME route gains: first establish the codec
mux, gain-register encoding, sample format and product-owned serial clocks.
Unity board routes currently prove connectivity only.

## Missing native bootstrap observation

No matching raw NPE-3 final publication or ROM6 mask image was identified in
the acquired collection or checked-out reference implementation. The latter
answers the parked shared-offset-2 read with zero; its later self-test
responder does not establish the earlier verifier's result. The separate
bridge default `1eff` is described as an 8210 value without a matching raw
NPE-3 capture, and must not be transplanted.

The smallest input that can justify an HLE continuation is an unmodified
NPE-3 v5.56 boot capture identifying the board/DSP revision and firmware
hash, and recording shared halfwords at MCU `10000` and `10002` around
the final (232nd) buffer acknowledgement. Include `10004/10006` and
the ownership words `100fe/10100` to distinguish phase/order, with raw
capture bytes and a hash. Read substitutions, boot patches and bridge-HLE
values must be disabled and declared. A standby snapshot alone can contain
later reused mailbox data, so it is not equivalent to this boundary capture.

Alternatively, core execution needs ROM6 PROM content/mapping at program
`ff87` under PMST `ffa8`, plus the COBBA register-F/status-D contract.
The stock MCU flash and PMM do not provide validated copies of those hardware inputs.
The same staged code on another product is a comparison, not a substitute.
