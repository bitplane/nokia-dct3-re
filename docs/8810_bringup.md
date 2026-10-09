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
Next recover the calculator/helper and protected fields completely before
choosing any own-storage fixture; no donor record or firmware flag override is
permitted.

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

Reproduce with the isolated-run command above, substituting `nse6r4t` and a
fresh run directory. Its ROM subdirectory additionally requires the hash-pinned
`nse1_rom4_dsp_program.bin` and `nse1_rom4_dsp_data.bin` declared in the driver.
Next, capture the firmware-owned verifier result and trace the self-test failure
with erased own storage. A successful handshake does not justify importing a
donor EEPROM or claiming native radio/speech.

The shared memory handshake is not permission to fabricate its result. Memory capacities,
reset/peripheral attachment, static negative fixtures and the first isolated
boot are established above; persistent-storage dependencies remain unresolved.

Promote graphical/input/SIM/service capabilities only with their own runtime
evidence. A historical repair EEPROM, if examined, remains a repair template,
not an authentic handset dump or independently justified provisioning.
