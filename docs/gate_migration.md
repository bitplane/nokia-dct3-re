# Acceptance-gate authoring

`gates.json` owns authored acceptance recipes; `tools/gate_generate.py`
renders them into `gates.mk`. Edit the data and run `make gates`; never edit
the generated rules. The Makefile owns build orchestration and shared runners.

`gates.mk` is included with `-include` and has a regeneration rule. GNU make
can remake included files and restart with either `include` or `-include`;
the latter suppresses a missing-file error if remaking cannot produce it.
It does not provide different regeneration semantics.

## Verification contracts

- `make gate-parity` checks extraction/rendering consistency, byte-for-byte
  regeneration and the cross-product parity audit. Its parser round trip is
  self-referential, not proof that GNU make interprets the rules identically.
- `tools/gate_database_diff.py --expand BEFORE AFTER` independently compares
  GNU make's rule databases: expanded recipes, prerequisites and phony status.
  Use it for changes intended to preserve recipe behavior. Also compare
  unexpanded commands when checking deliberate variable substitution.
- `docs/gate_parity_audit.md` is generated coverage of sibling differences.
  It owns current counts; do not freeze a second count in this guide.

For a before/after database comparison, use a separate worktree at the chosen
pre-change revision, then capture `make -pn --no-builtin-rules` in both trees.
Keep databases and worktrees under ignored repository `tmp/`. Run
`gate_database_diff.py --expand` on those captures. `make -n` is not an
equivalent oracle: recursive MAME builds obscure the relevant rule changes.

## Raw recipes and acceptance safety

Some gates retain shell control flow and are marked `# shell:` in generated
rules. Their families may be reported as not comparable. The report and
`gates.json`, rather than a historical migration total, define that scope.

When changing a gate, preserve or explicitly adjudicate:

- normalization prerequisites for every image it consumes;
- phony status, independent of files with matching target names;
- explicit product/profile arguments instead of a checker's default;
- fresh versus preserved storage and complete predecessor-state links;
- isolated logs and the runner's own capture responsibilities; and
- checkers, their arguments and the physical-input/restore acceptance order.

The one-time Makefile-to-data migration comparison is retained in Git.
Its durable result is the independent comparison method and the safety
conditions above, not a permanent list of differences between old revisions.
