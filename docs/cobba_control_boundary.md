# COBBA control boundary

## Current result

The current COBBA device models a 16-address, 12-bit control-register file and
the analogue conversion endpoints. This is not a claim that every COBBA
revision uses the same physical bus. MAD2 owns the typed PCM wire. The DSP backend
owns the control policy. The MCU ROM does not directly address COBBA,
and the current DSP HLE does not fabricate COBBA control writes.

Consequently, microphone/output selection and gain remain explicit HLE profile
data. They are not decoded COBBA register semantics. The production driver
must not translate MCU call state or Nokia mailbox values directly into COBBA
register writes.

Nokia's NSE-1 System Module manual (03/98, pp. 3-11, 3-32 and 3-41)
distinguishes the physical interfaces: the parallel connection has twelve
data lines, four address lines, read/write strobes and a data-available signal;
audio PCM has separate serial data, clock and frame-sync wires. The parallel
connection carries control and transmit/receive samples. See the
[Nokia manual text](https://www.eserviceinfo.com/preview_html.php?fileid=26879&previewid=13251).
The recovered DSP control-word packing is not evidence that this physical
control bus is serial. Nor does its twelve-bit width establish how MAD2
presents a sample at DSP I/O port `0x27`: address selection, sign extension,
word ordering and readiness still require a mapping or timing capture.

### Product-specific physical buses

Do not generalize the NSE-1 parallel connection across DCT3. Nokia's
[NSB-5/7190 System Module manual](https://manualmachine.com/nokia/7190/5055290-service-manual/)
(03/01, pp. 19, 37-38 and 42-43) describes serial connections between
COBBA_GJP and MAD2WD1. Its pin tables distinguish control data/select
(`COBBASDa/COBBACSX`), separate bidirectional I/Q sample lines
(`COBBAIDa/COBBAQDa`), and the audio PCM wires. COBBA also has mode-dependent
pin assignments; the listed I/Q pins are not proof of the active mode in
another handset. This corroborates a revision/product distinction, not a
replacement register map for NSE-1 or a serial bit format for all products.

The attachment methods below are logical transaction boundaries, not an
implemented serializer for either RF bus. Promoting native RF support on
another product requires its own bus/mode evidence plus the MAD2-to-DSP
mapping. Neither service manual establishes DSP port `0x27` sample packing,
sign extension, interleaving or ready/clock behavior. Keep these contracts
separate from the already typed audio PCM wire.

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

MCU images do not directly expose DSP-local COBBA register traffic. The native
NSE-1 C54x backend now exposes boot/self-test transactions, but this does not
close the mux/gain contract. Operational DSP execution or physical bus capture
is still needed for those fields. The opaque register file and declared HLE
voice profile remain the current boundary.

Nokia's [NSB-6 technical documentation, page 37](https://www.manualslib.com/manual/1616779/Nokia-Nsb-6-Series.html?page=37)
describes a 24-bit hardware-random serial read from COBBA and used with
software/identity data to establish stored flash authority. This is evidence
that hardware/provisioning pairing exists in the family, not a register map
for NSE-5. In particular, neither that description nor control-bus
traffic establishes registers 5/6 as analog measurements or unique identity
fields. Their current values are calibrated inputs, not measured defaults.

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

## Native NSE-1 address-space audit

`tools/c54x_rom4_codec_observe.lua` passively taps DSP data and I/O spaces
separately, recording the first 16 accesses per direction/address without
additional device reads. A fresh four-second `noki5110` run with its own
generated EEPROM observed the following sequence:

| Space/address | Observation | DSP PC reported by tap |
| --- | --- | --- |
| Data `0022` | `c008`, then `c0c8` | `0e22`, `0e24` |
| Data `0021` | Self-test transmit `0aaa` | `0e31` |
| Data `0020` | Self-test receive `0aaa` | `0e5d` |
| I/O `0021` | Initialization writes `1482`, `1482`, `0482` | `4555`, `4559`, `455d` |
| I/O `0021` | Repeated reads `0482` / `0c82`, writes `0c82` / `0482` | `3221` / `3228`, `33f6` / `33fd` |

The PC is sampled during the memory callback, after operand fetch; it is not
automatically the instruction's start address. Reproduce with `-log`, an
isolated NVRAM directory and `-autoboot_script tools/c54x_rom4_codec_observe.lua`.
The audit's output cap limits observations, not execution.

The backend keeps I/O `0021` in its opaque saved I/O bank, independently of
data `0020/0021` and COBBA's boot echo. Firmware initializes the I/O word to
`0482`, sets bits `0c00` on frame entry and clears `0800` on exit; the observer
therefore records `0482 -> 0c82 -> 0482`. I/O writes do not transmit codec
samples. The removed mapping returned the retained `0aaa` self-test echo at
every I/O read and discarded firmware bitfield preservation. That alias was
not an evidenced microphone input. This partial register-storage model still
does not decode hardware status transitions or establish silicon ownership.
Establish operational sample source, frame cadence and readiness separately
from the boot echo before promoting native microphone/earpiece behavior. The
four-second audit does not prove absence of other register paths or validate
physical serial timing.

The local ROM also makes the bit operations explicit. At `321e`, `PORTR 21`
loads accumulator-low data cell `0008`, `OR #0c00` modifies it, and `PORTW`
writes it back. At `33f3`, the same port is read, masked with `f7ff` (clearing
bit `0800`), and written back. Initialization at `454e` builds two words in
adjacent memory and emits `1482`, `1482`, `0482` using post-decrement
addressing. `tools/c54x_rom4_codec_contract.py` checks all words in these three
bounded sequences against the acquired big-endian program image; its tests
reject a mutation at every checked word. This is not a whole-ROM ownership
census. These control-shaped sequences challenge the imported simple PCM
label but do not by themselves identify a MAD2 register or prove that no
operational sample path uses the port. Preserve the independent claim as a
hypothesis, not an implemented microphone contract.

The complete raw-word immediate-port census finds nine candidate reads and
twelve candidate writes to I/O `21`. All nine readers are covered by the
checker and perform constant mask/set operations before writing back to the
same port; the three remaining writers are the initialization sequence. The
seven additional readers are `3c6f`, `3c84`, `4231`, `4275`, `43c2`, `43ef`
and `4473`. They modify masks spanning low bits `0001/0002/0004` and higher
bits through `0800`. Thus the reviewed resident access surface is entirely
control-shaped; no sample-processing interpretation is established by these
sites. The checker rejects additional candidate sites rather than silently
claiming complete coverage after the image changes. This quantified scope
excludes dynamic port addressing, flash uploads and other product masks;
raw-word census matches alone are not proof of instruction reachability.

Passing `--trace LOG` to the codec contract checker additionally verifies the
data-space `0aaa` self-test and three ordered operational I/O readback cycles.
The correction was checked with a fresh four-second native RF-boundary run
(865 frames), a fresh thirty-second no-cell processing run (6033 mode-1
frames), and physical Menu input against the existing exact Phone book frame
oracle. These establish regression preservation and register separation, not
native speech or physical audio clocks. The I/O bank already participates in
native backend save states; no new unsaved latch is introduced.

## Physical capture option

The NSM-3 v5.31 flash-staged verifier independently uses the serial port pair
`2c/2d` to read register F around a register-D status handshake. The isolated
core fixture corroborates the existing register-selection grammar and checks
input sensitivity, not the real 8210 register-F value or its meaning. See
`8210_bringup.md` for the unresolved DSP memory contract. No production
COBBA/DSP response is promoted from those fixture inputs.

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
