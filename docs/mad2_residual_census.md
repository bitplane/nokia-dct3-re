# MAD2 residual census

Conservative literal-seeded Thumb analysis of the remaining CTSI surfaces.
The 32-instruction seed span deliberately excludes distant pointer guesses;
dynamic and table-driven accesses remain outside coverage.

| ROM | reset R/W | DSP reset R/W | watchdog R/W | clock R/W | ext status R/W | FIQ8 R/W |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 3210-v6.00 | 4/3 | 5/5 | 0/3 | 10/10 | 5/0 | 7/8 |
| 3210-v5.01 | 4/3 | 6/6 | 0/3 | 10/10 | 5/0 | 7/8 |
| 3310-v6.39 | 4/3 | 6/6 | 0/3 | 10/10 | 6/0 | 10/5 |
| 3330-v4.50 | 4/3 | 6/6 | 0/3 | 10/10 | 6/0 | 8/5 |
| 3410-v5.46 | 4/4 | 17/13 | 0/3 | 10/10 | 6/0 | 10/5 |

## Conclusions

- The watchdog register is write-only in all five images (three resolved writes each).
- The external-status bank is read-only in all five images (five or six resolved reads, zero writes).
- Every image has ten clock-control reads and ten matching writes. The paired 3210 decode
  assigns boot bits 2-3, SIMI bits 5-6 and the one-shot ARM-stop bit 1; the census finds
  no additional product branch that can identify the remaining physical consumers.
- Every image contains the FIQ8 control family. Firmware semantics identify it as the
  100 Hz centisecond source, but static code cannot establish oscillator behavior during sleep.
- No emulated component asserts ninth IRQ line 8. Firmware proves its status/acknowledge
  register contract, but its physical owner is not recoverable from these access sites.

The unresolved items now require a MAD2 data sheet, physical timing/logic capture, or an
organic product lifecycle that exercises the relevant input. They are not safe targets
for calibrated implementation.
