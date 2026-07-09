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

## The runtime message protocol (recv side — task 22)

Task **22** (`dsp_if_task_2b6548`) is the DSP-interface RTOS task: it `recv`-loops and
dispatches via `dsp_if_message_dispatch_23d62c`. That dispatcher:
- copies message fields into a state struct at `[0x11251c]` (`[msg+2]→+0`, `[msg+7]→+1`,
  `[msg+0]→+3`),
- forwards to `0x2b5f88` with a code `0x8257`,
- **dispatches on `[msg+3]`** (a subtract-cascade on values `2,3,5,6,0xa,…`), like the
  contact-service command dispatcher.

This task *runs* (it services the SIM/CCONT/scheduler's use of the DSP mailbox), but on
our boot it only ever handles those housekeeping messages — never the L1/network traffic.

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
- Static RE of the `0x2b7xxx–0x2c9xxx` L1 driver: enumerate the DSPIF command/status
  protocol and the L1 message primitives (large, code never runs on our boot).
- Identify the two downloaded blobs (`[0x200+]` from flash `0x200040`; `[0xe00+]`): DSP
  program vs coefficient tables vs audio codec params.
- The MCUIF (`0x40000`) memory-range configuration.
