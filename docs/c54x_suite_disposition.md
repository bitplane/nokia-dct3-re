# C54x suite reduction

This is the working disposition table, not a retention manifest. Existing
profile counts and green results do not justify retaining a family. Detailed
contract evidence belongs in `fixtures/c54x_test_reviews.json`; this table
sets the reduction order. Unreviewed families remain undecided.

| Family | Distinct contract / expectation basis | Overlap and disposition | Next action |
| --- | --- | --- | --- |
| Address-context modes / immediate DP | TI Category-I separation, DP addressing, explicit expected addresses | Keep standalone `mode-latency`; delete duplicate default adapters (done) | Renew reduced-default continuation at a broad checkpoint |
| FRAME / compiler SP consumers | Signed stack allocation and separated PSHM/POPM consumers; TI stack rules | Keep standalone `stack-sp`; delete duplicate default adapters (done) | No further wrapper rewrite |
| Register logic / shifts / NEG / ABS | Logical bit formulas, arithmetic boundaries and status effects | Existing generic builder is already shared; default logic/shift/NEG adapters duplicate standalone execution. Merge execution ownership; review numeric cross-products separately | Remove duplicate adapters together, then assess representative boundary coverage rather than preserve every Cartesian tuple |
| Legacy atomic instruction corpus | Atomic functional examples; distinct from staged pipeline interactions | Undecided. 5,525 lines in `tms320c54x_test_atomic_corpus.cpp`; size is a prioritisation signal, not grounds for deletion | Compare contracts with typed ISA examples; retain independent boundaries, delete overlapping sweeps |
| Legacy staged addressing | Address generation, mapped-register dependencies, bus ordering | Undecided. 10,551 lines in addressing module; multiple contracts, not one family | Split review by loads/stores, mapped moves and address-update interactions; avoid moving the entire module unchanged |
| Legacy staged arithmetic | Memory arithmetic, producer/consumer ordering, status effects | Undecided. 7,110 lines; register-only duplication is the first separable batch | Review memory forms against generic instruction expectations and bus-specific obligations |
| Repeat/control interactions | Interrupt/repeat ownership, delay-slot and save/restore behavior | Undecided; interaction coverage cannot be replaced solely by atomic arithmetic examples | Classify ownership boundaries before reducing operand permutations |
| Firmware integration | Private-ROM instruction sequences and Nokia context restoration | Separate from architectural conformance; retain only justified integration regressions | Do not include relocation in deletion measurements |

## Execution policy

- Initial investigation is limited to 15 minutes per ambiguous family. Record
  the concrete unresolved question rather than invent a retention rationale.
- Use focused native tests per batch; full default and handset gates are broad
  checkpoints, not prerequisites for every mechanical deletion.
- Full retained regression includes standalone profiles. Removing duplicated
  default execution must not silently remove those profiles from final testing.
- Report combined CPU and relocated-test source, execution duplication and
  measured runtime separately. Generated bulk is not a reduction.

The module sizes above were inspected at `843c4c8d`. They identify where to
look next, not immutable targets or semantic classifications.
