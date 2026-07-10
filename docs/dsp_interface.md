# The MAD2 MCU↔DSP interface (DCT3 / Nokia 3210)

A map of how the ARM7 MCU talks to the on-chip **DSP** (which runs the GSM Layer-1
baseband + audio codec) in the Nokia 3210 firmware. Built from static disassembly
(`tools/disrom.py`) + live tracing (`NOKI3210_TRACE_DSPIO`, which logs first-touch of
every DSP shared-RAM offset and DSPIF register with direction/value/PC over the boot).

**TL;DR for emulation:** at boot the MCU treats the DSP interface as (a) a RAM
self-test it passes by echo, (b) a handful of "DSP ready" status flags, and (c) a
**one-way download** of coefficient/program blobs from flash into shared RAM. It does
**not** wait on any DSP-*computed* result during the reachable boot. The bidirectional
L1 message protocol — where real DSP emulation would matter — is **downstream of the
MMI/coherent-boot wall and never executes**. So the DSP is **not** the keystone for the
current stall; it sits behind it.

## The two hardware windows

| MMIO | region | driver | boot usage |
|---|---|---|---|
| `0x10000–0x10fff` | **DSP shared RAM** (0x800 halfwords) | `dsp_ram_r/w` (stub, real backing store `m_dsp_ram`) | heavy — self-test, config, blob download |
| `0x30000–0x30003` | **DSPIF** control register | `mad2_dspif_r/w` (stub: reads 0, writes no-op) | **almost none** — one early `0`-init at pc `0x2001a4`; its ~287 references are all in the L1 code that never runs |
| `0x40000` | MCUIF (memory-range config) | `mad2_mcuif_r/w` (stub) | early config |

The atlas counted ~444 references to the shared-RAM base and 42 pool-literal references
to DSPIF across the image, but **`TRACE_DSPIO` (deduped per distinct offset+dir+PC) shows
the *reachable boot* touches DSPIF exactly twice** — `W[0]=0` and `W[1]=0` at `0x2001a4`.
The one *other* boot-region DSPIF write — `strh #4 → 0x30000` ("command 4") at `0x29103c`
in the DSP-service handshake — is **gated by the branch at `0x291030` and skipped**,
because `MODEL_DSP_SERVICE` fakes the service completion so that path isn't taken. All
remaining DSPIF use is in the `0x2b7xxx–0x2c9xxx` GSM-L1 driver that never runs (see
`network_scouting.md`). So DSPIF's command/status protocol is **static-only** for us.

## Shared-RAM layout at boot (`0x10000` base; offsets are byte offsets)

| offset | what | evidence |
|---|---|---|
| `[0x00–0x24]` | **self-test / RAM echo region** | written with a walking pattern at `0x295f48`, read back + compared at `0x295fc0`/`0x295fd6`. A RAM test of the shared window; passes trivially against a real backing store. |
| `[0x00–0x04]` | **DSP status fakes** → `0x01` | `dsp_ram_r` HACK; the firmware reads these as "DSP alive". |
| `[0xe0]` → `0x00`, `[0xfe]`/`[0x100]` → `0x01` | **DSP ready/busy flags** | HACK fakes; `0xe4` = lower-service pending counter (MCU writes `0x0002` at `0x290c98`; `MODEL_DSP_SERVICE` drains it + raises IRQ 4). |
| `[0xf6–0x102]` | **config words** (`0x0100 0x0300 0x0001 0x0000 0x0001 0x0001 0x0200`) | 7 individual writes at `0x290a44–0x290a64`. |
| `[0x200–0x600]` | **coefficient/parameter table** (512 halfwords) | strided copy at `0x290a94`: reads one halfword per 0x20-byte record from flash `0x200040`, packs into `[0x200+]`. |
| `[0xe00+]` | **second blob** (~240+ halfwords) | ARM block-copy (`stmia`) at `0x2b5bd0` (reached via `bx pc` ARM-mode switch). |

So the MCU **stages DSP tables/program from flash into shared RAM** at boot — the DSP's
data *is* in the image. On a real phone the DSP would then execute/consume it; with the
DSP stubbed the blobs just sit in `m_dsp_ram`, harmless.

## The L1↔DSP mailbox protocol (2026-07 dig — both directions mapped)

Task **22** (`dsp_if_task_2b6548`) is the DSP-interface RTOS task. Its loop: `recv`
(`0x26a458`) → if the code < `0xc0` call the dispatch `0x23d62c` (`0x2b6564`); codes
`0xc0–0x1bf` are delayed-message markers, freed and ignored.

**Recv side (DSP→MCU downlink) — dispatch `0x23d62c`:**
- copies message header fields into a state struct at `[0x11251c]` (`[msg+2]→+0`,
  `[msg+7]→+1`, `[msg+0]→+3`),
- **dispatches on `[msg+3]` = message class**, a subtract-cascade over
  `{2, 3, 5, 6, 7, 8, 0xa, 0x11, 0x13, 0x14, 0x47, 0x64, 0xd5}` → handlers
  `0x23c4fc / 0x23c55c / 0x23c9e8 / 0x23cbe8 / 0x23cde0 / 0x23d158 / 0x23d2fe / 0x23d430`
  (payload pointer = `msg+9`). Class `0xd5` is a special inline (wheel-only: sets state
  `{0xc,1,0}`, forwards id `0x3957`).
- **Two-level protocol**: each handler sub-dispatches on `[msg+9]` = primitive (e.g.
  `0x23cde0` branches on `0x10/0x13/0x16/0x18/0x1a/0x25/…`). So a message is
  `{class:[+3], primitive:[+9], payload:[+10…]}`.
- **Routing to the upper L1 layers uses the resource-message bus**: the "forward" helper
  `0x2b5f88(id, msg)` is actually `resource_available(id)` + `resource_acquire(id, msg)`
  (the same `0x2b12b4`/`0x2b12dc` pair as the display) — i.e. L1 messages are delivered to
  whichever task registered for message-id `id` (`0x82xx`/`0x03xx`). A routing table at
  `0x2b66b0` (0xc-byte records) maps DSP primitives `0x10–0x29` ↔ upper-layer message ids
  `0x035c–0x037b`.

**Send side (MCU→DSP uplink):** write a command halfword to the **DSPIF register
`0x30000`**, then poke the doorbell interrupt at `0x20008` (pattern seen at `0x291038`:
`strh #4→[0x30000]; strb #2→[0x20008]`). The DSPIF has **287 write sites, almost all in the
L1 driver `0x2b7xxx–0x2c9xxx`** — the per-command send stubs.

**Runtime status — the whole protocol is DORMANT (`TRACE_DSPMSG`).** On our boot the
dispatch `0x23d62c` is reached **0 times**: task 22's handler is entered only ~twice
(t≈0.37, 3.76) for the low-level echo/handshake and stays `recv`-blocked — no DSP→MCU L1
message ever arrives, and the 287 DSPIF send stubs never run. The mailbox plumbing (task 22,
IRQ4 lower-service, dispatch, routing table) is fully present and now mapped, but **no
GSM-L1 traffic flows** because nothing runs the L1 stack: there is no DSP executing the air
interface to emit measurement/sync/registration primitives, and the MCU-side L1 senders sit
behind the same coherent-boot/network-attach phase we never reach.

This task's *housekeeping* use (SIM/CCONT/scheduler mailbox) is what keeps it alive; the
L1/network traffic is the dormant half. Diagnostic: `NOKI3210_TRACE_DSPMSG` (recv classes/
primitives at `0x23d638`).

## What the boot waits on from the DSP: nothing (the key finding)

`TRACE_DSPIO` shows **no MCU reads of DSP-computed results** beyond the self-test region
and the hardcoded ready-flags. The MCU inits the interface (self-test → config → blob
download → "DSP ready" flags) and proceeds. It does not block on the DSP. This is why the
existing 4-line stub is enough to reach "Insert SIM card".

The **bidirectional L1 protocol** — MCU sends "search/sync/measure/attach", DSP returns
cell/RSSI/registration — lives in the `0x2b7xxx–0x2c9xxx` driver and **never executes on
our boot** (it's entered only after the MMI/coherent-boot phase we can't reach). So it is
**static-analysis-only** today: we cannot trace it live until the boot coherently reaches
the network-attach phase.

## Emulation feasibility & the dependency re-ordering

- **To reach where we are:** the DSP needs *nothing more* than the current echo + fake-ready
  stub. Done.
- **The DSP is downstream of the current wall, not the keystone.** Earlier framing (get the
  DSP right → coherent boot → UI) had the order backwards. The immediate stall is the **MMI
  resource-content / coherent-boot** layer, which passes the DSP self-test and does **not**
  wait on the DSP. Emulating the DSP further would not move it.
- **For the network (operator name + signal):** a message-boundary DSP stub (answer the L1
  commands with "camped on a fake cell, operator X, RSSI y") is feasible *in principle* and
  is the right MAME approach — but it is **doubly blocked**: (1) the L1 protocol is
  static-only until coherent boot reaches it, so we can't observe the exact handshake to
  stub it; (2) it is behind the same MMI/coherent-boot wall. Signal RSSI itself is separately
  injectable (CCONT ADC ch1, `network_scouting.md`).
- **Full DSP-core emulation** (a TI Lead core running the downloaded blobs) is a much larger
  project and would still need a faked air interface; not warranted given the above ordering.

**Net:** the reachable MCU↔DSP interface is now mapped and is satisfied by a trivial stub;
the interesting L1 half is gated behind the coherent-boot wall and can only be RE'd
statically until that wall falls. The practical unlock order is **coherent boot first, DSP
L1 second** — the reverse of the intuition. Trace knob: `NOKI3210_TRACE_DSPIO`.

## Open items (future deep-dives)
- Decode each recv handler's primitive set (`0x23cde0` etc.) and the full `0x2b66b0`
  routing table — the DSP→MCU message *content* (the class/primitive skeleton is mapped
  above; the per-primitive payload semantics are not).
- Static RE of the `0x2b7xxx–0x2c9xxx` L1 driver *send* side: enumerate the 287 DSPIF
  command stubs and their command encodings (large, code never runs on our boot).
- Identify the two downloaded blobs (`[0x200+]` from flash `0x200040`; `[0xe00+]`): DSP
  program vs coefficient tables vs audio codec params.
- The MCUIF (`0x40000`) memory-range configuration.
