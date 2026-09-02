# EEPROM and permanent-memory analysis

The Nokia 3210 uses a separate 16 KiB 24C128 serial EEPROM for calibration,
security and product configuration. Later DCT3 products may store equivalent
permanent-memory data in flash, so storage placement is product configuration,
not necessarily a shared platform constant.

## Current emulation

MAME's `I2C_24C128` device is connected to MAD2 GenIO:

- SDA: signal register `0x20`, bit 0;
- SCL: signal register `0x20`, bit 3; and
- SDA direction: direction register `0x24`, bit 0, where input releases the
  open-drain line.

Firmware drives ordinary SDA/SCL transitions and receives acknowledgement/data
from the MAME device.

## Contract audit

Transport, storage, and provisioning are classified separately:

| Surface | Classification | Basis and limitation |
| --- | --- | --- |
| 24C128 storage device | Reused hardware model plus timing extension | MAME's native `I2C_24C128` owns serial addressing, acknowledgements, page writes and persistence. The Nokia instance selects the documented 64-byte page and conservative 5 ms maximum self-timed write cycle; exact board latency and interrupted writes remain unvalidated. |
| GenIO SDA/SCL wiring | Derived contract | Firmware bit-bangs `0x20.bit0` SDA, `0x20.bit3` SCL, and `0x24.bit0` SDA direction. Organic reads and acknowledgements work with open-drain release semantics. Other GenIO pins and electrical timing remain outside this contract. |
| Firmware serial protocol | Derived contract | START, STOP, ACK sampling, device address `0xa0/0xa1`, 16-bit byte addresses, page wrap and busy-cycle ACK polling are exercised through mapped PUP pins. A two-process gate proves persistence. |
| EEPROMSelX `0xa00000..0xdfffff` | Unmapped | The removed handler returned immutable input bytes and discarded writes. A five-ROM census finds no executable direct literal-derived access to its former 16 KiB slice; dynamic and table-derived accesses remain outside that absence proof. |
| Checksummed startup blocks | Derived data contract | Both checksum algorithms and the firmware comparisons are mapped and generator-tested. This validates the format used by the ROM, not the provenance of the generated contents. |
| Fallback NV record copied from flash | ROM-derived fixture | The v6.00 fallback bytes are copied with the required byte-lane transform. The hard-coded source addresses have not been validated for v5.01 or another product. |
| Identity/security profile | Synthetic provisioning | IMEI prefix, security code, and derived state are coherent with mapped firmware transformations but do not represent a dumped handset. |
| ADC/config patches | Working fixture | Several non-erased values allow meaningful firmware paths. Until each field is decoded or sourced from legitimate PMM data, they are test inputs rather than factory defaults. |

The transport is substantially stronger than the data profile. A valid checksum
proves only that firmware accepts the supplied bytes; it does not make those
bytes authentic calibration, identity, or product configuration.

The GenIO input path must distinguish the output latch from the physical SDA
level. When direction bit 0 is clear, the MCU has released SDA and must read the
EEPROM's line directly rather than combine it with the output latch. With that
open-drain behavior, the failure branch at `0x234826` is absent and service-session status
keeps bit 6 through EEPROM validation (`0xc8` -> `0xcc`). The later clear at
`0x237b04` is the independent disabled service-channel/PM-read predicate described
in `service_bootstrap.md`.

The complete EEPROMSelX range is unmapped. `make storage-static-census`
conservatively follows literal-derived pointers in both 3210 revisions and the
3310/3330/3410 controls. It finds no executable direct consumer of the removed
16 KiB alias. Apparent sibling accesses are classified separately from code.
This does not exclude dynamic pointers or future products.

## Firmware transport

| Address | Role |
| --- | --- |
| `0x2b0318` | I2C START condition |
| `0x2b0188` | send eight bits and sample ACK |
| `0x2b024a` | read byte |
| `0x2b038a` | I2C STOP condition |
| `0x2af858` | EEPROM transfer using device address `0xa0/0xa1` and a 16-bit byte address |
| `0x2af8f4+` | read/write wrappers |
| `0x2af9c6` | presence probe at EEPROM address zero |

Firmware bit-bangs this transport through GenIO. No firmware-address checks are
used by the EEPROM device.

## Observed use

EEPROM reads begin around 6 ms into boot and
cover more than a thousand byte accesses. Important observed ranges include:

| Range | Purpose |
| --- | --- |
| `0x0000..0x0247` | tune, security and startup configuration blocks |
| `0x0248..0x025b` | descriptors 2-4: ADC calibration fields loaded into `0x1122f8..0x11230b` |
| `0x02e0..0x0353` | ADC-monitor calibration/configuration |
| `0x0394..0x04ab` | configuration records |
| `0x0544..0x05c6` | configuration records |
| `0x06c8..0x06fb` | unknown record |
| `0x0db0..0x0f3f` | generated NV descriptor `0x0757`, variant zero |
| `0x18a8..0x18cb` | v6.00 display-profile descriptor `0x0749`: three 12-byte records |

The EEPROM is therefore part of early boot state, not a later optional feature.

## Display profiles

The v6.00 descriptor table entry at `0x2dad78` maps NV id `0x0749` to EEPROM
offset `0x18a8` with a 12-byte record length. The aligned v5.01 entry at
`0x2cf510` maps the same id and length to `0x18a0`; this address difference is
firmware-version data, not a board-register difference. Initializer `0x29a996`
loads variants 0..2 into three records at RAM `0x112188`.

For the active v6.00 path, `0x29a768` copies record-0 byte 5 (`0x11218d`) to
active-profile byte 7 (`0x11fc87`). Setup-message initializer `0x2b1e80` tests
that byte against `4`. The generated EEPROM supplies the source byte; the
driver does not intercept the later RAM read.

Two 3210 EEPROM images are collected. Both have the applicable three-record
range erased, so neither is an authentic configured profile. The ROM
does, however, contain a product reset constructor: v6.00 `0x29a802` and the
signature-aligned v5.01 `0x296d6e` explicitly author the same fields before
committing variants 2, 1 and 0 through the ordinary NV writer. The records are:

| Variant | ROM-authored bytes 0..8 | Unassigned bytes |
| --- | --- | --- |
| 0 | `00 09 01 34 01 04 01 01 00` | 9..11 |
| 1 | `01 08 01 34 01 04 ?? 01 00` | 6, 9..11 |
| 2 | `00 09 01 34 01 04 01 01 00` | 9..11 |

The generator provisions those explicit assignments and leaves the unassigned
bytes erased. This is ROM-authored reset data, not a claim about the contents
of a factory-programmed EEPROM. Firmware loads the records through I2C/NV,
copies record-0 byte 5 to active slot 7 and selects the 3210 LCD setup without
a RAM-read override. The later profile-update handler `0x29ae68` does not
execute during coherent boot. `make verify-display` checks the descriptor,
all three generated records and the active-profile copy.

An otherwise identical negative-control run with all three records left erased
loaded `0xff` into active slot 7 and did not enter the update handler or any
firmware fallback. The later setup emitted commands that the PCD8544 rejected
and the final panel remained blank. Descriptor `0x0749` is therefore required
product provisioning rather than a hardware register default. The generated
ROM-default fields are the strongest available provisioning fixture; bytes the
constructor does not assign remain unknown pending an authentic configured
record.

Both collected images have erased bytes across the descriptor-2/3/4 range, so
this ROM substitutes its validated defaults. The source-7 gain denominator is
descriptor-2 bytes `0x024a..0x024b`; firmware uses gain `563.0 / denominator`
and falls back to denominator `0x0233` (unity gain). See
`battery_classifier_analysis.md` for the complete ranges and boot-safety result.

The current security-lifecycle audit also identifies three related records:

| Offset | Length | Firmware use |
| --- | --- | --- |
| `0x000c` | 8 | first fourteen IMEI digits in high-nibble-first BCD; `0x265244` calculates digit fifteen |
| `0x0110` | 3 | five-digit phone security code in BCD |
| `0x06c8` | 8 | identity-derived security state loaded into RAM `0x112460` |

The selector at `0x2ae61a` formats the identity, adds callback state
`[0x11fcb4]`, and compares that sum with the big-endian word in security-state
bytes 6-7. An erased profile produces question-mark identity text and stored
`0xffff`, so callback `0x47` legitimately presents **Security code**. This is
phone-side provisioning, not a SIM PIN or a corrupt top-level EEPROM checksum.

## Checksummed regions

The 3210 v6.00 firmware validates two early permanent-memory regions:

| Block | Data | Stored checksum | Algorithm |
| --- | --- | --- | --- |
| Combined tune/security | `0x0000..0x011d` | `0x011c..0x011f` | `sum16` compared with a 32-bit big-endian word |
| Contact/config | `0x0120..0x0243` | `0x0244..0x0245` | firmware sum with the observed correction at `0x154` |

At `0x264c56`, the firmware reads `0x120` bytes from offset zero, sums the first
`0x11e` bytes through `0x2a41d0`, and compares the 16-bit result with the
big-endian 32-bit word at `0x011c`. The checksum field overlaps the last two
summed bytes; the generated profile stores zero there and the sum in
`0x011e..0x011f`. Its computed and stored values are both `0x1ae4`.

NokTool describes independent tune and security sub-block checks at `0x003e`
and `0x011e`. That is useful format evidence, but it is not the check executed
by this ROM and must not be treated as a validated 3210 firmware contract.

The contact/config check uses checksum routine `0x234588` and comparison site
`0x234810`; its computed and stored values are both `0x1c25`. A mismatch clears
the service-present bit used by startup. EEPROM
validity is one real service-session startup prerequisite, but not the only
one. The check is demonstrated organically over the native serial path, with
no firmware hook or RAM override.

## Synthetic self-test profile

`tools/make_eeprom_profile.py` generates the ignored local file
`roms/noki3210/3210 selftest eeprom.bin` from the user-supplied 3210 flash. It:

- starts with an erased 16 KiB image;
- copies the firmware's fallback NV descriptor `0x0757` into
  `0x0db0..0x0f3f`;
- supplies the firmware-defined default four-byte availability record
  `{30 00 80 90}` at `0x0150..0x0153`;
- supplies the known checksum/config bytes; and
- initializes the ADC-monitor selector/weight records that cannot sensibly be
  all `0xff`.

With `--provisioned-imei-prefix DIGITS`, it can additionally create a synthetic,
internally consistent identity/security fixture. The generator follows the
firmware transformations at `0x265244`, `0x2ae4e8`, `0x2ae598`, and `0x2ae61a`;
it does not copy a real handset identity or patch firmware state. The test
prefix `49015420323751` produces check digit `8`, default security code `12345`,
and derived record `32 d8 fa 97 00 00 03 17`.

The unprovisioned security editor independently validates the code encoding.
After a physical `12345` plus softkey sequence, verifier `0x2ae704` confirms a
five-character input, transforms it through `0x2ae4e8`, compares four bytes
against RAM `0x112460`, and returns one. The accepted run's callback publication
is `0x05e1`; incorrect input may use the same scalar in a different
callback-local state, so acceptance is established by the comparison result
rather than a global status-code interpretation.

The same fixture is reproducible through
`make eeprom-profile PROVISIONED_IMEI_PREFIX=49015420323751`. Omitting the
variable preserves the erased-identity default used by the canonical oracle.

`ERASED_IDENTITY_SECURITY_CODE=12345` is a narrower research fixture. It leaves
the IMEI bytes erased, but writes the BCD code and the verifier record derived
from the ROM's fifteen-question-mark formatted identity. A delayed physical
`12345` plus left-softkey sequence then exits the security editor without
inventing an IMEI. This was used to disprove the UI lock as the reason the
ordinary-text MT-SMS fixture does not emit a CP/RP tail. The separate
port-addressed multipart fixture now emits and completely closes both CP/RP
transactions. Its application continuation proves that the first part survives
the intervening RR release and that the matched pair reaches the ringtone
parser, yet flash and EEPROM remain bit-identical until the user chooses Save.
Only the ordinary control changes SIM NVRAM. Reassembly before that physical
choice is therefore ordinary firmware RAM state, not a hidden host cache or
persistent PMM/flash/SIM record.

`make verify-radio-smart-message-persistence` continues through the NSE-8
v6.00 storage boundary. Physical Options → Save has no name editor or
confirmation prompt: firmware uses the RTPL title and displays `Ringing tone
saved`. It writes one received-tone object to the fitted 24C128 range
`0x0b24..0x0c5f`. The record begins `00 02 fc 09 00 0a 01 8a 1c 40 ...`,
contains the complete playable command stream, and ends with
`Test for Dhiram 00`. Flash and SIM NVRAM remain unchanged. A preserved-NVRAM
cold boot exposes the object as `9-2-39 Test for Dhiram` in the ordinary
Ringing tone selector and replays the same note-varying PUP melody. Physical
Options → Discard instead returns directly to idle and leaves the entire slot
erased. Save-state replay immediately before the physical Save transaction and
after its terminal notification retains exactly one object. There is no
separately observable name editor, confirmation phase or multi-write interval
on this ROM. Leaving the physical Options menu through C leaves the slot
erased. Receiving and saving the same named tone again after a preserved cold
boot replaces the single received-tone slot and still leaves exactly one title;
it does not create another list entry or ask about a duplicate name. This is an
NSE-8 EEPROM-layout result, not a generic DCT3 offset.

The comparison products independently disprove a shared received-tone storage
topology. `make verify-3410-radio-smart-message-persistence` takes NHM-2
v5.46E through its three-location chooser and explicit `Replace tone? (empty)`
confirmation. The terminal `Ringing tone saved` operation changes only the
PMM-backed portion of the fitted 32-Mbit flash; its 24C128 and SIM images remain
identical to a delivered-but-unsaved control. A preserved-NVRAM cold boot then
finds `Test for Dhiram` at option 36 through Profiles → General → Personalise →
Ringing tone and replays varied PUP notes. `make
verify-3310-radio-smart-message-persistence` independently proves that NHM-5
v6.39 also changes only its product-local PMM-backed flash, retains the saved
entry after cold boot and exposes it through Menu 5 → Ringing tone. Its
localized title rendering and DSP-tone playback contract differ from NHM-2.
Its MCU-visible DSP mailbox repeatedly publishes a 900-Hz oscillator, while
the saved melody's pitch remains DSP-local; the comparison therefore proves
durable listing and physical non-silence but not note-pitch-correlated playback.
Neither product inherits NSE-8's EEPROM offset or promptless one-slot policy.

That fixture makes the identity comparison succeed and removes the Security-code
editor. It paints the idle frame (SHA-256 prefix `dbf2704cb945d56b`) without
moving task 1 out of mode `0x0004`. A physical left-softkey fixture opens the
firmware-owned `Phone book` menu from that frame, proving the UI is interactive
and mode `0x0004` is not a blocked startup state. The provisioned profile and
menu interaction remain acceptance fixtures rather than the default oracle.

This profile is explicit test provisioning. It is not a factory EEPROM dump and
does not belong in the hardware device. The default, unprovisioned image loaded
by the MAME ROM definition currently has:

- CRC32 `7f7fd703`;
- SHA-1 `3402e47e133dc74c7fa03863fee44a171f15100e`; and
- SHA-256 `d7561ddd13d5c1c584bc785514102e68de5a39c3f1078467f769ef47e4850d67`.

With test prefix `49015420323751`, the provisioned fixture instead has CRC32
`c6c04a5e`, SHA-1 `3ddb5b76fed37970fb2ae6e0e9e213b7ed19257a`, and SHA-256
`3b3ee14965e53241038fbdebff52efe73261eda8f5bc65830ff4342cd3adada6`.

The `run` target serializes profile generation, copying and NVRAM seeding.
This ordering is required: parallel `build` and `prepare-run-nvram` targets can
otherwise seed MAME from the previous EEPROM image and misclassify a provisioned
run as an unprovisioned security-editor result.

## Remaining work

1. Establish the purpose and checksum rules of records beyond `0x0245`.
2. Decode the remaining fields in the identity-derived security record and
   separate calibration, identity/security and ordinary user settings into
   documented data structures.
3. Validate against a legitimately obtained provisioned EEPROM image without
   committing personal identifiers or calibration data.
4. Validate exact AT24C128 board timing and interrupted-write behavior.
5. Decode flash-backed permanent-memory formats beyond their established
   product placement and persistence boundary.
6. Parameterize or signature-locate the fallback-record source before treating
   a v5.01-generated profile as a data-layout control; the current generator's
   source addresses are v6.00-specific.

Generator unit tests cover both checksum algorithms and the synthetic
identity/security derivation. Integration oracles exercise organic serial reads,
deterministic NVRAM seeding, mapped-pin page wrap, busy polling and persistence.
Open-drain electrical timing and power interruption remain untested.

The external-storage boundary is independently configured for NSE-8's 24C128
and NSE-3's 24C64. Flash-backed sibling storage remains a separate contract.
