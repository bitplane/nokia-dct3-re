# Nokia 6190 / NSB-3 admission

## Current boundary

`noki6190` is a conservative MCU instrument ending at `CONTACT SERVICE`.
Separate `nsb3r4t` executes with explicit unproved NSE-1 ROM4 compatibility
inputs. With one nominal ADC2 hypothesis and six integrity-only corrections
to its own firmware-created EEPROM, it reproducibly reaches `Insert SIM card`.
An explicit laboratory card and separately initialized phone-code retry record
then support physical security entry, a clean Messages menu, and SIM contact
save/cold Details retrieval. No donor identity/calibration is imported.
Ordinary idle-content provenance remains unresolved. Fitted-mask compatibility,
RF registration, calls and SMS delivery remain unqualified; native liveness is not
evidence for those contracts.

Collection provenance and the unprovided MCU-to-PPM gap are in
`roms/README.md`. Reproduce the static report:

```sh
.venv/bin/python tools/nsb3_static_check.py roms/noki6190/v613/nsb3___6.130.bin
```

The normalized MCU SHA256 is
`4ae57f0eed01125efbe0f4e63abf5412b44539069b424f5d6dcf030659a8ebda`.
Addresses below refer to this own image in big-endian CPU order.

## Startup and peripheral contract

ARM startup begins at `200040`; the ADD/BX pair at `2000e4` enters Thumb
at `2000ec`. The own startup stack literal is `10fbf4`.

| Boundary | Own instruction/table evidence | Recovered contract |
| --- | --- | --- |
| EEPROM | `2b057e`, `2b064a`, `2b06bc` | SDA bit 0 and SCL bit 2 at `20020`; direction at `20024` |
| CCONT serial | `2b129e`, pool `2b1374` | Select `28=22`, byte write `2a`, status `29` bit 2, receive `2d` |
| LCD serial | `2b19e4`, `2b1a4a` | Status `29` bit 0; command `2c`, data `2b` |
| Keypad scanner | Entry `2b2a2c`, fragment `2b2a60`, pool `2b2bc0` | Five-row scan, row signal `31`, direction `2f`, columns `30`; raw `row*5+column` |
| SIMI | `2a19c0`; observed TX `20036` / RX `20037` | ATR/PPS, startup file reads and organic ADN update/readback with laboratory card |

Offsets in the table are relative to MAD2 `20000`. Header descriptor `f4`
decodes to 8 KiB, two-byte addressing and 32-byte pages; this is not a fitted
part identification. No calibration values, serial timings or fitted LCD
geometry follow from these instruction fragments.

Decoder `2b359e` calls the scanner and translates layout zero through normal
table `2d69f8`; selector RAM is `10c482`. The own normal table is:

```text
3e 3e 3e 3e 3e
11 19 01 02 03
0e 17 04 05 06
0f 18 07 08 09
10 1a 0c 0a 0b
```

Special table `2d6a14` is `3e 3e 3e 3e 0d`, selecting raw `84` / column
bit 4 as logical Power `0d`. Matching sibling logical tables do not prove
shared physical side-key labels, IRQ delivery or interactive UI acceptance.

## Dual DSP catalogues

Two `70`-byte initializers each provide 28 pointer slots, with null slots
11/26/27 and 25 bounded nonzero descriptors. The checker uses the already
tested shared catalogue reader, not a guessed descriptor extent.

| Pointer table | Initialized RAM | Slot-25 bootstrap | Identity written to DSP data `0803` | Payload SHA1 |
| --- | --- | --- | --- | --- |
| `2b67d0` | `10ad9c` | `2bcf08` | 3 | `73b0cc6e6fc602f3a2f7934eb0c35d7f31c96b3f` |
| `2b6848` | `10ad2c` | `2c2100` | 4 | `e1e8f15a305bf32aea98f444074f30828d933fc4` |

Both bootstrap headers are `0f00 0000 00ba 0700 00b4 0000` (186 words).
The identities are explicit `76f8 0803 0003/0004` instructions, not inferred
from a product label. The installer names ROM4 files, but the runtime selector
is statically recovered: initial consumer `2946a0` reads context word
`10ad0a` and uses catalogue RAM `10ad9c` only for value 3, otherwise
`10ad2c`. Indexed consumer `294acc` applies the same test. Both initial cold
runs observe value 4. This is runtime branch selection, not fitted-mask
compatibility. Declared
payload ranges are not proof of execution, relocation or complete mask content.

## Initial erased-storage instrument

The own installer INI says `EraseDefinedArea=Yes`, `LowEraseLimit=200000`,
`HighEraseLimit=2FFFFF`. Therefore the image builder fills the unsupplied
`2d6d00..2e0000` gap with erased bytes, not donor content. Reproduce into a
fresh file:

```sh
.venv/bin/python tools/nsb3_archive.py roms/archive-dct3-packages/nsb3_613.exe --image roms/noki6190/6190f613a.fls
```

The 1 MiB image SHA256 is
`927f94aed4ece8e84ad2eda28614bcede1272e89a2d046b16192f88499da19c2`.
The machine uses capacity-compatible Intel 28F800B3T and 24C64 devices;
fitted identities are not proved. It shares only hardware construction with
the 5190 profile, then applies its own independently checked contracts.
Its minimum RAM window through `10ffff` covers stack `10fbf4`; upper aliases
remain unmapped. Host logical key positions match the own table, not a claim
that all 5210 physical legends apply to the 6190. Boot and DSP masks are
`NO_DUMP`, boot transfer remains HLE, SIM socket is empty, and no DSP or
application peer acknowledges firmware work.

After `make build`, use a fresh isolated directory:

```sh
.venv/bin/python tools/run_mame_isolated.py --mame-dir mame --run-dir tmp/nsb3-new -- noki6190 -rompath /data/gaz/src/nokia-dct3-re/roms -noreadconfig -video none -sound none -nothrottle -log -seconds_to_run 6 -autoboot_delay 0 -autoboot_script /data/gaz/src/nokia-dct3-re/tools/nsb3_boot_observe.lua
.venv/bin/python tools/nsb3_boot_check.py roms/noki6190/6190f613a.fls tmp/nsb3-new
```

Samples at seconds 1..5 show PC `2a8ac8/2a8acc`, LR `2a8a89`, selector
`0004`, catalogue first pointers `2b7168/2bd0a8`, DSP control `02`, shared
words `10000=0000`, `10002=ffff`. This is the idle-loop latch test at
`2a8ac6..2a8acc`, not by itself a DSP ownership wait or proof of completed
boot. Firmware changes 68 EEPROM bytes from erased state; the resulting
8 KiB SHA256 is `4dc2b8ea96b930813282e930ebb9cddfddb916809c8f17d972540d59dd004917`.
Complete flash remains byte-identical to the own installer-derived image.
`tmp/nsb3-erased-first` and `tmp/nsb3-erased-repeat` agree on full EEPROM,
flash and card files. The next trace must distinguish pending hardware work
from a normally idle task; do not synthesize messages from a sampled PC alone.

## Passive graphical and upload boundary

`tools/nsb3_startup_observe.lua` adds bounded read-only debugger samples,
an installed-program dump and a five-second screenshot. Use the same fresh
isolated command above with `-debug -debugger none` and this autoboot script,
then check with:

```sh
.venv/bin/python tools/nsb3_boot_check.py roms/noki6190/6190f613a.fls tmp/nsb3-startup-qualified --startup
```

The 372-byte shared-window dump exactly matches the own catalogue-4 bootstrap
at `2c210c`, SHA1 `e1e8f15a305bf32aea98f444074f30828d933fc4`. This proves
installation, not execution by a DSP. The reviewed 84x48 screenshot says
`CONTACT SERVICE`; its RGB pixel SHA256 is
`ed09afeeadae383b77840654453b183a05e10b2cad5ea5757e8c803cbd6b674b`.
Own flash and complete EEPROM remain equal to the cold-run controls.

The sampled idle permission routine `28dff4` returns zero. Its complete
short-circuit chain tests byte `10be1a`, predicate `27562c`, then byte
`1080ef`; it returns one only if all pass. Zero is not a direct DSP wait.
At five seconds the idle latch is `01`, companion `00`, global control `05`,
FIQ/IRQ pending both zero, masks `e3/de`. Early catalogue breakpoints did not
fire in this autoboot observation window, so they do not establish absence
of earlier activity; the exact installed payload is the stronger evidence.

## Explicit native ROM4 compatibility input

`nsb3r4t` replaces only the DSP HLE backend with the acquired NSE-1 ROM4
program/data inputs at a declared 52 MHz research cadence. It retains own
NSB-3 executable, erased EEPROM, full-scale default ADC2 and empty SIM socket.
This is not a relabelled NSB-3 mask, verified clock or native RF/voice claim.
The ordinary `noki6190` remains unchanged. Mask filenames and hashes retain
their NSE-1 provenance in the ROM declarations and `roms/README.md`.

The first run exposed a real compatibility-configuration defect: the default
program overlay ended at `2800`, while own catalogue-4 entry 1 (descriptor
`2bd53c`, payload `2bd548`, destination `25b4`) installs through `2b2a`.
A passive program write tap observes exactly 811 writes at `2800..2b2a`.
Before widening the overlay, 800 of those words read as resident ROM instead
of their uploaded values. This profile now admits overlay through `2fff`;
the extent is a research geometry, not physical SRAM identification.
All 811 words then match own source bytes beginning at `2bd9e0` exactly.

`tools/nsb3_native_upload_observe.lua` captures that upload window during a
fresh two-second `nsb3r4t` run. A separate fresh six-second run with the
startup observer captures native state and the panel. Check both with:

```sh
.venv/bin/python tools/nsb3_native_check.py roms/noki6190/6190f613a.fls tmp/nsb3-native-upload-fixed tmp/nsb3-native-fixed-graphical
```

Optional `--negative-upload tmp/nsb3-native-upload/nsb3_program_2800.bin`
checks the retained pre-fix 800-word mismatch, not merely a passing source
convention. The accepted run observes DSP PC `408d`, `ILLEGAL=0`, `IDLE=1`
at five seconds and a terminal interface summary with 1313 completion strobes,
76 mailbox writes, 1272 slot expiries and 1296 frame expiries. These are
bounded execution/liveness observations, not RF acquisition or speech proof.

The five-second frame is entirely white (RGB SHA256
`5fdab89cdada4b6ac821d32facb589dfb541e9856fe3b569e8da49646fc3d249`),
not standby. Own flash remains unchanged; the independently initialized
8 KiB EEPROM SHA256 is
`53584321c1f9e317e59cf67401fb5a28aa84fc4db32c67d264731825aa194d19`,
33 bytes different from the silent-peer cold control. No edited provision or
identity is imported. Next decode own acquisition/self-test gates under the
full-scale ADC input; do not infer an analogue fix from another product's
successful nominal-value experiment.

## Own acquisition rejection and nominal comparison

`acquisition_contract` pins the complete helper `27a3e0..27a4b4`, including
both conditional sample paths and signed range comparisons. Reader `2b1eee`
uses own channel table `2d6a1c` (`00 04 05 06 07 03 01 02 02 01`);
source 7 selects ADC2 through the own serial ADC routine `2b5244`.
Gain/offset are RAM `10fe54/10fe58`; both transformed samples must be in
inclusive `1800..5500`. At identity gain and zero offset, the transform is
integer `raw * 1500 / 229`. Accumulator `27a4b4` decrements remaining byte
`10a0e2` only after a successful helper return.

The full-scale native control records 16 failed attempts at return site
`27a4c2`, each with both samples `1a2c` (6700), gain `3f800000`, offset
zero and remaining `0a`. This independently explains the uncompleted
acquisition under raw ADC2 `3ff`; it does not assign physical units by itself.

The [original 6190 owner manual](https://www.instructionsmanuals.com/sites/default/files/2019-05/Nokia-6190-en.pdf),
Technical Information, specifies nominal operating voltage 3.6 V. The retained
PDF is `roms/reference-docs/nsb3/nokia-6190-owner-manual.pdf`, SHA256
`a0a5a8e039bc3cb6548553b4bcf546aee2ef685ee51a8f571b65a1e46c3ce144`.
Combined with the previously documented typical CCONT VBAT transfer of
6.5 mV/bit, this supplies one external raw-input hypothesis, `round(3600/6.5)=554`.
It is not measured NSB-3 board calibration or a value chosen by sweeping.

Only the research machine exposes the existing two-choice ADC2 input. Its
default remains full-scale. Prepare a fresh config-only comparison:

```sh
.venv/bin/python tools/nsb3_nominal_input.py roms/noki6190/6190f613a.fls tmp/nsb3-acquisition-nominal-new
```

Run the prepared directory using the isolated `nsb3r4t` command with
`-debug -debugger none`, six seconds and `tools/nsb3_acquisition_observe.lua`.
An independently fresh full-scale run with the same observer supplies the
control. Compare them with:

```sh
.venv/bin/python tools/nsb3_acquisition_check.py roms/noki6190/6190f613a.fls tmp/nsb3-acquisition-fullscale tmp/nsb3-acquisition-nominal
```

The nominal run logs external raw `22a`, ten successful attempts with both
samples `0e2c` (3628), and remaining values `0a..01` before each decrement.
The entire EEPROM remains equal to the full-scale native control, and both
flash images remain unchanged. The five-second panel stays white. Thus this
acquisition rejection is removed without resolving presentation; no report,
counter, timer, identity or calibration content was injected. The remaining
self-test/provisioning and application lifecycle must be investigated separately.

## EEPROM integrity contract and empty-socket presentation

The complete decoded checksum helpers are pinned independently of the
fixture generator. Helper `270aa4` reads `0000..011f`, sums bytes
`0000..011b` with the 16-bit byte accumulator at `2a5b14`, and compares
that zero-extended result with the big-endian 32-bit word at `011c..011f`.
Helper `23ce68` sums `0120..01fd` while excluding `0154..0155`, then
compares against the big-endian halfword at `01fe..01ff` through `23d0c0`.
These are integrity formulas, not authentication or valid calibration claims.

Own native cold storage has stored calibration checksum `ffff` but computed
value `db24`. The failed comparison clears self-test flag `10fde9` bit `40`
at `23d0dc`. A two-byte control repairing only `1fe/1ff` passes that exact
comparison but still fails result-array slot `12`, code `12` at `10fcb2`.
The slot writer calls `29967a`, whose `270aa4` security-block integrity
predicate fails. Aggregate scanner `23d12c..23d150` then independently clears
bit `40` at `23d144`. This negative control prevents a passing calibration
sum from being mistaken for complete self-test success.

The combined control repairs only `11c..11f` and `1fe..1ff` in the qualified
own cold EEPROM. It preserves every identity, security, calibration and
settings content byte. The tool refuses other source hashes or reuse of an
output directory, and records the complete seeded EEPROM and changed offsets.
It is an explicit external integrity fixture, not a factory EEPROM image.

```sh
.venv/bin/python tools/nsb3_nominal_input.py roms/noki6190/6190f613a.fls tmp/nsb3-all-checksums-new --checksum-control-from tmp/nsb3-selftest-nominal --all-decoded-checksums
```

Run the prepared directory for 15 seconds with `nsb3r4t`,
`-debug -debugger none` and `tools/nsb3_selftest_observe.lua`. A separate
12-second preparation without `--all-decoded-checksums` is the calibration-only
negative control. The full acceptance command is:

```sh
.venv/bin/python tools/nsb3_selftest_check.py roms/noki6190/6190f613a.fls tmp/nsb3-selftest-nominal tmp/nsb3-calibration-control-final tmp/nsb3-all-checksums-repeat
```

Both controls pass the calibration comparison; only the combined one has no
aggregate error and retains final self-test bit `40`. Two independently seeded
combined runs reach the reviewed `Insert SIM card` frame (84x48 RGB SHA256
`a879c658a532b6526d3343287b42848f8a16bf10b7215086c1e215db45ee67cc`).
Initial and final complete EEPROM are identical, SHA256
`18022fd2313fc9d5a904f2a9c5ef26f498d059d9d8e7326273986c8fe360e92d`;
the own flash also stays unchanged. The tray remains empty; no SIM protocol,
menu interactivity, registration or native speech is promoted by this frame.

### Inserted card and phone-code retry contract

The research configuration can attach a fresh laboratory SIM through the
existing SIMI controller; its default socket remains empty. The explicit
`--insert-sim` fixture reaches `Security code`. This is a phone-code prompt,
not evidence of a SIM PIN request. Wire observation must read TX at `20036`
and RX at `20037`; `2003b` is not TX.

Physical `12345` and the left softkey reach validator `2af450` with `r0=1`
(credential matches), but retry byte `10c444=ff` triggers its `>=5` rejection.
Own descriptor `2d03a8` declares record `0709`, EEPROM offset `04ac`, length
one. The accepted-code tail `2af48c..2af4a2` resets this count to zero and
returns one. Record `0708` holds the separate four bytes at `04a8`; they
must not be replaced to bypass the retry guard.

`--initialize-phone-retry` is a separate external provisioning experiment,
requiring the qualified own checksum-control source. It changes only `04ac`
in addition to the six documented integrity bytes, preserves the stored
credential, and pins both the own descriptor and reset-tail instructions.
It is not an integrity-only repair or an authentic factory dump.
With the same physical entry, observation reports `match=1 retry=00`;
a separate cold run reaches the matched branch at `2af48c` with retry zero.
The subsequent idle-content display remains visually odd through 30.4 seconds,
not a qualified ordinary idle frame. However, a physical left-softkey press
then opens a clean `Messages` menu with Select/Exit softkeys. Its 84x48 RGB
SHA256 is `52f6ae5577f82efd58e386e4606c4ce713a16b2c67b243ba8d0a708a0dd117be`.
This establishes interactive menu presentation after matching security entry;
it falsifies treating the odd idle frame alone as a frozen UI or broken LCD.
Determine the source of that idle content separately. Neither menu presentation
nor native DSP liveness proves registration, calls, SMS, persistent contacts
or a fitted DSP-mask identity.

Reproduce with the three fixture switches `--all-decoded-checksums
--insert-sim --initialize-phone-retry`, the same qualified source run, and
`tools/nsb3_phonecode_input.lua` for 36 emulated seconds. All input is physical
matrix contact state; all breakpoint and LCD wire observations are read-only.
Check the resulting directory with `tools/nsb3_menu_check.py RUN_DIR`.
The checker requires the seven-byte external fixture, physical credential and
menu contacts, the matched-code branch, and the reviewed menu pixel hash.

### Physical SIM phonebook persistence

After matching security entry, Menu is exited physically; the idle right
softkey opens Names. Add entry accepts `A` and `12345`, reports `Saved`, and
issues exactly one absolute `A0 DC 01 04 20` UPDATE RECORD to selected EF_ADN
`6f3a`. The stored 32-byte record has name `41`, number trailer
`04 81 21 43 f5` at byte 18, and erased padding. The other 49 ADN records stay
erased. This is a card-side protocol write, not a host-edited contact.

`tools/nsb3_phonebook_input.lua` exercises the physical save in 68 emulated
seconds using the same seven-byte provisioning fixture. Prepare an independent
cold process with:

```sh
.venv/bin/python tools/nsb3_phonebook_check.py prepare-reload SAVE_RUN COLD_RUN
```

Only that handset's persisted flash, EEPROM and SIM are copied, plus its
input configuration; executable hash and source storage provenance are checked.
No guest RAM or save state is transferred. Run the cold directory for 50
seconds with `tools/nsb3_phonebook_reload.lua`. It physically enters the same
phone code, opens Names, submits an empty Search and opens Details. Reviewed
frames show `A`, then `Number: 12345`. The cold process issues READ RECORD for
ADN record one, no UPDATE RECORD, and leaves the complete SIM file unchanged.

```sh
.venv/bin/python tools/nsb3_phonebook_check.py check SAVE_RUN COLD_RUN
```

The checker decodes the complete observed PPS/APDU TX stream with the shared
SIM parser, requires exact physical contact ordering and the matched-code
branch, checks Saved/name/number pixel oracles, and compares whole-card storage.
This qualifies laboratory SIM contact persistence, not handset-memory contacts,
all SIM security services, ordinary idle content or radio completion.

## Native radio-boundary inventory

Own sender `29421a` forms a header from the supplied byte length and first
message byte. Writer `29419e` packs subsequent payload bytes high-byte first,
wraps the 82-word MCU-to-DSP ring before `100a4`, and publishes producer last
at `2941fe`. Space reader `294374` compares producer `100a4` with consumer
`100a6`, reserving one word. The sole direct BL to `29421a` in the qualified
MCU extent is `2a928a`, after its queue/space checks; this is not proof that
all higher-level message constructors have been enumerated.

Receiver `294268` extracts header length/type and allocates a firmware object;
`2942fe..29435a` copies its body and publishes the consumer. Own literal pool
`29457c..294587` independently names consumer `101ca`, producer `101c8`, and
ring start `10100`. Receive indices span `80..e3` relative to shared base
`10000`. These routines and literals are pinned by `tools/nsb3_radio_check.py`.

With the same fresh seven-byte/card/security fixture, passive observation
records 32 MCU-to-DSP packets and 15 native DSP-to-MCU packets through 65 seconds:

| Direction | Qualified bounded inventory |
| --- | --- |
| MCU to DSP | Two `05`; sixteen `70`; one `3c`; four `0d`; seven `51`; one `1a`; one `4a` |
| DSP to MCU | Fourteen `74` packets, including `0d00`, `0a090004`, and repeated 52-byte bodies beginning `3532`; then one `8b` of 162 bytes |

The final MCU packets are type `1a`, length 68, body beginning `00419800`
with the remainder zero; then type `4a`, length four, body `02000000`.
Their exact NSB-3 producer/consumer meanings are not yet qualified. Do not
import another product's request/reply interpretation or fabricate an ack.
All observed MCU packets are consumed. At 8, 20 and 35 seconds, TX
producer/consumer are both `002d`, RX producer/consumer both `008b`, native
PC is `09b9`, and illegal-instruction count is zero. No subsequent ring
activity occurs in that shorter window. It is insufficient to diagnose a stall:
the existing NSB-1 contract suggested extending the capture, and NSB-3 emits
type `8b` at 41.035 seconds, beginning `00400328ff6f0329ff6f`. At 45 and 60
seconds the RX pointers are both `00dd`, native execution remains legal, and
TX remains drained at `002d`. This disproves a silent DSP in this fixture,
not missing RF inputs or a compatible fitted-mask identity. Zero/default RF
input cannot establish real reception or a usable network from that vector.

Own receiver `2b3a7a..2b3a96` subtracts `80`, then `3`, before indexing the
complete 13-entry `83..8f` dispatch table at `2b3a98`. Type `8b` is index eight,
entry `2b3ab8`, selecting `2b3ae4 -> 28a5a2`. That wrapper replaces the status
with own literal `13b8` at `28a690` and posts to task `0c` via `26c8cc`.
The runtime observer checks entry to this own forwarder. Its task-12 consumer
and candidate-acceptance lifecycle, not a guessed DSP ack, are the next boundary.

Run `tools/nsb3_radio_observe.lua` on the fresh fixture for 65 seconds, then:

```sh
.venv/bin/python tools/nsb3_radio_check.py roms/noki6190/6190f613a.fls RADIO_RUN
```

The checker validates complete packet bytes, extent/wrap arithmetic, sequential
indices, bounded capture counts, own sender/receiver code and settled native
state. It also requires the physical matching-code/menu sequence. Decode the
NSB-3 constructors/native dispatch for `1a`/`4a` alongside task-12 consumption
of the late `8b`; this inventory alone is not registration,
a missing-response diagnosis, or permission to add an HLE radio reply.

### Task-12 result retention

The same native receive object is consumed at `219c7e`, whose task loop is
`219c70 -> 26cb20`. Its `13b8` subtract-cascade selects table index `0e`
at `219d3c`, target `219d82`. Runtime observes state `106fcc=3`, phase
`106fd1=1` and no previous cache. The branch allocates an `a8`-byte object
and copies 160 bytes from message `+6` to cache `+6` at `219dbe..219dc2`.
The captured bytes equal the native `8b` body after its first two bytes,
byte for byte. At `219dee`, state 3 takes `219df4 -> 21a158`, bypassing
further candidate processing. This is a retained result, not proof of an
empty vector, absent delivery or successful acquisition.

The read-only state watch sees only zeroing at `20010e` and own task
initialization to 3 at `219c3e` (1.184 seconds), retained through the 65-second
run. Owner `106fb8` contains state at `+14` and phase at `+19`; task-local
owner `106b58` holds the cache pointer at `+28`. Captured heap addresses
`102c40`/`102e80` identify this fixture's objects, not fixed ABI locations.
The diagnostic object read tap must be revalidated against the forwarder
address if allocation changes.

Own initializer `218bb0` sets state 1 at `218bf6` and retains its argument
at phase-owner `+15`. A two-byte-aligned Thumb BL scan across the `d6d00`-byte
MCU extent finds direct callers `218d50` and `21a13e`; this does not close
indirect calls, tail branches or computed aliases. The latter is the own
task-12 `043f` branch `21a10c`, passing message byte `+2`. These are routes
to classify, not statuses to inject. No ordinary `043f` producer is proved;
do not change state 3 or invent a DSP acknowledgment to use the cache.

Run `tools/nsb3_radio_consumer_observe.lua` for 65 seconds on the same fresh
fixture, then extend the native checker with `--consumer`. It requires the
same forwarded/consumed object, observed state initialization and retention,
cache allocation, and exact captured-vector equality in addition to the
complete native ring and physical security/menu controls.

### Activation caller coverage

Constructor `2937c4` has a `043f` case at `29386e`, copying bytes `+5/+4`
from own control record `10c3d8` into message `+2/+3` before posting to task
12. This does not establish that the case has an ordinary producer.
The two-byte-aligned direct-Thumb-BL scan over the complete `d6d00`-byte MCU
extent finds 38 constructor candidates; the shared constant-recovery census
decodes all 38 and recovers all 38 inputs, with no undecoded candidates.
None supplies `043f`. Caller `257b82` supplies `0440`, which selects a different
task-12 branch; it must not be relabeled as the initializer input. The NSB-1
`043f` producer cannot therefore be imported as an NSB-3 fact.

The alternative task-12 input `13a3` selects `219f94 -> 218cac`. Its `218d50`
path can call initializer `218bb0` using the retained argument byte. Own
publisher `211174` constructs `13a3`, copies byte `108947` to message `+2`,
and posts to task 12. Its three direct BL callers are `2112f2`, `213b64`,
and `213c02`. These bound the alternative `13a3` publication surface;
the observed asynchronous predecessor is described below.

With native ring/cache controls passing, counters sampled at 64 seconds
observe zero `043f/0440` constructor entrances, zero initializer entrances,
zero `13a3` consumer entrances and zero `13a3` publisher entrances. The
debugger counters use `temp5..8`; parent startup/acquisition observers reserve
`temp0..4`. This scoped absence is not a whole-ROM proof: indirect callers,
tail branches, interpreted tables and later or other physical lifecycles
remain open. Do not inject either status or initialize the controller byte.

```sh
.venv/bin/python tools/nsb3_activation_census.py roms/noki6190/6190f613a.fls --run ACTIVATION_RUN
```

Prepare the same fresh external fixture, then run
`tools/nsb3_radio_activation_observe.lua` for 65 seconds. The optional run
check requires the existing physical/security/native-ring/cache controls as
well as the bounded activation counts; an unqualified null trace is rejected.

The live predecessor is not absent: task 11's own handler `213950` receives
`03e90100` from caller `214930`. Argument 1 takes the shared tail
`213f24..213f3c`, which calls `212658(2)`. Runtime then observes publication
at `21289c`: class `0002`, length `44`, type `1a`, body prefix `00419800`.
The native-ring capture carries the same request. Constructor
`2126f2..21270c` and publication `212880..21289c` are pinned. This is a working
asynchronous search initiation, not an unstarted MCU task or proof that its
completion should be synthesized as `043f`/`13a3`.

A distinct data-bearing ingress exists in this own image: receive type `80`
branches through `2b3b24 -> 28a09c` (one direct Thumb-BL candidate). A gated
path at `28a2a6` stores own status `040b` from literal `28a650`, copies a
discriminator to message `+b`, and posts to task `0e` at `28a2d6`. No type-80
packet occurs in the qualified native capture. This is separate from the
delivered `8b -> 13b8 -> task12` result; neither that cached vector nor the
existence of a type-80 decoder proves received radio data. Its pre-publication
gates and downstream completion still require qualification. Native RF input
and operation prerequisites are shared research boundaries documented in
`djr_dsp_integration.md`; a guessed type-80 payload would bypass them, not
validate them. The next radio question is the own asynchronous completion/data
path and its native receive prerequisites, not another startup-state poke.
The current machine composition does not bind COBBA's `rf_receive_cb`;
`nokia_cobba_device` defaults it to zero and owns the deterministic-silence
policy. Thus no software RF environment is currently supplied to this native
fixture. A received-block completion requires an evidenced RF/sample and
activation contract, not fabricated MCU state. This boundary does not prevent
independent SIM, storage and offline application work.

## Offline SMS draft lifecycle

The same fresh own-EEPROM/card/retry fixture accepts physical text composition
and displays `A` with 159 characters remaining. Save emits exactly one
`A0 DC 01 04 B0` UPDATE RECORD to EF_SMS `6f3c`, record one, then presents
`Message saved`. The 176-byte unsent record begins
`07 02 81 f0 01 ff 00 81 00 00 01 41`, followed by erased padding; the shared
`sim_sms_draft.py` contract checks this byte-exactly and the other nine free
records. ADN remains erased. No RF connection or submission is involved.

A separate cold process physically authenticates, opens Outbox and reads `A`.
It issues READ RECORD, no UPDATE RECORD, and preserves the whole SIM file.
Physical Options/Erase and confirmation in a third process change only the
saved record's status byte at serialized-card offset `664`, from `07` to `00`.
A fourth cold process displays `No messages`, issues no UPDATE RECORD, and
preserves that complete deleted-card image. Stale record payload bytes remain
behind the free status; deleting is not modeled as blanking the whole record.

Use `tools/nsb3_sms_input.lua` for 66 seconds on the fresh fixture. Prepare
the cold read and deletion directories from that save with:

```sh
.venv/bin/python tools/nsb3_sms_check.py prepare-reload SAVE_RUN COLD_READ
.venv/bin/python tools/nsb3_sms_check.py prepare-reload SAVE_RUN DELETE_RUN
```

Run `tools/nsb3_sms_reload.lua` for 50 seconds and
`tools/nsb3_sms_delete.lua` for 60 seconds respectively. Then prepare and run
the final empty check:

```sh
.venv/bin/python tools/nsb3_sms_check.py prepare-empty DELETE_RUN COLD_EMPTY
```

Use `tools/nsb3_sms_empty.lua` for 47 seconds, then accept all four phases:

```sh
.venv/bin/python tools/nsb3_sms_check.py check SAVE_RUN COLD_READ --deleted DELETE_RUN --empty COLD_EMPTY
```

The preparer copies only own executable/EEPROM/SIM and input configuration,
hashes the immediate source files, and follows cold-run provenance back to
the qualified original external fixture; cyclic or missing provenance fails.
No guest RAM/save state or donor identity is transferred. The checker requires
physical contacts, matching phone code, complete APDUs, exact whole-card
transitions and reviewed editor/save/read/erase/empty pixel oracles.
This qualifies offline draft persistence/deletion, not SMS transmission,
reception, delivery reports or laboratory registration on NSB-3.

## SIM PIN and phone-code separation

The explicit `--pin-enabled` external-card fixture uses the shared generated
current SIM profile, preserving the existing own EEPROM integrity/retry
fixture. It does not change the handset's encoded security code. The card
first presents `Enter PIN code`; physical `4321` plus OK sends exactly
`A0 20 00 01 08` followed by the padded digits. The card returns `9804`,
the phone displays `Code error`, and only persistent PIN1 attempts at
serialized-card offset `dbc` change, from 3 to 2. All credentials, enabled
state and other card bytes remain identical to the generated profile.

A fresh process receives that exact two-attempt card and the handset's own
saved flash/EEPROM. Physical `1234` plus OK sends the corresponding VERIFY
APDU, returns `9000`, and restores only that retry byte to 3. It then displays
the separate Nokia `Security code` prompt. Physical `12345` reaches the own
matched-code branch, and Menu renders the reviewed Messages frame. SIM PIN
acceptance is therefore not being mistaken for phone-lock acceptance.

Prepare the usual fresh own integrity/retry fixture with `--pin-enabled` in
addition to `--insert-sim`, and run `tools/nsb3_pin_input.lua` for 22 seconds.
Then:

```sh
.venv/bin/python tools/nsb3_pin_check.py prepare-correct WRONG_RUN CORRECT_RUN
```

Run `tools/nsb3_pin_correct.lua` in the new directory for 42 seconds; its
phone-code input starts at 20 seconds, after SIM authentication. The default
phone-code fixture timing is unchanged. Accept the pair with:

```sh
.venv/bin/python tools/nsb3_pin_check.py check WRONG_RUN CORRECT_RUN
```

Acceptance requires exact physical contacts, complete VERIFY APDUs, distinct
card status/retry transitions, whole-card comparisons, cold source hashes and
initial-card snapshot, unchanged own executable, and reviewed PIN/error/phone
prompt/menu frames. No guest RAM/save state, forced authentication result or
donor provisioning is supplied. PIN change/disable, PIN2, exhaustion/PUK and
security-state restoration remain separate unqualified NSB-3 contracts.
