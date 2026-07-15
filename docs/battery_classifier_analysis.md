# Nokia 3210 battery ADC and classifier contract

This note records the current 3210 v6.00 firmware contract. It intentionally omits
the earlier service-72 and generic-channel-label hypotheses that runtime tracing
disproved.

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
0 through 7. Runtime tracing confirms source 7 selects physical channel 1. The
generic driver labels are not a proved 3210 PCB netlist, so channel 1 is described
by selector number here rather than named BSI, RSSI, or VBAT. A sibling emulator
maps it to BSI, but its own 3210 profile labels that mapping and its pack values as
pending reverse engineering. That is useful corroboration, not independent proof.

The calibration/scaling performed by `0x27cc74` is:

```
calibrated = int(raw_adc * gain + offset)
sample = floor(calibrated * 1500 / 313)
```

`gain` is the float at `0x11fe14`; the active path sets `offset` at `0x11fe18`
to zero. A second logical sample can enter through source 8, but the cold-boot
safety average and the classifier's primary voltage sample both use source 7.

The separate early power-on check at `0x2a84b0` directly reads physical channel
0. Its accepted input window does not establish channel 0's physical signal name,
and it must not be conflated with the source-7 path.

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
classifier publication. The ordering audit in `mmi_settlement.md` shows
that the current contact-peer session releases the checklist group much later.
Both routes then continue into equivalent interactive-initialization tails, so
the classifier's mode choice does not gate ordinary UI initialization.

## Pack-characterisation audit

The proposed pair-recognition boundary at `0x2a68c4` was audited exhaustively.
It has 11 direct callers:

```
21c796 21cfac 21d67c 21ea3c 21efbe 220584
2205f6 220cca 220d94 230b16 23579e
```

The function does not interpolate a `(selector 1, selector 4)` pair. Its complete
data path is:

```
selector 1 raw
  -> 0x2b64fc (signed integer to software float)
  -> 0x2b59e0 (divide by [0x11fde0 + 0x34])
  -> 0x2b5446 (multiply by [0x11fde0 + 0x38])
  -> 0x2b63cc (software float to integer)
```

Thus `0x2a68c4` is an affine calibrated selector-1 reader. The two structure
fields are coefficients, not axes or table pointers. There is no selector-4
read, lookup table, loop, piecewise interval, or recognition result.

Selector 4 is handled separately by `0x2b4f2c`, which has five direct callers
at `0x220576`, `0x22071e`, `0x22087e`, `0x220da6`, and `0x2a6734`. Its complete
decision is a single threshold:

| Raw selector 4 | Stored `0x11fde0+0x66` | Init mode at `+0x72` |
| --- | --- | --- |
| `< 39` | `0x0141` | 4 |
| `>= 39` | `0x04fb` | 1 |

The subsequent range test merely recognizes the stored sentinel interval
`0x0140..0x0171`, so only the `< 39` branch selects mode 4. This supports a
temperature-like interpretation for selector 4 from its surrounding charging
uses, but it does not implement pair recognition. The electrical name still
requires a 3210 schematic or measured hardware trace.

The initialization function at `0x27dd30` copies that mode to `0x11043d`. Modes
1 and 5 load one calibration record; modes 2 and 6 load two. The cold-boot guard
`0x27d5fc` is called for every nonzero mode except mode 4, so modes 1, 2, 5, and
6 consume five selector-1 samples and enforce the `0x0834` floor. Modes 0 and 4
skip the guard. Mode 4 is therefore the only selector-4-selected lifecycle in
which a constant selector-1 value can reach state 3 without first failing that
guard.

The independent selector-4 consumer at `0x2a90ac` supplies the direction check:

| Raw selector 4 | State `0x112448` | Event producer |
| --- | --- | --- |
| `< 26` | 0 | calls `0x2a689c`, posting task-19 event `0x44` |
| `>= 26` | 2 | no event-`0x44` post on this branch |

Both consumers therefore identify low selector 4 as the non-fault direction:
below 26 both selects mode 4 and clears the independent charge/fault state. This
corrects the earlier description of event `0x44` itself as abnormal. It is a
healthy result for that consumer, but it is not sufficient to complete the
separate mode-4 startup lifecycle.

Event `0x43` does not close the missing link. The only recovered direct poster
is `0x2a6880`, selected by command-handler arm 3 at `0x23587e`; the audited
calibration and selector-4 functions do not call it. The earlier low-selector-4
fixture reached event `0x44`, confirming that the healthy charge-state result and
the mode-4 completion event are distinct contracts.

### Bounded result

Coverage is complete for the proposed boundary: all 11 direct callers of the
selector-1 reader, all five direct callers of the selector-4 decision, the sole
classifier caller, and the sole recovered event-`0x43` poster were classified.
No ROM table or two-input recognition predicate exists in that surface. There is
therefore no unique evidence-backed `(selector 1, selector 4)` ordinary-pack
fixture to run. Choosing values on that premise would be the prohibited input
sweep under a different name. The separate lifecycle audit below later justified
one fixture from two independently decoded scalar thresholds; it does not revive
the disproven pair-recognition premise.

## Mode-4 state-3 fixture

The wider lifecycle audit subsequently supplied one unique, non-swept fixture:
selector 4 `0x14` is below both independently decoded healthy thresholds, and
selector 1 `0x180` scales to 1840 at the ROM-default unity calibration, below the
state-3 boundary 1900. Selector 0 and all unrelated channels remained at the
coherent frontier profile; selector 5 remained zero for no charger.

The eight-second run in `run_battery_mode4_state3` selected init mode 4 at
`t=0.200674` and did not power off, proving the guard was skipped. It did not
initialize the monitor or reach state 3: no `0x110436` transition or terminal-report caller
occurred, task 1 remained in mode `0x000d`, and its checklist stopped at `0x0b`
rather than the coherent frontier's `0x0f`. SIM initialization consequently did
not begin. This is not an ADC-range failure; mode 4 remains waiting on its distinct
event-`0x43` completion while the independently healthy selector-4 consumer posts
event `0x44`.

The fixture therefore falsifies the remaining constant-input escape hatch. A
backward census then closes the apparent event-`0x43` boundary: `0x2a6880` is
called only by payload selector 3 of incoming class-`0x40` service command `0x8e`.
Task 2 dispatches that command to `0x235848`; the handler's sole MCU construction
is a one-byte `0x8e` acknowledgement at `0x2358a0`, after the service action has
already run. The ordinary firmware therefore does not initiate this transition.

Command `0x8e` belongs to the external service/test peer contract, is absent from
the coherent contact manifests, and must not be added to the normal boot peer to
advance startup. Mode 4 is consequently a service-controlled battery lifecycle,
not an evidenced ordinary boot branch. The battery/event-`0x43` route is closed.

## Cold-boot safety contradiction

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

This falsifies the pre-registered prediction that a constant, faithfully calibrated
pack input can both survive cold boot and organically supply state 3. It also means
calibration tuning cannot supply the shutdown-level result: all safe
constant-input fixtures remain in states 1 or 2, while lower fixtures enter a
genuine firmware shutdown path. It
does not make the proper mode-0d advance impossible: unclassified state 0 is a
separate accepted input to `0x2a6942`.

The unresolved question is now narrower: why the emulated readiness checklist
completes only after the first state-1 publication, and whether real hardware
ordinarily reaches the same gate while state 0 is still current. This is an
ordering/lifecycle question, not evidence for changing ADC values.
