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
- The unvalidated 6250 retains its previous generic composition; it does not
  silently inherit the new 6210 contract through a shared machine config.

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
The literal `Headset` accessory label remains unresolved. Neighbour/handover
and speech are not accepted; unrecovered handover fields remain unset.

### Phone-service acceptance

`verify-6210-outgoing-call` physically dials `1234567` and presses Send/End.
`verify-6210-incoming-call` queues one laboratory network call, captures
ringing and connected screens and physically answers/ends it. Both require
ordered CC/RR establishment, traffic assignment, Connect acknowledgement,
Disconnect/release and return to paging. Own traffic/release configurations
are 24 bytes, not the NSM-3 20-byte forms. Physical End emits release
parameter `14`, now selected by the NPE-3 peer. Incoming Call Confirmed is
`8308040460020081150101`. These prove signaling, **not speech**.

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

All service runners start with new working directories; incoming network
events are selected through MAME configuration, not firmware injection.
No donor PMM, forced phone state or borrowed DSP verdict is used. The staged
verifier's declared PROM/COBBA inputs and runtime HLE substitution at missing
mask routine `2c75` remain research assumptions. Native mask execution,
measured DSP self-test values, speech/audio parity and electrical ADC units
remain separate work, not implied by the phone-service gates.

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
