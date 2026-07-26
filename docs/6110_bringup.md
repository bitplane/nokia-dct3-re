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
case. The firmware's first APDU sequence still requires an organic boot trace,
so SIM and registration coverage remain unpromoted.

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
references, while `0x10b97c` has none. Indirect/table-mediated use remains
outside that negative result, so the second word is still unconstrained.
Publishing the generic HLE ready word `0x0001` cannot satisfy all NSE-3
firmware paths.

The surrounding service selectors also preserve the revision namespaces in
firmware rather than merely in our documentation:

- `0x03` (DSP external software) follows a flash-indirect source rooted at
  `0x2ab52c`;
- `0x09` (DSP internal software) reads a separately populated runtime buffer
  at `0x10bcf0`;
- `0x0c` (system ASIC) reads MAD2 register `0x20000`; and
- `0x0d` (COBBA) projects the bootstrap-captured `0x10000` word as `B06`.

The exact-image gate pins all four dispatch/source paths. A product profile
must therefore never use COBBA `B06` as a DSP software revision, MAD mask
revision or ROM3/ROM4 flash selector. Conversely, learning one of those other
identities cannot supply the missing DSP-side rule that publishes the COBBA
word.

This remains deliberately MCU-side evidence. We do not yet know whether the
staged stream is DSP code, its DSP-side destination, how the DSP derives or
publishes the COBBA identity, what the second captured word represents, or
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
