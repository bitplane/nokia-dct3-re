# Nokia 8810 (NSE-6) bring-up boundary

## Current Result

The acquired v6.02 PPM A package normalizes to a complete, contiguous 2 MiB
CPU-big-endian flash image. Research machine `nse6stage` now executes its own
reset/initialization and reaches the verifier's first DSP acknowledgement wait.
There is no graphical/input/card/service acceptance yet. Other products'
provisioning and DSP publications are not evidence for this handset.

The reset stack base `0x125f30` fits the documented 256 KiB SRAM window starting
at `0x100000`; importing a 128 KiB product configuration would be incorrect.
The silent baseline waits for its first DSP acknowledgement. The separate
`nse6r4t` compatibility fixture executes acquired NSE-1 ROM4 code and reaches
CONTACT SERVICE, without proving that mask belongs to the 8810.
An explicitly diagnostic six-byte integrity correction removes the two decoded
erased-storage failures and reaches SIMI initialization, but remains blank
through 30 seconds. No usable handset or authentic provisioning is established.
The current bounded question is the product-specific source-7/source-8
input and calibration contract: their transformed samples exceed the
acquisition acceptance range. The missing
report bit leaves the input controller in state `0x10` with all keypad
columns masked.

### Report-0x14 Producer Boundary

Stub `0x2dca88` posts target `1`, code `0x14`, through `0x27b318`.
A halfword-aligned Thumb-1 BL scan of the complete `0x170000`-byte MCU
extent finds 35,774 syntactic BL pairs and one direct call to that stub,
at `0x21fba0`. This is not an indirect-call or data-driven-producer closure.
The call follows `0x294c90` at `0x21fb94` and a flag update at
`0x21fb98..0x21fb9e`; no subsystem name is inferred from the report number.

Dispatcher `0x222532` bounds its state index to `0x1d` and branches through
30 big-endian words starting at `0x222558`. Its state `0x18` target is
`0x221a1c`; state `0x1a` targets `0x221c8c`.
A fresh 12-second ROM4-compatibility run with the six-byte integrity-only
fixture observes state `0x18` at 0.459572 seconds, followed by repeated
state `0x1a` from 0.926731 seconds. The eight-second counters record 86
dispatcher selections and no hits on the report stub, `0x294c90`, or the
direct report path. Readiness remains `0x06` and report bits remain `0x0e`.
The observer is read-only; no report, column-mask change, or firmware state
was injected.

The uncapped eight-second event histogram corrects the impression left by
the first-32-event trace: it contains event `0x49` twice, `0x26` 75 times,
and `0x4a`/`0x4b` four times each (plus the initial zero event).
Receiver `0x21e00c` maps raw `0x019b` to `0x49`; both raw deliveries are
observed. They are not missing peer responses.
On a separate fresh run the `0x49` selections occur in state `0x1a` at
3.861834 seconds, state `4` at 5.317832 seconds, and state `3` at
8.725481 seconds, with countdowns four, three, and four respectively.
State `4` maps to `0x221d38`; state `3` maps to `0x221df0`.
Thus state `0x1a` is not a permanent park. Follow state `3`'s continuation,
not an invented `0x019b` response.

The signed sample used at `0x221d06` is at `0x13fe54`: the state entry
loads base `0x13fe18` and adds `0x3c`. It is not context `0x120768 + 0x3c`.
The comparison is signed against `0x01fe`, alongside the decremented
countdown. The observer now uses the decoded sample address; no physical
units or battery ownership are assigned from this arithmetic alone.

State `3` handles `0x49` by checking context bytes `+5` (countdown) and
`+0x0f`. If the latter is not one, `0x221e20` loads descriptor `0x9004`
and calls CCONT field reader `0x2dfc60`. Its command-table index `0x10`
contains `0x70` at `0x303534`; the returned status byte is masked by
`0x04` and shifted to a Boolean. A result other than one follows the
countdown-decrement path at `0x221da0`. This is an independently decoded
CCONT status-bit-2 check, not evidence that the model should assert it.

A fresh 60-second integrity-only compatibility run observes countdowns
four, three, two, one, zero at the state-3 `0x49` deliveries at 8.725481,
12.125078, 15.525316, 18.924897, and 22.325122 seconds.
Context `+0x0a` remains one and `+0x0f` remains zero at these selections;
the correctly addressed sample is `0x03ff`.
Readiness/report bytes remain `0x06`/`0x0e` through 60 seconds. Waiting
longer than the initial 12-second window does not supply report `0x14`.
At countdown zero the decoded branch reaches `0x221de0`; map its ensuing
event/state selection and the predecessor of `0x21fb94` next. Do not
replace this investigation with an asserted CCONT bit or injected report.

### Report Event and Sample Acquisition

Dispatcher comparison `0x21f7b4` routes event `0x21` to `0x21fb1e`,
which leads to the report-`0x14` posting path. Receiver `0x21e00c` accepts
raw `0x21` directly; it also maps raw `0x01a0` to the byte at `0x11eb59`.
The latter route supplies the observed repeated event `0x26`.
The read-only write watch observes initialization of that byte to `0x26`
at PC `0x294e4c`, followed by repeated `0x26` writes on the `0x21faf0`
path. This is runtime evidence, not a complete writer census.

Event `0x26` invokes sample accumulator `0x293d94`. It calls acquisition
helper `0x293cc0`, decrements the remaining-sample byte `0x11eb5e` only
on success, and accumulates two transformed samples. Acquisition reads
source 7 and, conditionally, source 8, then requires both result halfwords
in the inclusive range `0x0708..0x157c` (1800..5500).
A fresh integrity-only cold run observes acquisition return zero on all
16 captured attempts, remaining count `0x0a`, and both transformed
samples `0x19d6` (6614). Thus the high-bound checks explain the observed
failure; a missing timer message does not.

The current product uses an explicitly unvalidated CCONT ADC tuple.
Decode this ROM's source mapping and calibration before changing it.
Neither the acceptance range nor the observed numbers establishes physical
units, a valid battery tuple, or authentic EEPROM calibration. Do not sweep
inputs or edit the counter/event byte to manufacture report `0x14`.

### Source Mapping and Calibration Boundary

Source reader `0x2e0ada` uses the ten-byte selector table at `0x3033d8`:
`00 04 05 06 07 03 01 02 02 01`. Source 7 selects CCONT ADC channel 2
through `0x2e5db8`. Sources 8/9 instead return cached halfwords from
`0x121588 + 2*source - 12`; source 8 therefore reads `0x12158c`.
The request API at `0x2e0a8a` separately starts acquisition for those cached
sources; a table entry alone is not evidence of an immediate conversion.

The sample transform uses calibration fields at `0x13fe4c` and `0x13fe50`,
then the own-ROM constants 1500 and 232. A fresh diagnostic cold run records
source 7 raw `0x03ff`, gain bits `0x3f800000` (1.0), and offset bits zero.
For that observed identity case, integer scaling `1023 * 1500 / 232`
gives 6614, matching the rejected `0x19d6` sample. This grounds the current
failure in a full-scale input rather than an invented completion event.

The original NSE-6 service chapter identifies a nominal 3.6 V battery and
CCONT-controlled battery measurements (pages 3-12 and 3-26/27), but its
text preview does not identify ADC selector numbers or the conversion scale.
Do not transfer the sibling profiles' channel-2 VBATT interpretation solely
from matching firmware tables. The next evidence target is the own power
schematic/CCONT transfer specification, plus the writer of the source-8 cache.
Until then these fields establish a digital contract, not a physically
calibrated NSE-6 battery model or authenticated provisioning.

The cache writer is now identified: `0x2e09c4` loads table `0x3033e0`,
indexes by `source - 8`, calls ADC reader `0x2e5db8`, and stores at
`0x2e09d6` into `0x121588 + 2*source - 12`. The two selector bytes are
`02 01`, so cached source 8 also uses ADC channel 2, while source 9 uses
channel 1. This is a recovered writer, not an exhaustive store census.
Initializer `0x2e0b5a` sets both cache halfwords to sentinel `0x8000`.
A fresh 12-second run observes only zero-fill and those two sentinel writes;
there is no observed conversion-result store in that window.

Acquisition `0x293cc0` checks flag bit 0 before consulting source 8. Its
other branch derives the second value from the transformed first sample
minus context halfword `+0x3a` (`0x293d00..0x293d04`). Thus absence of a
cache update is not itself evidence of the current failure; do not fabricate
one. Both observed rejected samples remain `0x19d6`.

### Acquired Circuit Appendix

The original NSE-6 circuit appendix is now available locally as
`roms/research/nse6-v602/reference/03us8a3-schematics.pdf` (ignored reference
input, not distributed emulator source). It was retrieved from the public
[8810 schematic download](https://altehandys.de/downloads/ser-no-8810-schematics.pdf),
linked by the [handset archive page](https://altehandys.de/phones/phones-n-r/nokia/8810.html).
SHA256: `23885bfacb5cd8876d8e67a83860bc4ef6b063fba51f35f590c7858bf8b78a9e`;
size 866,659 bytes; 14 PDF pages; document title `03us8a3`.
The [text preview](https://www.eserviceinfo.com/preview_html.php?fileid=5448&previewid=3001)
omits the circuit drawings and is not a substitute for visual inspection.

Visual inspection of PDF page 5, printed page 3/A3-5, establishes:

| Board Fact | Evidence |
| --- | --- |
| Power-supply drawing | Version 7.0, edit 257, layout version 07, original 08/98. |
| Power-management component | N100, marked `CCONT_2F_uBGA_0.8P`. |
| Battery-voltage connection | Battery connector X100 BVOLT joins the VB supply net and CCONT VBAT input D2. |
| Separate analog inputs | N100 identifies ICHAR, VCHAR, BSI, BTEMP, VCXOTEMP, RSSI, and EAD inputs separately. |
| Charging component | N101, UBA2006T; battery/current-sense and charger connections are physically distinct. |

This confirms board wiring independently of the firmware table, but the
drawing does not label internal ADC selector numbers or conversion units.
Keep the recovered source-7/selector-2 fact separate from the VBAT/D2 board
fact until a CCONT transfer/mux specification connects them. No ADC tuple
has been changed on the strength of this drawing alone.

The original [NSE-6 tuning instructions, page 12](https://www.eserviceinfo.com/preview_html.php?fileid=5448&previewid=3007)
describe distinct default, battery-voltage, charger-voltage, battery-size,
temperature, and current calibration operations. They do not provide an
ADC raw-code transfer point. Their service-junction-box supply settings are
not battery-pin voltages and must not be substituted into the ADC model.

For the observed identity-calibration branch alone, the decoded arithmetic
admits exactly raw codes `279..850` to the `1800..5500` acceptance interval;
raw 278 is below it and raw 851 is above it. The checker tests all 1024
ten-bit inputs against that arithmetic. This mathematical enumeration is not
an emulator input sweep, a pack model, or a calibration measurement. It
identifies what remains to establish electrically, not which raw code to
choose to make the boot proceed. The acquired drawing and tuning text narrow
the source hunt but do not yet close the transfer/mux specification boundary.

The original Nokia NSE-6 system-module chapter, pages 3-41/3-42, specifies
16 Mbit flash (2 MiB), 2 Mbit SRAM (256 KiB), and 256 Kbit serial EEPROM
(32 KiB). These are physical capacities, not a complete BUSC alias map.
[Original service chapter](https://www.eserviceinfo.com/preview_html.php?fileid=5448&previewid=3000).

## Acquired Input

The self-extracting ZIP is read as archive data, never executed:

| Artifact | Identity |
| --- | --- |
| `nse6_602.exe` | SHA256 `abc2fa6a0b1b0f5e33206c7ffb5ce17e81ab46e155733584e23aa3459b3349e9` |
| MCU `NSE67016.020` | Decoded extent `0x200000..0x36ffff` |
| PPM `NSE67016.02A` | Decoded extent `0x370000..0x3fffff` |
| Combined image | SHA1 `e3b548816fa027da906be3daf049ce6332a9f257` |
| Combined image | SHA256 `63c60e637d19c16b86bc8d16db5e3b5691126880ecc60a7447a5efbad3547beb` |

The package's `nse-6.ini` selects that MCU under `NSE-6_RESTOREFILES`
and that PPM under `EURO_A`. The two decoded extents meet without an erased gap.
Local inputs and normalized output live under `roms/research/nse6-v602/`.

Reproduce normalization with the existing structured record decoder:

```sh
.venv/bin/python tools/extract_dct3_wintesla.py \
  --mcu roms/research/nse6-v602/NSE67016.020 \
  --ppm roms/research/nse6-v602/NSE67016.02A \
  --flash-output roms/research/nse6-v602/8810-v602-ppm-a.fls
```

## Reset Facts

Direct ARM-big-endian disassembly of this image establishes:

| Address | Operation |
| --- | --- |
| `0x200040..0x20005c` | Reads the first flash word, transforms byte lanes, writes `0x40000`. |
| `0x200068`, `0x200090`, `0x2000b0` | PC-relative literal loads all resolve to stack base `0x125f30`. |
| `0x2000a0` | Establishes peripheral base `0x20000` in `ip`. |
| `0x2000b8..0x2000c4` | Copies eight words from `0x200180` to address zero. |
| `0x2000c8..0x2000d4` | Writes `0xff` to peripheral bytes `0x2000a` and `0x2000b`. |
| `0x2000d8..0x2000e8` | Clears CPSR interrupt-mask bits and switches into Thumb execution at `0x2000ec`. |

The first Thumb calls are `0x27afe8` and `0x23190e`, before the zero-fill
of `0x100020..0x1216cb`. The startup entry at `0x2d330e` then calls the
DSP initialization/verifier at `0x2b6118`. The pre-clear routines access retained
RAM near `0x13ffxx`; this also fits the documented SRAM capacity.

## Recovered Interface Boundaries

### GENSIO and CCONT

CCONT reader `0x2dfc60` selects control `0x28 = 0x22`, writes the cached
command OR `0x04` at `0x2a`, waits for status `0x29` bit 2, and reads `0x2d`.
Writer `0x2dfba8` uses the same select, clears the command's read-request bit,
then sends the modified byte at `0x2a`. The recovered six-register transport
tuple is consequently `(0x2a, 0x28, 0x2b, 0x2d, 0x29, 0x2c)`, in
CCONT-write/control/LCD-data/CCONT-read/status/LCD-command order. It matches the
shared earlier-layout transport without relying on a sibling's addresses.

This is digital attachment evidence, not an ADC calibration tuple, measured
conversion latency, or validated interrupt routing. Those remain separate
configuration questions before runtime promotion.

### Serial EEPROM

The byte transmitter `0x2de3d4` constructs PUP base `0x20020`, uses data mask
`0x01` and clock mask `0x04`, and controls SDA direction at `0x20024`. Thus
the own transmit contract uses SDA bit 0 and SCL bit 2. The ACK sampling at
`0x2de484..0x2de492` returns the inverted SDA bit after clocking the ninth bit.
The reader `0x2de496` releases SDA direction at `0x20024`, samples bit 0 with
clock mask 4, and assembles eight bits MSB-first.

Initializer `0x2dd100` reads flash byte `0x200005`, value `0xf6`:

| Field | Firmware decoding | Own result |
| --- | --- | --- |
| Capacity | `1 << ((descriptor & 7) + 9)` | 32768 bytes |
| Page size | `1 << (((descriptor >> 3) & 7) - 1)` | 32 bytes |
| Address width | Descriptor bit 6 | Two address bytes |

The address setup at `0x2dcef0` consequently uses control byte `0xa0`, then
high and low address bytes through `0x2de3d4`. The write wrapper at `0x2dcf7a`
splits transfers using the configured page size. These contracts agree with
the manual capacity, but do not identify a manufacturer, establish measured
write-cycle latency, or supply authentic EEPROM contents.

### DSP Startup

The own verifier `0x2b6118` reads one 16-bit word every 32 flash bytes from
`0x200040`, sends 127 blocks of 512 words and a final 510 words followed by
two `0xffff` words. Buffers alternate between `0x10200` and `0x10600`, with
handshake halfwords at `0x100fe/0x10100`. At `0x2b6200` it waits while the
separate result cell `0x10002` is `0xffff`, then copies `0x10000/0x10002`
into context `0x1205ca/0x1205cc`. The static checker resolves the result-base
and context literals independently of the transfer-handshake addresses.

This identifies a protocol shape shared with other recovered verifier streams,
not a fitted DSP mask or a successful verdict. No resident-ROM compatibility
is established and no DSP response is fabricated.

The independently serialized stream is 131,072 bytes, SHA-1
`1e9487dbc339646937dc9e5bb1bb6c3e2e759dac`. The static checker pins the
stride, halfword lane and two terminators. Its block/stride shape matches
the NSM-1 verifier, but its contents differ from that product's stream.
Neither resemblance nor a digest identifies the fitted resident DSP ROM;
this is sparse external-flash input, not an extracted DSP program.

### Display

Initializer `0x2e1194` selects GENSIO control `0x28 = 0x21`, configures PUP
direction `0x24` bit 5, and pulses PUP data `0x20` bit 5 low/high with a delay.
Command sender `0x2e1146` polls status `0x29` bit 0 then writes `0x2c`;
pixel sender `0x2e1184` uses the same ready test then writes `0x2b`.

The initialization sends `0x24`, then for six banks sends `0x40|bank` and
column address `0x80`, followed by 84 zero pixel bytes each, and finally `0x20`.
This establishes an 84-column, six-eight-pixel-bank surface and a command
grammar consistent with the shared PCD8544-family implementation. It does not
identify the exact physical controller or prove a rendered runtime frame.

### Keypad

Own scanner `0x2de164` uses row register `0x31`, column register `0x30`,
direction register `0x2f` and interrupt mask `0x33`. Ordinary scans select
rows 1 through 4 and test five active-low columns; raw keys are `row*5+column`.
The separate special-key scan yields `0x80+column`.

Decoder `0x2e049a` maps normal keys through `0x3033b4` and special keys through
`0x3033d0`, indexed by layout byte `0x1214af`. Recovered layout-zero tables:

```text
normal:  3e3e3e3e3e11190102030e170405060f18070809101a0c0a0b
special: 3e3e3e3e0d
```

Special column 4 maps to power key `0x0d`, requiring column mask `0x10`.
The bytes coincide with independently recovered NSE-1/NSM-1 tables, but are
checked against this image rather than assumed from those products. Host
button labels and runtime input acceptance are not yet established.

### SIMI

Initializer `0x2ca910` writes `0xff` at IIR `0x38`, configures the controller
and finally writes control `0x39 = 0x32`. Transmit routine `0x2ca5ac` loads
base `0x20036`; receive routine `0x2ca986` loads `0x20037`, checks RX count
at base+5 (`0x3c`), and drains bytes while that count is nonzero. The interrupt
receive routine `0x2ca80c` uses the same count/data pair, with FIFO control at
`0x3d`. This supports the existing controller register window, not a claim of
completed ATR/PPS/APDU exchange on NSE-6 or measured serial timing.

## Acceptance Required Before Promotion

### First Isolated Run

`run_8810_stage_20261009` ran nine emulated seconds with erased EEPROM and
the required HLE backend's exchange/service contracts disabled. No resident
DSP mask, fabricated acknowledgement, radio peer or external-service responder
is selected. The MCU boot-exit HLE remains explicit; this is not native reset.

The own verifier starts at 0.011511154 s. Its first 512-word block is sent;
at 0.014975308 s execution reaches `0x2b61bc..0x2b61c0`, waiting for the first
handshake halfword to become nonzero. Both handshake halfwords stay zero through
9 s. Display initializer and SIMI initializer do not run; the screenshot is
blank. This result validates execution to that boundary, not the downstream
peripheral contracts or fitted DSP compatibility.

The EEPROM device is a capacity/address-compatible 24C256 instrument, not an
identified fitted part: its modeled page size is 64 bytes while the own firmware
splits writes at 32. Page-wrap fidelity and write latency remain unvalidated.
Shared host matrix labels remain provisional; the raw tables are independently
checked, but no runtime key acceptance has occurred.

```sh
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir run_8810_stage_20261009 -- nse6stage \
  -rompath ../run_8810_stage_20261009/roms \
  -video none -sound none -nothrottle -seconds_to_run 9 -log \
  -autoboot_delay 0 -autoboot_script ../tools/nse6_stage_observe.lua
```

The run ROM directory must contain `nse6stage/8810-v602-ppm-a.fls`. Use a new
run directory for a fresh erased-NVRAM experiment.

Run the hash-pinned package/reset check with
`python -m tools.nse6_v602_static_check roms/archive-dct3-packages/nse6_602.exe`.
Its memory-capacity fields cite the service chapter; they are not decoded from
the reset instructions. The check explicitly reports no runtime acceptance.

### ROM4 Compatibility Run

`nse6r4t` substitutes the acquired NSE-1 ROM4 program/data for the disabled HLE
exchange. It retains the own 8810 flash, 256 KiB SRAM and erased 32 KiB EEPROM;
no handset identity, repair template or fabricated DSP verdict is supplied.
The MCU reset exit still uses the declared boot HLE. This is a compatibility
experiment, not a fitted-mask profile or full native phone.

The isolated nine-second run reaches final verifier wait `0x2b6200` at
0.228983538 s, LCD initializer `0x2e1194` at 0.245563231 s and keypad scanner
`0x2de164` at 0.386279385 s. The eight-second snapshot displays CONTACT SERVICE.
Handshake cells `0x100fe/0x10100` later read `0x04ec/0x1074`; these are sampled
live cells, not a captured final verdict. No SIMI initializer observation or
runtime input acceptance was obtained.

A second cold run observes the verifier's caller return at `0x2d333c`:
context `0x1205ca/0x1205cc` contains `0x0000/0x0004` at 0.230685000 s.
This captures the firmware-owned result before later shared-cell reuse; the
meaning of result `4` and its contribution to CONTACT SERVICE remain to be
decoded. The observer uses a return target, not a mid-routine fetch tap whose
silence could merely reflect translated straight-line execution.

The syntactic Thumb literal census finds 13 loads of context base `0x1205c0`
and two direct loads of `0x1205ca` within the MCU extent. This is not a
whole-program reference closure: derived pointers and ARM/data-driven accesses
remain outside its coverage. Direct reader `0x2c0436` formats the first word's
nibbles; `0x2d3914..0x2d391c` compares that word against literal `0x0b06`.
In the nine-second cold observation the latter reads zero at 0.385486231 s.
No firmware read of the second word was observed in that bounded run. The
observer's own context read is separately identifiable at `0x2d333c`; adjacent
context fields are excluded by byte-lane masks. Consequently `4` must not be
promoted to a self-test failure code without a decoded consumer.

### Storage Activity at the Failure Frame

The own serial address setter `0x2dcef0` receives the EEPROM address in `r0`
and operation selector in `r1`; its decoded branches select random-read setup
for zero and write setup for one. The observer records the first 32 requests
and counts all entries. In a fresh erased-storage run it counts 598 entries
through eight seconds. Early reads include `0x0000`, `0x006c`, `0x0074`,
`0x0070`, `0x0374` and `0x03f0`. Firmware-owned write requests begin at
`0x0074` at 0.231416308 s, followed by `0x0254/0x0260` and `0x0274/0x0280`.
These are address-setter observations, not successful transaction completions
or validated stored records. They exclude a never-started storage subsystem,
but do not distinguish erased-record validation failure from transport errors.
Address requests alone do not justify a provisioning change.

### Erased-Storage Integrity Failure

The controller initializes context at `0x13fd78`. Its failure-display handler
`0x243ba4` uses the MCU strings at `0x243e18` (CONTACT) and `0x243e20` (SERVICE).
The reason byte `0x13ff74` is initialized to one at `0x2d3308`, before DSP
verification; it is not evidence that the DSP subsequently failed.

The integrity calculator `0x240918` reads a block starting at EEPROM `0x40`,
sums bytes and subtracts the two bytes returned by helper `0x2c0ac8`. The
allocated-buffer path covers `0xde` bytes; the allocation-failure path covers
`0x9e` bytes, so these paths must not be conflated into one checksum recipe.
The caller reads stored words at `0x011e` and `0x0090`; it first compares the
calculated value with the former, then tests the OR of the latter and the
calculated value for zero. Either failure selects `0x240c1e`, writes status
`0x0c` into its status record and clears flag `0x40` at `0x13fde1`.

The cold erased-storage run takes that branch at 0.509262231 s with calculated
`0xdb24`, stored `0x011e=0xffff`, and stored `0x0090=0xffff`. The failure-display
handler runs at 1.476262308 s. This establishes an integrity failure in this
fixture, not an authentic factory profile or the absence of additional faults.
Protected identity semantics are not established by this integrity check;
no donor record or firmware flag override is permitted.

The observed allocated path's arithmetic is reproduced independently: sum
EEPROM bytes `0x40..0x11d`, subtract the high and low bytes returned by
`0x2c0ac8`, then wrap to 16 bits. That helper returns zero at 0.507807231 s;
the calculator returns `0xdb24` at 0.507815538 s. The persisted own EEPROM is
32,768 bytes, with `0x74..0x75=0000` written by firmware, sum `0xdb24`, and
both stored comparison words still `ffff`. Thus the arithmetic explains the
runtime value without assuming the two written bytes remained erased. The
calculator calls service primitive `0x2dfe9e` before returning, so the pure
arithmetic helper does not claim to model all service-side transformations.
Tests cover both allocation lengths, excluded-byte subtraction and modular
underflow; they do not establish semantic validity of protected identity data.

The arithmetic callback is class/command `0x6209`. Its class gate first tests
enable byte `0x13ff1c`, then the class bitmap. The erased cold run records
enable zero, bitmap zero and class mask `0x20`, so the callback does not change
this calculation.

### Checksum-Only Diagnostic

`python -m tools.nse6_integrity_fixture <fresh-nvram>/nse6r4t/eeprom` creates
an erased 32 KiB image changing only offsets `0x011e/0x011f` to `db24`.
It refuses an existing destination. This is an integrity diagnostic, not an
identity/lock profile or factory provisioning. Tests enforce the two-byte
change boundary and checksum invariance when firmware updates excluded word
`0x0074` to zero. Run `nse6r4t` with that fresh NVRAM directory and the existing
read-only observer; never seed the diagnostic from another handset's storage.

Two isolated checksum-only runs avoid the `0x240c1e` integrity failure but
still display CONTACT SERVICE. The subsequent 24-entry status scan accepts
values `00/ff/fe`, ignoring entry `0x0b`; other entries clear flag `0x40`.
In this fixture entry `0x12` at `0x13fcb2` contains `0x12`, causing that clear
at 0.557605000 s. Its initializer `0x240b46..0x240b50` writes `0x12` when
helper `0x2be71e` returns nonzero. Its predicate is decoded below.

### Config Integrity and Current Frontier

Helper `0x2be71e` calls validator `0x28d5ae`, which reads 64 EEPROM bytes
starting at zero, sums the first `0x3c` bytes using the 16-bit byte-sum primitive
`0x2ce3d0`, and compares the zero-extended result with the big-endian 32-bit
word at offset `0x3c`. It succeeds only on equality. This is a byte sum, not
CRC; it does not inspect identity semantics.

The fixture's optional `--config-integrity` corrects that stored word to
`00003bc4` in addition to the independent `db24` field. Tests enforce exactly
six altered bytes (`0x3c..0x3f`, `0x11e..0x11f`) and leave all identity fields
erased. In two fresh runs, validator entry returns sum/stored pair
`00003bc4/00003bc4` at 0.471406000 s, the status-`0x12` failure disappears,
and SIMI initializer `0x2ca910` executes at 3.592007923/3.592108077 s.
The failure-display handler is not observed. Both the eight-second and
28-second frames are blank; the 30-second run continues executing firmware,
with one-second samples after six seconds in `0x2d33d6..0x2d33dc`.

These diagnostic results isolate the two integrity gates but do not provide
valid identity, lock configuration, completed APDUs, physical input acceptance,
registration or native speech. The next question is the post-initialization
firmware wait/UI startup boundary. Additional storage changes require their
own decoded contract; no donor identity is justified by a blank frame.

### Parked Task and Physical Input

The sampled loop `0x2d33d6..0x2d33dc` waits on byte `0x1216bc`. Its entry
tests predicates in order; the observed first predicate `0x2c76fe` returns
zero at 5.355727077 s, selecting `0x2d33cc`. That predicate itself tests two
state bytes and helper `0x2a15cc`; no lifecycle meaning is yet established.
The loop flag has clear writers in handlers `0x2deb9a` and `0x2dec1c`, and
runtime interrupt traffic reaches those writers. Thus a sampled parked task
is not sufficient evidence that all firmware execution or UI work is blocked.

`tools/nse6_keypad_fixture.lua` applies physical digit/softkey/navigation
contacts at 8, 10 and 12 seconds using the inherited, provisional host labels.
Three fresh two-integrity runs remain blank at 28 seconds. The scanner runs
once during startup and the decoder never runs. At every input edge MAD2 IRQ
pending is `00`, IRQ mask `ce`, control `05`, and keypad column mask `3f`.
All columns are masked, explaining why these contacts produce no keypad IRQ;
this is not acceptance of the host labels or proof of a controller defect.
The harness must not unmask columns or force a task publication to make this
fixture appear interactive.

### Input-Enable Contract

The syntactic Thumb literal census finds seven loads of `0x20033` within the
MCU extent. Four controller paths (`0x2882da`, `0x288b82`, `0x288d30`,
`0x288fe6`) clear ordinary column-mask bits using AND `0xe0`; scanner/setup
paths at `0x2de16e`, `0x2de296`, `0x2e0570` set them. This direct-literal
inventory is not a closure over constructed/derived register pointers.
The live mask initially reaches `0x20` after the first scan, then becomes
`0x3f` at setup store `0x2e0578` at 0.458432846 s.

The independently decoded table `0x288a84` contains 17 controller targets;
state `0x10` selects `0x288c3a`. Context `0x121564` receives initial event
`0xb1` in state one at 0.386226308 s, then remains in state `0x10` while
receiving actual events. This state's ordinary advance requires low nibble
`0x13ffa8 == 6` and low nibble `0x12147d == 0xf`, then tests additional helpers
before clearing the column mask. Report `0x14` sets bitmap bit zero; reports
`0x16/0x15/0x17` set bits one/two/three respectively. The cold run receives
those last three reports but not `0x14`; sampled readiness is `06` from two
seconds and the report bitmap is `0e` from four through twelve seconds.
Thus the missing report bit is a concrete unmet prerequisite, not proof that
supplying it would complete every later gate. Next census its own producer
and input conditions. No external startup report is to be synthesized merely
because a sibling profile supplies similarly numbered reports.

Reproduce with the isolated-run command above, substituting `nse6r4t` and a
fresh run directory. Its ROM subdirectory additionally requires the hash-pinned
`nse1_rom4_dsp_program.bin` and `nse1_rom4_dsp_data.bin` declared in the driver.
A successful handshake does not justify importing a donor EEPROM or claiming
native radio/speech. Captured results and the two integrity failures are above;
the current input-enable boundary is distinct from those resolved questions.

The shared memory handshake is not permission to fabricate its result. Memory capacities,
reset/peripheral attachment, static negative fixtures and the first isolated
boot are established above; persistent-storage dependencies remain unresolved.

Promote graphical/input/SIM/service capabilities only with their own runtime
evidence. A historical repair EEPROM, if examined, remains a repair template,
not an authentic handset dump or independently justified provisioning.
