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
first exact discovery packet. It does not enable an application responder.

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

Recover the request-derived D0 discovery response and subsequent application
registration contract reached behind the now-gated DSP service boundary.
Keypad, SIM, persistent storage, external-service and radio contracts remain
disabled until product-local evidence establishes each one.
