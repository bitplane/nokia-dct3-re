# COBBA control boundary

## Current result

COBBA owns the 16-address, 12-bit serial-control register file and the
analogue conversion endpoints. MAD2 owns the typed PCM wire. The DSP backend
owns the serial-control policy. The MCU ROM does not directly address COBBA,
and the current DSP HLE does not fabricate COBBA control writes.

Consequently, microphone/output selection and gain remain explicit HLE profile
data. They are not decoded COBBA register semantics. The production driver
must not translate MCU call state or Nokia mailbox values directly into COBBA
register writes.

## Implemented capture seam

`nokia_cobba_device::control_data_w`, `control_select_w` and
`control_data_r` are the attachment contract for a future real DSP backend or
physical trace replay. With verbose logging, every select transaction records:

```text
cobba: control sequence=N direction=read|write address=A data=DDD t=T
```

The sequence and read/write counters are saved device state. The conformance
gate checks ordered writes, non-destructive reads, 12-bit data masking and
four-bit address selection without assigning meanings to any register.

## Evidence required for promotion

A trace intended to replace an HLE voice profile must include ordered COBBA
transactions across these boundaries:

- DSP reset and audio initialization;
- traffic-channel setup before speech starts;
- physical Answer and the first accepted PCM block;
- volume and hands-free route changes;
- physical End, channel release and return to idle; and
- save/load or reset while a route is active.

The same transaction pattern must be corroborated by a second ROM using the
same COBBA revision before it becomes a shared register decode. Differences
belong in a typed product/DSP contract. Silence, board schematics, or the
current HLE gain values establish pin connectivity at most; they do not define
register fields.

## Remaining boundary

No supported MCU image exposes DSP-local COBBA register traffic. Closing the
mux/gain contract therefore requires a real DSP core, DSP firmware visibility,
or physical bus capture. Until then, the opaque register file and declared HLE
profile are the honest stopping point.

The public 5110 ROM4 analysis provides a software-only route when its
bring-your-own mask image is available. It identifies serial I/O ports
`0x2c/0x2d`, a distinct parallel MFI `0xCxxx` control frame, and bidirectional
PCM ports. These are separate buses and must remain separate local device
interfaces. See `djr_dsp_integration.md` for provenance, addresses and the
missing-overlay limitation.

The ROM4 boot self-test separates the two owners. It first reads COBBA control
register 8, sets `0x0610`, and writes it back. It then writes C54x BSPC
`0xc008` followed by `0xc0c8`, transmits `0x0aaa` through BDXR and polls BDRR
for the same word. Finally it clears the `0x0610` bits in COBBA register 8.
Texas Instruments' BSPC definition shows that the changed `0x00c0` bits are
RRST and XRST, enabling the DSP receiver and transmitter; DLB bit 1 remains
clear. The echo is therefore externally returned by COBBA under its register-8
test mode, not the C54x's internal digital loopback. See the
[TMS320C54x DSP CPU Reference Guide](https://www.ti.com/lit/ug/spru131g/spru131g.pdf),
section 9, for the DSP-side register contract.

`codec_serial_transmit`, `codec_serial_receive` and
`codec_serial_receive_ack` now model COBBA's side of that word boundary. The
only decoded register-8 behavior is the complete evidenced predicate
`(value & 0x0610) == 0x0610`; individual bit names remain unknown. The methods
operate on completed serial words, so a future C54x backend must still own
BSPC, BDXR/BDRR ready flags, interrupts and bit timing. The production HLE does
not call this test path.

## Physical capture option

The Nokia 5110 NSE-1 service material names factory test points for `DSPXF`,
`VCOBBA`, `COBBARSTX`, `COBBAWRX`, `COBBARDX` and `COBBACLK`. This makes a
logic-analyser capture plausible using test pads rather than soldering onto
the COBBA BGA. A fixture still needs a stable battery/bench supply, common
ground, voltage-compatible high-impedance probes, and a working phone placed
through the same lifecycle cases listed above.

The 3210 repair guide identifies COBBA clock and reset measurement points and
diagnoses parallel/serial-bus failure, but the reviewed public pages do not
yet establish equivalent exposed data/strobe pads. Do not transfer NSE-1 test
point numbers to NSE-8. Schematics or board continuity work are still needed
before proposing a 3210 probe layout.
