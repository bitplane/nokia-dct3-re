# Nokia 3610 NAM-1 bring-up

## Current result

The hash-pinned v5.11 PPM-E image executes under a fail-closed `noki3610`
machine profile. Runtime evidence establishes the product's GENSIO attachment:
firmware writes control `0x22` at offset `0x2d`, transfers CCONT bytes at
`0x2c`, and polls status at `0x6d`. Because control bit 2 remains clear, the
controller must publish receive-ready after each byte write. With that
product-owned contract, firmware completes 62 CCONT reads and begins organic
display traffic without a firmware hook or borrowed handset profile.

The display contract is also established independently. Each refresh writes
nine banks of 96 bytes through GENSIO offsets `0x6e/0x2e`; the conservative
84x48 presentation clipped and reversed the rendered text. A 96x72 controller
with a 96x65 visible area and reversed segment order presents the complete
firmware `CONTACT SERVICE` frame. `make verify-3610-frontier` protects that
exact frame together with organic LCD activity and absence of soft resets.

The DSP transport is independently bounded. NAM-1 performs exactly 64
zero-acknowledged exchanges and consumes three `0001` startup publications.
It then raises six command-4 doorbells with service-pending value `2` and
control words `0x900f`, `0x8002`, and `0x7000`. Completing that observed IRQ4
request advances firmware into seven type-`0x05` D0 discovery publications.
`make verify-3610-dsp-service` protects the bootstrap count, doorbell, IRQ4 and
first exact discovery packet.

The common service transport derives both D0 responses from that first packet:
an unnotified correlation frame and a notified D0/04 completion. NAM-1 consumes
them and emits four type-`0x70` setup blocks followed by the exact compact
service-control request `0d00`. `make verify-3610-discovery` protects this
request-derived exchange. No unsolicited application responder is enabled.

The compact type-`0x74` echo of that exact request is consumed organically.
Firmware then emits two type-`0x3c` blocks, eight type-`0x0d` blocks and the
one-way type-`0x70` follow-up `0a09`. `make verify-3610-service-control`
protects this complete observed transition. The external application remains
disabled at this checkpoint.

NAM-1 acknowledges peer registration sequence `0x42`, accepts channel-map
sequence `0x43`, and then organically exercises channel `0x5f`. Its first
status report contains `EXIT ANYSTATE`; subsequent reports identify periodic
connection checks and charger states. `make verify-3610-application` protects
the exact registration acknowledgement, channel-map acknowledgement and first
channel-`0x5f` report. The generic transport currently acknowledges each
report and the firmware continues publishing channel-`0x5f` status, so the
steady-state service lifecycle is not yet claimed complete.

With the physical 423.1 Hz M2BUS timer enabled, NAM-1 independently transmits
the checksum-valid terminal discovery frame
`1f ff 00 d0 00 01 01 01 31`. The common terminal derives its transport fields
from that request; firmware accepts the ACK and D0/04, emits D0/05, and accepts
the final ACK. `make verify-3610-mbus` protects the complete byte-boundary
exchange. This is independent product evidence for the shared physical
protocol, not inheritance from NAM-2.

## Established inputs

- MCU/PPM normalization and hashes are recorded in `roms/README.md`.
- The normalized image occupies `0x350000` bytes of a 32-Mbit flash region.
- The established later-MAD2 map reaches flash entry `0x200040`.
- Static scans recover 670 direct MAD2 accesses and the 18-entry CCONT
  descriptor vocabulary at flash address `0x00489b74`.
- The initial conservative run stopped polling GENSIO status at `0x003d7982`.
- The byte-write-ready contract advances beyond that loop and produces
  ordinary LCD writes with no soft reset.

## Open boundary

Identify the missing product-state/PMM contract that retains the diagnostic
service mode after the terminal exchange.
Keypad, SIM, persistent storage, external-service and radio contracts remain
disabled until product-local evidence establishes each one.
