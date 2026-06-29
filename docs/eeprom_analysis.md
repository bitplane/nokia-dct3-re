# EEPROM / Permanent-Memory analysis (Nokia 3210, DCT3)

Goal: document how the firmware uses the EEPROM, so we can (eventually) synthesise a
valid image instead of the blank one, and so we know *when* the EEPROM actually matters.

## 1. Hardware

- The 3210 stores its NV/calibration in a **separate serial 24C128 EEPROM** (16 KB,
  `0x0000–0x3FFF`), on the MAD2 I²C bus. (Unlike the 3310, which keeps it in flash at
  `0x3D0000`.) Driver region: `"eeprom"`, `0x4000` bytes.
- **It is currently blank: 16 KB of `0xFF`.** (`roms/noki3210/3210 virgin eeprom,24c128.bin`
  is all `0xFF`; its SHA1 matching `dsp_drom` just means that file is also all-`0xFF`.)
- I²C lines via MAD2 IO reg `0x20`: SDA = bit 0 of reg `0x20`; clock/enable via reg `0x24`.
  Modelled in the driver by `serial_eeprom_*` (`mad2_io_w` dispatch on reg `0x20`).

## 2. Firmware I²C / EEPROM transport layer (mapped)

| addr | role |
|---|---|
| `0x2b0318` | `i2c_start` (drives reg `0x20`/`0x24` SDA/SCL for a START condition) |
| `0x2b0188` | `i2c_send_byte_get_ack` (8 bits + ack; returns 1 on ack) |
| `0x2b038a` | `i2c_stop` |
| `0x2af858` | `eeprom_xfer(addr=r7, read=r8)` — sends device byte (`0xa0 | block`), `addr_hi`, `addr_lo`; repeated-START + `0xa1` for reads. Retries up to `0xc8` on NAK. Device-byte logic: if `[dev+0]!=0` use fixed `0xa0` (24C128 path, full 16-bit address in two bytes); else compute block bits from `(addr>>7)&0xe`. |
| `0x2af8f4`+ | read-byte / write-byte wrappers around `eeprom_xfer` |
| `0x2af9c6` | `eeprom_present_probe` — xfer at address 0 just to confirm the chip ACKs; returns 1 if present. (Called from the contact-service terminal `0x2b4dda` for reasons 2/3/4.) |

The driver hooks the bit-bang at the exact PCs `0x2b0318` (start), `0x2b01ac` (write bit),
`0x2b028e` (read bit), and `0x2b0188`+ (SDA ack), and serves bytes from `serial_eeprom_byte`.

## 3. The EEPROM IS read — early and extensively (corrected)

> **Correction.** An earlier version of this doc claimed "0 EEPROM reads". That was a
> harness bug: the `NOKI3210_TRACE_EEPROM` knob was never wired into the Makefile (a
> `grep -q TRACE_EEPROM` guard matched the pre-existing `TRACE_EEPROM_I2C`), so the probe
> never ran. With the knob actually passed, the result is the opposite.

Over the forcing-free 8 s boot the firmware reads the EEPROM **1247 times**, starting at
**t≈0.006 s** — long before the contact-service stall (~0.54 s). So the blank EEPROM is
read early and is very much in play.

**EEPROM field map (regions actually read, all returning 0xFF except the overlay bytes):**

```
0x0000-0x025b   config block (~600 B)            ← read first, t≈0.006
0x02e0-0x0353   ADC monitor calibration          (matches overlay 0x2e0/0x310/0x330 ranges)
0x0394-0x04ab   config (~270 B)
0x0544-0x05c6   config (~130 B)
0x06c8-0x06fb   52 B
0x18a8-0x18cb   36 B record near top of chip
```

High-level block read is at `~0x2b1d0e` (first reads at t=0.006); the I²C transport is the
`0x2b03xx`/`0x2af8xx` layer in §2. The firmware copies these regions into RAM and (per the
service manual) validates checksums; the stored EEPROM is blank, so every byte reads 0xFF.

(Repro: `make run-boot-progress … TRACE_EEPROM=1` → `eeprd:` per byte, `eepi2c_pc:` per
distinct reg-`0x20` PC.)

## 4. The boot-time self-tests do NOT touch the EEPROM

The self-tests reached near the stall (hooked by the removed `FORCE_STARTUP_SELFTEST_RESULT`):

- `0x2961ec` = **flash** block checksum (iterates a stride-6 block table at `0x200000+`,
  reads per-block checksums) — not EEPROM.
- `0x2961ac` = a **config blank-check**: calls `0x2b0a74`, which scans a 0x24-byte config
  record for any non-`0xFF` byte (returns 0 if all blank). RAM/config, not the EEPROM bus.
  Result codes: 0 = pass, `0xfd` = fail, `0xfe` = pending/blank. The readiness manager
  `0x29bc70` checks `0x2961ac == 0xfd`.

So the EEPROM checksum self-test is a *different*, later routine (a function that drives
`eeprom_xfer` to read a block, then sums it and compares a stored checksum) — not yet
located because the boot never reaches it.

## 5. Known EEPROM fields (from the existing `selftest` overlay)

The driver's `serial_eeprom_byte` `NOKI3210_EEPROM_PROFILE=selftest` overlay supplies a
hand-found set of NV defaults (the only field map we have so far). These are the bytes
prior RE found the firmware needs once it *does* read the EEPROM:

```
0x0170=01 0x0171=00            (config-block word)
0x0244=1e 0x0245=e1            (config-block checksum, big-endian = 0x1ee1; see §5b)
0x048c=0a 0x048d=00 0x048e=0a 0x048f=80     (record)
0x0394=0a 0x0395=00 0x0396=0a 0x0397=80
0x0398=09 0x0399=00 0x039a=00 0x039b=00     (record)
0x02e0..0x02eb = 00            ) ADC monitor calibration / config records read by
0x0310..0x0313 = 00            ) 0x2a7230; blank 0xff makes invalid selector/weight
0x0330..0x0337 = 00            ) tables at 0x11145a/0x111d3c/0x111d5c.
```

These are partial and empirical — not the real factory layout. The overlay bytes sit inside
the read ranges in §3; the rest of those ranges read 0xFF.

## 5b. EEPROM checksummed-block layout (cross-validated with NokTool 1.8)

The EEPROM is a sequence of **additive-checksummed blocks**. This is established from two
independent sources that agree exactly:

- **NokTool 1.8** (the Borland Delphi service tool, reconstructed under `noktool-src/` via
  IDR). Its checksum routine `sub_0046AAA8(start, end)` is a **16-bit additive byte-sum**
  over `bytes[start..end]`; `TForm1.e2prom1Click` (`0x46ab10`) validates two blocks, reading
  each block's **big-endian** stored checksum at the block's last two bytes.
- **The 3210 firmware itself**: the contact-service check (sum routine `0x234588`, compare at
  `0x234810`) covers the next block and stores its checksum big-endian at `0x244..0x245`.

| Block | Data range | Checksum (big-endian) | Algorithm | Source |
|---|---|---|---|---|
| Tune / calibration | `0x0000..0x003d` | `0x003e..0x003f` | `sum16` | NokTool |
| Security / IMEI / locks | `0x0040..0x011d` | `0x011e..0x011f` | `sum16` | NokTool |
| Contact-service / config | `0x0120..0x0243` | `0x0244..0x0245` | `sum16 − [0x154] word` | firmware (this work) |

The blocks **tile**: each block is `data[start..cksum-1]` + a 2-byte BE sum at `[cksum,cksum+1]`,
and the next block starts at `cksum+2`. The security block ends at `0x011f`, and the firmware's
config block starts at exactly `0x0120` — independent confirmation of the boundary. Named in the
driver as `FW_EEPROM_{TUNE,SECURITY,CONFIG}_BLOCK_{START,CKSUM}`.

**Block contents** (from NokTool's UI labels and routines):
- **Tune block** (`0x0000..0x003d`): RF/PMM calibration ("tune") data.
- **Security block** (`0x0040..0x011d`): **IMEI** (original + current) and the **4 SIM locks**
  (lock1 = MCC+MNC, lock2 = GID1, lock3 = GID2, lock4 = current SIM) plus SP-lock config.
  NokTool reads/writes these over FBUS, not from the dump's raw offsets, so exact in-block byte
  positions still need confirmation against the firmware's own parser (a later task).
- **Config block** (`0x0120..0x0243`): contact-service / startup config (the block whose
  checksum gates bit 6 — see §8).

NokTool's 3210 (NSE-8) profile: `eeprom_size = 0x800`, `tune_end = 0x40`, `security_end = 0x120`.
Note our physical part is a 24C128 (16 KB), so blocks past `0x800` likely exist (RF/ADC records
at `0x0394+`, `0x048c+`); NokTool only models the first two. NokTool gives the **structure and
checksum algorithm**, not the actual factory calibration *values* — those still need a donor
image. NokTool's MBUS traffic is PC-side **FBUS** service commands (lock/IMEI/trace), which are
*not* the phone's internal D0/D9 startup commands, so they are not mirrored as driver constants.

## 6. Gaps / TODO for a real image

To synthesise a valid EEPROM we still need:
1. **The checksum algorithm + where the stored checksum lives** — find the function near
   the early reads (`~0x2b1d0e`, t≈0.006) that reads a block via `eeprom_xfer`/`0x2af858`
   and validates a checksum. Cross-check against NokiaTool/Rolis `noktool18` "recalc
   checksum" (x86 RE) — algorithms aren't copyrightable.
2. **Field semantics** for the read ranges in §3 (IMEI, product code, PSN, RF calibration,
   display profile, service-channel config) — partly via the firmware code that consumes
   each range, partly via documented DCT3 PM layout.

## 7. Conclusion (corrected, twice)

The EEPROM transport is fully understood. Two corrections over the earlier write-up:

1. Contrary to an earlier (knob-bug) conclusion, the EEPROM **is read first**, at
   t≈0.008 s — 1247 byte reads across 1204 distinct addresses, before everything else.
   (The earlier "0 reads" was a harness bug: the knob was unwired *and* I was grepping
   make's stdout instead of `run_*/error.log`.)
2. But the running boot is **not** reading the blank file — it runs `EEPROM_PROFILE=selftest`,
   a hand-built overlay (~16 bytes + three zeroed ranges in §4/§5); the other ~1188
   addresses still read 0xFF.

> **⚠️ Updated — see `service_bootstrap.md` Executive summary.** The "ack `0x11fedb` is the root"
> wording below is **superseded**: the ack is a red herring (never written by firmware). The real
> gate is service-present **bit 6**, and the EEPROM is **not** "secondary/redundant" — it is one of
> bit 6's direct gates. **Two** EEPROM checksums clear bit 6 and are now modelled in the `selftest`
> overlay: the config checksum (`0x244`, below) **and** the tune/security checksum (`sum16[0..0x11b]`
> == word `0x11c`, the idx18 service-channel gate). Read the rest of this section as "the EEPROM
> config checksum mechanism", not "a redundant secondary gate".

**(Earlier framing, kept for the mechanism):** the EEPROM is **not** the *sole* root of the CONTACT
SERVICE halt — but there **is** a real EEPROM gate that I initially missed and then verified:

### EEPROM checksum gate on the contact-service present bit (`0x234588`/`0x234810`)

The contact-service handler `0x2346b2` computes a **16-bit byte-sum checksum over
`EEPROM[0x120..0x243]`** (routine `0x234588`, reading bytes via `0x2af97a`) and compares it
to the stored halfword at **`EEPROM[0x244..0x245]`**. On mismatch it clears bit 6 of the
service-present byte `0x11fed1` (`and #0xbf` at `0x234832`), which is the **batch-2 resume
gate** for task 0x14. So a checksum-valid `0x244` IS required for task 0x14 — but only as a
*secondary* gate: at runtime bit 6 is already cleared earlier (`0x2347b2`) because
`service_ready 0x110c2c`==0, so the checksum failure is currently redundant.

**Net:** building `EEPROM[0x244]` = `sum16(EEPROM[0x120..0x243])` is a small, well-defined
fix needed for task-14 batch-2, *after* the service-startup completion (`0x110c2c`) and the
MBUS ack (`0x11fedb`) are solved. The checksum algorithm is now known (byte-sum) and matches
the kind of recalculation NokiaTool performs. The rest of a full image remains a later
concern (RF/ADC/config consume EEPROM throughout t=0.006–0.40; phases past `94a2dc` may gate
on it).
