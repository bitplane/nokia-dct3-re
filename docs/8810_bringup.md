# Nokia 8810 (NSE-6) bring-up boundary

## Current Result

Normal `nse6stage` remains at its own DSP-verifier acknowledgement boundary.
The separate `nse6r4t` fixture retains own MCU flash and uses the acquired
NSE-1 ROM4 mask as an explicitly unproved compatibility input. Its decoded
integrity/retry EEPROM fixture and nominal ADC profile establish physical
security-code entry, Menu / Names and persistent SIM phonebook save/recall
in the documented compatibility experiments. Those observations are not a
current full-native handset acceptance gate across the port-`0x36` wait.
They do not establish a fitted DSP mask, stable native radio, registration,
calls or SMS. The current native resumption boundary is described below;
the full-scale ADC startup observations are negative controls, not that frontier.

The current native execution boundary is DSP I/O port `0x36`, not an MCU
message producer. At 3.649008 seconds, resident routine `0x9129..0x9161`
reads `0x0080`, clears bits 1--4, sets `0x801c`, and writes `0x809c` to
the port. It then reads the same retained value at `0x913e` and loops at
`0x9142` while bit `0x4000` is clear. A passive I/O trace confirms the
write followed by unchanged reads; AR2 is `0x0060` and control word
`0x06f3` is zero. The backend currently provides only opaque saved storage
for this port. Completion would leave the loop, clear bit 15, and write
the port again. The physical block, status-bit ownership, trigger and
latency are not established. The surrounding port-0/port-36 sequence is
observed below, but its physical completion contract remains open; do not assign
the port to COBBA or a PLL from the polling shape alone.

A fresh ten-second control after removing the duplicate MDISND commit-time
HPINT reproduces this boundary: `0080 -> 809c`, unchanged poll reads, and
PC `9142` at five seconds. `nse6_serial_control_check.py` passes with decoded
transaction time 3.649006673 s and no completion claim. The shared fix that
permits sustained NSB-1 execution does not establish this independent port's
missing completion bit, nor stable long-run NSE-6 operation.

The trigger is an organic type-`0x4c` packet (`0x044c`, payload
`0080,0104`) consumed at 3.649007 seconds. Resident dispatch `0x3bcd`
calls `0x9232` (saved return `0x3bcf`); that routine clears control-word
`0x06f3` bit 0 at `0x9253` and calls `0x9129` (saved return `0x9258`).
The passive entry trace confirms both call edges. Thus this is a control
reconfiguration after memory uploads, before search-command consumption,
not a wait entered by the search handler itself. The existing
`tools/c54x_rom4_port_census.py --port 0x36` finds 18 read and 13 write
candidates, including absolute Smem forms omitted by a two-word scan.
Most accesses are in `0x8d96..0x92a4`; additional reads occur at
`0x34da` and `0xb8b7`. This is a candidate-site inventory, not an exhaustive
instruction-boundary or computed-port census. The other entry at `0x9116`
is not observed in this run; do not infer its callers from the blocked
`0x9129` path.
The scanner now handles all absolute-address aliases `f8..ff`, rather than
only `f8`; sixteen read/write alias controls and incomplete-word cases guard
address-before-port selection. The corrected acquired-ROM census retains
the same 18 port-36 read and 13 write candidates. It adds raw candidates
`71e5` (port `0f`) and `71ef` (port `0a`) elsewhere; neither is promoted to a
validated instruction site or peripheral contract. The before/after inventories
are `tmp/c54x-port-census-{before,after}.txt`.

The recovered legacy IDA database also has no names or comments for this
serial-control region; see the [archive annotation contract](djr_dsp_integration.md#recovered-ida-annotations).
Its codec-function labels are independent research leads, not a peripheral
register specification.

The acquired archive's `full.s54` agrees with the independently decoded
`BITF 0x4000` / `BC NTC` loop but does not name the physical block. A fresh
12-second NSE-1 cold run with its own EEPROM profile and the same native
backend retains port `0x36=0x0080`. Its counts for `0x3bcd`, `0x9232`,
`0x9129` and `0x9140` are all zero. The working handset therefore bypasses
this restart transaction in the measured boot; its success is not evidence
for a port-36 ready-bit value. `tools/c54x_rom4_port36_observe.lua` is the
portable passive comparison instrument. Complete ownership of the port's
fields remains a software-research target; this negative control alone does
not establish a hardware-only blocker.

Current fresh compatibility execution still reaches this wait after native
parameter/request separation and queued-transfer modeling. Live readback at
`9129` verifies both the four words at `23e4` and all 59 words at
`9009..9043` against the checked resident image. The former contains the
return/NOP hooks at `23e4/23e6`, not an uploaded clock/completion routine.
The latter calls `23e4`, sets INTM, initializes internal serial pointers,
counters and state, and consults port-36 bit `0080`; it does not write
the peripheral or publish bit `4000`. The surrounding restart request,
port-0 writeback and completion loop are separately byte-pinned.
These observations exclude a hidden replacement of those helpers in this
execution, not every possible peripheral producer or the unknown fitted mask.
They supply no baud divisor, physical clock-enable identity or completion
latency. Keep the status input unresolved rather than substituting a
ready constant or a reset delay chosen to make this compatibility test pass.

The portable observer retains live-helper readback and bounded entry counters.
Validate it with `tools/c54x_rom4_codec_contract.py <program-image>
--serial-helper-trace <error.log>`. That optional check requires both complete
unmodified helper ranges; its result explicitly denies a peripheral-completion
claim. The standalone NSE-1 boot still does not exercise this restart.

### Native port-34/35/36 contract

The static port census finds five writes to `0x34` and two reads from
`0x35`. The following meanings are firmware-consumer contracts, not a
validated silicon register map:

| Site / field | Recovered use | Remaining uncertainty |
| --- | --- | --- |
| Port `0x34` | Data writes at `0x34e8`, `0x34f4`, `0x920a`, `0xb821`, `0xb8bf` | Wire, framing, width and transmit cadence |
| Port `0x35` | Data reads at `0x8ed8`, `0xb792`; `0x8ed3` feeds the `0x8fxx` framed-byte state machine | Receive-ready cause and data delivery |
| Port `0x36`, bit `0x2000` | Set permits data writes in `0x34da..0x34f4` and `0xb8b7..0xb8bf` | FIFO depth / holding-register behavior |
| Bits `0x0600` | `0x8ee8` tests the combined mask; bit `0x0400` selects parser state 4 rather than 2 | Error/event identities and acknowledgement |
| Bit `0x0080` | Read by parser and buffer-reset policy; parser also sets/clears it in control writeback | Direction and clearing semantics |
| Bits `0x001e` | Restart temporarily sets them to `0x001c`, then restores them from `0x06f3` | Format/rate/mode meaning |
| Bits `0x8000` / `0x4000` | Restart writes the former, waits for the latter, then clears the former | Completion source and elapsed time |

The parser at `0x8f41..0x8f86` recognizes byte markers `0x1c` and `0x1e`
and accumulates bytes into word buffers. This is evidence of a framed data
path, not an RF sample or PLL-lock contract. AccIF is a candidate owner:
Nokia's [NSW-5 manual](https://www.manualslib.com/manual/2113842/Nokia-Nsw-5-Series.html)
identifies a serial accessory DSP-UART, while the
[NSB-5 manual, System Module pp. 17--18](https://manualmachine.com/nokia/7190/5055290-service-manual/)
describes AccIF encoding/decoding of infrared pulses on the FBUS path.
Those are other products' physical descriptions; neither maps ROM4 I/O
ports `0x34..0x36`. The MCU producer contract below corroborates serial
configuration, but does not identify the exact AccIF wiring or status-bit
implementation. The completion bit remains unmapped, so no reset timer or
UART-ready constant is yet justified.

The own-product Nokia NSE-6 System Module manual, Original08/98,
corroborates a separate accessory path without closing the register mapping.
Its [text preview](https://www.eserviceinfo.com/preview_html.php?fileid=5448&previewid=3000)
identifies AccIF separately from MFI and DSP PCM (pp.3-31..33), and gives
FBUS_RX/TX rates9.6..230.4kbit/s (p.3-12). It therefore strengthens the serial
candidate over an RF interpretation, but names no DSP I/O address, restart
bit or completion delay. The recovered port34/35/36 transaction must still
be linked to that physical interface before implementing status transitions.
The local reference is an HTML text preview, not the full schematic PDF;
see `roms/README.md` for its hash and acquisition scope.

The adjacent `djr-747` emulator at revision
`b93a7f7143ea8d6d636ae920ac29258d3f6666f6` does not close this boundary.
Its native `mad2_dsp_c54x.c` port-36 read is an explicit
`DSP54_PORT36` diagnostic override, not a stateful restart/completion model.
Its separately modeled COBBA serial register path uses ports `2c/2d` and
bit `1000`; those semantics do not establish this interface's bit `4000`.
The inspected native adapter and inherited core supply no matching restart
trigger or latency evidence. This is a bounded reference check, not proof
that no software-accessible specification exists. Do not transplant the
diagnostic override or COBBA ready behavior into the NSE-6 backend.

The passive serial observer now includes ports `0`, `2`, and `34..36`,
retains uncapped totals/last values and logs at most four events per site.
It makes no target reads or writes of its own. A fresh twelve-second run in
`tmp/8810-serial-neighbors` uses own flash and the declared ROM4 compatibility
mask with a newly generated checksum/retry diagnostic EEPROM, not donor
storage. It observes this ordered restart at 3.649 seconds:

- Port `0`: read `001d`, write `000d` (clear bit `0010`).
- Port `36`: read `0080`, write `809c`.
- Port `0`: read `000d`, write `001d` (restore bit `0010`).
- Port `36`: repeated `809c` reads at the completion loop.

The original archive's `full.s54` independently agrees on the literal
clear/restore at `924c/924f` and `9135/9138`. Firmware again clears the same
port-0 bit at `914c/914f` after successful completion, before restoring
the port-36 format bits. This establishes a firmware-owned gating sequence,
not the bit's electrical or interrupt/clock identity. An earlier native
port-34 write publishes `1eff` at 0.385935 seconds; no port-35 receive read
is observed in this run. Neither event identifies the external endpoint or
justifies fabricating receive data or completion status.
The separate fresh five-second run `tmp/8810-serial-neighbors-final`
validates the uncapped observer: 7,902,023 port-36 reads end at `809c`,
three writes end at `809c`, one port-34 write carries `1eff`, and port-35
has no accesses. All four restart-site counters are nonzero. The summary
records observed accesses, not extra debugger reads or physical hardware
acceptance; the compatibility mask and diagnostic EEPROM limitations remain.
The fresh same-observer NSE-1 control `tmp/5110-serial-neighbors` also writes
`1eff` once to port-34 and has no port-35 traffic, but ends with port-36
`0080` after four reads/two writes and all four restart-site counters zero.
Thus that transmit word is shared startup behavior, not evidence for the
8810 restart's missing completion or for its physical endpoint.

The MCU producer is constructor `0x29b9ea`, called organically by initializer
`0x29c022` at `0x29c094` (observed LR `0x29c099`, 3.648453 seconds).
Its arguments are both zero. It allocates eight bytes, writes prefix 2,
length 4 and type `0x4c`, then encodes payload bytes as
`r0, 0x80 | (r1 << 2), 1, (r0 == 0 ? 4 : 0xff)` with byte truncation.
The resulting `0080,0104` object reaches task 3 through `0x27b1c8` at
3.648771 seconds. A bounded direct-Thumb-BL scan of the MCU extent finds
four constructor callers: `0x231db2`, `0x29ba90`, `0x29c094`, `0x29c69e`;
computed callers remain outside that coverage. Nearby helpers `0x2bb268`
and `0x2bb276` access RAM (`0x120564` and `0x120534` respectively), not
peripheral registers. They do not corroborate AccIF ownership. The next
ownership question is the initialized module's service/callback contract,
not whether a missing MCU packet needs to be synthesized.

The module's configuration setter `0x29ba3c` has five direct BL callers in
the bounded MCU scan: `0x223f62`, `0x22446e`, `0x29bd94`, `0x29c08c`,
`0x29c594`. The first two lie in AT-command handlers carrying the literal
responses `+ICF: (0-6),(0-3)` at `0x224014` and
`+IFC: (0-3),(0-2)` at `0x2242d8`; the adjacent handler advertises `+IPR`
rates through 115200 at `0x2245b0` (the string starts with `+` at `0x2245b0`).
[ITU V.25ter sections 6.2.10--12](https://www.itu.int/dms_pubrec/itu-t/rec/v/T-REC-V.25ter-199508-S%21%21TOC-HTM-E.htm)
identify these commands as DTE rate, character framing and local flow control.
This independently supports a serial-configuration role; it is not evidence
for a particular baud value or completion delay in DSP port `0x36`.
The getter `0x29b9d4` returns four stored configuration bytes; the setter
stores its first three arguments and a separate mode byte. Initializer
`0x29c022` supplies `(9,4,1,1)` to that setter before sending type `0x4c`
with arguments `(0,0)`. These two argument tuples must not be conflated.

Measured control decoding sets `0x06f4=0x0880` at PC `0x3bb3`,
`0x06f6=21` at `0x3bbd`, and `0x06f5=866` at `0x3bca`, before the
restart wait. The `0x0800` flag is set by the delayed branch at `0x3ba0`;
the control word is not a verbatim copy of payload `0x0080`.
The timeout arithmetic is `(high_byte * 347) >> 4` and
`(low_byte * 3467) >> 4`, with low byte `0xff` selecting sentinel `0xffff`.
`0x9185..0x9189` compares counter `0x06c4` against `0x06f6`;
`0x8e4b..0x8e67` compares `0x06b3` against `0x06f5`, bypassing the sentinel.
Both counters increment in `0x8e6e..0x8e74`. At the measured 216--217 frame
ticks/s these are approximately 0.1- and 4-second software deadlines, not
baud divisors or evidence for hardware restart latency. Reproduce the
ordered packet-to-parameter observation with
`tools/nse6_serial_control_check.py run_8810_serial_parameters/error.log`.
That check makes no peripheral-completion or native-search claim.

Initializer callbacks `0x2affb0`, `0x2b00a4`, and `0x2affc8` post to task 25;
this identifies a firmware delivery boundary, not the task's semantic name.
Nearby MCU routine `0x29b830` builds type-`0x50` byte-buffer packets.
Do not infer that these use the same peripheral from code proximity:
resident dispatch `0x398d..0x3995` indexes data-ROM table `0xb00f + type`,
whose entries map `0x4c` to `0x3b92` and `0x50` to `0x3e72`.
The latter calls `0x7e9c`, fills buffer `0x1896`, and sets `0x1f25` bit 4;
it does not directly call the port-34 transmitter. Continuity from that
buffer to the serial peripheral is unproven. These are acquired ROM4
consumer contracts, not evidence that the unknown fitted NSE-6 DSP mask
has identical packet semantics.

Native TX completion now wakes the MCU pending queue through MAD2 FIQ1.
IRQ dispatch `0x2deb9a` sends IRQ4 to `0x2b66fc`, which handles
shared-service counters and block uploads, not TX-queue wakeup. FIQ dispatch
`0x2dec1c` sends FIQ1 to `0x2b5e78`, which posts control 1 to task 3. FIQ0
handler `0x2b5e96` posts control 4 to RX task 4 and conditionally posts control
6 to TX task 3 when bit 0 of shared word `0x101f0` (DSP `0x08f8`) is set,
clearing that bit first. The separately adjacent literal `0x1205dc` is the
TX payload source-pointer slot, not this notification flag.
The missing route was native port-1 completion bit 1: the backend previously
sent every completion to FIQ0 and had no FIQ1 output. The corrected transport
pulses FIQ1 when the resident DSP writes that bit. It retains the existing RX
doorbell pending a complete decode of higher-bit routing; this is a partial
completion model, not closure of every port-1 bit.

The completion census observes port-1 value `0x0002` from resident PC
`0x395e`, following the TX service routine's call to `0x3860`, and value
`0x0001` from RX publication PC `0x380a`. With the bit-1 route restored, a
fresh 30-second compatibility run observes 13 FIQ1 handler entries. The real
type-`0x1a` object `0x10d758` passes the free-space test (162 bytes for a
68-byte payload) and reaches ring publication at 6.565502 seconds. The
checker `tools/nse6_tx_wakeup_check.py` requires ordered same-object mailbox,
space-check and writer observations, plus native completion and FIQ1 evidence.
It does not claim DSP consumption or search execution. The resident DSP
subsequently remains near `0x913e..0x9142`; mode initialization still does not
run. That loop is the next native boundary, not a reason to force the queued
search command. Overlay integrity and the four measured memory uploads pass;
both 3210 baseline gates and the native 5110 Phone book menu also pass.
The fresh trace observes no native write to the conditional shared flag
`0x08f8`; that negative result is bounded to twelve seconds, not a complete
writer census. Other port-1 bits (`0x001c`, `0x0100`, and bootstrap values)
have unresolved routing and must not be assigned meaning from this pair.

DSP MMR `0x0029` bit-3 pulses are present during bootstrap and at 6.482857
seconds (PC `0x38f9` / `0x38fc`). The latter resident path tests and clears
DSP word `0x086e` (MCU shared offset `0xdc`), not the TX consumer pointer.
The backend already routes these transitions to MAD2 IRQ4; an entirely
absent IRQ4 connection is not the explanation.

The acquired v6.02 PPM A package normalizes to a complete, contiguous 2 MiB
CPU-big-endian flash image. Research machine `nse6stage` now executes its own
reset/initialization and reaches the verifier's first DSP acknowledgement wait.
The ROM4 compatibility fixture now has graphical security-editor and physical
digit-entry acceptance. An initialized synthetic retry count permits default
code acceptance, Menu / Names navigation, and persistent SIM phonebook
save/recall; broader card/service parity remains unproved. Other products'
provisioning and DSP publications are not evidence for this handset.

The reset stack base `0x125f30` fits the documented 256 KiB SRAM window starting
at `0x100000`; importing a 128 KiB product configuration would be incorrect.
The silent baseline waits for its first DSP acknowledgement. The separate
`nse6r4t` compatibility fixture executes acquired NSE-1 ROM4 code and reaches
CONTACT SERVICE, without proving that mask belongs to the 8810.
An explicitly diagnostic six-byte integrity correction removes the two decoded
erased-storage failures and reaches SIMI initialization. Authentic provisioning
and stable long-running handset operation are not established.
The nominal VBAT profile now completes ten acquisitions and publishes report
`0x14` organically at about 1.763 seconds. The input controller leaves state
`0x10`, reaches state `4`, and changes the column mask from `0x3f` to `0x20`.
The display presents the firmware's Security-code editor. Physical contacts reach matrix scanning and
decoding: the 30-second fixture records 23 scanner entries and 19 decoder
entries by 28 seconds. This proves input execution, not the inherited host
labels beyond those exercised. `tools/nse6_security_fixture.lua` enters five
digits through physical contacts: the 12.5-second snapshot shows five masked
characters, and OK produces Code error at 13.5 seconds. The late frame returns
to the editor in the erased-retry negative control. Initializing the synthetic
retry count permits the same physical code to be accepted, reaching a Menu /
Names frame. The current native compatibility configuration preserves the own
upload through `0x2fff`; a fresh 40-second run executes without a logged
unsupported opcode, accepts the physical code at 13.022352 s and presents
Messages, Call register and Missed calls under physical navigation. The
previous stack/ring corruption is resolved by retaining the executable upload
bank, not by changing CALL/RET semantics or firmware state. The firmware still
requests a reset at 31.967763 s, so stable long-running boot remains open.
No donor identity or lock state is supplied.

The display-transport observer records 15,684 data bytes, 2,774 nonzero,
and 584 commands by 28 seconds in the digit/softkey/navigation fixture.
Inspecting its actual `snap/8810-stage-late.png` corrects the stale blank-frame
claim: display traffic and editor presentation are present. This is not native
speech, network registration, or proof of the fitted DSP mask.

The profile is an explicitly nominal silicon-transfer hypothesis, not measured
NSE-6 calibration. The own-board schematic connects BVOLT to CCONT VBAT pin D2,
and the system manual specifies a nominal 3.6 V pack. The original Nokia
[NSE-8/9 service manual](https://manualmachine.com/nokia/3210/8179317-service-manual/),
Table 30 (page 2-54), specifies the CCONT VBAT input's typical 6.5 mV/bit
transfer (6.4–6.6 limits). Thus `round(3600 / 6.5) = 554`; the own-ROM identity
calibration transforms this to `3581`, inside its acquisition interval.
This uses the silicon VBAT input specification, not the 3210's different
board-level battery routing. Other ADC channels remain research defaults.

### Menu Execution and Restart Boundary

**Current native contract:** recover the port-`36` completion described under
Current Result. The earlier MCU TX queue/ring-space observations are not a
second unresolved message-producer frontier: native upload/control processing
now reaches the independently observed port transaction before further search
work can complete. Neither a fabricated heartbeat nor altered NV reset policy
is an evidenced correction. The acquired NSE-1 mask remains a compatibility
instrument, not an identified fitted NSE-6 mask.

`tools/nse6_menu_fixture.lua` accepts the default code then applies physical
Menu, Down and Select contacts. Snapshots show Messages, Call register, and
Missed calls respectively. `tools/nse6_names_fixture.lua` opens Names, selects
Add entry, and reaches name and number editors with entered characters.
This validates the exercised softkeys/navigation and editor inputs. Call
signaling and SMS transport remain unproved.

### Persistent SIM Phonebook

The Names fixture now presses Save at 31 s, before the known reset; its
31.5-second frame reads **Saved to SIM card**. The generated card NVRAM's
first 32-byte ADN record contains name `A`, BCD number `123` (`03 81 21 f3`
at bytes 18..21), and erased padding. The other 49 records remain erased,
verified by `sim_phonebook_check.validate_phonebook_storage(data, b"A")`.
No record is preseeded: physical contacts enter the name/number and firmware
owns the card write.

`tools/nse6_phonebook_recall_fixture.lua` starts a new MAME process using
that same NVRAM directory, accepts the default phone code, and applies
Names / Search / blank search / Detail contacts. The 23-second frame shows
`A`; the 25-second frame displays **Number: 123**. This establishes the
exercised SIM save and cold-start UI recall, not phone-memory storage,
edit/delete coverage, native calls, SMS, or long-running stability. The
six-byte integrity and initialized-retry diagnostic remain explicit inputs;
identity is still erased and the fitted DSP mask is still unknown.

The focused checker combines reviewed whole-frame pixels, the final physical
save/detail contact, and exact A/123 card storage. It does not substitute a
key log for visible recall or assert an APDU-level trace. Reproduce with a
new diagnostic EEPROM directory, run `nse6_names_fixture.lua` for 32 s,
then run `nse6_phonebook_recall_fixture.lua` for 30 s in a separate process
**preserving the same NVRAM directory**. Use `run_mame_isolated.py` with
separate run directories and the own normalized NSE-6 plus ROM4-compatible
research ROM path. Validate each stage:

```sh
.venv/bin/python tools/nse6_phonebook_check.py save \
  run_8810_phonebook_save/error.log \
  mame/nvram-8810-phonebook-save/nse6r4t/sim_card \
  run_8810_phonebook_save/snap/8810-names-save.png
.venv/bin/python tools/nse6_phonebook_check.py recall \
  run_8810_phonebook_recall/error.log \
  mame/nvram-8810-phonebook-save/nse6r4t/sim_card \
  run_8810_phonebook_recall/snap/8810-phonebook-recall-25.png
```

These paths are disposable run evidence, not shipped provisioning. The
checker is currently a manual research gate, not a supported-product gate.

The longer 40-second menu run returns to reset entry `0x200040` at
31.967763462 seconds. At 31 seconds the CCONT countdown is `0x13` and MAD2
watchdog register is `0xff`; neither expires. Firmware writes `0x31` to
`0x20003` at 31.001033692 seconds and reset-control `0x05` to `0x20001` at
31.967763000 seconds, from `0x2e51e0`, caller return `0x243c27`.
Own dispatcher `0x243a24` routes input `0x00d4` to `0x243ba4`. That branch
increments counter `0x13fde6` if activity byte `0x13fdeb` is zero, otherwise
clears it; at count >=15 it can request reset. This is not the observed
late-reset cause. A fresh native run receives D4 at 31.001021 s, increments
the counter to one, then clears it through `0x2c91c8` at 31.015760 s.
At 31.966920 s it receives **D6**, enters `0x243af6`, and follows the shared
reset-call tail `0x243c22` with reason `0x68` and counter zero. The shared
caller return `0x243c27` alone cannot distinguish D4's timeout from D6.
The receive primitive's timer-queue branch at `0x27b604` dequeues timer ID
`0x16` from record `0x10b2ac` for task record `0x10c884` at 31.966903 s.
Its ROM mapping at `0x2fa344 + 8 * 0x16` supplies status D6; the dispatcher
receives it 17 microseconds later. Thus the observed D6 is a timer expiration,
not a demonstrated DSP response. The verified arm call at `0x27a76e`
receives ID `0x16` and delay
`0x7d` at 31.000918 s, with caller return `0x2d3473`. The arm routine starts
at `0x2d3460`, queues control value `0x1b` to task 1, then arms the timer.
A further fresh run enters that routine with caller return `0x2c6973` at
31.000869 s. Own code at `0x2c696e` calls it after selecting and storing
reason `0x68` through `0x2d3408`. The selector uses the NV policy and SIM
status decoded below, plus global guards; their broader lifecycle semantics
remain unresolved.
This is not evidence of an idle-loop timeout or a missing DSP heartbeat.
The selector entry is `0x2c68c8`. A fresh run enters with input `1`, caller
return `0x2e25f5`, at 31.000193 s. Its NV read uses descriptor `0x0607`;
at `0x2c6908` the local NV byte is `0xff`, the independently acquired status
byte is `5`, and the selected reason is `0x68`. Input 1 tests bit 0 of the
NV byte; input 2 separately tests bit 1 and can select `0x69`. Erased NV is
therefore an observed input to the live reset decision, not proof of a missing
DSP response. The remaining guards also pass in this run. No valid default
or authentic replacement value for descriptor `0x0607` is established.
The status reader's SIM provenance is established below. The descriptor's
authentic default remains unknown; altering timer delay or suppressing
expiration would not resolve the empty receive window.

The own descriptor resolver `0x2d6b98` uses eight-byte records selected by
the descriptor's high byte. Group 6's directory entry at `0x2fcb40` points
to `0x2fc748`; record `0x2fc780` contains `(0x0607, 0x03f2, 1)`.
The live block read at 31.000221 s reads exactly one byte from EEPROM
`0x03f2` into `0x122238`, independently confirming this mapping.
A halfword-aligned Thumb PC-relative literal scan over the `0x170000`-byte
MCU extent finds two loads of literal `0x0607`: the live selector at
`0x2c68d8` and initialization at `0x2c6a76`. The latter reads a cached byte,
tests bit 7, and clears that cache if bit 7 is zero. This establishes another
policy interpretation, not a default value or exhaustive reader/writer census:
computed IDs and table-driven callers remain outside that scan. No storage
fixture is changed on this evidence alone.

The selector caller belongs to DSP receive dispatch at `0x2e25d8`.
It arms timer ID `0x1d` with delay `0x09cd`, receives queued notifications,
and drains the packet reader `0x2b6026` on control value 4. Each dispatched
packet increments halfword `0x12166e`. Status `0xdd` checks that count:
nonzero rearms the timer and clears the count; zero calls `0x2c68c8(1)`.
A fresh runtime observes `status=00dd packets=0000` at 31.000192 s,
immediately followed by the previously mapped reset selector. Thus erased
NV enables a reset policy whose live trigger is an empty DSP receive window;
it is not established that changing NV would repair the underlying behavior.
Native ring-index writes alone do not prove packets reach this dispatcher.
The next transport check is producer publication versus MCU draining and
task notification over this window, retaining the fitted-mask uncertainty.

The counter/dispatch trace confirms all four startup packets reach the MCU
dispatcher: counts 1, 2, 3 at 0.473940, 0.483414, 0.483657 s, then count 4
at 2.278505 s. At 6.533113 s, own MCU code `0x211318..0x211322` rearms
timer `0x1d` and explicitly clears `0x12166e`. No further dispatcher entry
appears before status DD at 31.000192 s. Thus the zero count describes the
new operational window, not failure to count or deliver the startup packets.
The fresh reset repeats the same startup publications and counter-clear path.
The next unresolved contract is what native DSP traffic the operational
window expects after that MCU transition; the observed startup transport
works. Do not substitute synthetic heartbeat packets or alter policy storage
without recovering that contract.

Per-second transport samples through 30 s show equal TX producer/consumer
and equal RX producer/consumer, with native work flags `0x0866` zero at
those samples. These samples exclude a persistent ring backlog, not brief
in-flight work. A native write tap on TX consumer `0x0853` observes the
resident dispatcher at PC `0x3909` consuming complete commands. After the
last startup RX packet, it consumes header `0x5051` at 3.649007, 6.482854,
6.533623 and 10.392084 s. No further consumer publication precedes reset.
The tap covers the declared `0x52`-word TX ring and remains below its
128-event cap. Consumption alone does not prove semantic handling of that
command family. The next native boundary is decoding these own-MCU commands
against the acquired resident DSP dispatcher, not repairing a proven backlog.

The four `0x5051` bodies are memory uploads, not evidenced reply requests:
each contains a destination word address followed by 39 data words.
Destinations are `0x2286`, `0x22ad`, `0x22d4`, `0x22fb`, forming a contiguous
156-word interval through `0x2321`. Passive readback on the following
one-second samples matches all 39 words in each fragment; all four fragments
also match after the software reset. This confirms native semantic handling
of these uploads beyond consumer-index advancement. It does not identify
the data's use or establish an expected packet response. The operational
silence must be investigated outside these successfully stored fragments.
`nse6_native_overlay_check.py --memory-uploads` requires the four contiguous
39-word readbacks after consumption; mutation tests reject missing fragments,
wrong spans, mismatches and reversed timing. This remains a data-handling
gate, not an operational radio or speech acceptance gate.

The native consumer also observes own type-`0x70` control bodies:
`1304 8547 df11`, `140c` plus six erased words, `1514` plus ten erased
words, `1618` plus twelve erased words, `0d00`, then `0a09`.
Their consumption times are 0.473622, 0.473762, 0.478238, 0.482854,
0.483120 and 2.278256 s. No further type-`0x70` consumer event appears
before reset in this fixture. This bounds the exercised control sequence;
it does not assign native command semantics from another MCU build or prove
that every command received a valid response.

Runtime dispatch distinguishes the native special controls from the generic
host-packet table: all six type-`0x70` packets execute CALL `0x4951` at
`0x3982`, with A=`0x70`, before their consumer publication. They do not
execute the generic BACC at `0x3995`. Thus generic table entry
`0xb00f + 0x70 = 0x037e` is not their observed handler.
Resident code `0x4951..0x4963` extracts the command byte, bounds it against
`0x18`, then branches through data-ROM table `0xb0a6`. Static entries for
the captured command bytes are `0x0a -> 0x49fc`, `0x0d -> 0x4a16`,
`0x13 -> 0x4ac7`, `0x14 -> 0x4b1f`, `0x15 -> 0x4b3e`,
`0x16 -> 0x4b73`. The runtime validation below establishes the exercised
consumers. The acquired table is compatible-reference evidence, not proof
of the fitted NSE-6 mask identity.

Native entry/return taps now validate all six candidate consumers in a fresh
40-second run. Each executes its expected entry and RET, without a logged
unsupported instruction. Command `0x13` changes identity scratch words
`0x1f0c/0x1f0d` from zero to `0x0016/0x0010`. Commands `0x14`, `0x15`,
`0x16` consume the erased records and leave flag word `0x1f11` zero at
their returns (`0x4b3d/0x4b72/0x4bac`); the scratch first words then read
`0xd1cc`, `0x1bef`, `0x3532`. Command `0x0d` reads result word `0x06f9`
(zero in this run) and reaches RET `0x4a25`; command `0x0a` reaches RET
`0x4a15` after the `0a09` input. The same handler sequence repeats after
reset. This excludes a stalled special-control dispatcher for these inputs.
Zero flags are an observation, not yet a proven error or permission to
invent valid identity/security records. Their downstream control meaning
and relationship to operational RX remain open.

A full-window passive read tap on `0x1f11`, excluding the observer's own
reads, sees only native PCs `0x4b3a`, `0x4b6f`, `0x4b9b` during the three
bootstrap read-modify-write operations (and the same three after reset).
No operational reader appears through 40 s. Static raw-word candidates
include `0x3de6`, which compares the word against `7` before continuing
from the type-`0x1b` path, plus indirect range handling near
`0x49a7/0x4a88`. These are dormant possibilities, not observed blockers:
no type-`0x1b` consumer is captured in this fixture. Changing the flag or
provisioning the erased records to satisfy that dormant gate would not be
an evidenced repair of the live receive-window reset.

The MCU status getter `0x2a1e3c` reads byte `0x11f535` after checking its
availability flag. In the fresh run, initialization sets it to zero;
`0x20b43c` publishes `5` at 6.081712 s. That writer decodes object
`0x10d608`, header `0x0067`, returned by `0x2bb768`. Its byte `+0x12=1`
selects value 5; fields `+0x18/+0x1c` are `0x030140b3/0x0a01b881`.
Thus value 5 is MCU-decoded object state, not a direct DSP hardware verdict.
The selector field's SIM provenance is established below; other object
fields retain no assigned semantics here.
At the later monitoring transition `0x211318` (6.533039 s), context
`0x11d040` has count `+8=1`, flags `+0x1b=0`, and the companion mode
byte is `0x12`; the status is already 5. This narrows the next MCU contract
to the source object and transition ownership without assigning meaning to
those numeric states from another ROM.

Own caller `0x2bb768` constructs a SIM STATUS command (`A0 F2`), queues it,
and waits for the result object. The live writer of object `+0x12` is
`0x298b28`: parser `0x298b1a..0x298b2a` extracts bit 7 from response-buffer
byte `+0x10`. A fresh trace sees response buffer `0x11bfc0`, characteristics
`0x81`, and selector value 1 at 6.081262 s. This is the directory status's
CHV1-disabled characteristic: the card model's `queue_fcp` supplies
`fcp[13] = 1 | 0x80` when CHV1 is disabled. With that normal synthetic card
state, the MCU status decoder takes its value-5 branch. This is not a
SIM failure, a DSP verdict, or evidence that CHV must be enabled to boot.
Changing the card security policy merely to escape the reset selector would
conceal the empty DSP window rather than resolve its traffic contract.

### Native Clock Observation Scope

The 40-second interface summary is reset-relative: `device_reset` clears its
frame/slot counters. Its roughly 1,737 frame expiries describe the final
eight seconds after the software reset, not a slow 40-second clock.
Fresh per-second snapshots observe 6,274/6,227 frame/slot expiries at 29 s,
6,491/6,444 at 30 s, and 6,708/6,661 at 31 s. CCONT frame-clock enable is
one at all three samples. The 216--217 expiries/s match the declared
13 MHz / 12 / 5000 timing; counters restart at the known software reset.
This excludes the apparent fivefold timer slowdown and a persistently
disabled frame-clock gate over those samples. Timer expiries alone do not
prove correct DSP ISR processing, physical clock validation, operational
packet production, or a valid RF sample stream. No clock setting is changed.

The shared passive `c54x_rom4_mode_observe.lua` now also records frame
entry/exit sites and is used by the NSE-6 observer. An isolated 30-second
pre-reset run observes 6,428 entries at `0x3204`, 6,428 bypasses at `0x32c3`,
and 6,428 common exits at `0x33f3`. Sites `0x3249` and `0x3268` each execute
51,424 times, exactly eight per serviced frame. Processing entry `0x3274`
and mode initializer `0x4414` (including its observed sub-sites) record zero;
final mode word `0x00ac` is zero. Thus INT0 and its idle receive-read loop
execute, not merely the external timer. This measured lifecycle never enters
the candidate processing tail. The next native contract is entry into an
operational mode, not repair of missing frame interrupts. These counts do
not prove a valid RF input or the fitted NSE-6 DSP identity.

The corresponding 30-second caller census records zero at the seven known
initializer call sites (`09a4`, `0ab8`, `4de3`, `4e00`, `9b62`, `9b9f`,
`9bb0`), control dispatcher `3660`, type-`1a` entries `3d5e/3d70`, and
type-`1b` entry `3db8`, with 6,428 frame entries as a positive control.
The 24-word executable overlay and all four 39-word memory uploads still
pass readback checks. Unlike the current native NSE-1 fixture, this lifecycle
never reaches the known mode-initialization entrances. This is a measured
cross-product control-sequence difference, not an exhaustive indirect-call
closure or evidence that a particular missing host command must be injected.

### Native NSE-1 Control Comparison

The same passive observer on fresh NSE-1 and NSE-6 fixtures gives this
bounded comparison (30 s, acquired ROM4 resident code in both):

| Contract | Native NSE-1 | NSE-6 compatibility fixture |
| --- | --- | --- |
| Search-list packet | `441a`, consumed 2.091369 s; handlers `3d5e/3d70` execute | No type-`1a` consumption observed |
| Mode-initializer callers | `4de3` at 2.104560 s, `4e00` at 2.109178 s, `0ab8` at 2.138677 s | All three dormant |
| Mode `00ac` | `0 -> 1 -> 2 -> 9 -> 1` | Zero |
| Scheduler branch `30bf` / call `30d6 -> 4dce` | Once each | Zero, despite 6,449 tests at `30a1` |
| First four memory-upload destinations | `2286`, `22ad`, `22d4`, `22fb` | Same |
| Compared 156 uploaded words | All match except word `22c9 = 0202` | `22c9 = 0000` |

The resident control entrance is now traced rather than inferred from timing.
Type-`1a` writes `06e3 = 1` at PC `3db6`, 2.091369 s, after setting bit 3
of `06bc`. Scheduler `309d..30a2` tests bit 0 of `06e3` and enters `30bf`
at 2.091377 s, clears the word at `30cb`, then calls `4dce` from `30d6`
at 2.100506 s. That routine reaches the observed mode-initializer callers.
NSE-6 runs the scheduler test but never publishes the enabling word or
enters that branch. Both fixtures hold `06be = 0010`, so that word is not
the observed cross-product distinction. No firmware state is forced.

One own-MCU publication chain is `0837 -> 0445 -> type-1a`. Dispatcher
`262056..26205c` selects `262bf4` for input `0837`; that branch loads `0445`
and enters the status constructor at `2a94f4` through shared tail `26225a`.
Receive loop `218540` routes `0445` to `218a42`, then calls `2184b6` at
`218a6a`. The latter allocates/clears 72 bytes, sets payload length `44`
and type `1a`, supplies control bytes, and queues the packet to task 3.
A halfword-aligned direct-BL scan over the own `170000`-byte MCU extent
finds that one builder caller; this does not exclude indirect callers.

In the fresh 30-second fixture the upstream dispatcher instead observes
`083b` at 3.614168 s; constructor `2a94f4` publishes `0448`, and the search
receive loop consumes it at 3.614318 s. The constructor later publishes
`03e9` at 6.528498 s. No `0445` construction or search-builder entry is
observed; constructor and receive taps have positive controls and remain
below their 64-event caps. The next own-MCU boundary is the producer and
lifecycle selection upstream of search, not a missing native initializer
implementation. Dispatcher `202cb0..202cd0` maps incoming `0a33` through
`206598` to `0837`, and `0a35` through `206584` to `083b`; both publish
through `2007f8`. The live `0a35` is constructed by `218c66`, caller return
`218c45`, at 3.613780 s, then selected by the dispatcher at 3.613971 s.
Own routine `218c24` frees a linked list, clears its stored pointer, and
unconditionally constructs that status. Thus this observed path is explicit
MCU cleanup, not a failed attempt to send the search packet. The literal
`0a33` constructor at `21a8e2` calls the same helper and then sets a context
byte to 2. Its branch at `21a870` requires receive-helper `218fb4` to return
`1587` at the later wait `21a864`. A distinct initial wait at `21a748` also
consumes `1587`, but proceeds through initialization rather than publishing
`0a33`. The fresh run observes the initial producer `2896fc` at 3.612416 s,
posting to mailbox `13`, and its initial consumption at `21a74c` at
3.612529 s. The helper subsequently receives `1592`, `158b`, and `043c`;
no return at the later wait or entry at `21a8dc` is observed. Thus `1587`
has a working producer and initial delivery; the unresolved condition is
entry into the later lifecycle, not inventing that status or reusing its
initial publication.

The controller's current wait is `21b07c`, with context `11f2f8` and mode
word `11f31e = 002d` after 6.533544 s. Before that it consumes `1592` from
wait caller `21d76d`, then `158b` from `21b7ab`. Wait `21cd2a` returns the
observed `043c`; its consumer at `21cd30` accepts `09f0` or `0434`, not
`043c`, and runtime enters common fallback `21b312` at 6.533520 s.
The main 66-entry mode table at `21b098` maps index `2d` back to `21cd30`.
Thus this is a live controller state with an unmatched completion status,
not evidence that the later search wait was entered. Own code `210fca`
contains a `0434` constructor but does not execute in this fixture. Its
entry and publication contract, and the distinction from `043c`, are the
next boundary; neither status should be substituted by the model.
The literal-seeded scan identifies candidates, not every computed publisher.

There is also an independently recovered type-`1a` builder at `211100..2112a2`:
it allocates a 72-byte object, builds the 68-byte search payload, and posts
it to mailbox 3. The fresh fixture reaches its post-return site `2112a6`
at 6.532938 s with object `10d758`, prefix `0002`, length `44`, type `1a`,
and control byte `11`. Thus MCU search construction is **not absent**;
the dormant `2184b6` path cannot close the complete publication surface.
The native consumer still records no type-`1a`, while neighboring type-`51`
uploads are consumed and read back. The next contract is mailbox-3 receive,
packet selection, and ring publication for this specific object, not forcing
`0a33/0837/0445` or modifying the DSP mode word. The monitoring timer is
armed just after this organic post. The separately mapped `0434` constructor
at `210f90` remains dormant and is not established as the source of `043c`.

The exact search object `10d758` reaches the TX mailbox handler at `2d354c`
at 6.532581 s, before the sender resumes from its post at 6.532938 s.
It retains prefix `0002` and type `1a`; this excludes loss before mailbox
delivery. TX task `2d36de..2d36fc` processes the oldest queued object,
requires free space >= payload length + 2, then calls writer `2b5f62`.
At search delivery the head is still upload `10e034`, length `50`, with
162 bytes free. It publishes that upload, then tests the next upload
`10e238` with only 80 bytes free, below its 82-byte requirement. The later
`e0` mailbox event at 10.390270 s publishes `10e238` and again leaves the
next upload (`10e2e4`) waiting with 80 bytes free. Earlier queue arrivals
show the same head-retention behavior. No search-object ring-space test or
writer call occurs during the 30-second fixture; all three TX taps stay
below their 128-event caps. Native consumption subsequently frees the ring,
but the remaining queued work is not reconsidered before reset. Next trace
the legitimate task wakeup/flow-control publication for freed ring space;
do not increase ring capacity, drop uploads, or manually drain this queue.
Identity/control payloads also
differ, with NSE-6's records erased; no NSE-1 data may be copied into NSE-6
to conceal these genuine input differences. The NSE-1 processing checker
passes on this capture; NSE-6's overlay/upload checks also pass.

The differing upload word is not an evidenced activation prerequisite:
native NSE-1 enters mode 1 at 2.104560 s, before publishing `22c9 = 0202`
at 2.180221 s (writer PC `3f4a`). NSE-6 publishes zero at the same address
at 6.482853 s. Passive data-read taps observe no reader of `22c9` in either
30-second run, excluding NSE-6's explicit upload-readback observer. This
excludes the differing word as the cause of NSE-1's first mode activation;
its later or dormant purpose is unassigned. Initial zero writes and upload
writes confirm the target mapping. The corrected 39-word comparison retains
the destination separately from payload indexing; `22c8` is `0800` in both.

`0x27a76e` takes a timer ID, not the delivered status: tracing only R0=D6 there
does not enumerate this timer's arm sites. A syntactic immediate scan finds ten D6 candidates in the MCU extent;
several are timer-control or unrelated payload uses, not proven publishers.
Runtime taps and literal scans do not exclude table/indirect delivery.
Do not disable a hardware watchdog to conceal this software-requested reset.
The native compatibility run exposes these independently documented CPU gaps.
Instruction semantics come from [TI SPRU172C](https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/81/6136.spru172c.pdf);
TRN's MMR `0x0f` and page-zero indirect addressing follow
[TI SPRU131G](https://www.ti.com/lit/ug/spru131g/spru131g.pdf).

| Encountered Word / PC | Implemented Contract | Executable Coverage |
| --- | --- | --- |
| `0387 / 21a9` | Unsigned-memory ADDS to B | SXM independence, A preservation, carry and one-cycle cost |
| `f4cf / 21b6` | TRAP vector transfer, no INTM/IFR changes | 32 vectors, both masking states, stack and three-cycle cost |
| `8dd2 / 1caf` | ST TRN plus explicit debugger/save state and MMR | 12 MMR/read/write, direct/circular/absolute store cases |
| `4ac0 / 0803` | Indirect PSHM through page-zero MMR address generator | 8 direct/indirect/post-decrement/circular cases with DP/CPL nonzero |
| `ae31 / 0816` | Parallel LD with MAC/MAS, optionally rounded | 64 destination/rounding/SXM/FRCT/pointer cases preserving old T and carry |
| `1780 / 1968` | LDR memory high plus bit 15, SXM-dependent, no status changes | 64 destination/sign/value/direct/indirect/post-decrement/absolute cases, OVM set |

### Native Upload Geometry

NSE-6 uploads executable words at `0x2946..0x295d` from DSP PC `31d7`
at 0.256053 s with PMST=`ffac` (OVLY enabled). The continuation helper
`24cf -> 266b` recognizes stacked return address `a53b`, restores ST0/AR2,
discards one return word with `FRAME +1`, and follows the uploaded branch
at `2678` to `2946` (resident ROM destination: `27aa`). The NSE-1-sized
overlay ending at `2800` discarded these program writes and executed zeros
at that target. This retained interrupt contexts, drove the stack into
executable entry `1ca4` at 0.926814 s, then into shared ring indices at
1.002107 s. Sampled ordinary CALL/RET and context-save paths balanced;
their semantics did not require a workaround.

The NSE-6 compatibility fixture now configures the existing overlay through
`2fff`, retaining the own upload without changing the default NSE-1 geometry.
This is an evidenced executable-bank requirement, not a fitted-silicon
capacity measurement or proof that NSE-1 ROM4 is the correct mask.
`tools/nse6_native_overlay_check.py` verifies all 24 observed upload words
against subsequent program/data readback, stack samples through 28 seconds,
and the observed receive-ring index range. The corrected 40-second run has
41 snapshots (SP=`1ec4` outside active handlers), eight valid producer
publications, and no logged unsupported instruction. Read-only index/header
taps remain bounded, not an exhaustive writer census.
Run the checker on the isolated menu fixture's log with
`python tools/nse6_native_overlay_check.py <run-directory>/error.log`.
The C54x conformance suite, 3210 default/frontier gates and native 5110 menu
gate pass with this product-only geometry change. The checker does not
establish security/menu presentation, late-reset stability or service parity.

MCU packet drain `2b6026..2b60ba` reads consumer `101ca`, producer `101c8`,
and headers at `10000 + 2*consumer`, wrapping indices `e4 -> 80`.
The verifier returned `0/4` at 0.230685 s; packet-drain activity is not a
verifier wait. Current remaining boundary is the software-requested reset
described above, followed by SIM/phonebook/service parity. No ring indices,
notifications, context state or watchdogs are forced to improve presentation.
CMPS/TRN update semantics and indirect POPM remain separate decoder gaps.
The TRAP-only composition's post-reset `0xfce7 / 06ce` differs from its cold
path; retained-state and instruction-stream provenance still need checking
before interpreting every encountered word as a missing instruction.
Phonebook persistence must be retested only after native boot stabilizes.

### Security Storage Observation

Own loader `0x28cc7e` reads three BCD bytes from EEPROM `0x0038`; invalid
digits select the literal `12345` at `0x28d09c`. Codec `0x2dd17c` uses its own
four-byte table `0x303704`, not the 3210 table. Startup derives the cached
code, and the physical submission reaches comparator `0x2dd398`.
The negative control observes a match (`r0=1`) at retry gate `0x2dd344`, but
RAM retry byte `0x1215b4` is `0xff` and the gate rejects counts >=5.
An eight-byte read from EEPROM `0x17f4` into `0x1215b0` establishes this byte's
storage offset `0x17f8`.

`nse6_integrity_fixture --config-integrity --initialized-retry` changes only
that retry byte in addition to the six integrity bytes. Identity and code
remain erased; firmware chooses and validates its own default code. The
positive run observes match one, retry count zero, and a Menu / Names frame
at 28 seconds after physical submission. This is synthetic initialization,
not recovery of authentic handset provisioning or bypass of a real lock.

Own block-read wrapper `0x2dd07e` receives EEPROM address, destination and
length in r0/r1/r2. The 30-second physical-code fixture records 638 calls,
below the observer's 4096-call cap. During submission it rereads eight bytes
at `0x000c` at 13.020477615 seconds, via identity formatter `0x28ced8` and
its caller return `0x28cef7`; no separate code-record read is observed then.
This does not prove absence of a cached verifier or another storage path.
The formatter has three syntactic direct Thumb BL callers in the MCU extent;
it is not a security-only function.

The descriptor reader `0x2d6f28` resolves records before calling that block
wrapper. Its bounded 6–7-second trace maps selector `0x0707` to eight bytes
at `0x1884` (caller return `0x295cf5`), `0x0706` to one byte at `0x188c`
(`0x295acd`), and `0x0705` to five eight-byte records at `0x185c..0x187c`
(`0x295aeb`). These are address/ownership observations only: none is yet
classified as the phone-lock verifier. Do not import the 3210's `0x06c8`
security-state format or provision these records without decoding their own
consumers.

### Report-0x14 Producer Boundary

The full-scale-input observations below describe the negative control, not
the current nominal-VBAT result. With ADC2 `554`, ten successful acquisitions
decrement the remaining count from ten to one before the report path runs;
the report bitmap becomes `0x0f`. Physical input still reports pending IRQ
`0x00` with mask `0xca` and column mask `0x20`, so report completion alone
does not establish an interactive handset.
Two independent fresh integrity-only boots reproduce all ten sample values
and the report-path timestamps exactly. The input-edge fixture also observes
column values change with physical contacts. A byte-lane-filtered MMIO watch
observes firmware acknowledge IRQ0 (`0x20009 = 1`) for all six edges at
`0x2de2dc`. A zero pending sample after `set_value` therefore does not prove
missing delivery. With the inherited no-cause-register profile, the
scanner/decoder counters remained one/zero at 28 seconds. The own IRQ handler
`0x2de292` reads `0x20034`, tests its low five bits, and calls `0x2e0400`
before acknowledging IRQ0. Exposing the existing device-owned pending-column
bits at this register restores ordinary scan/decode without any firmware
publication or RAM override. `KEYPAD_NSE6` retains the independently decoded
row/direction/input/mask offsets and adds this status register; other products'
contracts are unchanged.

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
registration or native speech. The nominal ADC and keypad status contracts
below resolve this negative control's startup/input prerequisites; the current
native boundary is the port-`0x36` completion in Current Result. Additional storage changes require their
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
With the former full-scale ADC2 input, three fresh two-integrity runs remain
blank at 28 seconds. The scanner runs
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
Thus the missing report bit is a concrete unmet prerequisite in the
full-scale ADC negative control, not the current nominal-input frontier.
The Report-0x14 Producer Boundary and Source Mapping sections recover its
own producer and input conditions. No external startup report is to be synthesized merely
because a sibling profile supplies similarly numbered reports.

Reproduce with the isolated-run command above, substituting `nse6r4t` and a
fresh run directory. Its ROM subdirectory additionally requires the hash-pinned
`nse1_rom4_dsp_program.bin` and `nse1_rom4_dsp_data.bin` declared in the driver.
A successful handshake does not justify importing a donor EEPROM or claiming
native radio/speech. Captured results and the two integrity failures are above;
the current native port-`0x36` boundary is distinct from the resolved
nominal-input report and keypad-status contracts.

The shared memory handshake is not permission to fabricate its result. Memory capacities,
reset/peripheral attachment, static negative fixtures and the first isolated
boot are established above; persistent-storage dependencies remain unresolved.

Promote graphical/input/SIM/service capabilities only with their own runtime
evidence. A historical repair EEPROM, if examined, remains a repair template,
not an authentic handset dump or independently justified provisioning.
