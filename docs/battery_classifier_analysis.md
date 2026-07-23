# Nokia 3210 battery ADC and classifier contract

This is the detailed 3210 v6.00 firmware map for the battery ADC and classifier.
The ordinary-boot investigation is closed: current inputs select a safe monitor
state and do not block the interactive UI. Remaining work is physical: identify
selector nets and units from board evidence, then model battery dynamics only
when an organic application or charging path requires them.

## Signal path

The later battery worker obtains its primary sample from ADC-monitor source 7:

```
source 7
  -> source-to-selector table 0x2e2d74
  -> physical CCONT ADC selector 1
  -> 0x27cc74 calibration and scaling
  -> 10-sample moving average
  -> 0x27cbec classifier
  -> state byte 0x110436
```

The swap16-correct source-to-selector table is `0 4 5 6 7 3 2 1` for sources
0 through 7. Runtime tracing confirms source 7 selects physical channel 1.
Channel 1 is a second VBATT input: it uses the same voltage-calibration fields
as the direct battery path, scales the result by `1500/313`, and applies a
2100-unit shutdown floor. Channel 3 is consumed separately by the battery-size
input reader. This corrects the former BSI hypothesis for channel 1.

The calibration/scaling performed by `0x27cc74` is:

```
calibrated = int(raw_adc * gain + offset)
sample = floor(calibrated * 1500 / 313)
```

`gain` is the float at `0x11fe14`; the active path sets `offset` at `0x11fe18`
to zero. A second logical sample can enter through source 8, but the cold-boot
safety average and the classifier's primary voltage sample both use source 7.

The separate early power-on check at `0x2a84b0` directly reads physical channel
0 and task 18 accepts raw `0x02be..0x0314`; this is the other 3210 VBATT path.
The product profile therefore routes one stable battery input to channels 0 and
1 while retaining independent BSI, BTEMP and VCHAR inputs on channels 3, 4 and 5.

## Calibration records

Firmware `0x2a665e` resolves three EEPROM descriptors through `0x2abbf4`:

| Descriptor | EEPROM bytes | RAM destination | Length |
| --- | --- | --- | --- |
| 2 | `0x0248..0x024f` | `0x112304` | 8 |
| 3 | `0x0250..0x0253` | `0x1122f8` | 4 |
| 4 | `0x0254..0x025b` | `0x1122fc` | 8 |

The source-7 gain denominator is descriptor-2 field `0x112306`. Firmware validates
it in the inclusive range `0x01a1..0x0304`; invalid data falls back to `0x0233`.
The resulting gain is `563.0 / denominator`, so the fallback is exactly `1.0`.
The other descriptor-2 fields and their firmware fallbacks are:

| RAM field | Accepted range | Fallback |
| --- | --- | --- |
| `0x112304` | `0x0162..0x01df` | `0x0233` |
| `0x112306` | `0x01a1..0x0304` | `0x0233` |
| `0x112308` | `0x0118..0x0168` | `0x0141` |
| `0x11230a` | `0x01ce..0x034f` | `0x0277` |

Both collected 3210 EEPROM images have erased `0xffff` bytes across
`0x0248..0x025b`. They therefore exercise the ROM fallbacks and are not evidence
of handset-specific factory calibration. A populated, legitimately obtained
3210 EEPROM remains necessary to attach physical units to these coefficients.

## Classifier

`0x27cbec` has exactly one direct caller, at `0x27dcd2`. It returns state 1,
2, or 3 and `0x27dcfa` stores it at `0x110436`.
With the normal non-charging branch and ROM high thresholds `0x076c` and
`0x0910`:

| State | Primary averaged sample |
| --- | --- |
| 1 | at or above `0x0910` |
| 2 | `0x076c..0x090f` |
| 3 | below `0x076c` |

The current unity-gain channel-1 value `0x200` scales to approximately `0x0995`
and produces state 1. Exhaustive decode of `0x2a6942` gives the complete unsigned
mapping:

| Monitor state | Return |
| --- | --- |
| 0 | 2 |
| 1 or 2 | 0 |
| 3 | 1, or 3 when flag byte `0x110438` bit 5 is set |
| any other byte | 2 |

Its mode-0d caller accepts any nonzero result; its post-event-`0x74` caller
requires result 3. The previous shorthand that the alternate advance required
state 3 was therefore false: pre-classification state 0 is explicitly accepted.

These are scalar monitor states, not a recovered pack-type enumeration. The
classifier accepts two already-calibrated monitor samples and compares them with
threshold fields in the battery-monitor structure. It never reads selector 4 or
the battery-characterisation structure at `0x11fde0`. Calling state 3 a
"recognized pack class" is therefore unsupported by the recovered code.

The task-19 consumer gives the states their lifecycle meaning:

| State | Task-19 behavior |
| --- | --- |
| 1 | High/ordinary monitor region; clears transition flags 2 and 3 and resets the low-region counter. |
| 2 | Intermediate region; runs a bounded countdown/hysteresis path before changing power policy. |
| 3 | Low region; sets transition flag 2, arms the low-region timer/status path, and may reach the shutdown-level reporter. |
| 4 | Defensive task-19 case; no recovered initialized-state writer produces it. |

The task-1 terminal-report call at `0x21e40c` is inside the shutdown-level
branch. It is skipped when `0x27cd82` returns 1 (sample above the shutdown
predicate), confirming that state 3 is a low-monitor lifecycle rather than an
ordinary pack identity.

### Classifier flag ownership

The complete literal-reference census for byte `0x110438` contains 12 sites:
ten in task 19 and two readers in the battery-monitor module. The battery-init
function zeroes it through structure base `0x110434` at `0x27ddf6`. Task 19 then
sets or clears bits 0, 1, 2, 3, 4, and 6; no recovered instruction sets bit 5.
The two module-side references at `0x27cd92` and `0x27d1dc` are reads.

Consequently bit 5 has no in-ROM owner after initialization. `0x2a6942` returns
1 for state 3 when bit 5 is clear and 3 if it is set. The mode-0d caller accepts
that result, state 0's return 2, and the defensive default return 2, so bit 5 is
not its missing prerequisite. Return value 3 is only required by a separate
post-event-`0x74` path and is unreachable from the recovered MCU writers after
this initialization.

### Startup ordering implication

The initialized-state writer census is closed. `0x27ddfa` clears `0x110436` to
state 0; `0x27dcfa` is the only subsequent store and writes the sole classifier's
state 1, 2, or 3 result. The direct reference in task 19 at `0x21e0de` is a read.
No recovered firmware path writes state 4.

The coherent emulator reaches the mode-0d gate after the monitor has published
state 1, so `0x2a6942` returns zero and takes the observed mode-4 route. The
accepted state-0 path makes the real boot contract non-contradictory without
requiring an unsafe low sample, but it is not established as the ordinary route:
it is selected only if the readiness checklist finishes before the first
classifier publication. The ordering audit summarized in `mmi_layer.md` shows
that the current contact-peer session releases the checklist group much later.
Both routes then continue into equivalent interactive-initialization tails, so
the classifier's mode choice does not gate ordinary UI initialization.

## Battery-initialization modes

The complete caller audit disproves a two-input pack-recognition table. Function
`0x2a68c4` has 11 direct callers and is only an affine selector-1 reader:

```
21c796 21cfac 21d67c 21ea3c 21efbe 220584
2205f6 220cca 220d94 230b16 23579e
```

```
raw -> signed-to-float -> divide by [0x11fde0+0x34]
    -> multiply by [0x11fde0+0x38] -> float-to-integer
```

Its structure fields are coefficients, not table axes. Selector 4 is consumed
separately by `0x2b4f2c`; its five direct callers are `0x220576`, `0x22071e`,
`0x22087e`, `0x220da6`, and `0x2a6734`:

| Raw selector 4 | Stored `0x11fde0+0x66` | Init mode at `+0x72` |
| --- | --- | --- |
| `< 39` | `0x0141` | 4 |
| `>= 39` | `0x04fb` | 1 |

Initialization at `0x27dd30` copies the mode to `0x11043d`. Modes 1, 2, 5,
and 6 run cold-boot guard `0x27d5fc`; modes 0 and 4 skip it. An independent
selector-4 consumer at `0x2a90ac` treats values below 26 as its non-fault path,
clears state `0x112448`, and posts task-19 event `0x44`. This supports a
temperature-like interpretation but does not prove the PCB net name.

The independent selector-3 read at `0x2a90b4`, together with the standard
DCT3 channel assignment, identifies it as BSI with moderate confidence.
Selector 4 is correspondingly retained as BTEMP with moderate confidence.
Selectors 2, 6 and 7 remain unnamed because their recovered consumers do not
establish a board-level electrical quantity.

Mode 4 has a distinct, external completion contract. Its event `0x43` has one
recovered poster, `0x2a6880`, selected only by payload 3 of incoming class-`0x40`
service command `0x8e`. The sole MCU construction of `0x8e` is the reply after
that service action. A threshold-derived mode-4 fixture reached healthy event
`0x44` but remained at checklist `0x0b`, with no monitor initialization or SIM
start. Command `0x8e` is absent from coherent manifests and must not be supplied
during normal boot. Thus mode 4 is a service-controlled battery lifecycle, not
an ordinary-pack escape path.

Coverage includes every direct caller of the selector-1 reader and selector-4
decision, the sole classifier caller, and the sole event-`0x43` poster. No
two-input recognition predicate exists in this surface; external interpretations
of it as a `(BSI, BTEMP)` interpolation table must not be imported as premises.

## Cold-boot safety contract

Ordinary battery init mode 1 calls `0x27d5fc`. It takes five source-7 samples,
averages the primary scaled result, and powers the handset off when the average is
below `0x0834`. Mode 1 is selected organically from the physical-channel-4
temperature/pack check at `0x2b4f2c`; selecting its special mode 4 merely to skip
this safety check is not a faithful correction.

For a stable source-7 input, the two requirements cannot overlap:

```
cold boot survives: scaled sample >= 0x0834 (2100)
classifier state 3: scaled sample <  0x076c (1900)
```

At unity gain and zero offset this predicts a minimum safe raw value of `0x01b7`
and a maximum state-3 raw value of `0x018c`, leaving a 43-count raw gap. Coherent
runs confirm the exact safety edge: `0x01b6` never starts task 1 and powers off
after the retry watchdog; `0x01b7` reaches startup mode `0x000d`. Changing a valid
gain or common offset cannot remove the ordered 200-unit gap because both checks
consume the same calibrated/scaled source.

This falsifies the hypothesis that a constant, faithfully calibrated
pack input can both survive cold boot and organically supply state 3. It also means
calibration tuning cannot supply the shutdown-level result: all safe
constant-input fixtures remain in states 1 or 2, while lower fixtures enter a
genuine firmware shutdown path. Unclassified state 0 remains a separate
accepted input to `0x2a6942`. However, the mode-`0x000d` consumer audit proves
that this result selects a continuation; it is not a cold-boot completion gate.
The coherent phone remains interactive after the ordinary state-1 result
selects mode `0x0004`. Battery inputs must therefore be modeled from their
physical contract, not tuned to select state 0 or state 3 during boot.
