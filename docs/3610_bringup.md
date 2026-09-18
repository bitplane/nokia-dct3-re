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
request-derived exchange independently of the later application gate.

The compact type-`0x74` echo of that exact request is consumed organically.
Firmware then emits two type-`0x3c` blocks, eight type-`0x0d` blocks and the
one-way type-`0x70` follow-up `0a09`. `make verify-3610-service-control`
protects this complete observed transition independently of application
registration.

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

The supplied image ends at CPU address `0x54ffff`. Public DCT3 flash maps place
NAM-1's broad EEPROM/PMM partition at `0x550000..0x5fffff`; service tools expose
either the final 128 KiB (`0x5e0000..0x5fffff`) or a smaller active upload
window. A preserved Twister service-software collection contains NAM-1 firmware
installers and virgin EEPROMs for many adjacent DCT3 products, but no 3610
EEPROM. A matching artifact therefore remains valuable archival input.

It is not the current execution boundary. A passive product-owned flash census
observes no read or write in `0x550000..0x5fffff` during a 20-second coherent
run, despite the firmware completing the application registration, channel map,
channel-`0x5f` reporting and M2BUS terminal exchange. The focused
`make verify-3610-storage-boundary` gate protects the shorter reproducible form
of that result. Consequently a synthetic PMM cannot honestly repair the current
frontier: the firmware has not reached its product-state loader.

The immediate question is now pre-storage: which service/self-test lifecycle
must complete before the firmware starts ordinary application initialization
and accesses persistent state? Keypad scanning, SIMI initialization and radio
startup remain dormant. Continue backward from those dormant consumers and the
service-status lifecycle; do not enable their peers or borrow another product's
PMM merely because later handsets share those components.

Sources for the physical partition bounds and service-tool interpretation:

- [Nokia DCT3 flash-address table](https://www.nokia-tuning.net/index.php?s=flashadress)
- [UFSx/Tornado service manual](https://www.gsm-support.net/download/UFSxTornado_Manual_GSM-Support.pdf)
- [Preserved Twister service-software collection](https://archive.org/details/Twister_Mobile_Phone_Flashing_Box)
