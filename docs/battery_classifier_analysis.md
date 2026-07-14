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
by selector number here rather than named BSI, RSSI, or VBAT.

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

`0x27cbec` returns state 1, 2, or 3 and `0x27dcfa` stores it at `0x110436`.
With the normal non-charging branch and ROM high thresholds `0x076c` and
`0x0910`:

| State | Primary averaged sample |
| --- | --- |
| 1 | at or above `0x0910` |
| 2 | `0x076c..0x090f` |
| 3 | below `0x076c` |

The current unity-gain channel-1 value `0x200` scales to approximately `0x0995`
and produces state 1. `0x2a6942` returns zero for states 1 and 2. For state 3 it
returns 1, or 3 when classifier flag byte `0x110438` bit 5 is set. Its mode-0d
caller accepts any nonzero result; its post-event-`0x74` caller requires result 3.

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
calibration tuning cannot be used to obtain report code 7: all safe constant-input
fixtures remain in states 1 or 2, while lower fixtures enter a genuine firmware
shutdown path. The bounded ADC sweeps observed no legitimate code-7 caller.

The unresolved hardware question is now narrower: a real phone must either present
different source-7 values across distinct electrical phases, select a different
documented battery-init lifecycle, or reach the startup handoff through context not
represented by the current constant ADC model. That behavior requires hardware or
captured-trace evidence before it is modeled.
