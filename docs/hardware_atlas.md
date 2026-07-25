# Hardware atlas — the firmware↔hardware boundary (Nokia 3210, MAD2)

Boot exercises hardware, provisioning and firmware lifecycle together. This
atlas maps the actual MMIO boundary — what the firmware touches, what is
**emulated / partial / stubbed**, and where in the boot it's reached — so the *next* mystery hang
becomes "look up which stub the firmware is poking" instead of a fresh labyrinth.

Current fidelity claims are maintained in `mad2_fidelity.md`.

## Physical board anchor

The [iFixit Nokia 3210 teardown](https://www.ifixit.com/Teardown/Nokia%2B3210%2BTeardown/11328)
identifies the following packages on the photographed board. This is useful
package-identification evidence, not a specification of internal behavior or
address decoding:

| Marking / part | Physical role | Emulator relationship |
|---|---|---|
| TI MAD2PR1 | system ASIC containing MCU, DSP and system logic | decomposed into the ARM core, MAD2 peripheral devices, DSP transport and DSP HLE |
| CCONT NMP70467 | multifunction power-management ASIC | separate `nokia_ccont_device`, connected through MAD2 GENSIO |
| NMP0581 | power-supply component (teardown description) | power conversion is not modeled as a separate firmware-visible device |
| Intel F160B3TA | flash memory | 2 MiB firmware input/ROM region |
| Atmel 24C128 | external serial EEPROM | native MAME `I2C_24C128` device |
| Samsung KM68U1000 | 128K x 8 static RAM | confirms 128 KiB of physical SRAM; current wider CPU window decode remains provisional |
| BGY252 / BGY262 | UHF power-amplifier modules | analog RF chain, not electrically simulated |
| Hitachi HWYN202A | 900 MHz duplexer | analog RF chain, not electrically simulated |
| MZT-03C | piezoelectric buzzer | MAME beeper driven from MAD2 PUP enable and 13 MHz divider; volume response remains unmodeled |

The Nokia NSE-8/9 system-module documentation is the stronger architectural
source. It describes MAD2PR1 as one ASIC containing the ARM MCU, TMS320C542 DSP,
API shared memory and system peripherals, and identifies separate CCONT,
COBBA_GJP, EEPROM, SRAM, flash and UI-switch components. In particular, COBBA
is a mixed-signal RF/audio codec connected to MAD2 by serial-RF and PCM buses;
the current DSP HLE covers only the boot-visible behavior observed across that
larger DSP/codec boundary. It is not complete digital-baseband emulation.

Sources:

- [Nokia 3210 NSE-8/9 system-module technical documentation](https://electronicsandbooks.com/edt/manual/Hardware/N/Nokia/Phone/3210/ch2sys%20%5B103%5D.pdf)
- [Samsung SRAM databook: KM68U1000 family organization](https://bitsavers.trailing-edge.com/components/samsung/_Databooks/1996_Samsung_SRAM_Data_Book.pdf)

MAME device boundaries intentionally follow independently stateful interfaces,
not package count. The MAD2 internal blocks therefore remain separate reusable
devices wired by the phone driver; a package-level MAD2 aggregate is deferred
until those ownership boundaries stabilize.

## The chip

The 3210 board uses MAD2PR1, containing the emulated **ARM7TDMI MCU** and an
unemulated DSP core. Other DCT3 products use related MAD2 revisions, including
MAD2WD1; shared behavior must be established per block rather than inferred
from the family name. The MCU
runs the application/UI/control firmware; the DSP owns GSM Layer 1 and audio.
`nokia_dspif_device` models firmware-visible shared memory, DSPIF and packet
rings. Separate DSP-HLE and external-service devices attach the two semantic
peers without executing DSP instructions. Companion devices are **CCONT** (power/ADC/RTC/charger), the
**PCD8544 LCD**, **24C128 EEPROM** (I2C), and the SIM card behind MAD2 SIMI.

## CPU memory map (the emulated regions)

The driver still maps the wider `0x100000..0x17ffff` provisional family window
rather than asserting unrecovered address-line mirroring for the 3210's 128 KiB
KM68U1000. Runtime structural taps cover `0x120000..0x17ffff`; ordinary v6.00
and v5.01 boots perform zero reads and zero writes there, and all three
structural oracles enforce that observation.

| range | device | handler | status |
|---|---|---|---|
| `0x000000–0x00ffff` (mirror `+0x80000`) | boot ROM / low RAM | `ram_r/w` | emulated |
| `0x010000–0x010fff` (mirror `+0x8f000`) | **DSP shared RAM** | `nokia_dspif_device::shared_r/w` through `dsp_ram_r/w` | Extracted partial transport; peer-owned bootstrap state is published into backing RAM |
| `0x020000–0x0200ff` (mirror `+0x8ff00`) | **MAD2 I/O** (all peripherals) | `mad2_io_r/w` | Mixed partial hardware, calibrated behavior and backing latches; see per-block ledger |
| `0x030000–0x030003` | **DSPIF** (DSP API control reg) | `nokia_dspif_device::dspif_r/w` | stored command-4 doorbell; HLE scheduling still partly shared-write driven |
| `0x040000–0x040003` | **MCUIF** (memory-range config) | `mad2_mcuif_r/w` | Retained four-byte configuration latch; no decoded side effects |
| `0x100000–0x17ffff` | main RAM | `ram_r/w` | emulated |
| `0x200000–0x5fffff` | flash (the firmware) | `flash_r/w` | emulated (BYO dump) |
| `0x600000–0x9fffff` | ROM2 window/mirror | `rom2_mirror_r/w` | emulated |
| `0xa00000–0xa03fff` | EEPROM parallel alias | `eeprom_r/w` | **unproven** read-only view of the input region; normal access uses serial I2C |
| `0xa04000–0xffffff` | unmapped / reserved | — | — |

### Flash devices

The 3210 board carries a 2 MiB Intel F160B3TA. The recovered 3410 contract
instead requires an ST M28W320ECT-compatible 4 MiB top-boot NOR (manufacturer
`0x20`, device `0x88ba`): ordinary 64 KiB blocks followed by eight 8 KiB
parameter blocks at the top. Programming is physically one-way
(`stored &= written`) until erase; assignment-style writes allowed impossible
zero-to-one transitions and corrupted the virgin PMM during compaction.

The tracked MAME flash patch contains reusable ID-address and block-geometry
attributes plus the generic one-way NOR-programming correction; operational
code does not branch on Nokia part-number literals. The extracted
`nokia_b3_flash_device` adapts one limitation of the generic core: the 3410
firmware suspends an erase, accesses a different partition, and polls a fixed
status address while the core exposes a single global command state. The
adapter owns that timer and its save-state fields; the phone driver only maps
accesses and selects the product capability. This remains transitional device
debt, not a firmware-result override, and a generic partition/read-while-write
flash model would replace it upstream. Its one-second erase interval is an explicit,
unmeasured approximation. The firmware polls the ready bit, so no accepted
behavior currently depends on the exact duration.

## MAD2 I/O peripheral registers (`0x20000`, byte offsets)

Blocks: **CTSI** (clock/timer/IRQ/reset), **PUP** (MBUS / vibrator / buzzer / GenIO), **KBGPIO**
(keyboard), **GENSIO** (multiplexed serial: CCONT, LCD, + SELECT-muxed devices), **SIMI** (SIM UART),
**UIF** (CTRL I/O pins). Touch column: ✓ = read or written in a normalized
profile; — = not established.

### CTSI — clock, timer, interrupts, reset (mixed fidelity)
| off | reg | touch |
|---|---|---|
| `0x00` | ASIC version (r, → `0x40`) | ✓ |
| `0x01/0x02` | MCU / **DSP** reset control | ✓ |
| `0x03` | ASIC watchdog write | ✓ |
| `0x04/0x05` | timer-1 current counter MSB/LSB | wraps after terminal `0x7fff`; paired-ROM decode + organic shutdown regression |
| `0x06/0x07` | timer-1 fixed terminal count MSB/LSB | `0x7fff`; paired-ROM stable-read/race contract |
| `0x08/0x09` | FIQ / **IRQ lines active** | ✓ |
| `0x0a/0x0b` | FIQ / IRQ mask | ✓ |
| `0x0c` | interrupt control; bit 5 ninth-IRQ status, write bit 6 ninth-IRQ acknowledge | paired-ROM decode; source unknown |
| `0x0d` | clock control; bit 1 pulses ARM clock-stop, bits 5--6 are SIMI-owned, with bit 5 wired to its clock input | paired-ROM decode; bit-6 effect unknown |
| `0x0e` | **interrupt trigger** (r; read-only — why `assert_irq(4)` can't be SW-triggered) | ✓ |
| `0x0f–0x13` | programmable timer (divider/counter/compare) | ✓ |

Timer 0's divider/counter/compare behavior is modeled with the retained
33,055 Hz CTSI calibration. At divider `0xf9`, paired-ROM timeout code equates
its post-divider interval with Timer-1 intervals divided by eight. Timer 1 is a
separate 1,057 Hz calibrated counter with terminal count `0x7fff`, FIQ5 at the
terminal and wrap to zero on the following tick. Service documentation establishes the external
32.768 kHz source but not the internal divider tree. Applying that rate directly
to Timer 1 and the ratio-derived 1.024 MHz to Timer 0 drives coherent firmware
to reset reason `0x6c`, so the absolute internal dividers remain open.

Paired-ROM task-0 and shutdown code pulse `0x0d.bit1` to stop the ARM clock;
the bit self-clears rather than selecting a retained clock domain. Timer 0 and
Timer 1 continue in the sleep-clock domain. MAD2 resumes the ARM when an
unmasked pending source reaches either CPU interrupt line. Focused tests cover
Timer-1/FIQ5 and physical-key/IRQ0 wake plus save/load while suspended. The
current normalized boot does not reach task 0's idle request, so physical sleep
duty cycle and oscillator transition latency remain unmeasured.

The complete paired-ROM write census also bounds the retained gate bits. A
common initializer sets bits 2--3 once, SIMI routines exclusively manipulate
bits 5--6, and no site writes bit 4. Only bit 5 has an evidenced attached
effect, so MAD2 drives the SIMI device through a clock callback. The auxiliary
bit-6 effect and the peripherals clocked by bits 2--3 remain open rather than
being inferred from their positions.

Both 3210 IRQ dispatchers read `0x0c.bit5` as the ninth IRQ pending indication
and acknowledge it by writing `0x40`. No recovered component or normalized run
asserts that source, so the register grammar is cross-ROM evidence while the
physical owner remains deliberately unassigned.

### PUP — MBUS, vibrator, buzzer, GenIO
`nokia_pup_device` owns control `0x15`, vibrator/buzzer `0x1b..0x1e` and the
sparse GenIO `0x20/0x22/0x24` family. External EEPROM pins, beeper and vibra
output remain callback-connected board components; `0x22` is retained only as
a latch.
| off | reg | status / touch |
|---|---|---|
| `0x15/0x16` | PUP control / FIQ8 ctrl | bit 5 buzzer enable, bit 4 optional vibra-pack enable; periodic FIQ8 routing and clock-stop wake are tested, source rate remains provisional ✓ |
| `0x18/0x19/0x1a` | **MBUS control / status / RX-TX** | extracted controller with byte attachment, FIQ2 RX/TX lifecycle and 9,600-baud character timing; no bus peer ✓ |
| `0x1b` | vibrator frequency/mode | stored independently of the `0x15.bit4` enable; MAME `vibration` output and mapped-MMIO gate test ✓ |
| `0x1c/0x1d` | buzzer divider high/low | MZT-03C square-wave frequency = 13 MHz / divider |
| `0x1e` | buzzer volume | stored; acoustic response unmodeled |
| `0x20/0x22/0x24` | McuGenIO signal / ? / direction | partial: native EEPROM pins plus unknown stored bits ✓; paired-ROM physical-key traces do not corroborate the sibling emulator's proposed 3210 backlight bit 6 |

### KBGPIO — keyboard (extracted partial hardware ✓)
ROW `0x28` signal / `0xa8` direction, COL `0x2a` active-low input / `0x6b`
interrupt mask. Firmware drives a 4-row by 5-column matrix; physical key edges
latch MAD2 IRQ0, whose handler starts the firmware scan/decode sequence. CCONT
uses the separate MAD2 IRQ2 source. Registers `0x29/0x68/0x69/0xa9` and
`0x2b/0x6a/0xaa/0xab` remain backing storage with no established keypad role.
`nokia_kbgpio_device` owns the complete sparse families, scan state, IRQ latch
and cold-boot power sample; handset ports and MAD2 routing remain board wiring.

### GENSIO — multiplexed serial (CCONT, LCD, + SELECT-muxed)
| off | reg | status |
|---|---|---|
| `0x2c` / `0x6c` | **CCONT write / read** | extracted serial protocol; see CCONT below |
| `0x2d` / `0x6d` | GENSIO endpoint control / status | extracted partial hardware: LCD=`0x21`, CCONT=`0x25`, synchronous ready state |
| `0x2e` / `0x6e` | **LCD data / command write** (PCD8544) | extracted MSB-first pin serialization |
| `0x6f`, `0xad/0xae/0xaf`, `0xed/0xee/0xef` | GENSIO **SELECT1/2/3** latches | cross-ROM startup/RMW behavior mapped; attached peripherals remain unidentified |

### SIMI — SIM UART (`0x36–0x3f`)
`nokia_simi_device` owns the MAD2 UART/FIFO/IIR endpoint and FIQ6. The separate
`nokia_sim_card_device` owns ATR, PPS and the T=0 APDU lifecycle; response bytes
return through the organic SIMI register and FIQ6 path.
TXD writes enter a 16-byte hardware FIFO, `0x3e=0x00` flushes a chunk to the
T=0 parser, and `0x3f` reports its live fill. See `sim_subsystem.md` and
`sim_emulator_scope.md`.

### UIF — CTRL I/O pins (`0x32/0x33`, `0x70–0x73`, `0xb0–0xb3`, `0xf0–0xf3`)
General control I/O + directions; partly emulated, touched ✓. Register `0x31`
is CTRL-I/O signal register 1. Its six exhaustive firmware sites only
read-modify-write bit 1 from the power duty-cycle subsystem, so the driver
models it as an output latch; its physical PCB net remains unknown.

The NSE-8/9 service manual places both illumination outputs beyond COBBA_GJP:
COBBA emits separate `LCD_light` and `Key_light` signals to the N400 UI-Switch,
which supplies the LCD and keyboard LED current sinks. This rules out a direct
MAD2 GenIO backlight model for the 3210. It also agrees with the firmware
evidence: a v5.01/v6.00 trace drove a physical key after startup and observed
the candidate PUP/UIF output latches through a further 35-second timeout;
neither ROM changed bit 6 or any other candidate output in that interval.
Boot-time `0x20020` bit-3 traffic is the proven EEPROM clock, not illumination.
The remaining direct CTRL-I/O candidates are also closed: `0x20031` bit 1 is
owned by the power duty-cycle subsystem, while `0x20033` bit 3 is changed
alongside DSP/service serial control. The 3210 light-command semantics at the
MAD2-to-COBBA boundary remain unmapped, so no backlight state is synthesized.
A changed-write census of the complete MCU-visible DSP shared window adds a
negative boundary result: a physical key and the following timeout introduce
no distinct shared-word command. The only later writes are the already-mapped
packet-ring cadence and charger-report control words. Consequently the light
state must not be inferred from those transactions; its DSP/COBBA-side producer
or a lower serial-control surface remains to be recovered.

Physical source: Nokia NSE-8/9 Service Manual, System Module, issue 1 (07/99),
baseband description and Backlight section:
<https://manualmachine.com/nokia/3210/8179317-service-manual/>.

The optional 3210 vibra battery pack uses a separate, internally consistent
PUP contract: `0x20015.bit4` is the output gate and `0x2001b` stores its
frequency/mode control. The named MAME output and mapped-MMIO conformance test
validate that boundary. No current organic application run enables it, so
incoming-call ownership and the control-byte encoding remain open.

## CCONT — power / ADC / RTC / charger ASIC (serial, via GENSIO `0x2c`/`0x6c`)

Register file (`nokia_ccont_device::serial_r/w`), addressed inside the serial command (`addr = (cmd>>3)&0xf`):

| reg | role | notes |
|---|---|---|
| `0x0` | control | |
| `0x1` | unidentified control | compatibility storage; product families use different boot values, with no recovered side effect |
| `0x2/0x3` | ADC read LSB / MSB | |
| `0x5` | watchdog (WDReg) | nonzero data reloads an eight-bit seconds counter; `0x00` powers down; firmware helper `0x2b4dc0` services it organically in both supported 3210 ROMs with WDDISX released |
| `0x6` | unidentified control | all five ROMs write `0x54,0x56`; retained without an asserted side effect |
| `0x7–0xa` | RTC second/minute/hour/day read surface | deterministic binary counters and second/minute IRQs; firmware keeps calendar epoch state in software |
| `0xb–0xc` | RTC alarm minute/hour | partial one-shot comparator; hour bit 7 is a self-clearing disable/update strobe |
| `0xd` | unidentified control | stored latch; effects unknown |
| `0xe` | **interrupt/reset status** | reset `0x03`: persistent bit 0 ready, clearable bit 1 PWRONX cause; upper bits `0xf8` are write-one-to-clear IRQ sources |
| `0xf` | interrupt mask | |

**ADC selectors** (read via reg `0x0`/`0x2`/`0x3`): the driver deliberately exposes raw selectors
`0..7`; on NSE-8, selectors 0/1 are VBATT, 3 is BSI, 4 is BTEMP and 5 is VCHAR, while the remaining board-level
signal names remain incomplete.
Firmware boot reader `0x2a84b0` directly samples selector 0, whereas the later ADC-monitor source 7
maps through ROM table `0x2e2d74` to selector 1. Both are voltage paths; selector 1 uses voltage calibration and the 2100-unit shutdown floor. The complete logical-source table is identical in
3210 v5.01. Values come from the product's typed ADC tuple; electrical
scaling and PCB net names remain open.

A physical charger edge establishes selector 5 more narrowly: CCONT source bit 3 wakes the
firmware, which continues sampling selector 5 for the connected state. The MAME input now drives
that selector from zero to the explicit raw `CHARGER_ADC` scenario value and latches source bit 3
on both connection and removal through a typed CCONT input. Firmware routine `0x2b084c` takes six
VCHAR readings and debounces presence around raw `0x64`. This is an electrically coherent input
contract, not a recovered voltage scale or charging-current/battery-dynamics model; the organic
lifecycle does not re-read selector 7. MAD2 IRQ2 posts task-1 event `0x51`;
firmware reads and clears CCONT source bit 3, enters its charging lifecycle, and consumes a second
edge on removal.

With VCHAR present from reset, task 1 remains in charger mode `0x0009`; its
handler accepts reports `0x0e` and `0x02`, not the ordinary power-key shutdown
report `0x07` directly. A sustained physical power hold follows a separate
controller route through mode `0x000c` into acting-dead mode `0x0005`.
Post-power-off charger insertion now restores the complete MAD2 digital domain.
CCONT retains power and exposes reset-cause bit 2; both 3210 ROMs restart,
sample the connected VCHAR input and settle in acting-dead mode `0x0005`.
The scripted v6.00 lifecycle removes the rail at about 14.03 seconds and the
focused gate supplies its charger edge at 16 seconds. This bounds fixture
ordering only; battery-voltage/current evolution and physical rail timing
remain unmodeled.

## The DSP interface

The MCU-to-DSP boundary is shared RAM at `0x10000` plus DSPIF at `0x30000`.
The DSP core remains unemulated, but the shared-memory peer is no longer a
phone-state constant shim:

- **DSP shared RAM `0x10000–0x10fff`:** device-owned backing storage; the HLE
  peer publishes bootstrap-ready and busy transitions at `0x00..0x04`, `0xe0`,
  `0xfe` and `0x100`. The device owns MCU/DSP ring indices `0xa4/a6` and `0x1c8/0x1ca`,
  drains service pending word `0xe4`, raises IRQ4, and delivers inbound packets
  through FIQ0.
- **DSPIF `0x30000`:** written at boot (`pc 0x2001a4`) and by the reachable service
  command path (`0x290cf4`; command 4 at `0x29103c`, followed by doorbell byte 2). Stateful-SIM
  runs reach this path with service commands `0x30` and `0x32`; the peer retains the register,
  and service-transport ring/service completion now use independent timers. Their observed
ring-producer and service-pending triggers remain distinct from command 4.

The deterministic answered-call trace reaches a new use of that same
shared-control path. Between CC Connect and Connect Acknowledge, task 5 commits
command `0x08` with value `0x060b`; helper `0x290cf4` writes encoded word
`0x860b` to `[0x0a8]`, sets `[0x0e0]`, and rings command 4. The matched
unanswered control produces no non-ring shared writes at this point. A
separate task-9 sequence programs the known oscillator surface for a 900 Hz,
120.8 ms acknowledgement tone. Thus `[0x0a8]` is now the proved lower
call-audio control frontier, while the DSP-internal interpretation and
MAD2-to-COBBA PCM bus remain unmodeled.

The first media-plane slice is now explicit: the radio peer owns bounded
33-octet GSM full-rate uplink/downlink queues, enabled by organic RR traffic
assignment; a separate GSM-FR codec owns only the
33-octet/160-sample transcoding contract; and the DSP clocks those blocks to a
separate 8 kHz COBBA converter endpoint every 20 ms. The paired-ROM
command-`0x08` speech field `0x0201`, independently configured per product,
gates that PCM clock while the `0x0408` dedicated-channel field remains
separate. No audio is injected at the UI. The laboratory network's optional
raw frame loopback and network-side GSM-FR voice peer are explicit,
independent configuration. The latter encodes a service-test 1 kHz PCM source
outside the handset and supplies it only through the radio downlink queue.
Paired v6.00/v5.01 answered-call runs carry that signal through handset
decoding, MAD2 and COBBA: each proves 145 non-silent receiver blocks before
organic teardown. The product-configured serial PCM bus is modelled at its
documented clock/word boundary; MAD2 register programming and COBBA analogue
routing remain unmodelled.

Nokia's DCT3 system-module documentation identifies the physical audio
boundary more precisely. MAD2 contains a DSP serial port connected to PCM and
an MFI interface to COBBA's converters. The four-wire MAD2/COBBA bus carries
`PCMDClk`, `PCMSClk`, `PCMTX`, and `PCMRX`; NSE-8 COBBA supplies a 520 kHz
data clock and derives an 8 kHz frame clock. In the uplink direction the DSP
reads COBBA-produced PCM speech blocks. `nokia_mad2_pcm_device` now owns this
full-duplex, product-configured clock boundary. The configured clock ratio is
checked as 65 data-clock periods per frame, and generic reset defaults to no
clocks rather than silently assuming NSE-8. The closely related NSE-3
MAD2/COBBA-GJ diagram establishes the 16-bit, sign-extended 13-bit word.
Same-ASIC Nokia troubleshooting material establishes a one-clock active-high
frame pulse at 520/8 kHz, while Nokia's documented sign-extended PCM contract
places the MSB-first word on falling data-clock edges. NSE-8 product
configuration combines this family evidence, leaving 48 idle clocks after
the sync and data clocks. This edge choice remains explicitly configurable
pending a direct NSE-8 DSP or logic trace. Every emulated sample is serialized
and reconstructed across that word boundary instead of treating converter
values and GSM-codec-domain samples as interchangeable. Generic COBBA and MAD2
defaults remain unconfigured. Paired firmware proves the combined
command-`0x08` speech-path
field `0x0201`, but not the individual physical meaning of bits 0 and 9.
Paired negative-composition runs also prove that disabling this PCM component
blocks all handset codec/uplink media without stopping the independent
network downlink or firmware-controlled teardown. See the
[NSW-6 system-module description](https://manualmachine.com/nokia/nsw6/4544463-rf-description-and-troubleshooting/)
and the
[NSM-3 MAD2 block description](https://electronicsandbooks.com/edt/manual/Hardware/N/Nokia/Phone/8210/03SYS%20%5B54%5D.pdf),
plus the
[NSE-3 PCM timing diagram](https://electronicsandbooks.com/edt/manual/Hardware/N/Nokia/Phone/6110/03SYS%20%5B73%5D.pdf).
The falling-edge family contract is independently documented in the
[Nokia 12 integrator manual](https://fcc.report/FCC-ID/LJPRX-9/393500.pdf).

COBBA now exposes separate MIC1/MIC2/MIC3 analogue sound-stream inputs and EAR
and HF outputs. NSE-8 board composition terminates the emulator's generic
physical microphone specifically at MIC2 and its speaker at EAR. COBBA samples
the selected input at the documented 8 kHz converter rate and exposes the
resulting signed PCM only through MAD2's uplink direction. With no capture
backend the analogue pin is zero; neither the UI nor a call fixture supplies
samples. Firmware-selected analogue mux register encoding remains unknown and
is not synthesized; NSE-8 product configuration currently supplies the
documented internal-call MIC2/EAR path through an explicitly HLE-only route
API. It is not described as a power-on mux value, and MCU call state cannot
change it. The configuration also carries the nominal gains:
+18 dB on COBBA's MIC2 path and -10 dB on its EAR path; generic COBBA defaults
remain neutral. A `-sound none` headless run correctly leaves the physical
microphone at zero. Separate audio-enabled v6.00/v5.01 acceptance runs attach
an external host 1 kHz source through MAME's microphone endpoint. Each carries
150/150 non-silent, unclipped COBBA blocks through the configured 13-bit
serial representation, MAD2 and DSP encoding to non-zero output from the
network peer's independent GSM-FR decoder. The test source never appears in
the Nokia machine configuration or handset data path.

The generic DCT3 machine now instantiates COBBA without assuming handset
analogue wiring. Only the NSE-8 `noki3210` composition connects MAME's physical
microphone to MIC2 and EAR to the receiver speaker. The serial bus parameters
are grouped in a `bus_profile`, while the temporary analogue selection and
nominal gains are grouped in a separate `hle_voice_profile`; products with no
recovered contract inherit inert profiles and no host microphone route.

The wider firmware contains roughly 287 DSPIF references and 444 shared-RAM
base references, concentrated in the GSM-L1/audio layer at
`0x2b6xxx–0x2c8xxx`. The coherent boot now exercises more than the original
service-handshake corner: task 3 serializes DSP work into the TX ring and FIQ0
owns inbound delivery.
The request-driven peer now completes the startup D0 exchange, the organic type-`0x70/0x74`
service-session completion, and the external task-7 service session in one boot. The generic-service
`0x05e8` chain remains a mapped later radio/SAT path, not the current ordinary-SIM prerequisite.

## Current application coverage

The coherent profile exercises CTSI, MBUS, CCONT, LCD, keypad, GenIO/EEPROM,
SIMI and the modeled DSP/external-service boundary. It completes ordinary SIM
file traffic, clears no-SIM, sets SIM enable, scans the keypad, reaches the idle
screen, and opens `Phone book` through physical input. The optional security
transaction also completes through normal firmware paths. Conditional power,
reinitialization, and service/test display routes are classified separately.
Further application coverage is useful only when it exercises an unresolved
hardware contract. The negative baseline, coherent 3210 profile, interactive
menu, radio registration, and cross-product UI gates remain separate acceptance
profiles. The 3310, 3330, and 3410 now provide product-level portability
evidence. See `mmi_layer.md` and `cross_rom_confidence.md`.

## Diagnostic

Use `RUN_VERBOSE=1` for a bounded first-read/first-write trace of
MAD2 I/O. See `mad2_fidelity.md` for the authoritative implementation ledger.
