# Nokia 8855 / NSM-4 admission

## Current boundary

`noki8855` is a fail-closed MCU research instrument. Two independent cold
runs of own v5.18 MCU/PPM-P create identical storage headers, install the
recovered catalogue and flash callbacks, and reach the same DSP ownership
wait. Graphical boot, firmware input delivery, SIM, radio and phonebook
acceptance are not established. No DSP acknowledgement or donor PMM is supplied.
The next question is the exact upload/release contract at the shared `0fe`
wait's missing resident-code dependency, not a guessed completion value.
The own upload/release sequence is now qualified below. Fitted flash identity
remains unproved.

Reproduce the static report:

```sh
.venv/bin/python tools/nsm4_static_check.py roms/noki8855/v518/nsm4_5.180.bin
```

## Erased-storage execution

Generate the own contiguous prefix into a fresh file (no fabricated tail):

```sh
.venv/bin/python tools/nsm4_archive.py roms/archive-dct3-packages/nsm4_518.zip --image roms/noki8855/8855f518p.fls
```

The `0x380000`-byte result has SHA1
`8f740f901f51ea60dcca560de011891eb8026f8a`. The machine supplies an erased
tail in a capacity/command-compatible Intel TE28F320 research device; this is
not identification of the fitted part. Boot and resident DSP masks are
`NO_DUMP`; the shared MCU boot-transfer HLE remains explicit. The generic
512 KiB RAM window is a research allocation covering own stack `146a24` and
RAM programming code through `15a8xx`, not a fitted SRAM/alias claim. The host
matrix reuses matching logical positions, not another handset's provisioning
or physical side-key legends. The socket is empty and all DSP/service/radio
completion models remain disabled.

After `make build`, reproduce in a fresh isolated directory:

```sh
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame --run-dir tmp/nsm4-new -- noki8855 -rompath /data/gaz/src/nokia-dct3-re/roms -noreadconfig -video none -sound none -nothrottle -log -seconds_to_run 6 -autoboot_delay 0 -autoboot_script /data/gaz/src/nokia-dct3-re/tools/nsm4_boot_observe.lua
.venv/bin/python tools/nsm4_boot_check.py roms/noki8855/8855f518p.fls tmp/nsm4-new
```

The read-only observer samples seconds 1 through 5. Each sample has catalogue
selectors 0/27 equal to `3fae04/4019d8`, callbacks `3e0499/15a6b1`, DSP
control `01`, shared words `10000=0000`, `10002=ffff`, and PC in
`33e2e0..33e2e4` with LR `33e2d9`. This is an unmet DSP ownership boundary,
not a graphical boot or completed upload. The complete resulting 4 MiB flash
SHA256 is `7bc92e6b3bdedbc26a28e134b74909f478f2c5953020ff4f8681a7dd99c0a2fb`.
Only 83 tail bytes differ from erased state, including `PMM` at `590006`
and `EEPROM` at `5e0006`; every acquired MCU/PPM byte is unchanged. Captures
`tmp/nsm4-erased-boot` and `tmp/nsm4-erased-repeat` independently agree.

Collection provenance and the independent NSM-4 product identity are in
`roms/README.md`. The complete normalized MCU SHA256 is
`bf80e6961d37e071c73b0ec5eba448889b4134fa080dfd7f4a6dd04f3a69aac2`.
All addresses below refer to this own image, in big-endian CPU byte order.

## Startup and DSP declarations

ARM startup begins at `0x200040`, switches to Thumb at `0x200148`, and calls
`0x2ea064`, `0x23f900`, then `0x3ab530`. Its vector words at `0x2001dc`
are copied to address zero: decoding their relative branches at their flash
storage addresses instead of their execution addresses gives spurious targets.

The catalogue initializer at `0x3e8e60` declares `0x74` bytes for RAM
`0x13a9a8`: 28 nonzero descriptor pointers followed by a zero terminator.
The checker bounds and hashes all 28 declared payloads, retaining zero-length
entries. Declared destinations are not proof of program-space installation,
relocation, selection or runtime transfer.

The consumer at `0x33e95e` loads table RAM `0x13a9a8` from pool `0x33eb98`,
indexes it with selector times four, copies five descriptor halfwords to its
working header, reads the payload length from descriptor `+4`, and advances
the source pointer by twelve. This proves an indexed use of the catalogue,
not that a particular selector executes or that the DSP acknowledges it.

Selector 27 points at `0x4019d8`, header
`0f00 0000 00f6 0f00 00dc 0000`. The 246-word bootstrap payload has SHA256
`320c8b9c163e13491e0b867b24c2966e10bc51ad96beb2329e6d43eb32d8a9e2`.
At payload byte offset `0x18`, `76f8 0803 0006` writes identity 6 into
DSP data word `0x0803`. The stream includes direct CALL `f074 861f`.
Neither the identity nor this resident-code dependency proves compatibility
with a sibling mask. No resident mask is provided by the service archive.

## Own uploaded execution

`nsm4stage` separately executes the own uploaded program with the existing
C54x research executor. It supplies only selector 15's 104-word fragment
from `0x4007fc` and selector 27's 246-word verifier, never a sibling mask.
The MCU's `0x33e234..0x33e332` sequence selects catalogue RAM slot 27,
copies the payload, writes release fields
`0100/0300/001c/0000/0001/0001/0200` at `100f6..10102`, and requests
verification of `dff` full blocks from `200040` in `400`-byte strides through buffers
`10200/10600`. Ownership alternates through `100fe/10100`; the ordinary
silent-peer machine remains in its first `100fe` wait. These fields and
literal roots are independently pinned by `dsp_upload_contract`.

Reproduce the native negative boundary in a fresh directory:

```sh
.venv/bin/python tools/nsm4_native_boundary_check.py mame/mame roms tmp/nsm4-native-new
```

The own verifier starts at DSP `0f00` at 0.241519 s, writes `0006` to
immutable fragment word `ff87` at PC `0f12`, then executes its direct
`CALL 861f` at `0fd6..0fd7` (payload offset `1ac`). The executor stops on
unavailable program `861f` with PC `8620` at 0.243103 s. The checker requires
that precise controlled termination and rejects publication, loader handoff
or a different release. The 13 MHz cadence is an explicit research assumption,
not measured NSM-4 timing. No code at `861f` or return side effects are supplied;
the missing resident program remains a native execution boundary.

Public-reference check: the [parallel emulator's model inventory](https://github.com/djr-747/nokia-dct3-emulator/blob/main/README.md)
reports an NSM-4 v5.13 HLE standby result. That is a software lead, not a
resident-mask source or an independent native verifier oracle. No matching
ROM6 image was located by the bounded public `DCT3 ROM6 DSP ROM dump` and
`8855 NSM-4 DSP ROM dump` searches; this is a search result, not a proof of
global absence. Further native progress requires an independently qualified
source for the called program, rather than importing HLE completion values.

## Admission constraints

The own register contract is recovered from instruction fragments together
with their literal pools, checked by `peripheral_contract`:

| Consumer | Own evidence | Firmware-visible contract |
| --- | --- | --- |
| CCONT | `3ca4f4`, `3ca51a`, pool `3ca6a4` | Select `2d=22`, write `2c`, receive `6c`, status `6d` bit 2 |
| LCD | `3cda62`, `3cda6a`, `3cdad6`, pool `3cdc4c` | Select `2d=21`, command `6e`, data `2e`, status `6d` bit 0 |
| Keypad | Entry `3c0e68`; `3c0e9a`, `3c0eb2`, `3c0f2a`, pool `3c1150` | Row output `28`, direction `a8`, columns `2a`, raw index `row*5+column` |

Offsets are relative to MAD2 `0x20000`. The keypad routine manipulates row
bits 1 through 4 and tests five column bits; physical key labels require its
separate translation table. The header descriptor `fd` decodes to two-byte
EEPROM addressing, 64-byte pages and 16 KiB capacity; it does not identify a
fitted part or establish SDA/SCL wiring. Serial baud, ADC latency, electrical
polarity and LCD geometry do not follow from these instruction checks.

The logical decoder at `0x3c5fa4` calls scanner entry `0x3c0e68` (not the
interior prologue at `0x3c0e72`). Its layout selector is RAM `0x13de7b`.
Layout zero uses 25 normal entries at `0x45dfd4`, in raw-index order:

```text
3e 3e 3e 3e 3e
11 19 01 02 03
0e 17 04 05 06
0f 18 07 08 09
10 1a 0c 0a 0b
```

The special five-entry table at `0x45dff0` is `3e 3e 3e 3e 0d`, mapping
raw `0x84` to power logical `0x0d`. These are own firmware logical values;
runtime key delivery and all physical legends remain separate acceptance.

## Storage backend and display layout

Do not infer an external I2C EEPROM from header descriptor `fd`. Its decoded
geometry above is only a descriptor interpretation; the actual storage path
must be recovered independently. At `0x3383b8..0x3383c0`, firmware calls
`0x38952e` with `r0=0`, `r1=1`, `r2=c9` and `r3=0x338684`, an `EEPROM`
string. The byte-write routine `0x389212` performs word read/modify/program
through indirect callbacks: `0x389250` obtains its read pointer from RAM
`0x13c45c`; `0x38927e` obtains its program pointer from `0x13c458`.
The initializer at `0x3dbb52` selects one of two callback sets according to
its preceding identification result (values 1/3 versus other values). Both
sets install program pointer `0x15a6b1` (Thumb RAM entry `0x15a6b0`); read
pointers are `0x3e0499` and `0x3e04fd`. Both own read routines are exactly
`8800 46f7`: `ldrh r0,[r0]; mov pc,lr`. Thus this storage read path is
memory-mapped halfword access, not GPIO I2C. Identification result semantics,
particular fitted flash and PMM location remain unmapped; no particular part
follows from a callback address.

The copy roots at `0x3e04b0` and `0x3e0514` install respectively 120 bytes
from `0x45e8fc..0x45e974` or 136 bytes from `0x45e974..0x45e9fc` at RAM
`0x15a6b0`. The checker pins both complete payloads and copy roots. Both
limit programming to the half-open window `0x580000..0x600000`, use command
address `0x5fff00`, issue `50` (clear status), `40` (word program), write the
data halfword, poll status bit 7 and restore array mode with `ff`. The second
variant additionally issues `60/d0` before programming and `60/01` afterward.
This is an Intel-style command-family requirement, not a fitted-device ID.
The routines also inspect MAD2 `0x20073` bits `0c`; their physical protection
meaning is not established. The acquired MCU+PPM prefix ends exactly at the
lower programming bound; the archive does not provide those writable bytes.

The initializer at `0x3cdae6` selects LCD channel `21`, pulses GPIO bit 3,
then the `0x3cdb50..0x3cdbae` loop writes command `24`, selects banks with
`40|bank` and column `80`, writes 84 zero data bytes per bank, iterates six
banks and writes final command `20`. This establishes an 84x48 command-space
clear layout. It does not identify the physical panel, prove contrast or
refresh timing, or qualify pixel rendering in MAME. Its own MMIO literal is
`00020000` at `0x3cdc50`.

The conservative literal-seeded MMIO census, with constant-index recovery,
resolves 798 candidate accesses from 276 seeds (247 return, 16 decode-gap,
13 span-limit terminations; 10 seeds without an access). This is linear static
candidate coverage, not whole-program control-flow proof. Its byte input is
the normalized MCU with each 16-bit pair swapped, as required by
`mad2_static_census.analyze_image`; the checked fragments above use unswapped
big-endian CPU bytes. Do not interchange those representations.

The MCU contains SIMI initialization at `0x385874` matching the instruction
fragment `2138 20ff 5508 20c0 7220`. This alone is not a complete controller
or board contract. Older 8xxx exact CCONT/LCD/keypad fragments did not match;
that is a failed signature search, not evidence that those peripherals are
absent or incompatible. The own instructions above supersede that search as
the register-contract evidence; no older product's GPIO layout was inherited.

The service regions end at `0x580000`; no PMM or full-flash tail was acquired.
Do not borrow EEPROM/PMM, fabricate DSP completion, or assign a sibling board
profile merely to obtain a screen. Peripheral recovery, catalogue-consumer
recovery and an explicit erased-storage boot can proceed independently of
native mask acquisition.
