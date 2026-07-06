# SIM subsystem — map and Phase-1

Once the phone boots past the service wall (see `docs/boot_to_insert_sim.md`) it
tries to bring up the SIM, fails (no card), and shows "Insert SIM card". This
doc maps the SIM subsystem and records the validated Phase-1 injection point for
getting past that screen. Reaching the operator-idle home screen is a larger
faithful build sketched at the end.

## Hardware

The SIM UART (SIMI) is MMIO at base `0x20000`, offsets `0x36–0x3f`:
TXD `0x36`, RXD `0x37`, IIR `0x38`, CONTROL `0x39`, CLOCK `0x3a`, TxD-low-water
`0x3b`, RX_FILL `0x3c`, RX_FLAGS `0x3d`, TX_FLAGS `0x3e`, TX_FILL `0x3f`. Already
mapped in `mad2_io_r/w`. `TRACE_SIM` logs traffic on these registers.

## The power-on sequence (observed)

On boot the SIM driver configures the UART (clock `0x3a=03`, FIFO flags
`0x3d`/`0x3e`), asserts reset via `SIM_CONTROL 0x39` (`0x32 → 0x33 → 0xb3`),
writes TxD-low-water `0x3b=0x60`, then waits for the **ATR** (Answer To Reset)
in RxD. Our SIM answers nothing, so RxD stays `0x00`; the driver retries the
whole reset loop and, after N failures, declares no SIM → "Insert SIM card".

**Key asymmetry:** the MCU *writes* command bytes to `SIM_TxD 0x36`
(`0x2a0268`, from a RAM ring at `0x1106c0`) but **never reads `SIM_RxD 0x37`**
except the reset flush (whole-firmware scan confirms). SIM *responses* (ATR,
APDU results) come back through the DSP/service layer as service messages
(code `0xaf`), not the UART registers. Modelling SIM input therefore means
injecting service messages, not feeding the RxD register.

## The layers

| Layer | Address | Role |
|-------|---------|------|
| SIM task main loop | `0x27defc` | recv → dispatch on `[msg+4]`; ctrl struct RAM `0x10a8dc` |
| Reset-start | `0x27e024` | set state, call reset driver, schedule timeout event `0xe9` |
| Reset driver | `0x2a01b8` | SIM UART reset/activate; reset-state struct `0x1106d4` |
| TxD transmit | `0x2a0268` | write command bytes from ring `0x1106c0` to `SIM_TxD` |
| Data state machine | `0x29ff2c` | paged 250-byte transfer; results up via `0x234634` code `0xaf`; entered from contact-service dispatcher `0x2378e0` |

## SIM task dispatch (`[msg+4]`)

Confirmed by careful re-read (an earlier fast read mis-attributed several
branches; `0x2695f4` is event-post-to-self via ECB table `0x100140`, so the
else-branch just re-arms the `0xe9` retry tick):

| code | handler | meaning |
|------|---------|---------|
| `3` | `0x27df9e` | copy message into a 0x118-byte buffer at `0x10eede` |
| `5`, `8`–`b` | `0x27df64` | copy `[msg+2]` data into struct `0x10c8dc` |
| `0xc` | `0x27df52` | **SIM present**: set `[0x10a8dd]=1`, `[0x10a8e3]=1`, `[0x113cff]=1`, call startup-ready `0x279486` |
| `0x11` | `0x27df44` | reset-state sub-handler |
| else | `0x27df3c` | re-arm `0xe9` retry tick |

Our boot only ever sees the retry cycle (`01 → 11 → 06 → repeat`); the response
codes `3` / `5` / `0xc` are never delivered.

## Phase-1 result (validated)

`EXPERIMENT_SIM_PRESENT` forces the dispatch to take the code-`0xc` path once
during the retry window (`SIM_PRESENT_AFTER`, default the 6th dispatch). The
handler ignores the message payload, so this is functionally equivalent to
delivering a code-`0xc` "SIM present" message.

Result: the SIM retry loop **stops** (no more reset-starts), the "Insert SIM
card" screen is **removed**, and the phone advances to a blank MMI screen
(scrollbar + status icons, empty text area) — SIM present, no data. No crash,
stable. `display_idle` still doesn't fire. **The injection point and mechanism
are validated.**

## Why Phase 1 alone doesn't reach idle

The forced `0xc` clears the *screen* but not the *data*: traced afterwards, the
data state machine `0x29ff2c` and the `0xaf` sends never fire, there are zero
`SIM_TxD` writes, and at t≈3.5 the phone merely re-inits the SIM UART. So it
never requests SIM files. The blank text area is a phone that thinks a SIM is
present but has nothing to read from it.

## Toward operator-idle (future work)

Reaching the operator-idle home screen is a faithful SIM-protocol build:

1. **Faithful Phase 1** — deliver a real code-`0xc` "SIM present" message
   (responder-style) rather than forcing `r8`, so the SIM comes up through the
   genuine init path and drives the file-read flow.
2. **Phase 2** — provide SIM file contents (IMSI `6F07`, operator, service
   table, PIN-disabled) delivered as `0xaf` service messages into the data
   state machine `0x29ff2c`. Once the real init runs, `TRACE_SIM`/`TRACE_SIMSM`
   will show the exact file/APDU sequence to answer.
3. **Phase 3** — with a valid SIM and PIN off, the phone leaves "Insert SIM
   card" for idle. Network registration (RF, `RUN GSM ALGORITHM`) is out of
   scope; the idle screen appears before registration (empty operator / no
   service).

IP-clean: synthetic test IMSI/files; ATR and file structures are public
(ISO-7816, GSM 11.11 / TS 51.011). No real SIM dump needed.

## Diagnostics (opt-in knobs)

| Knob | What it shows |
|------|---------------|
| `TRACE_SIM` | SIM UART register traffic (`0x36–0x3f`) |
| `TRACE_SIMSM` | SIM task dispatch codes, reset-start, data SM, `0xaf` sends |
| `EXPERIMENT_SIM_PRESENT` / `SIM_PRESENT_AFTER` | Phase-1 SIM-present injection |
