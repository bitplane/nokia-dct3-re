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

`CLOSED` here applies to the later ADC-monitor/classifier path only. A separate
boot battery reader at `0x2a84b0` calls `ccont_adc_read_2b52cc(0)` five times and
is classified by task-18 code at `0x270848`. `0x2b52cc` shifts its argument
directly into the CCONT ADC control byte, so this really selects hardware channel
0; it does not pass through the source map below. Replacing the current generic
channel-0 value with a battery-like value changes coherent boot from mode 4 to
an early mode-1 stall, so the 3210 channel routing cannot yet be adopted from
static evidence alone. The next validation must compare both paths and their
early consumers.

The classifier's VBAT sample is sourced from the CCONT ADC, not the D5/MBUS path
(that was a wrong guess; `0x2a594c` references ROM tables `0x2e14xx`, never the
battery record). End-to-end:

```
CCONT ADC channel 2 (battery voltage)
  -> adc_monitor_source_read_2b1bb2(source=7): maps source 7 -> CCONT selector via
     ROM byte-map 0x2e2d74 ([0x2e2d74+7] == 1 with MCU byte lanes), calls
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

ROM source->selector map @ `0x2e2d74` (sources 0..7, MCU byte order):
`0 4 5 6 7 3 2 1`. The earlier `4 0 6 5 3 7 1 2` transcription read the
swap16 analysis image linearly and was wrong. Runtime pairs source 7 with
selector 1 and source 6 with selector 2.
Calibration constants live in RAM floats `0x11fe14` (gain) / `0x11fe18` (offset),
seeded during startup (likely from EEPROM/NV — the selftest profile is suspect).

## Calibration and current interpretation

The current synthetic EEPROM leaves the live calibration at gain `1.0f` and
offset `0.0f`, so selector-1 ADC `0x200` produces sample `0x995` and battery band
1. The reference at `0x112306` is EEPROM-fed, with firmware default `0x233` when
the record is invalid. A real PMM capture is still required before assigning
physical voltage units or declaring which band represents a charged pack.

The shutdown boundary is independently measured. Selector 1=`0x1b0` powers the
handset off before task 1 starts. Selector 1=`0x1c0` remains valid, reaches mode
`0x0004`, and produces band 2 without code 7. Reporter caller `0x21e40c` is the
battery event-`0x25` branch under the literal firmware string "Check voltage
level for shutdown" and reports only below its voltage gate. The adjacent
caller `0x21f8de` belongs to the same low-voltage/charger lifecycle.

## Modeling status

- The source-to-selector map and classifier arithmetic are validated.
- The 3210 PCB signal names and real factory calibration values are not.
- The generic driver channel labels must not be promoted to a 3210 wiring claim.
- A real EEPROM/PMM capture remains useful for physical fidelity.
- Battery calibration is not the active code-7 hypothesis. Tuning ADC or NV
  values until report 7 appears would model a shutdown condition, not ordinary
  boot.

`0x2a41d0` is a checksum over the threshold table, not an NV reader. Symbol
`battery_adc_sample_counter_update_27d51c` is an instruction inside
`battery_vbat_moving_average_27d500`, not a separate function entry.
