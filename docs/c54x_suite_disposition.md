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

### Unary review finding

The existing four unary profiles contain 28,704 numeric cases. Their earlier
reviews justified balancing replay points, but did not justify retaining the
entire numeric/status/page cross-product. Those preservation obligations are
not constraints on this reduction campaign.

NEG and ABS are the first numeric reduction target: keep all sixteen numeric
patterns across four source/destination selectors and OVM on/off; cover all
32 status combinations at representative overflow and zero boundaries rather
than for every pattern and page. Keep adjacent immediate producers for zero
and nonzero values. Program-page coverage belongs to fetch/control tests,
not every unary arithmetic tuple. Explicitly record the lost cross-product
coverage when implementing the replacement; matching opcode counts would
not establish equivalence.

SFTA/SFTL need a separate selection: SXM, saturation and incoming carry have
different relevance for right, zero and left shifts. Preserve all encoded
shift amounts and operand-width boundaries, but do not blindly preserve all
flag combinations for logical shifts. The current builders also seed ARP
nonzero in standard mode; correct those inputs against TI's ARP=0 rule rather
than preserving them as architectural witnesses.

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

## Atomic corpus: bounded inspection result

The 5,525-line module is one `check_atomic_corpus_results` function, not a
single coherent contract. It must be split by behavior before deletion:

| Contract | Evidence in current source | Disposition |
| --- | --- | --- |
| Port-bracket timing and control interactions | PORTR/PORTW counts and elapsed CPU cycles bracket BANZ, loads, XC, circular address updates and immediate repeats | Keep representative interactions. The generic numeric examples do not check these port observations. Do not discard the module as duplicate arithmetic. |
| Declarative functional examples | `begin_atomic_case` / `verify_atomic_case` reuse dual-address, register-bit, paired-memory, address, route, absolute-operand and shift example banks | Merge execution ownership with standalone ISA banks where independent reset/setup covers the same contract; first identify inherited-state inputs before bypassing legacy entries. |
| Immediate repeat count sweep | Phases6600..6855 execute all256 RPT immediates with the same MAR body and port-bracket formula | Rewrite to zero/one, signed-byte boundary and maximum-count representatives; retain setup-cycle and K+1 observations. Full immediate encoding enumeration has no distinct interaction rationale. |
| Status/XF side effects | Phases533..536/6547..6550 check status masks and XF publication counts | Keep separately from arithmetic result tests; output-pin behavior is a distinct observable. |
| Conditional annulment | Phases537..542/6543..6544 check accepted/rejected MAR, full status and port-bracket timing | Keep representative true/false and TC/accumulator predicates; do not equate register-result tests with annulled side effects. |

The first expected source reduction is removing duplicated declarative-bank
execution, not translating every handwritten phase into a new descriptor.
Port timing checks stay explicit unless a small shared fixture actually
removes complexity. The existing sequence inherits machine state between
cases, so deleting its setup without checking consumers would be unsafe.
