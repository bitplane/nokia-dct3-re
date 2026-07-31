# Acceptance-gate migration

The acceptance gates moved out of the Makefile. They are authored in
`gates.json` and generated into `gates.mk`, which make includes and executes.

```text
gates.json   authored gate records, press profiles, product runs, shell guards
    |  make gates  (tools/gate_generate.py --from-data)
    v
gates.mk     generated rules; never edit by hand
    |  -include
    v
Makefile     build orchestration, variables, run helpers
```

`gates.mk` has a rule and is included with `-include`, so a missing or stale
copy is rebuilt and make restarts. A mandatory `include` could not do this: it
fails during parsing, before any rule could run.

## Verification

Two checks cover different things, and the distinction matters.

`make gate-parity` is **self-referential**. It extracts and renders with the
same parser, so it proves the generator is internally consistent — that typed
step records reproduce their own commands, and that `gates.json` regenerates
`gates.mk` byte for byte. It cannot prove that make agrees, because it never
asks make anything.

`tools/gate_database_diff.py` is **independent**. It reads `make -p`, make's own
rule database, and compares every target's recipe, prerequisites and phony
status between two revisions. With `--expand` it resolves make's variables from
make's own dump first, so naming a repeated literal registers as identical
rather than as a difference. This is what covers variable scope, include
placement and `.PHONY` membership — exactly the semantics the self-round-trip
cannot vouch for.

`make -n` is deliberately not used as the oracle: it recurses into the MAME
sub-build, whose parallel output ordering is not deterministic and swamps the
signal.

## Result of the one-time comparison

Parent revision `57761cc` (gates still in the Makefile) against the migrated
tree, using a `git worktree` checkout so both were measured the same way:

```sh
git worktree add --detach /tmp/parent 57761cc
( cd /tmp/parent && make -pn --no-builtin-rules > /tmp/db_parent.txt )
make -pn --no-builtin-rules > /tmp/db_migrated.txt
python3 tools/gate_database_diff.py --expand /tmp/db_parent.txt /tmp/db_migrated.txt
```

14 targets differ. Every one is an intended change or a measurement artifact;
none is an unintended consequence of the migration.

| Difference | Targets | Why |
| --- | --- | --- |
| Gained `normalize-3410` | 6 | Adjudicated defect. These gates consumed extracted ROMs without reaching the step that builds them, so they passed only when an earlier run had left the images behind. |
| Gained `normalize-6110` / `normalize-6110-v548` | 3 | Same defect for the 6110 static gates. `verify-6110-v548-static` needs the v5.48 step, not the v4.06 one. |
| Became phony | 3 | `verify-dct3-type-1f-static`, `verify-radio-a5-1-degraded` and `verify-vibrator` were absent from `.PHONY`. A stray file with one of those names would have made make consider the gate up to date and skip it. The generator marks every gate phony. |
| One fewer command | 1 | `verify-3410-radio-periodic-location-update` copied `error.log` after `run-captured`, which already performs that exact copy. |
| Gained `--radio-profile nhm2` | 1 | `verify-3410-radio-reselection-same-lac` relied on the checker's NSE-8 default while its sibling gates named their profile. Harmless today, because the checker treats `nhm2` and `nse8` alike on that path, but it would silently keep the wrong branch the moment an NHM-2 case is added. |
| Absolute path to `libgsm.a` | 1 | Measurement artifact. `verify-gsm-fr-codec` uses `$(abspath ...)`, which resolves to the worktree directory in the parent measurement. Not a migration difference. |

No gate lost a checker, changed a checker's arguments, or changed its run
parameters.

## Working on gates

- Edit `gates.json`, then `make gates`. Never edit `gates.mk`.
- `make gate-parity` must pass: round trip, render, regeneration and the
  cross-product parity audit.
- Re-run `gate_database_diff.py --expand` against the pre-change revision for
  any change intended to be behaviour-preserving. Compare expanded commands to
  confirm nothing changed, and unexpanded to confirm something did.

The remaining 22 gates carry their recipes verbatim because their shell control
flow is not modelled. They are marked `# shell:` in `gates.mk`, and they are why
13 families in `docs/gate_parity_audit.md` report "not comparable".
