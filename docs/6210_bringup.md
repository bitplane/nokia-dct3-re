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

## Keypad

The NPE-3 v5.56 scanner `4f84c8` masks `6b`, drives `a8/28` and samples
`2a`. Its ordinary scan forms `drive * 5 + sense` at `4f8550..4f8556`.
Decoder `4facfa` uses the normal table at `2869b8` and the special table
at `2869d4`. The normal table is:

```text
5a 5a 5a 5a 5a
11 19 01 02 03
0e 17 04 05 06
0f 18 07 08 09
10 1a 0c 0a 0b
```

Special input bit 4 maps to Power (`0d`); the other four special entries
are `5a`. These tables independently select the same logical input layout
as the 5210, so the 6210 reuses its port declarations with a product-local
five-line controller contract and Power mask `10`, not its hardware profile.
The previous inherited 3310 input declarations and four-line default were
not NPE-3 evidence.

`make verify-6210-keypad-controller` checks both pinned ROM tables, all 20
matrix keys across all five driven lines (100 reads), and held-Power
press/release through the actual MAME controller. It is an MMIO conformance
fixture: Lua drives controller registers and host inputs, never firmware RAM
or messages. It restores the row/direction/mask registers. This proves the
configured matrix, not firmware decoding, debounce or usable menus; those
remain gated by the unresolved DSP bootstrap. The ordinary input exerciser
now selects this product's logical key layout too.

Nokia's [NPE-3 board schematics](https://altehandys.de/downloads/ser-no-6210-schematics.pdf)
(Version 1.0, 09.02.2001) show the fitted matrix switches on page 5 and
identify the MAD2WD1 ROM6 V16 part on page 4. The physical sheet's ROW/COL
names are not the input fixture's abstract drive/sense names. The archived
PDF is `roms/research/npe3/npe3-schematics-v1.pdf`, 2,656,135 bytes,
SHA-256 `559e9718a9dad703694f17d349383f75af4237b1c26cf94ecdb4aafe641f1e43`.
It does not specify the missing DSP PROM word or final HPI publication.

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

### Executable calculation and remaining inputs

`make verify-6210-verifier` runs the actual 223-word program with NPE-3's
own flash samples, remaining count `0001:d000` and 232 handoffs. It first
fails closed at the unsupported peripheral read (`port 002d`, PC `0f9f`).
Three explicit sensitivity configurations then attach the existing COBBA
register model and supply PROM word `ff87` as either 6 or 4. All calculate
fingerprint `f65a:0d46` at DSP data `04f7:04f8`, with PMST `ffa8`.
The companion 8210 fixture still calculates `c2e0:6006` over 116 blocks.

These are core-fixture calculations, not measured silicon results. At the
final publication, shared word 0 follows the supplied COBBA register-F value
(0 or `0016`), while words 1/2 follow the supplied immutable PROM value
(6 or 4). Neither is selected by the calculated fingerprint. Thus the
larger NPE-3 stream does not itself resolve the missing publication input;
ROM6 PROM mapping/content and the peripheral contract remain unvalidated.
The handset profile does not receive any of these sensitivity values.

At MCU `426cca..426cd2`, the loader stores shared word 1 at `16fff0`
and word 0 at `16ffee`, relative to bootstrap structure `16ffe4`.
The base pointer comes from literal `426ddc`. This identifies the result's
firmware-owned destination without claiming the later self-test responder
or its IRQ4 upload-completion flag supplies the earlier verifier result.

## Acceptance

`make verify-6210-bootstrap` performs a fresh isolated run and checks CCONT
receive-ready, all 232 ordered handoffs, no reset and the uncompleted final
wait. It is a frontier gate, not usable-phone acceptance. Generic harness
task/mode RAM addresses still describe the 3210 and are not NPE-3 semantics.
