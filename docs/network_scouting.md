# Network / registration scouting — can operator-idle be reached digitally?

Scouting pass (2026-07-09) to answer: after the fake SIM, does reaching the
classic **operator-idle** home screen (clock + **operator name** + **signal bars**)
have a digital path — i.e. can network state be faked at the message layer the way
`MODEL_SIM_CARD` fakes the SIM — or is it DSP/RF-bound?

## Verdict: DSP-bound (and not even attempted on our boot)

Operator-idle's two network-sourced pieces split cleanly:

- **Signal RSSI — injectable.** The raw signal value is **CCONT ADC channel 1**
  ("RF signal strength, from the COBBA path"; `docs/ccont_subsystem.md`,
  `docs/hardware_atlas.md`). We already model the CCONT ADC (`ADC_PROFILE` / the
  `adc_src` model), so a signal *value* is digitally settable. But bars only paint
  once the phone is **camped/registered** — which is the hard half.

- **Registration + operator name — DSP-bound.** These come from the **GSM Layer-1
  protocol stack**, which lives in the **unemulated DSP**. The MCU↔DSP boundary is
  DSP shared RAM `0x10000` + DSPIF `0x30000`, and the on-MCU GSM-L1 / audio DSP
  driver is a large layer at **`0x2b7xxx–0x2c9xxx`** (~444 refs to `0x10000`, ~287
  to DSPIF).

## Evidence (dynamic + static)

**The GSM-L1 layer is never reached on the full-SIM boot.** `TRACE_DSPDRV` (new,
logs distinct branch/call targets entering `0x2b6000–0x2c8000`) over the full model
stack (`…+MODEL_SIM_CARD+MODEL_RES_ENABLE`) shows **91 distinct entries, ALL in
`0x2b60xx–0x2b66xx`** — the tiny **DSP-interface plumbing** at the very start of the
range. **Nothing above `0x2b6fff` ever runs.** (This retires the atlas's older note
that the DSP layer "isn't reached until past the mode-`000d` limp" — we are now well
past the limp, and the *protocol* layer still doesn't run.)

**Who drives the DSP-IF plumbing:** the callers into `0x2b60xx` are the SIM
(`0x27cc9x`), the CCONT service (`0x2a70xx`), and the scheduler — using the DSP
mailbox for *their own* needs, not for network. Task **22** (`dsp_if_task_2b6548`,
in the 23-task table `0x2d7090`) is the DSP-interface task: it recv-loops and
dispatches via `dsp_if_message_dispatch_23d62c`, but never dispatches up into the
`0x2b7xxx+` L1 handlers.

**No dedicated network task, and few doors.** There is **no MM/RR/CC network task**
registered in the system task table (tasks 0–22 are supervisor / startup / contact-
service / display / MMI / SIM / service-transport / the DSP-IF task / the subsystem-
init reporters). Statically, the whole GSM-L1 layer `0x2b7000–0x2c8000` has only
**10 external `bl` entry points**, and their callers are **adjacent DSP-driver code
(`0x2c8xxx–0x2c9xxx`) and a high bank (`0x3axxxx`)** — not the MMI/service/boot code
we run. So the network stack is a **self-contained, message-driven subsystem behind
the DSP-IF task**, entered by driving the DSP mailbox — both ends of the GSM L1
protocol (the MM/RR commands down *and* the DSP L1 measurement/sync reports up).

## What this means for a "digital plan"

- Faking network is **not** analogous to the SIM. The SIM was one linear request/
  response conversation over an existing service transport; camping + registration
  is a **stateful multi-message GSM L1↔L3 procedure** whose lower half *is* the DSP.
  "Injecting a you-are-registered message" has no single seam — you'd be
  reimplementing GSM signalling against the DSP mailbox.
- It is **moot before the upstream wall anyway.** The phone never even attempts
  registration on our boot — it stalls earlier, at the MMI display-resource /
  content pipeline (`docs/sim_emulator_scope.md`), which needs the provisioned
  resource config. Network is a *second* wall behind that one.

## Honest ceiling

- **Reachable digitally (in principle):** "SIM present, no network" — i.e. the idle
  home layout with a searching / no-service indicator and no operator name — *if*
  the upstream MMI resource-content wall is first cleared (uncertain; needs the
  provisioned config). Signal RSSI is injectable via ADC ch1 but only matters after
  camping.
- **Not reachable digitally:** operator-idle *proper* (operator name + live signal
  bars), which requires real network registration through the DSP/RF — hardware.

So boot-to-"Insert SIM card" remains the clean, complete milestone. The next
digital frontier, if pursued, is the **MMI resource-content pipeline** (data), not
the network (hardware). Scouting knob: `NOKI3210_TRACE_DSPDRV`.

## Observed end-state (clarification)

Both the no-SIM boot and the full faithful-SIM boot **display "Insert SIM card"**.
The difference is internal: with the SIM models the read conversation completes and
the no-SIM reject clears *internally* (the SIM RE, `sim_emulator_scope.md`), and the
MMI renders a *complete* screen — frames evolve `blank → o058 → o074`, the o074 form
adding the scrollbar + status-icon chrome, i.e. a fully-up MMI. But the **displayed
text screen never advances past "Insert SIM card"**: the state→display handoff that
would repaint to the next screen doesn't fire (the same MMI resource-content wall).
So "Insert SIM card" is both the correct no-SIM home screen *and* the visible
ceiling of the faithful-SIM boot — the phone is further along internally than the
pixels show, but the pixels are gated on the resource-content pipeline, not the SIM.
