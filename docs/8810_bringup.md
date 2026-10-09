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

### Serial EEPROM

The byte transmitter `0x2de3d4` constructs PUP base `0x20020`, uses data mask
`0x01` and clock mask `0x04`, and controls SDA direction at `0x20024`. Thus
the own transmit contract uses SDA bit 0 and SCL bit 2. This is not a claim
of complete ACK/read/page-cycle recovery or an exact EEPROM manufacturer part.
The address setup at `0x2dcef0` supports the two-address-byte branch selected
by its configuration byte; its initialization still needs decoding.

### DSP Startup

The own verifier `0x2b6118` reads one 16-bit word every 32 flash bytes from
`0x200040`, sends 127 blocks of 512 words and a final 510 words followed by
two `0xffff` words. Buffers alternate between `0x10200` and `0x10600`, with
handshake halfwords at `0x100fe/0x10100`. At `0x2b6200` it waits while the
second handshake halfword is `0xffff`, then copies the two returned halfwords
into context `0x1205c0 + 0x0c/+0x0a`.

This identifies a protocol shape shared with other recovered verifier streams,
not a fitted DSP mask or a successful verdict. No resident-ROM compatibility
is established and no DSP response is fabricated. Display and keypad contracts
remain unexamined.

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
