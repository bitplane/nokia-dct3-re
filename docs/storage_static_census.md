# Permanent-storage static census

Conservative direct-access results for the five supported ROM controls.
Dynamic pointers and table-driven accesses are outside coverage.

| ROM | EEPROMSelX seeds | executable | rejected data | PUP signal R/W | PUP direction R/W |
| --- | ---: | ---: | ---: | ---: | ---: |
| 3210-v6.00 | 4 | 0 | 0 | 0/0 | 1/1 |
| 3210-v5.01 | 4 | 0 | 0 | 0/0 | 1/1 |
| 3310-v6.39 | 2 | 0 | 1 | 28/26 | 28/27 |
| 3330-v4.50 | 2 | 0 | 1 | 28/26 | 28/27 |
| 3410-v5.46 | 3 | 0 | 0 | 33/30 | 32/32 |

Resolved PUP accesses identify wiring, not protocol. The absence proof covers
only direct literal-derived accesses to the former parallel alias.
