# Nokia 6210 bring-up

## Current boundary

NPE-3 v5.56 PPM C reaches the final DSP verification wait at
`0x426cc2..0x426cc8` after 232 alternating buffer handoffs. The product-local
profile models the independently decoded MAD2 DSP release/ready and GENSIO
contracts. Final DSP completion is not published. Boot/UI, SIM, network,
calls/audio and handset save/load are not yet validated.

## Inputs

`roms/noki6210/6210_556c.fls` is 0x3a0000 bytes with SHA-1
`3d9ea319503e78ec69b60d72cda23e461e118ea9`. The product-local PMM tail is
0x6000 bytes with SHA-1 `b3a527ede1be87bd715fb3741a81eef5bd422efa`, mapped
at region offset `0x3fa000`. Sources and archive hashes are in
`roms/README.md`. Uniform-fill DSP audit files are not executable ROM6 evidence.

## Hardware contracts

- At `0x4dc0e4`, firmware writes CTSI offset 2 with `0x40`, then sets bit 2
  and polls bit 4. `LSRS #5` tests original bit 4 through carry, not bit 5.
  The profile exposes that ready bit only when the release line is asserted.
- At `0x4ec7b0`, GENSIO control `0x22` selects CCONT; a write to offset
  `0x2c` precedes polling status offset `0x6d` bit 2 at `0x4ec7bc` and
  reading offset `0x6c`. Control bit 2 stays clear, so receive-ready follows
  the byte write rather than a control-bit trigger. The LCD uses `0x2e/0x6e`.
- The unvalidated 6250 retains its previous generic composition; it does not
  silently inherit the new 6210 contract through a shared machine config.

## DSP upload

The pointer at RAM `0x170070`, read without modifying firmware state, selects
flash descriptor `0x225c2c`: `0f00 0000 00df 0f00 00dc 0000`.
The 223-word program following it has SHA-1
`6646da3c5be9c70deda7e0b5b9f257d5d2ace815`, identical to the stock NSM-3
staged verifier. Identical code does not establish identical final results.

The MCU initializes DSP buffer descriptors with remaining count `0001:d000`
and block size `0200`. The loop at `0x426c58` sends 231 full blocks, followed
by 510 samples and two `ffff` terminators. Flash source advances by `0x20`
per halfword. Shared offsets `0fe/100` alternate ownership, with 116 writes
of zero to each. The HLE acknowledges transport ownership but supplies no
final verdict. At `0x426cc2`, firmware waits for shared offset 2 to leave
`ffff`, then stores the result pair in its own bootstrap state.

The next question is the staged program's final publication under NPE-3's
ROM6 memory/peripheral contract and larger input stream. The NSM-3 fixture's
explicit ROM-version and COBBA assumptions must not be promoted as measured
6210 values. The collaborator's self-test responder is a later request/reply
lead, not evidence for this earlier bootstrap completion.

## Acceptance

`make verify-6210-bootstrap` performs a fresh isolated run and checks CCONT
receive-ready, all 232 ordered handoffs, no reset and the uncompleted final
wait. It is a frontier gate, not usable-phone acceptance. Generic harness
task/mode RAM addresses still describe the 3210 and are not NPE-3 semantics.
