# MMI and keypad lifecycle

This document summarizes the firmware-owned MMI input path at the current
coherent Nokia 3210 v6.00 frontier. `mmi_settlement.md` owns the startup
report investigation; this file owns the keypad hardware-to-firmware contract.

## Contract audit

| Boundary | Classification | What is established | What remains provisional |
| --- | --- | --- | --- |
| LCD controller | Reused device, partial integration | Firmware-generated command/data transfers reach MAME's PCD8544 device and reproduce a byte-exact visible frame. | Reset/electrical timing remains unmeasured. |
| MAD2 LCD serialization | Derived hardware contract | Both 3210 ROMs select GENSIO endpoint `0x21`, send command prefix `24 40 80`, and transfer at least one complete 504-byte LCD RAM image MSB-first. | The immediate bit-clock implementation is functional timing, not a measured serial waveform. |
| Display profile | Recovered NV boundary, minimal synthetic provisioning | Descriptor `0x0749` supplies three 12-byte profiles; record-0 byte 5 becomes active-profile slot 7 and the setup-message initializer tests it against `4`. An erased-profile control proves there is no usable firmware fallback and leaves the panel blank. | Every collected EEPROM has the descriptor erased. The generated EEPROM supplies only this recovered field and leaves unknown bytes erased; no complete factory record is available. |
| MAD2 keypad matrix | Partial hardware, strong evidence | v6.00 and v5.01 agree on the 4x5 active-low scan, IRQ0 source, register sequence, ROM keymap and decoded-key path. | Electrical debounce, mask-edge corner cases and timing outside the exercised lifecycle remain unvalidated. |
| Lua LCD mirror and key script | Acceptance tooling | The independent mirror makes headless frame hashes and the script supplies deterministic physical key edges. | Neither is emulated phone hardware; scripted delays are fixtures, not keypad debounce. |

## Current state

The provisioned profile paints an idle frame with a `Menu` softkey while task 1
remains in mode `0x0004`. A physical left-softkey fixture follows the firmware
key/event pipeline and opens the `Phone book` menu with a `Select` softkey.
This proves that the application desktop is interactive and that mode `0x0004`
is not a blocked pre-desktop state.

A scripted logical press changes the active-low MAD2 keypad state and raises
IRQ0. Handler `0x2b3084` starts the firmware's internal `0x41/0x42/0x43`
sequence, which scans the matrix at `0x2b2f90`, decodes the ROM keymap, and
publishes resource `0x6e02` at `0x2b4628`. This works while task 1 remains in
mode `0x0004`; IRQ6 belongs to CCONT rather than the keypad.
The same function first calls local key handlers `0x2979d8` and `0x2a27de`;
`0x6e02` is an availability-gated resource mirror, not the sole input route.

A coherent trace drives the left softkey 250 ms after readiness and decodes
keycode `0x19`. A scheduler-backed sequence can also drive `12345` plus the
left softkey after the editor publishes `0x057c`.
All physical press/release edges enter IRQ0, the digit path reaches the editor,
and submission completes the transaction through `0x0578`. The callback returns
`0x05e6`, the statically proved accepted-code result. The entered `12345`
therefore matches the firmware-derived value stored at RAM `0x112460`; keypad
delivery and the synthetic EEPROM security-code encoding are both validated.

A bounded 50 ms Up tap proves the physical lifecycle: IRQ0 fires on press and
release, the matrix scanner polls while held, and `0x2b4628` decodes one key.
Later `0x0367` polling, the accepted editor transaction and periodic `0x00c8`
and `0x05a7` traffic are firmware-owned MMI lifecycle behavior, not repeated
matrix scans or missing hardware acknowledgements. Their transition-level
evidence belongs in `mmi_settlement.md`.

## Hardware path

```text
MAME input ports COL.0..COL.4 (five columns, four row bits each)
  -> MAD2 row signal 0x28 / direction 0xa8
  -> active-low column input 0x2a / interrupt mask 0x6b
  -> physical press/release edge
  -> MAD2 IRQ0
  -> firmware ISR 0x2b3084
  -> task-1 event 0x41
  -> internal 0x41/0x42/0x43 scan/decode sequence
  -> resource 0x6e02
```

The IRQ source, polarity, acknowledgement, mailbox destination, and scheduler
delivery are therefore proved. MAD2 IRQ6 belongs to CCONT and must remain
separate. The driver does not post a decoded MMI key directly.

## Register contract

The complete v6.00 matrix scanner `0x2b2f90` aligns uniquely with v5.01
`0x2b0208`; the downstream decoder block `0x2b4628` aligns with `0x2b18b0`, and
the IRQ0 ISR `0x2b3084` aligns with `0x2b02fc`. Both firmware versions implement
the same sequence:

1. Save column-interrupt mask `0x6b` and set its low five bits to mask all.
2. Set row signal `0x28 = 0x0f` and row direction `0xa8 = 0` for the direct
   power-column check.
3. If no direct column is low, configure one of four row outputs in `0xa8`,
   drive that row low through `0x28`, and read five active-low columns at
   `0x2a`.
4. Restore idle row direction `0x0f`, row signal `0xf0`, and the saved column
   mask.

The five MAME `COL.n` ports are therefore columns, not rows. Their bits 1..4
are the four row contacts; power is the direct bit-0 input. Matrix positions
come from the 25-byte ROM table at `0x2e2d58`. The corrected model derives
columns from driven-low output rows and reports physical press and release
edges through IRQ0.

## MAME controls

The driver exposes handset buttons as ordinary remappable MAME inputs. Host
bindings are defaults, not part of the emulated electrical contract:

| Handset control | Host default | Matrix input |
| --- | --- | --- |
| Navi / left softkey | Enter | `COL.1` bit 1 |
| C / right softkey | Backspace or Delete | `COL.2` bit 1 |
| Scroll up / down | Up / Down | `COL.3` bit 1 / `COL.1` bit 2 |
| Digits `0..9` | matching number keys | ROM-derived matrix positions |
| `*` / `#` | Asterisk / Minus | `COL.4` bit 1 / `COL.3` bit 2 |
| Power | Space | special direct input |

The Lua acceptance harness addresses the same fields using semantic aliases
`navi`, `select`, `left`, `soft1`, `clear`, `back`, `right`, `soft2`, `up`,
`down`, `star`, `hash`, `power`, and the digit strings. It does not bypass the
MAME ports or firmware scan.

`make run-interactive` opens the provisioned v6.00 phone in a normal MAME
window without scripted input. Its NVRAM lives under `run_interactive/nvram`
and is preserved between launches. Handset settings persist in the emulated
24C128 EEPROM; SIM phonebook records persist independently in the removable
card device's NVRAM. MAME's Input Settings menu can remap every handset
control. The headless `run` and `verify-*` targets remain isolated and
deterministically reseed their own NVRAM unless a persistence test explicitly
requests reuse.

## Firmware consumer

IRQ0 handler `0x2b3084` calls the task-1 event-`0x41` publisher directly.
Those internal events call
`0x2b46da`; it scans the matrix and translates the result through the selected
keymap. Function `0x2b4628` then publishes decoded resource `0x6e02`. Its store
at `0x2b4652` arms `0x11239d` when the previous decoded key is `0x0d` and the
display state is 1. It is downstream of scanning, not a pre-input hardware
event.

## Acceptance evidence

A complete input milestone requires all of the following in one coherent run:

- service-session status remains healthy;
- SIM initialization remains organic;
- IRQ0 enters `0x2b3084` on physical press and release;
- the firmware matrix scan executes through the `0x41/0x42/0x43` sequence;
- a decoded key reaches the MMI event layer; and
- a multi-key editor transaction reaches its firmware-owned completion.

The hardware-to-editor contract satisfies those conditions. The provisioned
left-softkey run additionally proves the application desktop by opening the
firmware-owned `Phone book` menu; the PNG is corroborating output from that
organic transaction rather than the sole evidence.

## Diagnostics

`NOKI3210_TRACE_TASKS=1` provides generic mailbox-edge context for future
application work. The former MMI-specific handoff trace was retired when
`make verify-mmi-menu` closed the keypad-to-menu lifecycle.

`NOKI3210_TRACE_DISPLAY_PROFILE=1` records the descriptor-load, profile-update,
active-profile copy and setup-message boundaries. `NOKI3210_TRACE_DISPLAY_IO=1`
records only endpoint selection and LCD command/data bytes. `make verify-display`
checks both ROMs' NV descriptor layouts and serial transfers; its v6.00 leg also
locks down the minimal synthetic field, its firmware-owned copy and the absent
profile-update handler. The
byte-exact frame remains the end-to-end rendering oracle.
