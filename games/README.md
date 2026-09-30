# Games extraction workspace (not upstream work)

Everything under `games/` is scratch tooling for locating and extracting the
Nokia 3210 v6.00 Snake game for a GBA wrapper. It builds on the upstream
tools (`tools/disrom.py`, `ghidra/symbols/3210.csv`, the MAME Lua harness) but
is not part of the upstream MAME driver project. `games/run_*` dirs are
ignored by the repo's `run_*/` rule.

## Tooling (user-level, outside the repo)

| What | Where |
|---|---|
| Ghidra 12.1.4 | `~/ghidra/ghidra_12.1.4_PUBLIC` (symlink `~/ghidra/current`); `analyzeHeadless` and `ghidraRun` linked into `/opt/homebrew/bin` |
| JDK 21 | Homebrew `openjdk@21`; Ghidra points at it via `support/launch.properties` `JAVA_HOME_OVERRIDE` |
| Ghidra natives | built with `support/gradle/gradlew buildNatives` (public release ships no macOS binaries) |
| Ghidra project | `~/ghidra/projects/nokia3210`, program `3210f600a_swap16.bin` (base `0x200000`, `ARM:LE:32:v4t`), symbols imported from `ghidra/symbols/3210.csv` |
| Python venv | repo `.venv` (`make venv`), capstone for `tools/disrom.py` |

Headless examples:

```sh
analyzeHeadless ~/ghidra/projects nokia3210 -process 3210f600a_swap16.bin -noanalysis \
  -scriptPath ghidra/scripts -postScript ExportFunctionsByAddress.java out.c 0x240d82
```

## Scripts

- `tools/thumb_entries.py` — candidate function entries (BL targets, Thumb pointers, symbols, `push {lr}`) -> `data/entry_candidates.txt` (6813).
- `tools/nav_run.sh RUN_DIR "keys" SECONDS [ENV=..]` — headless 3210 run with scripted keys; `x` is a no-op slot. Env: `DELAY_MS` (use 12000, the UI is not ready before ~12 s), `DUR_MS`, `GAP_MS`, `CAP_MS`, `RUN_EXTRA_ARGS`.
- `mame/coverage.lua` — autoboot wrapper around the upstream harness; breakpoints on every candidate entry log `COV <pc>` to `error.log` once per phase (`SNAKE_COV_PHASES=name:sec,...`), optional raw PC trace window `SNAKE_TRACE=start:stop:file`. Needs `RUN_EXTRA_ARGS="-autoboot_script ../games/mame/coverage.lua -debug -debugger none"`.
- `tools/cov_diff.py LOG --new PHASES [--against PHASES]` — functions first seen in given phases.
- `tools/frame_sheet.py RUN_DIR [out.png] [scale] [from:to]`, `tools/pgm2png.py` — view the LCD mirrors.
- `ghidra/ImportSymbolsCsv.java`, `ghidra/CountStats.java` — headless helpers.

## Navigation (3210 v6.00, provisioned profile)

Main menu: Phone book, Messages, Call register, Settings, Call divert, Games (6),
Calculator, Clock, Tones, Net monitor. Digit shortcuts work after `Menu`.
Games: 6-1 Rotation, 6-2 Snake, 6-3 Memory. Snake menu: New game, Top score,
Instructions (and more below). Harness key names: `enter` (Navi/Menu/Select),
`c` (back), `up`, `down`, digits.

## Findings so far

Coverage runs (`run_cov2` Snake, `run_ctrl_memory` Memory control, identical
timelines) isolate 20 functions entered only in the Snake timeline. The
gameplay core cluster, all first entered when "New game" is selected:

```
00240604 00240610 00240d82 00241198 002413dc 002414dc 00241620 00243170
```

plus helpers `00296b90 0029a2a0 002a7556 002a7624 002a7734 002b328a 002b5df8`
(`2a7734` fires on each key press) and a game-over group `00262218 0026225e
00263468 0028c9f4 0029e94e 002a07b2 002a1df4` at about 3 s.

`00240d82` decompiles as the Snake event handler: case `0x49` initializes state,
`0x54` is the move/eat tick (calls `2413dc`, `241620`, `2414dc`, re-arms itself
with `sched_post_event_delay_2697aa(0x30, delay)` where the delay derives from a
speed table via 64-bit math in `2b63fc`/`2b49b8`), `0x57` draws via `241198`.
Decompiled text: `data/play_cluster.c`.

**Correction:** the tick does fire. The snake was moving so fast it hit the
wall within the first fraction of a second (see below), which is why the
later trace window only saw the idle loop. The earlier sentence is kept for
history but is wrong:

**(superseded)** in MAME the tick never fires. A raw PC trace of 19.7–20.2 s
(`run_trace/play.tr`) shows 96% of instructions in the task 0 idle loop and
no re-entry into the cluster; the LCD mirror shows the snake never moves and
"Game over, score 0" appears after ~3 s. Likely a scheduler delay/timer
fidelity issue upstream (delay event `0x30`), or the computed delay is wrong.
Static analysis of the cluster does not depend on this; dynamic tracing of
movement does.

## Driver fix found on the way (branch `fix/lcd-reset-alignment`)

MAME's own window showed a white LCD while the Lua mirror showed real frames.
Cause: device reset order (PCD8544 before GENSIO) left one stray SCLK edge in
the LCD shift register, so every serial byte was one bit misaligned (`0x24`
decoded as `0x12`). Fix: `m_lcd->reset()` at the end of `machine_reset()`.
Also: the MAME checkout had an older `mame-tms320c54x-test.patch` applied;
`npe3verify` was added to `mame/src/mame/mame.lst` by hand so the overlay's
already-applied check passes.

## Provisioning fix (branch `fix/games-nv-provisioning`)

Game settings live in NV descriptor `0x074c` -> EEPROM `0x0d9c`, four bytes per
game, five games: top score (BE u16), level index 0..8, one unused byte. Loader
`0x29a0e2` copies them into RAM records at `0x11040c` (8 bytes per game,
`+0` current score, `+2` top score, `+4` level). The Snake tick delay is
`10 * speed_table[0x2d9738][level] / 7.78125` scheduler ticks; the table is
`66 48 38 30 23 18 14 11 9`. The generated EEPROM left the record erased, so
level `0xff` indexed speed 0: instant wall death and a stuck Level selector.
`make_eeprom_profile.py` now provisions top score 0 and level index 0.
Verified headlessly: `games/run_play1` shows the snake steering and surviving.

## Game plugin table

`game_table_2d9484`: 12-byte entries `{thumb handler, ptr to a 3-byte record
list, 4 bytes of per-game parameters}`, indexed by `game_index_11fd1b`:

| index | handler | menu name |
| --- | --- | --- |
| 0 | `rotation_handler_241fd0` | Rotation |
| 1 | `snake_handler_240d82` | Snake |
| 2 | `memory_handler_24075c` | Memory |
| 3 | `game3_handler_242890` | not offered in the 3210 Games menu |

Every handler receives the same event codes: `0x49` init, `0x53` resume,
`0x54` tick, `0x57` draw, and key events as ASCII (`0x32/0x34/0x36/0x38` =
2/4/6/8, `0x35` = 5, `0x23` = `#`, `0x2a` = `*`). Settings records exist for
five games.

## Naming harness (pret-style, all three games + shared framework)

- `tools/callgraph.py` — BL closure from the games region (`0x240600-0x244000`)
  and framework (`0x2621c0-0x263500`); functions outside are *boundary*
  services (the wrapper's shim list) and are not descended into.
  Writes `data/callgraph.json`.
- `symbols.csv` — our names (address,kind,name); upstream's
  `ghidra/symbols/3210.csv` is merged underneath by `tools/names.py`.
- `notes.json` — `{"blocks":[{"addr":"0x...","lines":[...]}]}` comment blocks.
- `tools/decomp_refresh.sh` — pushes `symbols.csv` into the Ghidra project and
  re-exports decompiled C for the whole closure to `data/decomp/<addr>.c`
  (about 8 s).
- `tools/worklist.py [--top N] [--boundary]` — progress and ranked targets
  (`F` = frontier: called by a named function).
- `tools/packet.py ADDR` — callers, callees, resolved literal pool, cached
  decompile, disassembly, notes.
- `tools/map_next.sh [ADDR]` — one tick: worklist plus the packet for the top
  target (or the given address).

Loop: `map_next.sh` -> decide a name -> add to `symbols.csv` (and a note) ->
`decomp_refresh.sh` when names should propagate into decompiles -> repeat.
Runtime confirmation for ambiguous functions: `tools/nav_run.sh` with
`mame/coverage.lua` (`SNAKE_COV_ENTRIES` narrowed to the candidates) or a
`SNAKE_TRACE` window.

## Next

1. Static: map the cluster's callees and data (`_DAT_00241164..94` game state
   pointers), find the Games-menu dispatch that reaches `0x49` init, and the
   string IDs for `Snake`/`Game over` (English text block at `0x2f3c00` in the
   raw `.fls` byte order).
2. Dynamic: find why delay event `0x30` never returns to `00240d82` in MAME.
