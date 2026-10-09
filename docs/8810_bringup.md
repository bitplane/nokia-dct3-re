# Nokia 8810 (NSE-6) bring-up boundary

## Current Result

The acquired v6.02 PPM A package normalizes to a complete, contiguous 2 MiB
CPU-big-endian flash image. No NSE-6 machine configuration or runtime acceptance
exists yet. Other products' provisioning and DSP publications are not evidence
for this handset.

The reset stack base `0x125f30` fits the documented 256 KiB SRAM window starting
at `0x100000`; importing a 128 KiB product configuration would be incorrect.
The next question is the own DSP bootstrap/verifier and EEPROM attachment.

The original Nokia NSE-6 system-module chapter, pages 3-41/3-42, specifies
16 Mbit flash (2 MiB), 2 Mbit SRAM (256 KiB), and 256 Kbit serial EEPROM
(32 KiB). These are physical capacities, not a complete BUSC alias map.
[Original service chapter](https://www.eserviceinfo.com/preview_html.php?fileid=5448&previewid=3000).

## Acquired Input

The self-extracting ZIP is read as archive data, never executed:

| Artifact | Identity |
| --- | --- |
| `nse6_602.exe` | SHA256 `abc2fa6a0b1b0f5e33206c7ffb5ce17e81ab46e155733584e23aa3459b3349e9` |
| MCU `NSE67016.020` | Decoded extent `0x200000..0x36ffff` |
| PPM `NSE67016.02A` | Decoded extent `0x370000..0x3fffff` |
| Combined image | SHA1 `e3b548816fa027da906be3daf049ce6332a9f257` |
| Combined image | SHA256 `63c60e637d19c16b86bc8d16db5e3b5691126880ecc60a7447a5efbad3547beb` |

The package's `nse-6.ini` selects that MCU under `NSE-6_RESTOREFILES`
and that PPM under `EURO_A`. The two decoded extents meet without an erased gap.
Local inputs and normalized output live under `roms/research/nse6-v602/`.

Reproduce normalization with the existing structured record decoder:

```sh
.venv/bin/python tools/extract_dct3_wintesla.py \
  --mcu roms/research/nse6-v602/NSE67016.020 \
  --ppm roms/research/nse6-v602/NSE67016.02A \
  --flash-output roms/research/nse6-v602/8810-v602-ppm-a.fls
```

## Reset Facts

Direct ARM-big-endian disassembly of this image establishes:

| Address | Operation |
| --- | --- |
| `0x200040..0x20005c` | Reads the first flash word, transforms byte lanes, writes `0x40000`. |
| `0x200068`, `0x200090`, `0x2000b0` | PC-relative literal loads all resolve to stack base `0x125f30`. |
| `0x2000a0` | Establishes peripheral base `0x20000` in `ip`. |
| `0x2000b8..0x2000c4` | Copies eight words from `0x200180` to address zero. |
| `0x2000c8..0x2000d4` | Writes `0xff` to peripheral bytes `0x2000a` and `0x2000b`. |
| `0x2000d8..0x2000e8` | Clears CPSR interrupt-mask bits and switches into Thumb execution at `0x2000ec`. |

The first Thumb calls are `0x27afe8` and `0x23190e`, before the zero-fill
of `0x100020..0x1216cb`. The startup entry at `0x2d330e` then calls the
DSP initialization/verifier at `0x2b6118`. The pre-clear routines access retained
RAM near `0x13ffxx`; this also fits the documented SRAM capacity.

## Recovered Interface Boundaries

### GENSIO and CCONT

CCONT reader `0x2dfc60` selects control `0x28 = 0x22`, writes the cached
command OR `0x04` at `0x2a`, waits for status `0x29` bit 2, and reads `0x2d`.
Writer `0x2dfba8` uses the same select, clears the command's read-request bit,
then sends the modified byte at `0x2a`. The recovered six-register transport
tuple is consequently `(0x2a, 0x28, 0x2b, 0x2d, 0x29, 0x2c)`, in
CCONT-write/control/LCD-data/CCONT-read/status/LCD-command order. It matches the
shared earlier-layout transport without relying on a sibling's addresses.

This is digital attachment evidence, not an ADC calibration tuple, measured
conversion latency, or validated interrupt routing. Those remain separate
configuration questions before runtime promotion.

### Serial EEPROM

The byte transmitter `0x2de3d4` constructs PUP base `0x20020`, uses data mask
`0x01` and clock mask `0x04`, and controls SDA direction at `0x20024`. Thus
the own transmit contract uses SDA bit 0 and SCL bit 2. The ACK sampling at
`0x2de484..0x2de492` returns the inverted SDA bit after clocking the ninth bit.
The reader `0x2de496` releases SDA direction at `0x20024`, samples bit 0 with
clock mask 4, and assembles eight bits MSB-first.

Initializer `0x2dd100` reads flash byte `0x200005`, value `0xf6`:

| Field | Firmware decoding | Own result |
| --- | --- | --- |
| Capacity | `1 << ((descriptor & 7) + 9)` | 32768 bytes |
| Page size | `1 << (((descriptor >> 3) & 7) - 1)` | 32 bytes |
| Address width | Descriptor bit 6 | Two address bytes |

The address setup at `0x2dcef0` consequently uses control byte `0xa0`, then
high and low address bytes through `0x2de3d4`. The write wrapper at `0x2dcf7a`
splits transfers using the configured page size. These contracts agree with
the manual capacity, but do not identify a manufacturer, establish measured
write-cycle latency, or supply authentic EEPROM contents.

### DSP Startup

The own verifier `0x2b6118` reads one 16-bit word every 32 flash bytes from
`0x200040`, sends 127 blocks of 512 words and a final 510 words followed by
two `0xffff` words. Buffers alternate between `0x10200` and `0x10600`, with
handshake halfwords at `0x100fe/0x10100`. At `0x2b6200` it waits while the
second handshake halfword is `0xffff`, then copies the two returned halfwords
into context `0x1205c0 + 0x0c/+0x0a`.

This identifies a protocol shape shared with other recovered verifier streams,
not a fitted DSP mask or a successful verdict. No resident-ROM compatibility
is established and no DSP response is fabricated.

### Display

Initializer `0x2e1194` selects GENSIO control `0x28 = 0x21`, configures PUP
direction `0x24` bit 5, and pulses PUP data `0x20` bit 5 low/high with a delay.
Command sender `0x2e1146` polls status `0x29` bit 0 then writes `0x2c`;
pixel sender `0x2e1184` uses the same ready test then writes `0x2b`.

The initialization sends `0x24`, then for six banks sends `0x40|bank` and
column address `0x80`, followed by 84 zero pixel bytes each, and finally `0x20`.
This establishes an 84-column, six-eight-pixel-bank surface and a command
grammar consistent with the shared PCD8544-family implementation. It does not
identify the exact physical controller or prove a rendered runtime frame.

### Keypad

Own scanner `0x2de164` uses row register `0x31`, column register `0x30`,
direction register `0x2f` and interrupt mask `0x33`. Ordinary scans select
rows 1 through 4 and test five active-low columns; raw keys are `row*5+column`.
The separate special-key scan yields `0x80+column`.

Decoder `0x2e049a` maps normal keys through `0x3033b4` and special keys through
`0x3033d0`, indexed by layout byte `0x1214af`. Recovered layout-zero tables:

```text
normal:  3e3e3e3e3e11190102030e170405060f18070809101a0c0a0b
special: 3e3e3e3e0d
```

Special column 4 maps to power key `0x0d`, requiring column mask `0x10`.
The bytes coincide with independently recovered NSE-1/NSM-1 tables, but are
checked against this image rather than assumed from those products. Host
button labels and runtime input acceptance are not yet established.

## Acceptance Required Before Promotion

Run the hash-pinned package/reset check with
`python -m tools.nse6_v602_static_check roms/archive-dct3-packages/nse6_602.exe`.
Its memory-capacity fields cite the service chapter; they are not decoded from
the reset instructions. The check explicitly reports no runtime acceptance.

1. Recover own memory/reset map and peripheral attachment from firmware and
   original service information; recover EEPROM pins and BUSC configuration.
2. Identify own DSP bootstrap/verifier and persistent-storage dependencies.
3. Add hash-pinned static checks and negative fixtures before a runtime profile.
4. Run an isolated, forcing-free boot; record its actual frontier, even if blank.
5. Promote graphical/input/SIM/service capabilities only with their own runtime
   evidence. A historical repair EEPROM, if examined, remains a repair template,
   not an authentic handset dump or independently justified provisioning.
