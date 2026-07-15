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

This replaced the former firmware-PC-gated byte sequencer. Firmware now drives
ordinary SDA/SCL transitions and receives acknowledgement/data from the MAME
device.

The GenIO input path must distinguish the output latch from the physical SDA
level. When direction bit 0 is clear, the MCU has released SDA and reads the
EEPROM's line directly; combining that level with the stale signal-register bit
can hold SDA low after the MCU last transmitted a zero. That integration error
previously made the firmware calculate `0x00ff` and read stored checksum/guard
words as zero at `0x234810`, despite a correct backing image. With the released
line read directly, the failure branch at `0x234826` is absent and contact status
keeps bit 6 through EEPROM validation (`0xc8` -> `0xcc`). The later clear at
`0x237b04` is the independent disabled service-channel/PM-read predicate described
in `service_bootstrap.md`.

The memory map also exposes a read-only region at `0xa00000..0xa03fff`. Its
relationship to the physical serial EEPROM is not established; the current
handler reads the input region directly and is classified as an unproven
parallel alias in `mad2_fidelity.md`.

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

Earlier tracing established that EEPROM reads begin around 6 ms into boot and
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
| `0x18a8..0x18cb` | high-address record, semantics unknown |

The EEPROM is therefore part of early boot state, not a later optional feature.

Both collected 3210 images have erased bytes across the descriptor-2/3/4 range,
so this ROM substitutes its validated defaults. The source-7 gain denominator is
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
validity is one real CONTACT SERVICE prerequisite, but not the only one. The
native serial path now demonstrates this check organically; it is not satisfied
by a firmware hook or RAM override.

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
against RAM `0x112460`, and returns one. Callback `0x47` consequently returns
the accepted-code status `0x05e6`; mismatch would instead publish `0x05e1`.

The same fixture is reproducible through
`make eeprom-profile PROVISIONED_IMEI_PREFIX=49015420323751`. Omitting the
variable preserves the erased-identity default used by the canonical oracle.

That fixture makes the identity comparison succeed and removes the Security-code
editor. It paints a new idle-like frame (SHA-256 prefix `dbf2704cb945d56b`) but
does **not** publish report code 7 or move task 1 out of mode `0x0004`. Keypad
interaction is independently functional on the unprovisioned editor path; the
fixture is evidence about security provisioning, not the new default oracle.

This profile is explicit test provisioning. It is not a factory EEPROM dump and
does not belong in the hardware device. The generated image currently has:

- CRC32 `7f7fd703`;
- SHA-1 `3402e47e133dc74c7fa03863fee44a171f15100e`; and
- SHA-256 `d7561ddd13d5c1c584bc785514102e68de5a39c3f1078467f769ef47e4850d67`.

## Remaining work

1. Establish the purpose and checksum rules of records beyond `0x0245`.
2. Decode the remaining fields in the identity-derived security record and
   separate calibration, identity/security and ordinary user settings into
   documented data structures.
3. Validate against a legitimately obtained provisioned EEPROM image without
   committing personal identifiers or calibration data.
4. Determine whether the `0xa00000` parallel mapping is real MAD2 hardware,
   boot-ROM behavior, or an incorrect legacy mapping.
5. Define product-specific permanent-memory placement for phones that store it
   in flash.

The EEPROM component is considered validated only after both the serial
protocol and data placement work across a second ROM/product.
