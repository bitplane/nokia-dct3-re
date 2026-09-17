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
exact frame together with organic LCD activity, absence of soft resets and the
absence of a fabricated DSP-to-MCU completion.

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

Identify the first DSP bootstrap or service transaction reached behind the
now-gated display boundary.
Keypad, SIM, persistent storage, external-service and radio contracts remain
disabled until product-local evidence establishes each one.
