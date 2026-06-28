# Battery classifier (0x27cbec) — decoded logic

Ghidra decompiles this function to `halt_baddata()` (wrong Thumb mode), so this was
recovered by capstone Thumb disassembly of `ghidra_out/3210f600a_swap16.bin` at
flash offset `0x7cbec` (VA `0x27cbec`), cross-checked against the live
`probe:battery_classifier_decision` register trace.

## Signature

`int battery_classify(r0=lo_sample, r1=vbat, r2=hi_sample)`  → returns r5 ∈ {1,2,3}

The three args are computed by the caller (~`0x27dcd7`, the `0x27dc..` battery-update
path) from the classifier record at `0x110434`. **They do NOT come from the live CCONT
ADC channel** — verified empirically: sweeping `NOKI3210_ADC2` across 0x100/0x2d0/0x380
leaves r1 fixed at `0x995`. The record is fed by the service-72 / D5 (MBUS) path.

`r4` = classifier record base (`0x110434`); `r3 = r4 + 0x50`.
`r7` = high threshold table (`hi[0..3]`), naturally `076c/0910/08b6/00a0` from the
selftest EEPROM. `00a0` is a hysteresis delta, not a 4th band.

## Decision (charging bit clear; byte[r4+4] bit0 == 0)

```
if (hi_sample >= hi[1]) -> state 1     ; 0x27cc16  cmp ip,hi[1]; bge 0x27cc3e
if (vbat       >= hi[1]) -> state 1    ; 0x27cc1a  cmp r1,hi[1]; bge 0x27cc3e
if (vbat       >= hi[0]) -> state 2    ; 0x27cc20  cmp r1,hi[0]; bge 0x27cc4a
else                     -> state 3    ; 0x27cc24  b 0x27cc4e
```

So with `hi = 076c/0910/08b6`:
- **state 1** = vbat at/above hi[1] (`0x910`)  → highest band (full)
- **state 2** = hi[0]..hi[1] (`0x076c..0x910`)
- **state 3** = below hi[0] (`0x076c`)          → lowest band (empty)

## Current natural result

vbat args = `lo=0x8f5, r1=0x995, hi=0x9ac`, all **above** hi[1]=`0x910`
→ classifier returns **state 1**.

The store at `0x27dcfa` writes this return into `FW_BATTERY_STATE` (`0x110436`).

## Why forcing "works"

`FORCE_BATTERY_CLASSIFIER_THRESHOLDS` rewrites hi to `0a00/0a80/0900`, i.e. *above*
the `~0x995` vbat, so vbat now falls below hi[0] → classifier returns state 3.
It does not change the battery; it moves the goalposts above the reading.

## The real question this exposes

state 3 is the **lowest** voltage band. A charged pack reading `0x995` (well above the
selftest thresholds) classifies as **state 1 = full**, which is physically correct.
But downstream boot (`0x2a6942`, post74) explicitly requires `FW_BATTERY_STATE == 3`.

Either:
1. The downstream "== 3" requirement was reached under other forcing and is itself an
   artifact (state 1 may be the correct charged result and boot should accept it); or
2. The selftest-EEPROM threshold table is wrong, and a real handset EEPROM has higher
   thresholds such that a charged-pack vbat naturally lands in band 3; or
3. The vbat transform that yields `0x995` from the modeled battery is wrong.

Resolving this is the prerequisite to deleting the battery forcing pokes faithfully.
Do NOT just bake `0a00/0a80/0900` into the model — that is the forcing in disguise.

## Complete causal chain (decoded from ROM, verified by trace)

1. CCONT ADC channel-2 is NOT the classifier input. Sweeping `NOKI3210_ADC2`
   0x100/0x2d0/0x380 leaves the judged VBAT fixed at `0x995`. The value comes from
   the battery record `0x110434`, fed by the service-72 / D5 (MBUS) path.
2. `0x27d500` = the VBAT transform: a **10-sample moving average** (running sum at
   `[0x110434+0x44]`, ÷10, result stored at `[0x110434+0x10]`). This is what produces
   `0x995`. So the classifier judges an *averaged* VBAT.
3. High thresholds are a firmware **ROM default**, `memcpy`'d by `0x27d56c` from
   ROM `0x2e1ff4` → `076c 0910 08b6 ...` into `0x110494`. NOT from EEPROM. The
   classifier's threshold pointer is a fixed literal (`0x110494`); there is no
   battery-type block selection — it always uses `076c/0910/08b6`.
4. Downstream gates genuinely require band 3:
   - `0x2a6942` (post74 ready): returns 3 only if band==3 AND flag bit5 of `0x110438`.
   - `0x21e0de` (state-0x28 dispatch): band 3 → `vbat_ok_path 0x21e1ae` (proceed);
     band 2 → fast-vbat countdown; band 1/0 → clears countdown and RE-ENTERS the
     VBAT worker loop. This is the stuck loop.

## Verdict

Band 3 (boot-proceed) requires averaged VBAT **< 0x076c (1900 units)**. The model
feeds ~`0x995` (2453), which is *above* every threshold → band 1 → stuck loop.
The fix is to make the modeled battery report a correct on-battery averaged VBAT
below `0x076c`, faithfully sourced — NOT to raise the thresholds.

## Full VBAT pipeline (CLOSED — capstone disasm + runtime trace)

The classifier's VBAT sample is sourced from the CCONT ADC, not the D5/MBUS path
(that was a wrong guess; `0x2a594c` references ROM tables `0x2e14xx`, never the
battery record). End-to-end:

```
CCONT ADC channel 2 (battery voltage)
  -> adc_monitor_source_read_2b1bb2(source=7): maps source 7 -> CCONT channel via
     ROM byte-map 0x2e2d74 ([0x2e2d74+7] == 2 == BATTERY_VOLTAGE), calls
     ccont_adc_read_2b52cc
  -> battery_classifier_input_update_27cc74: float-calibrate the raw ADC:
       v = adc * gain_f[0x11fe14] + offset_f[0x11fe18]   (soft-float 2b59e0/2b5446)
       sample = (int)v * 1500 / 313                      (0x05dc=1500, 0x139=313)
     writes two samples (~0x8f5, ~0x995) per tick
  -> running sums at record 0x11045c (+=0x995) / 0x110454 (+=0x8f5); counter 0x11043e
  -> battery_vbat_moving_average_27d500: sum/10 -> averaged VBAT 0x110444 (~0x9ac)
  -> battery_classifier_27cbec: compares averaged VBAT vs ROM thresholds
     076c/0910/08b6 -> band (1/2/3) -> 0x110436 -> downstream gates
```

ROM source->channel map @ `0x2e2d74` (sources 0..7): `4 0 6 5 3 7 1 2`.
Calibration constants live in RAM floats `0x11fe14` (gain) / `0x11fe18` (offset),
seeded during startup (likely from EEPROM/NV — the selftest profile is suspect).

## Modeling target (the faithful fix)

The VBAT the classifier judges is fully determined by:
  (a) the CCONT channel-2 battery-voltage ADC value (driver currently feeds 0x2d0
      in the "sane"/"charged" profile), and
  (b) the float calibration gain/offset at `0x11fe14`/`0x11fe18` (from NV).

To make band 3 emerge naturally, model a real charged-pack channel-2 value plus
correct calibration so `(int)(adc*gain+offset)*1500/313` lands in the intended
band, then delete the `FORCE_BATTERY_*` shims. OPEN PHYSICAL QUESTION: band 3 is
the *below-076c* band; confirm whether a real charged 3210 reads into band 3 here
(i.e. 076c/0910/08b6 are correct and our ADC/calibration is too high) before
choosing the value — otherwise we just build a prettier force. Capture the live
`0x11fe14`/`0x11fe18` calibration floats (via the `NOKI3210_TRACE_VBAT_PIPELINE`
probe, extended) as the next concrete step.

## CALIBRATION SOURCE + BAND REFRAME (measured)

The VBAT gain is data-driven, not hardcoded:
`gain = 563.0f / float(ref[0x112306])` (the 563.0 literal lives at the writer
`0x21c4be`). The reference `0x112306` (with `0x112304`/`0x112308`) IS EEPROM-fed:
`eeprom_i2c read 0x2af9a4` loads it, gets `0xFF` (blank image), then `0x2a66ac`
writes the firmware **default `0x233` = 563** because the record is invalid. So
`gain = 563/563 = exactly 1.0`.

Key implication: a real tuned EEPROM's reference would still be ~563 (factory ADC
calibration is a small +/-% correction, gain ~1.0 -- a 2x correction to reach the
~0.5 gain that band 3 needs is not physically plausible). So **even a faithful
tuned EEPROM gives gain ~1.0, and a charged pack still reads band 1** (measured
`0x995` -> band 1). The firmware's own uncalibrated default (gain 1.0) yields band
1 for the live battery -- i.e. **band 1 is the normal operating state**.

Therefore the earlier "band 3 = charged" reading is WRONG. Band 3 is the
low/critical band, and the boot's apparent "needs band 3" requirement (met today
via `FORCE_POST74_BATTERY_READY` -> `0x2a6942` returns 3) is most likely an
ARTIFACT: a real phone boots in band 1, and that gate is either off the real path
or the force is masking an unrelated blocker. The decisive test: run with a
natural band-1 battery and `FORCE_POST74_BATTERY_READY` removed, and see whether
the boot still advances via a different branch.

Consequence for the "synthesize a tuned EEPROM" plan: it will NOT change the band
(tuned ~= default here), so it is not the boot fix. Re-evaluate before building it.

### CONFIRMED by experiment

Removed `FORCE_POST74_BATTERY_READY` (boot-progress now defaults it OFF) and ran:
the boot reaches the **identical** end state -- same `ccont=0x0600`, `mode=0x000d`,
80 task5 dispatch hits, and a **byte-identical LCD frame** (`94a2dc...`) -- with
the live battery in band 1. So the post74 battery gate was a no-op in this profile
and is retired. The battery measurement subsystem is fully understood AND proven
to be a red herring for the boot. The real blocker is unchanged: the task5
display-capability gate `0x0199` (needs `[0x11fc80+2]==4`, `[+5]==0`).

## BOX-OFF SUMMARY (battery VBAT measurement subsystem)

Status: fully mapped and root-caused. The band-3 "paradox" is resolved by the
calibration. Remaining work is one concrete model: load real VBAT calibration
from NV instead of leaving it identity.

- **Pipeline** (capstone + runtime trace, Ghidra decompile is corrupt here):
  CCONT ADC -> `adc_monitor_source_read_2b1bb2(src 7)` -> float calibrate in
  `battery_classifier_input_update_27cc74`:
  `sample = (int)(adc*gain[0x11fe14] + offset[0x11fe18]) * 1500/313` ->
  10-sample average `battery_vbat_moving_average_27d500` ->
  `battery_classifier_27cbec` vs ROM thresholds `076c/0910/08b6` -> band.
- **Measured calibration:** gain = `1.0f` (identity), offset = `0.0f`. So
  `sample = adc * 4.79`. Observed `sample = 0x995` from `adc = 0x200`.
- **Band logic:** band3 = `sample < 0x076c (1900)`; band1 = `sample >= 0x910 (2320)`.
- **Band-3 resolution (arithmetic):** a charged pack on CCONT ch2 (`0x2d0`=720)
  gives `720*4.79 = 3450` -> band1 with the identity gain. With the *real* NV gain
  (~<0.55) it gives `720*0.5*4.79 ~= 1725 < 1900` -> **band3**. So band3 IS the
  charged/normal state; the identity gain makes every reading ~2x too high and
  pins band1 (the "stuck VBAT loop"). The boot's band-3 requirement is correct;
  our uninitialized calibration is the bug.
- **Validation attempt:** overriding the gain float at `0x11fe14` via a RAM-read
  shim destabilises battery init (ch2 is never read, the classifier never runs).
  So `0x11fe14` is read *early* in battery init for more than the gain, and the
  calibration cannot be fixed with a RAM poke -- it must be fixed at the NV/EEPROM
  load that populates it (consistent with "model real hardware, not force RAM").

### Faithful fix to delete the FORCE_BATTERY_* shims (one model)

Make the startup NV/PMM calibration read populate `0x11fe14`/`0x11fe18` (and the
ADC-monitor battery source) with real values from a correct EEPROM image, so a
charged pack flows `ADC -> real gain -> band3` naturally. The exact gain/offset
come from a real handset EEPROM (the selftest profile leaves them identity).
Diagnostics retained: `NOKI3210_TRACE_VBAT_PIPELINE` (`probe:vbat_calib`,
`probe:vbat_rec_write`).

Functions newly identified: `adc_monitor_source_read_2b1bb2`,
`battery_vbat_float_calibrate` path in `battery_classifier_input_update_27cc74`,
ROM data `battery_source_channel_map_2e2d74`.

## ROOT CAUSE (measured live, gain/offset + sample)

The battery-voltage measurement path is **uninitialized**, not mis-tuned:

- Calibration floats are identity: `gain[0x11fe14] = 1.0f` (0x3f800000),
  `offset[0x11fe18] = 0.0f`. No PMM/EEPROM calibration is applied. (The NV reader
  `startup_nv_calibration_read_29bbd0` is the suspected loader — verify it runs and
  what it reads.)
- So `sample = (int)(adc*1.0 + 0.0) * 1500/313 = adc * 4.79`.
- Measured `sample = 0x995 = 2453`, which back-solves to `adc = 0x200` (512) — the
  **mid-scale ADC default**, NOT the battery-voltage channel's `0x2d0`. The
  source-7 read returns a 0x200 channel (RSSI/temp-like, ch1/ch4), so the ADC
  monitor ring's battery source is delivering a default, not real VBAT.

Net: VBAT = (mid-scale-default) x (identity-calibration) x 4.79 = a meaningless
`0x995` that lands in band 1. The `FORCE_BATTERY_*` shims were masking an
uninitialized measurement subsystem.

### Faithful fix (revised)

Not "pick a channel-2 value". Initialize the measurement subsystem:
1. Make `startup_nv_calibration_read_29bbd0` load real VBAT gain/offset into
   `0x11fe14`/`0x11fe18` (from a correct EEPROM/PMM image, or sane defaults).
2. Ensure the ADC-monitor battery source actually samples CCONT channel 2 (real
   VBAT), not the mid-scale default.
Then a real pack reading flows through real calibration to a real band, and the
`FORCE_BATTERY_*` shims can be deleted. Charger latch `FW_CCONT_CHARGER_EVENT`
reads `0x0001` during boot — consistent with the firmware not establishing a clean
battery start (the charging/limited path the LCD may be stuck on).

Note: `0x2a41d0` is `contact_service_checksum` (a checksum over the threshold table
in `0x27d56c`), NOT an NV reader — earlier guess corrected.

Note: the existing Target `battery_adc_sample_counter_update_27d51c` sits at an
instruction *inside* `battery_vbat_moving_average_27d500` (the `strb [r4,#0xb]`
ring-index update), not at a separate function entry.
