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

The board composition now also includes the ordinary later-MAD2 SIMI
controller and removable lab card. A static ROM census finds the complete
controller driver, including its live control/status reads, and the fitted
BLB-2 battery is represented by the independently recovered NSM-5 nominal
CCONT tuple. These are physical inputs, not startup assists. In the current
run the firmware does not activate SIMI because an earlier product-calibration
gate excludes its task.

## Established inputs

- MCU/PPM normalization and hashes are recorded in `roms/README.md`.
- The normalized image occupies `0x350000` bytes of a 32-Mbit flash region.
- The established later-MAD2 map reaches flash entry `0x200040`.
- Static scans recover 670 direct MAD2 accesses and the 18-entry CCONT
  descriptor vocabulary at flash address `0x00489b74`.
- The initial conservative run stopped polling GENSIO status at `0x003d7982`.
- The byte-write-ready contract advances beyond that loop and produces
  ordinary LCD writes with no soft reset.

## Current boundary

The supplied image ends at CPU address `0x54ffff`. Public DCT3 flash maps place
NAM-1's PMM at `0x5f0000`; the wider product-state allocation begins at
`0x550000`. The firmware archive contains MCU and PPM streams but no PMM.

That missing product-matched data is the present boundary. Service init creates
a 24-byte result table at `0x17fbe0` and initially sets service-status bits 6
and 7 at `0x269696..0x26969e`. Two calibration checks then fail:

- item 18 is reported as `0x12` through `0x387030 -> 0x2696dc`. Validator
  `0x38701e` calls logical-storage reader `0x2c6b2c`, which sums bytes
  `0x000..0x11b` of a `0x120`-byte calibration record and compares the result
  with the big-endian stored checksum at `0x11c`;
- item 12 is reported as `0x0c` by the independent calibration path at
  `0x2694e4..0x26974c`. The complete predicate is recovered below.

The result scan clears service-status bit 6 at `0x269754`. Supervisor selector
`0x378784` therefore omits its second resume batch, including task 19
(`0x13`). The dormant chain is now bounded from that selector through the SIM
task entry at `0x2fb194` and SIMI initializer at `0x2fa6d0`; keypad and radio
application work are excluded by the same product-readiness decision.

A passive flash-bus census still observes no *direct CPU* read or write in
`0x550000..0x5fffff` during the bounded run. This remains a useful observation
and is protected by `make verify-3610-storage-boundary`, but it is not evidence
that persistent state is unused: the validators read a firmware-owned logical
cache through `0x333438` rather than directly addressing flash.

A deliberately noncanonical NHM-6 PMM experiment confirmed both sides of the
boundary. Loading it at `0x5f0000` makes item 18 pass, proving the placement,
record form and loader path, but item 12 still fails and the handset resets.
The remaining calibration is product-specific. Do not ship a sibling PMM or
recompute checksums over foreign values; faithful progress requires a NAM-1
PMM dump, preferably the complete `0x5f0000..0x5fffff` active block (or the
wider `0x550000..0x5fffff` product-state region).

### Item-12 calibration predicate

Function `0x2694e4` registers object `0x6509`, then reads and sums the
`0x154`-byte logical calibration block `0x120..0x273`. Accessor `0x36f6e6`
independently reads the big-endian 16-bit word at logical offset `0x174`.
The function subtracts each byte of that embedded word from the block sum,
modulo 16 bits, and publishes the resulting residual through `0x6509`.

Startup then reads two more big-endian words directly:

- `expected = logical_word(0x274)`;
- `guard = logical_word(0x190)` (inside the summed block).

Item 12 passes only when `residual == expected` and `(guard | residual) != 0`.
Otherwise `0x26974c` stores result `0x0c` and `0x269754` clears service-status
bit 6. This is intentionally stronger than a conventional trailing checksum:
an erased or fabricated all-zero calibration block fails the nonzero guard even
though its arithmetic residual is zero.

For a recovered NAM-1 PMM, the acceptance recipe is therefore deterministic:
load it through the normal PMM catalogue, verify item 18's `0x000..0x11f`
record, then verify the item-12 predicate above before attempting a boot. The
values themselves remain opaque analogue/product calibration and must not be
borrowed from another handset merely because its checksums can be repaired.

Sources for the physical partition bounds and service-tool interpretation:

- [Nokia DCT3 flash-address table](https://www.nokia-tuning.net/index.php?s=flashadress)
- [UFSx/Tornado service manual](https://www.gsm-support.net/download/UFSxTornado_Manual_GSM-Support.pdf)
- [Preserved Twister service-software collection](https://archive.org/details/Twister_Mobile_Phone_Flashing_Box)
