# Nokia 8855 / NSM-4 admission

## Current boundary

Own v5.18 MCU and PPM regions are normalized and statically qualified. No
machine profile, graphical boot, input, storage or radio acceptance is claimed.
The next question is the product's peripheral register contract and the
consumer of its DSP catalogue, before choosing any existing device composition.

Reproduce the static report:

```sh
.venv/bin/python tools/nsm4_static_check.py roms/noki8855/v518/nsm4_5.180.bin
```

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

Selector 27 points at `0x4019d8`, header
`0f00 0000 00f6 0f00 00dc 0000`. The 246-word bootstrap payload has SHA256
`320c8b9c163e13491e0b867b24c2966e10bc51ad96beb2329e6d43eb32d8a9e2`.
At payload byte offset `0x18`, `76f8 0803 0006` writes identity 6 into
DSP data word `0x0803`. The stream includes direct CALL `f074 861f`.
Neither the identity nor this resident-code dependency proves compatibility
with a sibling mask. No resident mask is provided by the service archive.

## Admission constraints

The MCU contains SIMI initialization at `0x385874` matching the instruction
fragment `2138 20ff 5508 20c0 7220`. This alone is not a complete controller
or board contract. Older 8xxx exact CCONT/LCD/keypad fragments did not match;
that is a failed signature search, not evidence that those peripherals are
absent or incompatible. Recover their own instructions and literal bases.

The service regions end at `0x580000`; no PMM or full-flash tail was acquired.
Do not borrow EEPROM/PMM, fabricate DSP completion, or assign a sibling board
profile merely to obtain a screen. Peripheral recovery, catalogue-consumer
recovery and an explicit erased-storage boot can proceed independently of
native mask acquisition.
