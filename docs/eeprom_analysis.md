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
| `0x0000..0x025b` | tune, security and startup configuration blocks |
| `0x02e0..0x0353` | ADC-monitor calibration/configuration |
| `0x0394..0x04ab` | configuration records |
| `0x0544..0x05c6` | configuration records |
| `0x06c8..0x06fb` | unknown record |
| `0x0db0..0x0f3f` | generated NV descriptor `0x0757`, variant zero |
| `0x18a8..0x18cb` | high-address record, semantics unknown |

The EEPROM is therefore part of early boot state, not a later optional feature.

## Checksummed blocks

Firmware analysis and NokTool behavior agree that early permanent-memory data is
organized as additive-checksummed blocks with big-endian stored sums:

| Block | Data | Stored checksum | Algorithm |
| --- | --- | --- | --- |
| Tune/calibration | `0x0000..0x003d` | `0x003e..0x003f` | 16-bit byte sum |
| Security/IMEI/locks | `0x0040..0x011d` | `0x011e..0x011f` | 16-bit byte sum |
| Contact/config | `0x0120..0x0243` | `0x0244..0x0245` | firmware sum with the observed correction at `0x154` |

The contact-service check uses checksum routine `0x234588` and comparison site
`0x234810`. A mismatch clears the service-present bit used by startup. EEPROM
validity is one real CONTACT SERVICE prerequisite, but not the only one. The
native serial path now demonstrates this check organically; it is not satisfied
by a firmware hook or RAM override.

## Synthetic self-test profile

`tools/make_eeprom_profile.py` generates the ignored local file
`roms/noki3210/3210 selftest eeprom.bin` from the user-supplied 3210 flash. It:

- starts with an erased 16 KiB image;
- copies the firmware's fallback NV descriptor `0x0757` into
  `0x0db0..0x0f3f`;
- supplies the known checksum/config bytes; and
- initializes the ADC-monitor selector/weight records that cannot sensibly be
  all `0xff`.

This profile is explicit test provisioning. It is not a factory EEPROM dump and
does not belong in the hardware device. The generated image currently has:

- CRC32 `4d7bbbf5`;
- SHA-1 `a60510d9d4e84dc0d522f1f3dea69a96c39fb494`; and
- SHA-256 `2df63a1d2413f0f4818b48d2dd0af986f407fe1192282597afe9215df8341572`.

## Remaining work

1. Establish the purpose and checksum rules of records beyond `0x0245`.
2. Separate calibration, identity/security and ordinary user settings into
   documented data structures.
3. Validate against a legitimately obtained provisioned EEPROM image without
   committing personal identifiers or calibration data.
4. Determine whether the `0xa00000` parallel mapping is real MAD2 hardware,
   boot-ROM behavior, or an incorrect legacy mapping.
5. Define product-specific permanent-memory placement for phones that store it
   in flash.

The EEPROM component is considered validated only after both the serial
protocol and data placement work across a second ROM/product.
