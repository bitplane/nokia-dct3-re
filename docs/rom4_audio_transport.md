# ROM4 audio transport observations

NSE-1 data/I/O-space, BSP, DTMF and physical-clock maps. [COBBA control](cobba_control_boundary.md) owns the current interpretation; [C54x execution contracts](c54x_execution_contracts.md) owns processor behavior. Numeric MMR/I/O aliases do not imply a shared peripheral.

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

## Operational data-space sample path

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

## Organic tone request boundary

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

The unclocked production profile's remaining boundary is clock attachment and
its enable contract, not a synthesized tone command. TI's
[C54x Applications Guide, BSPC configuration example](https://www.ti.com/lit/ug/spru173/spru173.pdf)
(section 3, p. 3-38) defines receiver/transmitter reset bits and receive/transmit
ready flags separately. That primary reference supplies generic peripheral
semantics; it does not establish the MAD2/COBBA frame-clock rate, pin mapping,
or whether every native data `0022` write is the generic BSPC interface.
Keep those product attachments explicit before promoting operational audio.

## Externally clocked native DTMF

`make check-c54x-rom4-pcm-restore` additionally saves active synthesis at
8.150 s and reloads before the physical key release at 8.220 s. The fixture
owns the key through absolute emulated deadlines: Lua waits can wake early on
load, and host input is not automatically replayed with device state.
The checkpoint compares MCU R0..R15/CPSR/PC, DSP PC/SP/ST0/ST1/A/B/T/AR0..7/
IFR/IMR and data words 0021..0023/00f0..00fe; it never reads destructive BDRR.
All 400 subsequent output words and their absolute timestamps replay exactly
at the captured 1 ns text resolution. This checks future frame deadlines and
native oscillator restoration, not physical clock phase or all memory cells.
Raw capture counts include the replay; the ordinary waveform check removes
only that duplicated interval. The ARM7 debugger's named PC export must agree
with GENPC, rather than exposing a stale cached value after load.

`make check-c54x-rom4-pcm-clock` runs the same own-NSE-1 provisioning and
physical key-2 press/release on the normal `noki5110` machine and the separate
`nse1pcm` laboratory machine. The latter adds only an external 8 kHz
completed-frame source through the backend's existing BSP interface, supplying
zero-valued microphone words. It does not write CPU registers, firmware RAM,
mailboxes or interrupt flags. The established untimed self-test echo retains
exclusive ownership while COBBA is in its observed echo mode. The normal
machine receives no external frame source and still has only two boot BSP
transfers. The fixture is not a new product or a claim about clock-enable bits.

The input harness explicitly gives this research clone the NSE-1 serial-LCD
and physical-key profile. A requested key name alone does not establish which
matrix field a previously unknown machine used. The acceptance checker requires
physical 2 at 8.000 s and release at 8.220 s, followed by the original MCU's
`00e1` command, high-frequency input `14e0` and low-frequency input `0ae4`
on the correct ARM halfword lanes. These frequency inputs correspond to
1336 and 697 Hz at four units per Hz, matching key 2 in
[ITU-T Q.23 figure 1](https://search.itu.int/history/HistoryDigitalCollectionDocLibrary/4.256.43.en.1010.pdf).

The resident initializer's original words at `a5c7..a5ce` load T=`20c5`,
read from `0857` and shift each unsigned product right by 12; `a668..a66c`
publishes the accumulating phase's low word. Both sequences are byte-pinned
by the codec contract. The two inputs give integer steps 10945 and 5710;
at 8 kHz on a 16-bit phase cycle those are 1336.0596 and 697.0215 Hz.
This is numerical consistency with the captured tone, not a full independent
bit-exact model of its lookup table, envelopes or gain.

The paired cold capture observes 78,845 native BDXR writes and BDRR reads,
with 78,837 transmitted words captured explicitly, including zero words.
The checker requires a valid 8 kHz output grid, complete before/active/after
intervals, silent before/after samples and both sinusoidal components in
480 active samples. The measured 697/1336 Hz projection ratios are
approximately 0.2033/0.8019; finite-window projections are not orthogonal and
must not be summed as an exact energy partition. The engineering thresholds
reject missing, single-frequency or discontinuous output; they are not a full
Q.23 distortion, level, twist or analogue compliance test.

The runner records actual binary/ROM/observer/input hashes, fresh generated
storage, invocations and separate results in ignored disk-backed `tmp/`.
This establishes native runtime DTMF synthesis when supplied external frames,
not production clock gating, the 1 MHz bit-clock waveform, physical COBBA
gain/routing, analogue output, RF acquisition or native call speech. No HLE
tone generator supplies these words. The laboratory subclass is removable
when an independently evidenced production serial attachment replaces it.

## NSE-1 physical clock contract

`make check-c54x-rom4-pcm-wire` adds a distinct `nse1bit` laboratory case
to the unclocked/completed-frame controls and their active restoration tests.
It supplies actual 1 MHz clock transitions to the backend's RX/TX clock-input
methods, with a sync every 125 clocks and zero microphone data. No completed
frame method supplies the wire case. The native BSP shifts the transmitted
bits; the fixture assembles exactly 16 driven bits into each captured word.
Its active-high one-clock sync and RX/TX clock fan-out are explicit test
assumptions, not recovered product wiring. The untimed self-test echo remains
exclusive, and ordinary `noki5110` still has no attached audio clock.

The wire run observes 78,908 native writes/reads and 78,901 captured words.
All captured completions lie at the fixture's +16 us phase on its 125 us
grid; the checker rejects a half-microsecond phase shift even if the sample
rate and waveform still pass. It also checks sign extension of the documented
13-bit converter word across the entire capture, not only the active window.
The captured range is -2040..2039; this is word-format evidence, not ADC/DAC
full-scale, linearity or analogue gain validation. Silent pre/post intervals and the complete
480-sample DTMF window pass independently (697/1336 Hz projections about
0.2007/0.8008). These counts differ from the completed-frame shortcut:
interrupt and reset timing are not asserted to be interchangeable.
Saving at 8.150008 s, inside the word, and restoring reproduces all checked
CPU/peripheral snapshot fields plus 400 subsequent words and their absolute
timestamps at printed 1 ns resolution. The fixture's partial capture and
clock-device state are saved alongside the BSP shifters. This demonstrates
native edge-driven synthesis and partial-word replay, not electrical pulse
shape, production enable semantics, analogue output or native call speech.
The input bits in `nse1bit` are all zero. Its output waveform alone therefore
does not validate nonzero RX alignment against TX on a common wire. The
joined-pin controller tests below and the separate clocked-boot configuration
provide their own nonzero-input evidence.

`nse1pin` additionally gives the external clock ownership of boot loopback.
The backend's `set_external_codec_clock(true)` disables synchronous
BDXR-to-completed-frame handling; the laboratory connects held TX bits to RX
only while the observed COBBA register-8 test mode is selected. Other input
bits remain zero. The normal machine and both older laboratory controls are
unchanged. No firmware RAM, mailbox or interrupt flag is injected.

The native self-test writes `0aaa` at 0.133882038 s, the pin capture completes
at 0.134016000 s, and the firmware consumes the matching word at
0.134016173 s: 134.135 us after its first write. The old word-loopback counter
is zero. A separate uncapped first-success observation retains this read
after the generic poll trace cap; it performs no additional BDRR read.
Native DTMF, all 480 active samples and silent pre/post windows pass. The
partial-word checkpoint exactly replays 400 words and timestamps. Decoded
84x48 screen pixels match the paired original wire fixture, rather than
comparing PNG metadata. Reproduce all seven isolated controls and replay
cases with `make check-c54x-rom4-clocked-boot`. That gate also requires
complete instruction/extension decoding and the exact banked opcode-set
fingerprints: 626 words for the unclocked control, 642 for the original tone
clocks and 643 for clock-owned boot. Count, group or fingerprint drift fails
the gate; it must be reviewed rather than silently accepted. These execution
sets are not independent arithmetic assertions or silicon timing evidence.

This retires the untimed echo only in that declared clock-owned composition.
The real clock-enable/mux contract, physical sync timing, microphone data and
native call speech remain unresolved.

For instruction-level auditing, add `--coverage` to
`python -m tools.run_c54x_rom4_pcm_clock --mame mame/mame --rompath mame/roms --wire`.
This retains verbose opcode and fetched-extension records and requires a
supported real instruction boundary for every observed word. In particular,
actual `6f` extension tuples override the deliberately incomplete zero-extension
decoder probes; first-word support alone is insufficient. The unclocked control
reproduces the 626-form cold fingerprint;
completed-frame and wire cases both observe 642 forms in 111 high-byte groups,
set SHA-256 `e150bf2c9d9b52db524eab63c7270be0abfa9e7889b143e015bf3401de0f9fe7`.
The 16 additions are `0085 0193 0889 1184 1891 23f8 30f8 8194 f3f1 f461
f467 f47f f48c f658 f771 fd4c`. This identifies the arithmetic/status and
addressing surface exercised by native synthesis; decoder support and a valid
waveform alone do not establish independent exact-encoding conformance.

The completed default CPU conformance run and the separately completed
`mac-smem` and `xc-signed` profiles jointly cover all 642 forms with executable
assertion records: zero executed-only or absent forms. Existing independent
result/status, retained-operand, address-update and replay checks supply the
records; the exact `8194` selector additionally checks the stored word,
neighbour, AR4 increment, preserved accumulators/status and instruction cost.
Assertion logging is deduplicated, but every behavioral assertion still runs.
This is encoding coverage by selected fixtures, not proof of every operand,
all ISA flags, disputed silicon timing or native speech. In particular, the
accumulator-left-shift carry policy retains its documented manual-formula
qualification.

Validate each fixture's terminal completion and required markers with
`tools.run_c54x_core.check_result` before combining its inventory. Reproduce
the strict comparison with:

```sh
python tools/c54x_opcode_coverage.py WIRE/error.log \
  --fixture-log CORE/error.log \
  --additional-fixture-log MAC_SMEM/error.log \
  --additional-fixture-log XC_SIGNED/error.log \
  --all-gaps --require-extension-decoded --require-all-asserted
```

Each assertion must have execution evidence in its own fixture log; execution
in another profile cannot legitimize an orphan assertion.

Clocked boot adds `7721` at `0e61`, executed 626 times while retrying the
test-word store. Its inventory is 643 forms in 111 groups, SHA-256
`5cb892b2da30d8489a37d0c6a403c4ead9976b0d5ee1aa5765f869ca567a26fd`.
The native backend conformance fixture executes `STM #0aaa, BDXR` followed
by `LDM BDRR,A` on atomic and staged frontends. It requires the retained
previous RX word and pending TX with no new RX readiness or interrupt
before clocks, then returns the literal through real pin methods and checks
both CPU sources and preserved B/status. Its mandatory `clocked store: PASS`
marker and `[opassert] op=7721` follow those checks, not just execution.
Run `python -m tools.run_c54x_backend_bsp --mame mame/mame --coverage` and
include its completed `error.log` with another `--additional-fixture-log`
in the strict comparison above. All 643 forms then have assertion records;
the original 642-form controls retain their separate fingerprint.

The external emulator at commit `2b4c10ffd6b22a2d021c7f515f27ceb2fb6c78cd`
does not independently settle this attachment. Its
`third_party/c54x/mad2_dsp_c54x.c` frame source tests DSP RAM `00fe` bit 0
or nonzero `00aa`, then directly requests vector 20 and captures held DXR.
Those are software-lifecycle predicates, not recovered COBBA clock-enable
pins. Its `src/mad2/mad2.h` derives 18,642 Hz from phase increment `0c5c`
and an assumed 900 Hz beep, explicitly leaving the absolute hardware rate
unconfirmed. This is a useful candidate model, not independent clock evidence.
For comparison, our original key-2 initializer's steps 10945/5710 would yield
about 3113/1624 Hz at that rate instead of the standard 1336/697 Hz pair
observed with 8 kHz external frames. This rejects borrowing that rate for our
checked DTMF path; it does not prove every codec mode has the same clock.
Resolve the control/divider contract independently rather than copying RAM
gates, direct vector injection or the beep identity assumption into a device.

The external-frame checker also reconstructs register-8 values from the full
helper-entry trace: all 48 calls are retained and reconciled with the final
uncapped count and sequential hit ids. Register 8 is `0626` throughout each
checked interval: pre-key silence (7.5--7.9 s), active DTMF (8.13--8.19 s),
and post-key silence (8.6--9.0 s). Thus this value alone does not indicate
nonzero tone output. This is helper-path coverage, not an exhaustive proof
that no other transport writes the register, nor a disproof of a persistent
converter/clock-enable interpretation. The checker reports observed values
without assigning bit meanings and rejects truncated or unordered captures.

The resident helpers `a484`/`a4cb` maintain a software client-demand bitmap,
not a hardware register: the checked acquire tail `a4c1..a4c3` ORs A into B
and stores B at data `00ef`; release `a4d9..a4df` loads that word, complements
the input B mask with `ffff`, ANDs it with A and stores A back. Their control
paths retain the paired register-8 `0600` activation/deactivation operations.
Exact-word fixtures protect both commits; the passive observer records entry
mask/old value and the subsequent store without modifying either.
The PCM runner reconciles every entry, store PC/value and uncapped count.
Fresh unclocked/clocked runs observe 19/21 transitions respectively. Both
acquire mask `0001` and release masks `0002`/`0004`; only the clocked run
additionally releases `0001` (8.261 s), then reacquires it (8.318 s).
This demonstrates a frame-dependent software lifecycle difference, not a
physical enable-bit assignment. In the clocked run `00ef=0001` persists
across both DTMF and later silence, so neither this bitmap nor register 8
alone identifies a nonzero audio waveform. No production clock is gated on
these firmware addresses.

The control-write helper at `45c2` has 27 raw direct CALL/CALLD candidates
in the resident image. Reproduce their operand windows with
`c54x_rom4_port_census.py IMAGE --call-target 0x45c2 --context 10`.
This is full raw-word candidate coverage, not proven executable-call or
data-flow coverage; indirect calls and uploaded code are excluded. The
reviewed `b89c` sequence calls `a484` with register index 8, then supplies
`0602` to `45c2` with index 8, followed by `0944` with index 9. Its exact
words are checked by `codec_register8_constant_setup`, including mutation
tests. This is another local control-use context, not evidence that `0600`
means clock enable or that this path executes during the tone fixture.

The resident `a580..a597` block also consumes packed shared data `0854`:
`B = word >> 12` selects the register and `A & 0fff` supplies its payload.
When B is 8, a BITF test of local data `00ef & 000f` selects either
`A & 09ff` or `A | 0600` before the final 12-bit mask and `45c2` call.
This is a software control transformation, not a recovered analog bit map.
The executed reader is the alternate block at `27f0`: it handles register 9
through local data `1f92`, `19a2` and `19a4`, then rejoins `a584` or `a594`.
Both blocks have exact-word, mutation-tested contract fixtures; the raw
resident call census alone does not describe this block's data flow.

The native tone gate requires the ordered pre-key chain: MCU write
`84260000` with mask `ffff0000` at `100a8`/PC `272034`, DSP read `8426`
at `0854`/post-extension PC `27f2`, helper write register 8/value `0026`
returning to `a598`, then register 8/value `0626` returning to `a4bb`.
Thus `0426` is not passed unchanged to COBBA: the DSP clears `0600` in
one context and subsequently sets it in another. The separate post-key
activation observation remains required. Observation caps are per owner,
direction and boot/interactive phase; this is a bounded fixture, not an
exhaustive inventory of all writes or uploaded/indirect producers.

The passive helper-entry capture in the physical `1` fixture observes
register 8/value `0626` returning to
`a4bb` during the keypress (8.077 s). The tone checker requires that
post-key caller independently of the shared tone-cell writes. The constant
setup at `b8a2` is not observed in this fixture. This confirms execution of
the known activation path, not its clock-field meanings. Program-tap PC
must be `45c3` because fetch increments PC; the captured return address is
read from ordinary stack RAM. Output is capped at 256 entries with an
uncapped total, so a longer capture must check its truncation explicitly.

Run `make check-c54x-rom4-codec-tone RUN_DIR=run_native_codec` for the
isolated 11-second acceptance capture. Each invocation creates fresh own
EEPROM/NVRAM and a separate working directory under that root, retaining
stdout, error log and frames even on failure. It checks the local ROM
signatures, reset/echo/receive ISR, masked transmit request, physical key
delivery and register-8 activation caller, and rejects new data-`0032`
traffic or reset/illegal failures. This is a native serial/control gate,
not a waveform or speech gate; save/load remains independently tested by
`verify-5110-save-state`.

`make check-c54x-rom4-tone-levels RUN_DIR=tmp/native-tone-levels` runs four
isolated native 5110 cold boots with own-profile EEPROMs. The physical
sequence is `Menu,9,5,option,Clear,Clear,Clear,2`, with options 1--4.
The [5110 user's guide](https://altehandys.de/downloads/man-no-5110.pdf)
(printed pp. 25 and 44) identifies menu 9-5 as keypad-tone Off/three levels.
Each run retains its console, trace, NVRAM and final digit-2 frame.
The 84x48 grayscale pixel SHA-256 must be
`cf44d8d2cf6398d7d69aa5cb9b766d3b8a2905169c6edda6e313b4fc2d6b8cd6`
in every setting, independently proving the final key reached the UI even
when the tone command is absent.

| Selected option | Final key's observed control behavior |
| --- | --- |
| Off (1) | No new `00e1` tone copy to DSP `00fe` and no register-8 `0626` activation during the remaining capture. Earlier menu keys supply the positive control. |
| Level 1 (2) | Tone copy and register-8 `0626` activation occur; no new level-dependent shared control is required. |
| Level 2 (3) | MCU `920c` reaches native `27f2` intact and produces register 9/value `020c` before the tone copy; register-8 activation is still `0626`. |
| Level 3 (4) | MCU `9209` reaches native `27f2` intact and produces register 9/value `0209` before the tone copy; register-8 activation is still `0626`. |

The checker requires all eight physical press/release pairs, complete
helper-call and tone-copy observations, ordered level controls and the
unchanged two boot serial transfers. Per-access trace caps are 64 per
owner/direction/phase. The Off absence claim is rejected if tone-copy
counts do not cover the complete trace. These observations separate the
level-dependent packed control from the common activation write; they do
not establish the analog gain law, additional register-9 bit meanings, clock
enable bits, additional PCM samples or audible/native speech output.

**Accepted native parameter boundary:** MCU parameters at shared `0854`
(MCU `100a8`) do not assert INT2 or get cleared by peripheral acceptance.
Their explicit doorbell drives HPINT. The modeled INT2 sources are the live
ring-not-empty comparator and independent saved slot/queued-transfer compare latches.
All four physical tone-setting boots, native save/load and the clocked-boot
suite pass, retaining the exact reviewed instruction fingerprints and paired
pixels. Medium/High acceptance requires the ordered MCU publication, intact
native `27f2` read, register-9 delivery and tone copy; the former destructive
alias's captured zero read is a negative control for this check.

Do not reintroduce the direct payload/request alias to produce other ROM
dispatcher bits. It turned `9209` into zero before consumption and selected
register 0 instead of 9. Physical sources of other request bits still need
independent evidence; successful boot does not prove those bits unnecessary
for radio/call operation. The port-2 in-service mask still projects onto shared
`0855`/MCU `100aa` in this partial model; its full electrical ownership remains
unvalidated.

**RF snapshot provenance:** a retained pre-transfer 30-second ring-plus-slot capture
has 20,285 frame expiries and 648,192 RF reads, as in the existing scan
oracle, but 1,518 rather than 1,520 programming pairs. The two absent
`0041/0040` writes belong to queued writer `3712`; producer `a1bc` still
publishes them at `1922/1923`. Resident dispatcher `35c0` selects `369a`
on request bit `1000`, and that handler acknowledges `1000` before queue
service. The model no longer derives that request from a packed selector-9
word. The independent port-4 compare model below now services that queue;
the 1,518-pair capture characterizes the earlier ring-plus-slot model, not
the newly accepted queued-transfer fixture.

The accepted current scan profile is distinct from that negative control:
20,280 frame expiries, 648,032 RF reads and 1,537 programming pairs. Its
first sixteen outputs require two complete descriptor bursts, the original
scan writer and the beginning of the next burst. Both prior count regimes
are rejected, not accepted through a tolerance. The legacy single-entry
processing fixture remains a separately named checker mode; the Makefile's
30-second gate explicitly selects the multi-entry contract below.

The extended passive gate (`python -m tools.run_c54x_rom4_transfer
--mame mame/mame --rompath mame/roms --seconds 30`) retains an uncapped
histogram keyed by producer PC and both output words, alongside the capped
ordered prefix. The checker rejects missing/duplicate counts, disagreement
with the prefix, incomplete requested duration and changed queued/immediate
producer totals. Two fresh 30-second captures reproduce 1,537 output pairs:
four at `a240`, twenty at `3712` (each of the five queued payload pairs four
times), and 1,513 at scan writer `4028`. The earlier ring-plus-slot processing
capture counted 1,514 executions of scan-output block `4433` and 1,518 total
pairs. Thus the new total is twenty queued pairs gained and one scan pair
fewer at the fixed cutoff, not an unexplained nineteen-write producer.

The current long scan has 20,280 frame expiries, 648,032 RF reads and 18,927
transfer-compare expiries; port `38/39` reads remain zero. The first two
descriptor bursts complete at about 1.563/1.590 s and the next starts at
6.436 s. These are deterministic emulator observations, not physical clock
measurements. The independent processing capture now executes mode 1 1,024
times, its eight-iteration filter 8,192 times and continuation `33ac` twice;
RF and reduction inputs remain zero. The coordinated RF/processing snapshot
update is explicit, with negative tests for the former payload-alias and
missing-transfer regimes. No RF samples or firmware state are synthesized.

The processing contract checks all 20 early-reduction entries at `332b`:
`06fd=0` selects that arm until the original `3135` writer sets it to one.
The observed writer sequence is `0f10:0,312c:0,3135:1`, not twenty resets.
The remaining 1,004 mode-1 frames take the steady reduction. The unchanged
filter coefficients, circular pointers and four full-width operand snapshots
remain required; current filter-entry ST1 is `c900` rather than the legacy
fixture's `e900` (the difference is XF output bit 13).

Two original `33ac` control continuations occur with `00ad=0027`,
`2180=005a`, A=`002d0000` and B=`005a0000`; the first precedes the second
early reduction, the second follows the `06fd=1` publication. Both run in
mode 1 with zero RF/reduction input. Checked ROM signatures cover the
`06fd` selector and full comparison/call arm at `3376..33af`.
The gate joins selector writers, transition order, flags, control words and
accumulators rather than treating a global `42f6` count as proof of this
call site. These continuations are not a detection/acquisition result.

The passive `c54x_rom4_transfer_observe.lua` eight-second capture records ten
port-4 writes: initialization/disabling uses `3a98` (15,000), the four `a244`
writes use `0834` (2,100), and uploaded paths write `0032` (50) and `03e8`
(1,000). At `a244`, descriptor `1914..1917` is
`0005,0834,0001,0884`, with pointers `197b=1914`, `197d=1920`.
Later control writes see those pointers advanced to `1916/1922` while the
second data pair remains queued. All 1,485 observed request reads are `0002`,
`0008` or `000a`; none contains service bit `1000`. These position-shaped
values motivate an independent timed-compare hypothesis, not a serial-baud
constant or an accepted timer implementation. The observer retains the last
firmware counter read without adding I/O reads; its sometimes stale values
do not alone determine the match phase or distinguish relative from absolute
programming. Raw descriptor words, request counts and timing stay observable
as the baseline negative control for the independently modeled source.

**Derived queued-transfer compare:** checked original code initializes I/O
port 4 to the same `3a98` disabled position as the other compare channels.
Uploaded `a244` supplies descriptor position `0834`; resident handler
`369a` acknowledges request `1000`, and `3706..371c` emits the queued pair
and programs the next descriptor position. The model uses the existing
frame counter/quarter-symbol clock for an absolute, wrapping compare and
keeps its request latch separate from parameter RAM. This is a
firmware-derived partial controller model: the physical peripheral identity,
fine write-to-counter phase and measured clock relationship remain unvalidated.

`make check-c54x-rom4-transfers` runs a fresh own-profile native 5110 for
eight seconds. Its passive observer reads ordinary descriptor/payload RAM,
observes firmware I/O and retains the firmware's own counter reads; it never
posts a completion or changes firmware state. Two fresh captures reproduce
four complete bursts. Each starts with the original `a240` pair, then five
compare expiries drive original `3712` pairs at positions
`0834,0884,08d4,0924,0974`; firmware advances both pointers and disables
the channel with `3a98`. The checker joins each output to its captured
descriptor and payload, requires the acknowledgement-family request reads,
and checks trace accounting. All 20 queued pairs are required, not merely
the two previously fabricated by the payload alias. Native tone settings,
clocked-boot instruction/pixel fingerprints, save/load and the 3210 default
and coherent frontier regressions remain unchanged. This proves neither
native RF acquisition nor speech nor a measured serial-transfer cadence.

The exact checked ISR block `35c0..35f6` has predicate-mask union `17ff`:
bits 0--10 and 12 are tested, not bits 11 or 13--15. This independently
corroborates the external emulator's dispatch-mask hypothesis, but software
dispatch coverage is not an electrical acknowledgement specification.
Restricting the former DARAM clears to `17ff & ~2` would still turn `9209`
into `8000`, losing part of the packed selector. That mask is not a justified
replacement source model or reason to clear parameter data.

The existing modeled slot-compare request now has its own saved pending bit.
Expiry sets that bit without OR-ing `0008` into the shared parameter word;
the modeled port-1 result combines it with the ring comparator, not parameter
bits. Port-1/2 acceptance of bit 3 and DSP reset clear
the slot latch. Its cadence and request-bit assignment are unchanged.
`check-c54x-backend-requests` exercises sixteen isolated cases across both
compare channels: payloads
`0000/8000/9201/9209` through both accept ports, masked expiry and later unmask,
unrelated completion, pending-state restoration after acknowledgement,
and armed-timer restoration after reset cancellation. A separate coincident
expiry case restores both pending sources and accepts each independently.
CPU-line observations
settle at scheduler boundaries before snapshots. The old implementation
fails the payload-integrity assertion on its first expiry.

This is model storage/transition conformance, not independent silicon evidence
for the timer clock, port map or INT2 assignment. It includes the overlapping
bit-3 parameter `9209` and requires its preservation under acceptance and reset.
The live tone gate independently proves actual native parameter consumption.
Neither gate establishes all request producers, analog gain or native speech.

The checked MCU continuation `272036..27204e` distinguishes parameter data
from its explicit trigger: after the shared-field store at `272034`, the
selected update path writes a pending flag, writes halfword `4` to DSPIF
`30000`, then byte `2` to MAD2 FIQ acknowledgement `20008`. Literal words
at `272284/272288` establish those destinations. That sequencing is not
proof that all bits of the packed parameter themselves assert INT2, nor
does it identify the DSP-side request-latch register. Keep the API trigger,
HPI DARAM payload and DSP port request/accept registers distinct during
the next ownership audit.
In particular, the byte-2 write at MAD2 `20008` is FIQ1 acknowledgement,
not a second MCU-to-DSP doorbell or an independently recovered INT2 source.
This follows the modeled MAD2 write/ack contract and is corroborated by the
retained [MADos source reference](mad2_acquisition.md#retained-mados-source-reference):
`include/hw/ioports.h` names offset 8 as the FIQ latch, `include/hw/int.h`
names FIQ1 as MDISND, and `hw/dsp.c` sets DSPIF `30000` mask `0x04` to generate
the DSP interrupt. Those are cross-product corroboration, not a ROM4
port-1 bit map. MADos calls shared `0a8/2` initialization data; it does not
provide a request/acknowledgement alias for that payload. None of these
references supplies the missing non-ring request sources.
The tone-level runner also retains `host_mmio_census.json`, reusing the
literal-seeded Thumb analyzer on a byte-lane-corrected view of the original
MCU image. It resolves three seeds and three halfword writes to DSPIF
`30000`: `271764`, `272048` and `2721d2`. Exact original-byte fixtures
check the constant-4 store sequences and their literal destinations.
Every tone-level run requires all three writes on the upper bus halfword.
The last store can be observed with Thumb PC `2721d2` or word-aligned
bus-context PC `2721d0`; the checker accepts exactly those two forms, not
neighboring instructions or odd PCs. Those are not different writers. Boot's bytewise
zero initialization is dynamically observed but outside these literal-seed
results. Dynamic/table-driven sources and other MMIO windows are not closed.

The isolated MCU poster audit (`tools.c54x_rom4_host_poster`) executes the
original BE Thumb routine at `271d70` with Unicorn, stubbing only its paired
scheduler-lock calls. The baseline 2,352-case matrix covers 47 dispatcher fields, two
out-of-range field probes (`2f/ffff`), four payload probes, both notification
settings, two retained shared-word patterns and busy values `0/1/ffff`.
The 752 valid-field/zero-busy cases succeed. Fields `00..0f` write `(field << 12) | (payload & 0xfff)`
to `100a8`, independent of the retained HPI word. Selectors 8 and 9 additionally
initialize private shadows `10a9e6` and `10a9e8`. Fields `10..16` preserve the
first shadow except for one selected bit (`0400,0200,0020,0010,0008,0004,0001`);
only payload bit 0 controls that bit, not payload truthiness. Fields
`17..1a` update two-, two-, four- and four-bit slices of the second shadow
at shifts 10, 8, 4 and 0. Each writes its updated shadow to `100a8`.
Thus these are subfield updates of the packed selector-8/9 words, not
evidence of independent DSP request bits. For example, shadow `9209`
and field `1a`/value `000c` produce `920c`, regardless of the old HPI word.
The earlier retained-HPI probes did not exercise preservation of these
private shadows; they must not be cited as doing so.

All remaining parameter transformations are also independently checked:

| Fields | Shared output | Transformation |
| --- | --- | --- |
| `1b` | `100aa` | Full halfword. |
| `1c..1f` | `100ac` | Low-byte slices: widths 3/1/1/3 at shifts 5/0/1/2; upper byte cleared. |
| `20..23` | `100ae,100b0,100b2,100b4` | Low 14 payload bits. |
| `24,25,28,29,2d` | `100b6,100ac,100bc,100b8,100be` | Full halfword. |
| `26,27` | `100be` | Replace high/low byte, preserve the other HPI byte. |
| `2a..2c` | `100ba` | Replace bit 15, bit 14 or low four bits, preserve other HPI bits. |
| `2e` | None | Trigger only. |

An additional 3,168 original-routine executions test the eleven shadow-backed
fields using zero, all-ones and every single payload bit, both notification
settings, two independent HPI patterns and four nonzero shadow pairs,
including the observed packed `8426/9209` pair. All 5,520 cases check external
stores and the two private-shadow write streams against separately expressed
mask/shift rules. The extended cases have busy=0; busy rejection remains
covered by the baseline matrix. These are software field identities, not hardware
request-bit names. For every field, notification enabled produces the ordered
external stores `100e0=1`, `30000=4` (halfwords), then `20008=2` (byte).
Field `2e` emits that sequence even when notification is disabled, without
storing a parameter. Other fields with notification disabled do not emit it.
All 1,600 busy/invalid-field cases return zero with no external stores,
including trigger-only `2e` while busy. They still enter and leave the paired
scheduler lock. The MCU tests `100e0` before admitting the update; the mapped
DSP HPINT tail clears the corresponding data word `0870`. This is an explicit
software admission/completion handshake, distinct from the inferred port-1/2
bitmap. It does not prove what signals originally invoked that DSP handler.

Reproduce with:

```sh
.venv/bin/python -m tools.c54x_rom4_host_poster roms/noki5110/5110f530.fls --output tmp/native-host-poster-audit.json
```
The schema-3 report records the original image hash, emulator version and per-case
external/private-shadow stores. RAM contents and busy values are isolated fixture inputs,
not a handset boot or silicon experiment. The four payload/two retained-value
probes are not exhaustive data coverage; private-RAM effects beyond the two
checked shadows, scheduler lock
semantics, live contention and caller retry behavior are excluded. This enumerates the poster's dispatch
surface, not all MCU callers or the DSP port-1/2 alias. In particular,
the trigger-only field does not identify which DSP request bits assert, and
software stores to `100aa` do not prove that a DSP I/O acknowledgement clears
the parameter at `100a8`. The native backend therefore keeps parameter data
independent of its modeled peripheral request sources.

`check-c54x-rom4-tone-levels` joins these shadows to live publications without
reading or changing them in its Lua taps. The ARM bus places selector 8 in
the lower halfword at `10a9e4` and selector 9 in the upper halfword at
`10a9e8`; neighboring record traffic is excluded by lane masks. Every captured
publication must follow a fresh shadow write with the same word, rather than
merely matching an old cached value. Off/Low observe two selector-8 and nine
selector-9 publications each; Medium/High observe two and nineteen.
`private_shadows.json` retains per-run counts. No subfield ID `10..1a` is
observed in those four live captures: their coverage remains the isolated
matrix, not an organic handset claim. Existing physical keys, control
transactions and final digit-2 pixels remain independently required; none
of these results validates native gain or production audio clocks.

At the packed-field publisher, field 8/value `8426` has R5=1 and immediately
reaches the DSPIF trigger; field 9/value `920c` has R5=0 and its trigger
follows a later update. This independently exposes batching: payload writes
must not be assumed to assert the hardware request at their own timestamp.
The passive publisher capture records raw field, wire, R5--R8 and caller
context, with per-phase counts; it is not a producer-absence proof or a
replacement interrupt-source specification.
The native HPINT/vector-25 entry `3772` checks busy word `0870` and local
state `00ef`; its admitted path calls `a51b` at `3796`, strobes data `0029`
bit 3 and clears `0870` at `379e..37a0`. Both ranges are exact-word fixtures.
Suppressing every non-ring INT2 request also suppresses the slot timer: the
ring-only negative capture preserves `900f` but makes only 18 control calls,
never reaches ordinary `8426` activation and ends blank. That negative does
not apply to the accepted ring-plus-slot boundary, which retains HPINT,
reaches the ordinary native UI and delivers level commands intact. Keep the
two hypotheses distinct; neither result identifies all future hardware sources.
The tone-level fixture passively records capped request reads and their
following accept writes, with uncapped totals; it does not require the
current destructive behavior as an acceptance condition.

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

## Serial-control ownership correction

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
Extension bit 7 (FE) now combines with BSPC bit 2 (FO) to select 16/10/8/12
bits as specified by TI table 9-10. Each format transmits the right-justified
payload and sign-extends reception; all 65,536 input words are tested in
each of the four formats, including retransmit/ready-edge behavior. Native
frames consult the saved data-`0023` latch, without changing its reset value
or claiming that firmware uses FE in the observed boot. The generic TI
BSPCE reset value is `0003`, whereas the current MAD2 latch is `0000`;
the product-specific reset/decode remains unresolved. An explicit completed
duplex-frame interface now supports DLB: TI connects DX/FSX internally to
DR/FSR but still requires external CLKR when MCM is zero (section 9.2.3).
Only a caller establishing both clock domains may invoke this interface.
Setting DLB or writing DXR alone produces no receive event. At a supplied
frame, DLB selects the transmitted word instead of external receive data;
ordinary reset, sign extension, readiness and unread-word overrun rules
still apply. Executable helper tests cover all 65,536 values in all four
formats, absent TX, independently reset RX/TX, retained-word underrun,
overrun and copied-state frame continuation. Eight sustained 4,096-frame
streams additionally cover every format with external input and DLB,
loading TX every fourth frame and reading RX every third. An independent
expected held-word model checks payloads and ready edges; copying state
before each frame reproduces the next transfer, unread-word retention and
acknowledgement state. These explicit frame calls prove neither clock
timing nor CPU interrupt dispatch. The MAME build, native
tone-control gate and exact 5110 save-state round trip pass with the
additive interface; these existing gates do not exercise a periodic
duplex attachment or promote its physical clock relationship. No production
clock attachment calls the duplex interface yet; the existing independent
boot-echo TX/RX methods are unchanged. This does not bypass the missing
receive-clock attachment or establish handset playback.
The separate native core gate now tests the two architectural interrupt
sources used by the BSP backend: source 4 enters BRINT0/vector 20, and
source 5 enters BXINT0/vector 21. Nine synthetic cases cover enabled and
masked IMR bits, clear/set INTM, IDLE wake versus vector entry, retained
pending bits and simultaneous receive/transmit priority. With both pending,
receive is serviced first and transmit remains pending. The mandatory result
is `TMS320C54x BSP interrupt sources: PASS cases=9 save_replay=9 handset_audio_claim=0`
under `make check-c54x-core`. Each case also saves after the asynchronous
source edge has reached IFR, before CPU dispatch, and restores the sleeping
CPU and complete pending-source set. Replaying verifies the same vector,
mask, INTM wake and priority result. This does not test saving an input edge
that has not yet been delivered by MAME's scheduler.
The fixture derives source edges from the standard-mode BSP state's completed
duplex-frame result with receive-only, transmit-only or both directions
released. It checks ready transitions and transmitted payload before routing
them to the CPU. This tests completed-word state plus CPU dispatch; the
production backend is covered separately below. Resident codec ISRs,
physical frame timing and handset audio remain distinct boundaries.

`make check-c54x-backend-bsp` instantiates the actual `nokia_dsp_c54x_device`
with its ordinary transport, CCONT and COBBA dependencies and blank synthetic
ROM regions. Synthetic overlay IDLE/vector code exercises receive-only,
transmit-only and duplex completions through `codec_duplex_frame`, including
the transmitted/received words, CPU IFR bits, vector entry and simultaneous
receive priority. Each case saves the backend and CPU after source delivery
but before dispatch, restores, and repeats the vector and payload checks.
After replay, each case holds DSP reset, checks BSP reset state and rejects
further receive, transmit and duplex completions without output/readiness
changes or new CPU source bits. The source-bit check allows asynchronous
input delivery before inspecting IFR. It does not claim IFR clears on reset
assertion rather than release.
The mandatory marker is `C54x native backend BSP: PASS cases=3 save_replay=3 reset_cases=3 handset_audio_claim=0`.
The same fixture requires a separate observation result: two receive-capable
cases perform eight side-effect-disabled BDRR reads with ready-only and
overrun states. These reads return the retained word without acknowledging
either flag. Two saved overrun states survive CPU dispatch and restoration;
four ordinary BDRR reads return that same word and clear ready/overrun.
The runner requires the unique `C54x native backend BSP observation: PASS`
marker with those exact counts, independently of the original vector/reset
result. Debugger inspection must not consume a firmware-owned sample.
The backend additionally accepts explicit external receive pin levels through
`codec_receive_clock_w(clock, sync, data)`. Its bounded subset is standard
burst reception (FSM=1), or continuous reception (FSM=0), with DLB
and autobuffering off. CLKP selects the
sampling edge, FSP selects sync polarity, and FO/FE select 16/8/10/12-bit
words. A qualified sync pulse precedes MSB-first shifting; holding sync
active delays shifting, repeated clock levels do nothing, and a new sync
aborts an incomplete word unless continuous FIG=1 ignores it. The final bit alone can publish BDRR/RRDY and
source 4. RRST cancels a partial word. Unsupported DLB/autobuffer
pin configurations return false rather than supplying a completed word.
These rules derive from TI SPRU131G sections 9.2.4/9.2.6 and table 9-9;
dynamic format/polarity changes during a word remain outside acceptance.

The executable helper checks all 65,536 input words across four formats
and four clock/sync-polarity combinations, plus 184 partial-state copies,
long sync pulses, restart, reset and overrun controls. The actual-backend
gate separately supplies a 16-bit word, saves after eight bits, and repeats
its suffix after load. The snapshot holds the clock high before the ninth
bit: replay begins directly with its falling edge, requiring the stored
clock phase as well as the partial shift value. It checks readiness only at
the last bit, exact
`5678` reception, and three native BRINT0 vector/stack entries, including a
fresh frame after an RRST-aborted partial word. Its mandatory result is
`C54x native backend BSP receive pins: PASS` with `partial_restore=1` and
`vectors=3`. The shift register, remaining width and sampled pin levels are
saved backend state. This is caller-driven conformance: no handset config
or production clock source calls this interface, and no physical edge
alignment, PCM waveform or native call is promoted.

Continuous receive derives from SPRU131G section 9.2.5 and table 9-8.
After one qualified FSR, each word boundary arms the next word without another
sync. An unread BDRR is retained when its successor overruns; BSP then requires
both a BDRR read and a fresh FSR to restart. RRST clears continuous arming.
Mode changes without reset remain outside this pin subset. The helper checks 1,048,576 continuous FIG=0
word/format/polarity combinations with partial-state copies, plus overrun,
restart and reset controls. The actual-backend gate separately requires
`C54x native backend BSP continuous RX: PASS` with five successful word
completions, source 4, sync-free continuation, overrun restart and reset abort.
Its partial-word checkpoint is restored after resetting the live receiver,
requiring the saved continuous flag as well as shift bits and pin phase.
These tests supply external clocks explicitly; no production clock or
handset PCM attachment is added.
Completed-word calls and pin-edge calls are alternative attachments to the
same receiver, not independent channels to drive simultaneously. The backend
pin gate, native tone-control and exact 5110 save/load gates pass; native FR
encoder/decoder frame-history replay remains bit-exact.

The corresponding `codec_transmit_clock_w(clock, sync)` interface accepts
external CLKX/FSX in standard burst or continuous FIG=0/1 mode (internal
clock/frame generation, DLB and autobuffering are not implemented by this
pin interface). TI SPRU131G
sections 9.2.4/9.2.6 define MSB-first output, long-sync MSB holding and
high-impedance DX after the final bit when no continuous successor loads.
BDXR loads the output shifter at FSX;
XRDY and BXINT0 occur at that load, not word completion. A subsequent BDXR
write queues a separate word without changing the active shifter. A new external
frame sync can repeat the retained word after underrun without another ready
interrupt. PCM mode suppresses DX for BDXR bit 15 set without suppressing
the transfer or readiness. Table 9-5 additionally requires XRST assertion
with XRDY clear to raise XRDY and XINT; whole-device reset is distinct.

The helper covers 1,048,576 word/format/polarity combinations, 168 partial
copies, PCM suppression, overwrite, restart, underrun and reset. The backend
fixture saves with eight bits shifted, CLKX low, a queued successor and a
pending BXINT0. Original and restored suffixes agree, the queued word follows,
and independently acknowledged interrupts distinguish underrun silence from
the reset-generated interrupt. Its mandatory transmit-pin marker requires
five words, four vector entries, partial restoration, queued-word preservation,
underrun suppression and reset abort/interrupt checks. Saved state includes
the output shifter, remaining bits, clock/sync phase and PCM suppression.
This interface supplies no production clock or handset audio; physical COBBA
attachment and dynamic mid-word format changes remain unvalidated.

Continuous FIG=0 transmission follows section 9.2.5: a queued BDXR loads
at the next word boundary without another FSX, raising XRDY/BXINT0 at that
load. No queued word means underrun and high-impedance halt. This external-sync
subset does not invent a restart pulse for a later BDXR write; a supplied FSX
restarts it. Extra FSX aborts the active word just as in burst mode.
Switching modes without reset remains outside acceptance.
An additional helper matrix checks 1,048,576 continuous word/format/polarity
cases, 168 partial copies, suppression, restart and reset. The actual backend
requires `C54x native backend BSP continuous TX: PASS`: six word completions
(including suffix replay), five independently settled load-source observations,
queued boundary transfer, underrun halt, external restart and reset abort.
Its checkpoint is restored after resetting the live transmitter, requiring
saved continuous arming as well as the queued and partial words. These are
caller-clocked peripheral tests, not operational handset speech or a physical
COBBA attachment.

Continuous FIG=1 follows SPRU131G table 9-9 and figure 9-23: after the
initial sync pulse releases, subsequent sync pulses are ignored without
holding or restarting the shifter. The initial pulse still precedes the
first word; FIG does not create clocks or bypass reset/underrun recovery.
Burst mode remains sync-driven regardless of FIG. Two saved flags retain
whether the initial pulse has released, independently of current pin levels.
The helper checks 4,096 paired RX/TX pattern/width/polarity combinations,
with multi-edge pulses across word boundaries, copied partial state and reset.
The actual backend requires `C54x native backend BSP frame ignore: PASS`:
two receive and two transmit words, partial save/load after live reset,
ignored later pulses, sources 4/5 and reset abort. The attachment still
supplies every clock edge; no handset PCM routing or native speech is implied.

BSPC XSREMPTY (bit 12) is an active-low underrun indicator, not a direct
"shifter currently idle" flag (SPRU131G section 9.2.3 and table 9-5).
A BDXR write alone cannot set it: a fresh write followed by FSX sets it.
It clears after the last bit only when no successor was queued, and clears
on transmitter/device reset. A queued BDXR preserves it between words;
retransmitting an old word does not clear a previous underrun. PCM output
suppression does not change these transfer-status rules. The explicit pin
matrix checks status at load, every bit, long sync and completion; backend
acceptance requires `shift_status=1` and restores this state with both the
partial output word and queued successor after an extra live transfer has
asserted underrun. Its asynchronous interrupt is allowed to settle before
restoration, so this tests restoration from a genuinely different status
without leaving an undelivered scheduler edge behind. Completed-word calls represent
the whole transfer and therefore include its end-of-word underrun check.

BSPC IN0/IN1 (bits 8/9) reflect the supplied CLKR/CLKX levels, including
while RRST/XRST hold the engines reset; control writes cannot change them.
The helper checks all 65,536 control values against all four pin pairs.
The backend requires four actual save/load round trips with opposing clock
levels between save and restore, under the separate mandatory
`C54x native backend BSP clock inputs: PASS` marker. This models level
readback, not the 0.5--1.5 CLKOUT synchronization latency documented in
SPRU131G table 9-5. A whole-device reset initializes unattached input levels
low; attached sources must drive their own levels.

The joined-pin fixture connects the backend's actual TX output to its RX
input, using opposite shift/sample edges. External sync releases between
those edges after holding the TX MSB in the long-pulse case. A separate
short-pulse case qualifies RX sync before the TX load edge, as the laboratory
CLOCK source does. Both follow the shift/sample relationship in SPRU131G
section 9.2.4; neither establishes fitted Nokia timing. The standalone helper
checks every 16-bit input across both phases, four formats and four polarity
combinations. Backend acceptance additionally requires
`C54x native backend BSP joined pins: PASS`: 128 cases spanning both phases,
four formats, four polarities and four patterns. Each saves both partial
shifters and clock levels halfway through the word, completes and acknowledges it, resets the
live port, then restores and repeats the suffix. Received payloads include
the format's sign extension; CPU source 5 is pending before the checkpoint
and sources 4/5 after each completion. These are controller conformance tests,
not handset firmware-state fixtures or physical clock measurements.

Together these establish frame/pin interfaces, reset suppression and backend
state restoration, not a production frame-clock caller, handset ISR or
COBBA control interpretation. Completed-frame checks bypass bit clocks;
pin checks supply every edge explicitly. Neither is measured handset timing.
The suite also checks all 65,536 control-write values and the state transitions. Native coherent
boot, 30-second processing (6,033 mode-1 frames), organic physical-key tone
command delivery and exact idle save/load pass with this state model. The
unclocked tone-command run observes only the boot echo and its one receive
ISR, not continuous or post-key operational samples. The separate external
clock laboratories above have their own waveform and replay acceptance.

This remains partial hardware: clock-input synchronization latency,
autobuffering, reset-time transmit preloading and physical frame/bit timing
are not established. Only the separately evidenced external boot echo
provides an untimed transfer; ordinary pending samples do not acquire a
fabricated frame or interrupt. The separate clock-owned `nse1pin` laboratory
replaces its own untimed boot echo as described above, not the normal
machine's unresolved clock attachment. No production-clocked PCM is claimed.

The coherent gate now checks real serial reset/release and echo observations
instead of expecting synthetic COBBA parallel register-C logs. Data `0032`
now retains an opaque saved latch without forwarding writes into COBBA.
The local resident candidates at `3b48`, `3b5c` and `3b5e` store `c00c`,
`c008` and `c0c8`; exact sequence fixtures protect that serial-control-shaped
observation. No data-`0032` access occurs in the 11-second physical-key
capture. TI assigns `0032` to different peripherals across variants
(including TSPC and Timer1 TCR); neither identifies MAD2's implementation.
Thus this retires an unsupported MFI side effect, not completes a second
serial port or establishes hardware readback semantics. The actual MFI
attachment remains to be recovered independently. Before attaching the 8 kHz frame
clock, establish the remaining enable, frame and interrupt-routing contract
and validate word timing. MFI port mapping remains an independent boundary; do not
double-drive both owners to preserve an old log predicate.
