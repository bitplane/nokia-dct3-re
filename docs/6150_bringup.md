# Nokia 6150 NSM-1 bring-up

## Current boundary

The acquired v5.23 PPM C package normalizes to an exact 2 MiB CPU-order
image. `make verify-6150-static` regenerates it from a hash-pinned ZIP
wrapper without executing the installer, validates every Wintesla record,
and checks its reset and sparse DSP verification contract. Normal handset
support and graphical boot are not promoted. An explicitly separate
`nsm1r4t` executable compatibility instrument now exists.

The next prerequisite is establishing matching resident DSP behavior and
runtime validity of the explicitly derived product-local EEPROM fixture,
plus fitted flash attributes and address aliases.
A 6110 profile is not a safe substitute: this image uses a larger flash
verification stream and stack addresses outside NSE-3's 64 KiB SRAM.

## Executable compatibility boundary

`nsm1r4t` uses the own normalized flash and checksum-consistent EEPROM,
128 KiB SRAM and the acquired NSE-1 ROM4 native DSP program/data. That mask
is an unproved compatibility input, not an identified NSM-1 fitted mask.
The MCU reset exit remains declared boot HLE. The base 16-Mbit flash part,
ROM4 keypad GPIO layout and other conservative peripheral defaults are
research assumptions; fitted flash identity, aliases and physical input
are not validated. DSP/service/radio HLE publications are disabled.
SIMI and the removable laboratory SIM are enabled from the own reset/RX
register contract below; their presence is not SIM initialization acceptance.

Own CCONT read routine `0x2bf082` selects control `0x28=0x22`, writes its
command through `0x2a`, polls status `0x29` bit 2, and receives at `0x2d`.
The static checker pins this span and register grammar. Selecting this
GENSIO leg alone is insufficient: the later default keypad also owns
`0x28/0x2a` and intercepts the transaction. The explicit compatibility
instrument selects the non-overlapping ROM4 GPIO layout; this is not yet
an independently decoded NSM-1 keypad contract.

Read-only observer `tools/nsm1_native_observe.lua` records actual firmware
shared-memory writes, CPU snapshots and verifier fields; it never writes
firmware/MMIO state. The nine-second experiment progresses past the sparse
verifier into later firmware execution, captures result words `0x0000` and
`0x0004` at `0x111a9e/0x111aa0`, and records native mailbox/completion
activity. The captured 84 x 48 frame is blank. Leaving the verifier is not
proof that its result passes the product's later self-test policy. No
SIM, graphical boot, input, radio or speech acceptance is claimed.

Reproduce using an empty run directory and a ROM directory named
`nsm1r4t` containing the own flash/EEPROM and the hash-pinned NSE-1 program
and data files declared in the driver:

```sh
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame \
  --run-dir run_6150_native_cold -- nsm1r4t \
  -rompath /absolute/path/to/roms -video none -sound none -nothrottle \
  -seconds_to_run 9 -log -autoboot_delay 0 \
  -autoboot_script ../tools/nsm1_native_observe.lua
```

### Current unresolved predicate

Own supervisor `0x2b6180` repeatedly calls readiness predicate `0x29ed4c`.
It requires nonzero bytes at `0x111ea0` and `0x10e6d0`, plus selector helper
`0x27bde4`: selector byte `0x10be8c==0` requires `0x10e6ce==1`; a nonzero
selector requires `0x10e6ce!=0`. Failure at `0x2b61be` skips the subsequent
readiness chain and enters the supervisor latch loop. The static checker
pins all relevant branch instructions and state literals. These are
software states, not yet identified physical input semantics.

Fresh native observation at eight seconds gives first/selector/second/third
`01/ff/00/01`: **the second input alone is false**. The remaining question
is which legitimate firmware lifecycle writes `0x10e6ce`, and what supplies
its prerequisite. The literal scan identifies references to selector
`0x10be8c`, including message-driven stores at `0x2009a0` and `0x2038f4`;
this does not establish an EEPROM field or justify changing provisioning.

The native service path is not simply absent. Own `0x2ab6ce` posts `0x622a`,
waits for bit 2 of `0x11fdd1` to clear, and returns success if bit 6 is set.
The observed return at `0x2b601a` is `1`, with `0x11fdd1=0x48` and DSP state
`0x111a94=1` at eight seconds. This proves that completion path ran, not
that every self-test, radio or UI prerequisite passed.

Observed LCD initializer `0x2bfaa0` writes command `0x24`, bank commands
`0x40..0x45`, column command `0x80` and six 84-byte zero banks through data
register `0x2b`, followed by command `0x20` through `0x2c`. Its status polls
use `0x29` bit 0. Clearing the display is therefore real firmware activity;
a blank frame must not be classified as a missing LCD initializer.

### SIM lifecycle ownership

The false byte is offset 6 of the object at `0x10e6c8`. Its owning routine
`0x288ec8` initializes that offset to zero at `0x288ede`, then initializes
the SIM controller through `0x2af080`. Another candidate store at
`0x288fb2` sets it to one after the counter at offset 2 reaches four
(`0x288f4a..0x288f4e`); this is a retry-exhaustion continuation, not evidence
that setting the byte means successful card initialization. These candidate
stores are not an exhaustive writer census: aliases and bulk stores remain
to be classified.

Own `0x2af080` clears the interrupt cause at `0x20038`, configures UART
registers and writes control `0x32` at `0x20039`. Own RX routine `0x2af0f6`
polls count `0x2003c` and copies bytes from `0x20037` into its receive object.
Those instruction spans are pinned by the static checker. Attaching SIMI
and the standards-shaped removable card is supported by this register
contract, but does not manufacture the task's activation event.

The write-watch observes zero initialization stores to offset 6 and no
retry-exhaustion store in the cold run. With SIMI/card attached, the same
`01/ff/00/01` readiness inputs persist at eight seconds and the supervisor
loop persists through thirty seconds. Observed control writes initialize
the UART; no bit-7 activation write solicits the card's ATR. This is not a
falsification of card protocol behavior: the firmware has not requested it.

### Task delivery frontier

The task's receive loop is `0x288114`, calling RTOS receive primitive
`0x275db4`. It consumes event byte `+4` for ordinary objects, or `+8` for
class `0x1f` objects, subject to its class filter. The receive primitive
indexes descriptors at `0x1014ac + task * 0x1c` using current task byte
`0x100022`; descriptor offsets `0x10/0x11` are compared for available input.
Sender `0x27641c` indexes the same descriptor table by its task argument.
These instructions and literals are checked by `verify-6150-static`.

Cold observation confirms task `0x16` (decimal 22) enters this loop from
`0x2893de` at approximately 2.650 seconds, descriptor `0x101714`, with
receive head/tail both zero. No return object is observed through nine
seconds, and no task-22 invocation of sender `0x27641c` is observed in that
window. The entry trace confirms that the receiver probe was reached;
this is narrower evidence than claiming every possible sender is absent.
The receiver instrument accepts both SRAM and own-flash object pointers:
the RTOS can deliver immutable objects without allocating a RAM copy.

The static gate enumerates 27 direct Thumb BL candidates targeting
`0x27641c` over CPU addresses `0x200000..0x2e0eff`. This is a linear,
even-halfword candidate scan, not a control-flow proof: embedded data,
indirect calls, descriptor interpreters and other sender primitives are
not closed. Candidate sites and scan bounds are in the generated report.
The alternate blocking sender `0x275b60` uses the same task descriptor
table; its linear inventory has 370 direct-call candidates. Wrappers
`0x29c4b4`, `0x29c90e` and `0x29ce18` send own immutable objects at
`0x2e0730`, `0x2e0728`, `0x2e0738`, carrying event bytes `0x37`, `0x38`,
`0x02`. Their respective direct callers include `0x2076d0`, `0x20768a`,
`0x207a6c`; the latter is gated by nonzero object byte `0x10e6cf`.
These wrappers are not yet classified as ordinary startup producers.

### Physical socket control

`tools/nsm1_sim_socket_fixture.lua` removes the laboratory card through
the physical `SIM_REMOVED` input at three seconds and reinserts it at four.
It writes no firmware or MMIO state. NSM-1 card-detect interrupt routing
remains a compatibility assumption, so this is a controller/firmware
experiment, not validation of fitted-board wiring or unattended boot.

The cold socket-edge run delivers own ROM object `0x2e0a50` to task 22 at
approximately 5.922 seconds: bytes `000000000c000000002a0000`, ordinary
event `0x0c`. This object is outside SRAM and must be retained by receiver
observability. Neither of the two traced sender entrances publishes it in
the observed window. Own RTOS receive supplies a descriptor-driven alternate
path: `0x275fac` loads pointer-column base `0x2d869c`, then reads the node's
index byte at `+9` and scales it by eight. Entry `0xe3` at `0x2d8db4`
points to this exact object. The SIM task schedules index `0xe3` at
`0x288f80` through `0x275106`, with delay `0xff + 0x79 = 0x178`.
The cold socket-edge trace confirms index `0xe3`, delay `0x178`, at
3.016680 seconds, retaining caller `0x2ab893` in task 0. This is a
distinct path from the SIM-task schedule site: the caller is the return
from `0x2a70dc` at `0x2ab88e`, so the timer primitive is reached through
that helper rather than a direct call there. RTOS receive reaches the
descriptor branch `0x275f9c` with node `0x100bdc`, index `0xe3`, in task
22 at 5.922080 seconds; the exact ROM object returns ten microseconds
later. The SIM-task schedule site remains static-only. Readiness
still reads `01/ff/00/01` at eight seconds. The expanded no-edge cold control
observes no receive object. The receive path can therefore carry a physical
input-triggered event, but this does not complete SIM activation.

Next: map event-`0x0c`'s continuation and the socket scheduling helper,
then identify what
prevents ordinary boot from advancing into card activation. Include direct
queue/event-table paths, not only the two send wrappers. Keep validating NSM-1
GPIO ownership independently. Do not inject an event, force the readiness
byte, replace the verifier result or infer a missing DSP message solely
from the blank display.

## Own-firmware contract

Input identity and member extents are authoritative in `roms/README.md`.
The checker pins the reset, loader, identity formatter, serial reader and
security-check instruction spans and emits exact anchor/pool coverage counts.
It independently resolves three ARM stack loads.
It does not claim a whole-image control-flow or producer census.

| Boundary | NSM-1 v5.23 evidence |
| --- | --- |
| Reset | ARM entry `0x200040`, header reorder and MCUIF write at `0x20005c`, vector copy and Thumb switch at `0x2000e8`. |
| Stack roots | Three reset pool loads resolve to `0x1160f8`; exception setup additionally adds offsets. This proves the 64 KiB NSE-3 RAM map cannot cover the selected stacks, not the fitted SRAM capacity. |
| DSP verifier | Loader `0x2a479e` initializes `0x10000=0`, `0x10002=0xffff`; stages source halfwords at 32-byte intervals from `0x200040` through `0x3fffe0`. |
| Transfer count | 127 full 512-word blocks, then 510 source words and two `0xffff` terminators; 131,072 bytes total. |
| Handshake | Alternating acknowledgements use shared `0x100fe` and `0x10100`; payload starts at `0x10200`. |
| Completion | Waits for shared `0x10002` to leave `0xffff`; captures `0x10000` and `0x10002` into state `0x111a94` at offsets `0x0a` and `0x0c`. |

Stream SHA-1 is `0fac5ba57a3504f8cb4490a97ab961f468218121`.
This is a sparse external-flash verification candidate, not a contiguous
DSP program. The DSP-side verdict algorithm and compatible resident mask
are not established. No invented completion value or donor EEPROM may
be used to promote this product.

## Primary hardware leads

Nokia's NSM-1 service manual identifies the product as dual-band GSM
900/1800. Its UE4 chapter specifies a serial GD40 display with 84 x 48
one-bit RAM and a five-by-five key matrix. Its system chapter describes
MAD2's ARM/TI Lead blocks, CCONT, COBBA-GJ and the internal MIC2/EAR route;
the PCM link is 1 MHz with an 8 kHz frame clock, with sign extension in a
16-bit frame. These are hardware facts, not proof of a working firmware
peer or native speech.

Primary references:
- [NSM-1 system module, Original 10/98](https://www.eserviceinfo.com/preview_html.php?fileid=9648&previewid=5152).
- [UE4 UI module, Original 10/98](https://www.eserviceinfo.com/preview_html.php?fileid=9648&previewid=5146).

The [indexed original system manual, page 3-45](https://electronicsandbooks.com/edt/manual/Hardware/N/Nokia/Phone/6150/03sys%20%5B92%5D.pdf)
establishes 16 Mbit flash, 1 Mbit SRAM (128 KiB) and 128 Kbit serial EEPROM
(16 KiB). Complete PDF retrieval remains unavailable; fitted flash attributes
and address aliases are not inferred from those capacities.

## Product-local EEPROM input

The original NokiX 2011.07.24 archive contains a 16 KiB `nsm-1.bin` repair
template. Source/member hashes and acquisition URL are in `roms/README.md`.
The static gate audits its original bytes without applying repair scripts
or changing checksums. It is a historical product-local template, not a
verified factory handset dump or proof of boot compatibility.

The own-image directory `0x2da10c..0x2da48b` contains 112 distinct group-7
descriptors; all fit the documented EEPROM, with the highest end `0x3fa8`.
Records `0x0701` and `0x0702` select `0x03cc`/8 bytes and `0x03d4`/44 bytes;
nested record `0x070b` selects the checksum at `0x03d2`/2 bytes.
Settings load `0x2c3a70` places record `0x0702` at SRAM `0x11fc16`.
Validator `0x2bccfe` reads setting state `0x11fc35`, establishing index
`0x1f` and physical EEPROM byte `0x03f3` without a value sweep.
It sums a sixteen-byte identity buffer via `0x2b1e2c`, adds the setting and
compares the truncated result with checksum state `0x112826`.

### Identity and checksum closure

Own reader `0x2b8b90` resolves record descriptors and calls the raw serial
reader `0x2bcace`. The latter deposits bytes from receiver `0x2bdece`
unchanged; no record decoder intervenes. The security record is loaded into
`0x112820`, putting the compared checksum at offset `+6`, independently
agreeing with nested record `0x070b` at physical EEPROM `0x03d2`.

Receiver `0x2bdece` independently establishes the serial wiring: it builds
PUP GenIO data address `0x20020` and direction address `0x20024`, releases
SDA by clearing direction bit 0, raises SCL with data bit 2, samples data
bit 0 while SCL is high, then lowers SCL. Its shifting `0x80` receive mask
establishes MSB-first byte reception. The static gate pins these instructions;
this is not yet runtime verification of the complete serial transaction.
An executable NSM-1 profile must select SCL bit 2, not the NSE-8 default bit 3.

Identity wrapper `0x2ab73a` invokes `0x27e254` with selector 3. This reads
eight EEPROM bytes at `0x000c`, renders the first seven bytes as high/low
packed-BCD decimal digits, computes the fifteenth decimal check digit and
null-terminates the buffer. The wrapper also sets byte 15 to zero. Assuming
successful serial reads, the original template produces
`493006102132132` followed by zero. Raw BCD bytes must not be substituted
for that sixteen-byte ASCII buffer.

The original setting remains `0x58`. Its required checksum is `0x034d`,
not the stored `0x3124`; the unchanged historical template cannot satisfy
this initial security-check relationship. `verify-6150-static` generates
ignored `roms/research/nsm1-v523/nsm1-security-checksum-valid.bin`, changing
only EEPROM bytes `0x03d2..0x03d3`. SHA-1 is
`07335e492f72ba9da9c3436890c9eb4af67fd9a9`. Identity, lock/settings byte,
calibration and all other content remain unchanged. The canonical acquired
template is not rewritten.

This is an evidenced nonvolatile-input consistency fixture, not a factory
dump or boot promotion. Remaining checksums, runtime boot, native DSP and
radio acceptance are unproved. Do not alter a lock level to seek a passing
screen, transplant NSE-3 records or synthesize a DSP completion.
