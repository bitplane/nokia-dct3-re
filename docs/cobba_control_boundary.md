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

`verify-5110-save-state` now uses `c54x_rom4_codec_restore.lua` to observe
pre-save/post-load boundaries, comparing emulated time, DSP PC, ST0, ST1, SP
and the non-destructive I/O `21` and serial-control data `22` words exactly. `--restore-log LOG` rejects a
missing, duplicated or changed snapshot pair and a failed round-trip harness.
This check supplements, rather than replaces, physical Menu input and the
exact Phone book frame. The fixture does not write DSP registers or assert
that an idle snapshot covers an active codec transaction.

### Operational data-space sample path

The acquired resident ROM supplies a distinct serial ISR entry, independent
of I/O `21`. At `ff80`, `ST #ffa8,PMST` selects vector page `ff80`; entry
`ffd0` (vector index 20) delayed-branches to `3416`, with context-save slots.
Within that routine, the bit test on data `06be` mask `2000` selects a direct
copy branch: `LD data[0020],A; STL A,data[0021]` at `342d..3430`, then a branch
to the common epilogue. The other path ultimately executes `STLM A,0021` at
`358a`. These are actual data-space output instructions, not DSP I/O `21`
sample writes. The meaning of the `06be` bit and all intervening processing
branches remains unclassified.

The resident tone initializer at `a598` separately reads shared data `0856`,
masks bit 0 and conditionally selects its disabled tail at `a5e2`. This
identifies an organic command input to trace, not permission to set the cell.
The codec contract checker covers the vector-page setup, vector entry, direct
copy, accumulator publication and tone-gate words. Its mutation tests apply
to these signatures too. This static evidence does not prove that vector 20
is exercised continuously in ordinary idle, define its external pin/cadence, or promote
native microphone/earpiece PCM. The next attachment must establish the
firmware's serial readiness/enable contract before introducing a codec clock;
neither a fixed interrupt cadence nor shared tone-cell writes may be added
merely to make this routine run.

TI's CPU Reference Guide tables 6-24/6-25 identify vector 20 as BRINT0
and vector 21 as BXINT0 on the buffered-port variants. This corroborates the
local vector-20 receive handler; it is not a measured MAD2 pin assignment.
The coherent native idle mask `035f` enables source bit 4 (vector 20) but
not source bit 5 (vector 21). Therefore transmit-ready changes alone cannot
be assumed to drive the observed sample ISR. The next attachment audit must
establish the receive frame source and its ready edge, with the BSP receive
enable and firmware interrupt mask observed independently. IMR/IFR are
CPU-internal MMRs in this implementation and bypass data-space taps; an
empty tap at data `0000/0001` would not prove absent mask activity.

The native endpoint routes each accepted receive-ready edge to source bit 4.
The existing external boot echo consequently exercises vector 20 after the
firmware's own polling read: BDRR retains `0aaa` for the ISR read at `3464`,
and the real processing tail writes `ffd5` at `358b`. No second receive edge
or periodic clock is synthesized. This exposed the genuinely executed
`9a5f` at `346f` (`STH A,15,*AR3-`), now implemented from TI SPRU172C
section 4-169 with 128 executable shift/source/address/SST cases. The CPU
suite, coherent boot, physical tone delivery, exact save/load and unchanged
Phone book hash validate the route. This is a boot-ISR execution result,
not continuous codec playback or speech.

An accepted boot transfer following a DXR write also raises BXINT0 (source
bit 5, vector 21) on the XRDY rising edge. The physical-key observation
retains `IFR=0020` with `IMR=035f` through the 11-second run: this request
is pending but masked, not a second sample ISR. Retained-word underrun
frames without a preceding DXR write must not generate another XRDY edge.
The tone gate checks this pending/masked observation independently of its
unchanged two TX/two RX boot accesses; continuous frame attachment remains
unimplemented.

The native backend exposes completed-word `codec_transmit_frame` and
`codec_receive_frame` attachment methods. Both the existing boot echo and
a future physical frame source use the same ready-edge interrupt routing.
Transmit underrun repeats the retained word without raising BXINT0 again;
disabled/unloaded TX and RX overrun do not manufacture ready edges. The
executable BSP helper tests distinguish these cases. These methods provide
no timing source and do not enable the currently unattached NSE-1 PCM clock.

The physical `1` fixture now retains separate boot/interactive serial trace
caps and observes IMR/IFR plus non-destructive BSPC readback. At idle,
`IMR=035f`, `IFR=0020`, `BSPC=c8c8`: RRST/XRST are released, XRDY is set,
and RRDY is clear. At 8.076920923 s the organic tone initializer writes
`c008` at `a4a7`, then `c0c8` at `a4a9`; its COBBA control traffic writes
register 0 with `0000` and reads/re-writes register 8 with `0626`. Later
release handling repeats the BSP reset/release sequence. Counts still show
the boot echo plus its one receive-ready ISR and no post-key samples. Thus this
fixture does not lack BSP release or the receive interrupt mask; it lacks
observed receive-frame delivery. The COBBA register-0/8 field meanings and
clock/mux gating remain unresolved, so these numeric transactions are not
permission to fabricate microphone samples or assert a periodic interrupt.

The resident register-8 pair is independently checked from ROM words:
`a4a5` resets/releases BSPC; `a4b4` reads selector 8 through `4610`, ORs
`0600`, and writes through `45c2`. Conversely, `a502` resets BSPC, reads
selector 8, ANDs `09ff` (clearing those two bits), and writes it back. Thus
`0600` is a paired firmware activation mask, distinct from the self-test's
additional `0010` echo bit. These instructions do not identify the two
bits as oscillator enables, converter enables or routing controls. The
checker protects all three sequences against word mutations.

The own-product manual's pin table (pp. 3-40/3-41) identifies MAD2 pin 135
as PCMTxData/DX, pin 137 as PCMRxData/RX, pin 138 as PCMDClk/CLKX and pin
139 as PCMSClk/FSX. This strengthens the external transmit-clock/frame
attachment, but does not separately identify a CLKR/FSR wiring or internal
MAD2 clock fan-out. Do not import another COBBA revision's clock rate or
assign receive timing from a pin name alone. The public manual documents
rates and connectivity, not the register-8 decode or frame edge/pulse width.

The extension-register observation retains data `0023` taps alongside BSPC.
In the coherent physical-tone fixture no write to `0023` is observed and
its saved backend latch remains `0000`. Under the generic BSPCE definition
in TI section 9.3, zero selects no autobuffering, default clock polarity
(receive on falling edges, transmit on rising edges), and active-high frame
sync. BSPC `c0c8` selects external clocks/frame sync, burst transfers and
16-bit words. These are compatible with the own-product PCM interface;
they do not establish the undocumented MAD2 receive-clock fan-out or prove
that its register reset matches every generic C54x variant. Also, FSM=1
requires a frame pulse per word, not specifically a one-clock-wide pulse:
TI section 9.2.4 explicitly supports longer pulses with delayed shifting.
The PCM wire's current one-clock profile constraint must not be mistaken
for an independently measured NSE-1 sync width.

### Organic tone request boundary

`tools/c54x_rom4_tone_observe.lua` combines the physical-input harness with
passive MCU/DSP tone-cell taps and the serial-word observer. ARM taps span the
complete 32-bit bus word `100ac..100af` and retain the raw byte mask; the upper
halfword corresponds to DSP shared cell `0856`, not its neighboring parameter.
Boot and interactive records have separate caps, and final serial counters
are uncapped.

In a fresh eleven-second NSE-1 run, physical key `1` at 8 s produces an MCU
upper-halfword `00e1` write at 8.076714462 s, followed by native reads of `0856`
and a copy to data `00fe` at `a5de` by 8.076919730 s. Release at 8.22 s is
followed by control `00e0`, then `0001` and its native initializer copy. Total
tone-cell reads/copies are 29/7, while total data-space serial writes/reads are
2/2: the boot `0aaa` exchange and its receive-ready ISR's retained-word read
and computed `ffd5` output, no post-key operational samples. `--tone-log LOG`
checks the physical key, ordered post-key command/initializer, intact boot
echo, control readback and uncapped final counts; it refuses to use an earlier
boot initializer as evidence of a key response. These observations rule out
missing MCU tone delivery for this fixture, not missing serial clocks on real
hardware or tone support in other images.

The next native implementation boundary is therefore serial enable/readiness
and externally clocked word delivery, not a synthesized tone command. TI's
[C54x Applications Guide, BSPC configuration example](https://www.ti.com/lit/ug/spru173/spru173.pdf)
(section 3, p. 3-38) defines receiver/transmitter reset bits and receive/transmit
ready flags separately. That primary reference supplies generic peripheral
semantics; it does not establish the MAD2/COBBA frame-clock rate, pin mapping,
or whether every native data `0022` write is the generic BSPC interface.
Keep those product attachments explicit before promoting operational audio.

### NSE-1 physical clock contract

The [NSE-1 System Module manual](https://www.eserviceinfo.com/preview_html.php?fileid=26879&previewid=13251)
(03/98, pp. 3-32--3-33) supplies product-specific clock evidence: COBBA
divides the 13 MHz reference by 13 to generate a 1 MHz PCM data clock, then
by 125 for an 8 kHz sample/frame clock. Its diagram shows a 16-bit word with
13 converter bits and sign extension. This is not the 520 kHz profile used
by other product-family attachments. The same manual specifies a 13 MHz DSP
reference multiplied internally to 52 MHz, corroborating the native backend
clock rather than leaving it solely as a co-simulation pacing calibration.

`PRODUCT_5110` now records those four PCM properties. Sync width and transfer
edge are not promoted from another handset; the unsupported shape remains
inert and the native backend is not attached to the HLE block-transfer path.
Thus these constants supply the physical clock boundary for future native
serial work, not a periodic interrupt source or proof of audio output. The
native tone fixture still requires serial readiness/frame delivery to be
modeled separately.

### Serial-control ownership correction

TI's [C54x CPU Reference Guide](https://www.ti.com/lit/ug/spru131g/spru131g.pdf),
section 8.2 tables 8-2 through 8-7, maps data `20/21/22` to BDRR0/BDXR0/BSPC0
on the relevant buffered-serial variants. Section 9.2 distinguishes writable
configuration from read-only readiness/clock-pin fields and defines reset
and frame-sync behavior. Combined with the local ROM's `c008 -> c0c8`,
BDXR write and BDRR poll, this corroborates the serial-port interpretation;
it does not corroborate a COBBA parallel register-C transaction.

The native backend owns data `20/21/22` through a saved standard-mode
completed-word state model. Writable configuration is masked with `c0ff`;
RRDY, XRDY and receive-overrun status derive from receive, transmit and reset
transitions rather than firmware writes. An unread receive word is preserved
on overrun, reception resumes after its acknowledgement, and transmit
readiness returns at an explicit frame transfer. Executable C++ tests cover
transmit overwrite before a delayed frame and retained-word retransmission
on externally clocked underrun (TI section 9.2.4): repeated frames do not
invent a new XRDY transition. Reset cancels the valid loaded-word state;
reset-time preloading remains outside the established subset. FO=0 16-bit
words and FO=1 8-bit words are tested over all 65,536 input values: the
transmit latch retains its upper byte while the wire ignores it in 8-bit
mode, and BSP reception sign-extends the low byte (TI section 9.2.2).
The extension-selected 10/12-bit formats remain unimplemented. The suite also
checks all 65,536 control-write values and the state transitions. Native coherent
boot, 30-second processing (6,033 mode-1 frames), organic physical-key tone
command delivery and exact idle save/load pass with this state model. The
tone run observes only the boot echo and its one receive ISR, not continuous
or post-key operational samples.

This remains partial hardware: clock-pin status, transmit-shifter status,
autobuffering, reset-time transmit preloading and physical frame/bit timing
are not established. Only the separately evidenced external boot echo
provides an untimed transfer; ordinary pending samples do not acquire a
fabricated frame or interrupt. No operational PCM is claimed.

The coherent gate now checks real serial reset/release and echo observations
instead of expecting synthetic COBBA parallel register-C logs. Data `0032`
still uses the legacy forwarding path: the generic TI assignment varies by
part, so that address needs its own local sequence/variant evidence, not a
shared "interrupt-masked alias" assumption. Before attaching the 8 kHz frame
clock, establish the remaining enable, frame and interrupt-routing contract
and validate word timing. MFI port mapping remains an independent boundary; do not
double-drive both owners to preserve an old log predicate.

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
