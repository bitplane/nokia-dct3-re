# Nokia 8210 bring-up

## Current boundary

The normal NSM-3 v5.31 PPM C machine remains at the final sparse-flash
verification wait, `0x2cadce`. The separate `nsm3stage` research composition
executes this product's uploaded verifier and loaders, then retains native
ownership and stops at absent resident DSP routine `0x2c75`. Physical-verdict
equivalence and complete native DSP runtime are not claimed.

The separate `nsm3hle` composition explicitly hands off at that missing-code
call. With the acquired unchanged base-record storage fixture, seven fresh
isolated acceptance scenarios pass: graphical/physical security and menus,
calculator, SIM initialization and cold-persistent phonebook, registration
and operator idle, both call-signaling directions, and both SMS directions.
Identity and record success replies remain unselected. This is research-HLE
phone-service acceptance, not a promotion of the normal machine, complete
hardware fidelity, speech, or authentic factory-default provisioning.

The explicit-version fixture produces 6, while the collaborator bridge reports
an 8210 verdict of `1eff`. A matching raw capture has not been recovered;
neither value is promoted as a measured silicon result.

## Native upload acceptance

Both catalogue origins are decoded from the pinned acquired flash, not a donor
image: ROM5 initializes at `0x30ced4` into `0x135810`; ROM6 initializes at
`0x30cf50` into `0x13579c`. Each contains 28 descriptors. The selected ROM6
fragment starts at file offset `0x11ab14`; its 104 words have SHA-1
`440bf49f1eba4cadb12f7f7581c992b0025807d6`.

The second-loader descriptor at `0x31ac34` contains
`0a00 1000 026f 0200 03e8 0000`: **623 words**, not the 613-word extent of
earlier products. Its payload starts at file offset `0x11ac40`, SHA-1
`8e9e4aefa311375ae090b90a607f00cb8e7059ca`. Loader control is data `0x0880`,
whose observed value is `0x0078`; using `0x087f` selects a zero field and fails
the loader contract. These are product configuration, not transport behavior.

The read-only `tools/noki8210_staged_observe.lua` fixture observes the MCU
consuming native publication `0000/0006`. The native loader requests selector
`0x14` once and selector `0x01` 133 times, verifies all 623 second-loader words,
installs 422 program words at `0x0590..0x0735`, and stops at `0x2c75` with
ownership retained. No runtime HLE handoff occurs in this fixture.

Reproduce with a fresh NVRAM/config directory, `nsm3stage`, `-debug -debugger
none`, the observer as `-autoboot_script`, and `-seconds_to_run 12`. Check the
result with:

```sh
.venv/bin/python tools/noki8210_staged_check.py RUN/error.log \
  roms/noki8210/8210_5.31ppm_c.fls
```

This establishes execution under explicit version/peripheral inputs and the
own-upload boundary; it does not supply missing ROM6 mask code, validate the
physical PMST mapping, or prove graphical boot.

## Runtime service frontier

`nsm3hle` suspends native execution only at the observed missing `0x2c75`
call. Without a service peer, its own MCU sends type `05` body
`1eff00d000030101e000` repeatedly. Enabling the shared request-correlated D0
discovery transport yields response `1e0002d000030401c100`, which this MCU
acknowledges with `1e0200d0000305014100`. This is a research HLE transaction,
not native execution of missing resident code.

After discovery the acquired composition publishes type `70` requests:

| Command | Payload Length | Observed Body |
| --- | --- | --- |
| `13` | 6 | `1304d52a008f` |
| `14` | 14 | `140c47ecf9859aff4d3ba84f86bd` |
| `15` | 22 | `151429ad7d6cc76d5b55a0dbc4d7ffffffffffffffff` |
| `16` | 26 | `161839a29f978ca9fe05c7a73068f2947c45e76a574e9c7f8ad8` |
| `0d` | 2 | `0d00` |

No identity or record response is configured by this composition. The own
class dispatcher `0x243a02..0x243a3e` sums to class `0x74` and calls
`0x240db0`. Its command cascade selects `0x0d` at `0x240e20`, checks bit 2
of flag byte `0x13fde1`, cancels timer `0x18`, and interprets response byte
`+9` bits 0/1 as faults. It clears the pending field at `0x13fbef` and stores
the two outcomes at `0x13fbf0/0x13fbf1`. The pinned instruction/literal check
is `tools/noki8210_selftest_contract.py`; sibling addresses are not its input.

The original acquired journal is a negative control: compact `0d00`
completion reaches the own handler armed (`flag=84`) with cleared DSP fault
outcomes, but local NV validation has already failed. Its frame remains
blank. The unchanged base-record fixture below instead preserves the local
validation bit (`flag=c4`) and supports graphical/runtime acceptance.

Run the same fresh-directory command with `nsm3hle` instead of `nsm3stage`,
then add `--runtime` to `noki8210_staged_check.py`. This acceptance requires
native upload completion, explicit ownership handoff, the own discovery
request and its firmware acknowledgement. It makes no display or handset
functionality claim.

Add `--selftest` to require the own armed handler and cleared fault outcomes;
add `--base-record` for that explicitly labelled storage fixture. Local NV
validation, not an unsolicited service map, explains the missing startup
initializers in the original-journal negative control. Identity/record
responses remain unselected in the accepted runtime composition.

## Board and physical-input observations

Nokia's [8210 user guide](https://www.telefonguru.hu/manuals/nokia_8210_en.pdf)
specifies the BLB-2 battery. The research composition selects the existing
nominal BLB-2 board tuple instead of the conservative full-scale placeholders;
its raw values are calibrated laboratory inputs, not measured NSM-3 units.
The own firmware then samples BSI/temperature selectors 3/4 and VBATT selector
2 repeatedly, rather than the earlier selector-3-only path. This change alone
does not paint the screen.

The own scanner `0x305940` drives row pins 0..4 and computes row*5+column at
`0x3059c8..0x3059ce`. Decoder `0x307dbe` loads the matrix pointer at
`0x307e40`, which resolves to `0x33ee78`. Its 25 bytes are
`3e3e3e3e3e11190102030e170405060f18070809101a0c0a0b`: row 0 is unused,
and the four physical rows occupy pins 1..4. The research machine therefore
selects the five-column host layout and row-pin shift 1. This is static wiring
evidence; the physical fixtures below also validate decoded input.

`tools/noki8210_menu_input.lua` applies only a physical Menu field for 150 ms
at 12 seconds and captures the result. The original-journal negative control
has no decoded key and a blank frame. On the checksum-valid base-record
fixture the same key decodes as `19`; the security, menu and application
fixtures below demonstrate organic physical UI interaction.

## Startup readiness boundary

The physical-input observer retains bounded, read-only probes of the own
startup report primitive `0x2885ac` and readiness checklist. The original-
journal negative control observes task-1 reports `17`, `16` and `14`,
but not `15`. The four-report consumer at `0x2a59c4..0x2a5a86` records bits
8/2/1 for those reports and bit 4 for `15`; its completion checks low nibble
`0xf` as well as the separate mode predicate. Missing `15` is therefore a
concrete startup dependency, not a guessed service message.

Keyboard initialization `0x307df6` is invoked from `0x2a59a2`. Its write at
`0x307e1c` ORs `0x1f` into column-mask register `0x6b` at 1.406088 seconds,
after low-level enable `0x305a5c` has executed. The recovered MMIO write
identifies this writer; neither the IRQ0 masking helper `0x305a3a` nor a
host input failure accounts for that write.

Report `15` is posted by `0x2ff7ac`, whose only direct BL caller is
`0x24a9ac`. The preceding handler `0x24a8e4` accepts input codes `0c..15`
through a ten-record jump table at `0x24a910`; `13` is a no-op. Inputs mark
nine bytes at `0x137e44`, and the report posts only when all nine are nonzero.
Input `64` clears the array. In the original-journal negative control the
handler sees only that reset, called from `0x2925aa`; all nine bytes remain
zero. The accepted base-record fixture instead completes all nine inputs.

An aligned direct-BL scan of the acquired image finds ten calls to this
handler: one reset plus these nine literal-input publishers. This is direct
call coverage, not a claim that indirect producers cannot exist.

| Input | Checklist Index | Own Direct Publisher Call |
| --- | --- | --- |
| `0c` | 0 | `21b876` |
| `0d` | 1 | `25d316` |
| `0e` | 2 | `250240` |
| `0f` | 3 | `20b222` |
| `10` | 4 | `2564c4` |
| `11` | 5 | `225eba` |
| `12` | 6 | `2bb346` |
| `14` | 7 | `2085ba` |
| `15` | 8 | `2aaa7e` |

The strongest preceding failure is local NV validation, not an absent DSP
self-test verdict. Firmware clears bit `0x40` of `0x13fde1` at `0x240c00`
before the compact self-test response arrives. The checksum routine
`0x2408bc` sums logical NV bytes `0x120..0x253`, excluding `0x154/0x155`,
and the caller compares that result with the word at `0x254`; it also
requires the checksum/companion word at `0x170` not both to be zero.
Logical reads use the cache at `0x11ea18` through `0x2ca3ec`.

The acquired PMM's base record is valid for this check: its body at file
`0x10026` computes and stores `0x77e2`. A passive cache snapshot at the
failure has the same bytes throughout `0x120..0x253`, but stores `0x7b26`
at `0x254`. A write watch proves that firmware first copies `0x77e2` at
0.044949 seconds, then writes `0x7b66` at 0.200631 and `0x7b26` at
0.208416, all through copy routine `0x30bc98`. Own sector scanner
`0x2c9d40` invokes flash-copy routine `0x2eca08`: the initial body is read
from MCU `0x3e0026`, then the checksum updates from `0x3e821e` and
`0x3e866c`. They are existing journal records, not a new computation.
`tools/noki8210_pmm_check.py` independently replays all 870 records through
file `0x1f68a`; its entire 32 KiB result exactly matches the passive runtime
cache snapshot. The archive's journal is therefore inconsistent with this
firmware's checksum, while the storage read path reproduces it correctly.
Preserve the original archive. Before selecting any derived base-record
fixture, establish its provenance and which later product records it would
omit; do not mistake a donor or edited success verdict for a storage fix.

A diagnostic fixture retaining the unchanged acquired base record and
erasing only its later low-record journal (`0x18026..0x1ffff` in the PMM
tail) passes local validation. Other PMM sectors and all base-record fields,
including identity and checksum, remain unchanged. It is an acquired
earlier snapshot, not an established factory-default profile. With this
fixture all nine readiness publishers execute, report `15` arrives, and
physical Menu decodes as `0x19`. The current `nsm3hle` composition includes
the laboratory SIM and radio peer; their separate acceptance contracts are
below. None establishes native DSP completion.

Generate that explicitly diagnostic persistent flash in an isolated run:

```sh
.venv/bin/python tools/noki8210_pmm_check.py \
  'roms/noki8210/8210 virgin eeprom 003d0000.fls' \
  --mcu roms/noki8210/8210_5.31ppm_c.fls \
  --base-record-flash RUN/nvram/nsm3hle/flash
```

Run `nsm3hle` with the physical Menu fixture and that NVRAM directory.
`nsm3hle` now composes SIMI and the removable laboratory card. On this
labelled snapshot, the firmware activates the card, performs its own
SELECT/STATUS/read conversation, reads all 50 EF_ADN records (`6f3a`), and
reaches the phone security editor. `tools/noki8210_security_input.lua`
presses physical digits `1..5` and Menu at 12 seconds; the own decoder logs
`01..05` and `19`. The editor dismisses to a Menu/Names idle presentation,
and the subsequent physical Menu press opens the Messages menu. This is
interactive UI and SIM-read acceptance; phonebook writes, registration,
calls and SMS require the separate scenario checks below.
Retain the original-journal negative control and do not silently promote
this fixture to the normal machine ROM.

### SIM phonebook persistence

On the same labelled base-record composition,
`noki8210_phonebook_input.lua` navigates Names -> Add entry and enters
`A / 123` with physical keys. The firmware issues absolute EF_ADN record-1
UPDATE RECORD (`A0 DC 01 04 20`), receives `9000`, and displays “Saved to
SIM card”. A fresh process running `noki8210_phonebook_read.lua` with the
same NVRAM reads the contact through Search -> Detail: captured screens
show `A` and `123`. No storage is seeded between these processes.
`noki8210_phonebook_check.py WRITE_LOG READ_LOG SIM_NVRAM` requires the
ordered physical save/APDU completion, cold record read, absence of writes
in the readback process, and exact persisted `A/123` record with the other
49 entries erased. Inspect UI captures separately; the checker does not
claim to recognize screen text. Preserve the write log before the second
process replaces `error.log`. Handset-local contacts remain untested.

### Radio acquisition boundary

The own ring dispatcher at `0x306fa6` selects the thirteen-entry
`0x83..0x8f` table at `0x306fd4`. Type `8b` calls `0x2df484`, which posts
to task 12 through `0x28845c`. Type `89` calls `0x2df210`; instructions
`0x2df22e..0x2df238` correlate body bit 0 with the pending channel context.
`noki8210_radio_contract.py` checks these own-ROM facts and the full table.
Startup organically publishes a 160-byte type `56` candidate window.

The declared `RADIO_NSM3` research contract selects request-correlated
candidate acquisition and bit-0 assigned-channel confirmation. Release,
handover and neighbour contracts remain unset pending observations. A
65-second coherent run observes serving-cell selection, handset Location
Updating Request (own capability octet `33`), acknowledgement of Location
Updating Accept and Channel Release, writes to EF_LOCI LAI and status, and
subsequent paging/BCCH reconfiguration. This is preliminary registration
transport evidence is now independently checked by
`noki8210_registration_check.py LOG SIM_NVRAM`: ordered own request with
capability `33`, accept/release acknowledgements, EF_LOCI LAI update,
deconfiguration/confirmation and post-release paging. Persisted EF_LOCI
contains LAI `00f1100001` and status `00`. A cold process retaining that
storage passes the same exchange and `noki8210_registration_input.lua`
captures `DCT3 LAB` at both 24 and 44 seconds after physical unlock.
Incoming calls, SMS, mobility and other bands remain unproved.

### Outgoing call signaling

`noki8210_outgoing_call_input.lua` physically unlocks, dials `1234567`,
presses Send, then End. The own decoder emits Send/End `0e/0f`, the
laboratory session receives exactly that number, and the UI shows Call 1
with Options/Hold. The firmware organically configures traffic with
`040002000271012fc10000010000000400000000`; physical End publishes
`040000001117001a600000040000001400000001`. Its observed `14` release
parameter is now declared in `RADIO_NSM3`.
`noki8210_outgoing_call_check.py LOG` requires the ordered physical and
CC/RR lifecycle, exact called number, assignment/connect/disconnect counts,
own traffic/release packets, idle confirmation and return to paging.
The final capture returns to DCT3 LAB. This validates signaling and UI,
not speech or native DSP execution.

### Incoming call signaling

Seed the isolated run's configuration directory with
`fixtures/radio_incoming_call_answered/nsm3hle.cfg` and run
`noki8210_incoming_call_input.lua`. The external network queues one call;
no handset message is injected. The ringing capture presents `5551234`,
physical Send answers, and physical End restores DCT3 LAB idle.
Own Call Confirmed is `8308150101` (five bytes); do not inherit the
sibling's eleven-byte expectation. `noki8210_incoming_call_check.py LOG`
requires paging/contention, cipher/MM information, SETUP/Alerting,
traffic assignment, physical Answer/End and full CC/RR release followed by
idle confirmation and paging. Exactly one SETUP, CONNECT and DISCONNECT
must occur. Speech and native DSP execution remain unproved.

### Incoming SMS

Copy `fixtures/radio_incoming_sms/nsm3hle.cfg` into the isolated run's
configuration directory and run `noki8210_incoming_sms_input.lua`.
The laboratory network delivers one ordinary message; the phone shows
“1 message received”, and physical Read opens `hello`.
`noki8210_incoming_sms_check.py LOG SIM_NVRAM` requires paging, the own
`0080ffffffffffffffff0000` cipher-control publication, SAPI-3 establishment,
segmented CP-DATA, CP/RP acknowledgements, RR release, physical reading,
and exact durable read-status/content. Exactly two EF_SMS record-1 writes
occur: delivery and marking read. Screen captures are independently
reviewed, not recognized by the trace checker.

### Outgoing SMS

`noki8210_outgoing_sms_input.lua` navigates Messages -> Write messages,
physically enters `A` and `5551234`, and confirms Send. The success capture
shows “Message sent”; the composer subsequently clears. The network
decodes the exact GSM 7-bit SMS-SUBMIT, receives one accepted submission,
and closes CP/RP and RR before returning to paging.
`noki8210_outgoing_sms_check.py LOG` uses shared GSM transaction checks,
but requires the own physical-key log. TP-MR is allowed to advance across
successive submissions; destination, alphabet and exact `A` payload remain
pinned. Two consecutive preserved-storage runs pass with TP-MR `01/02`.
### Application and isolated acceptance

`noki8210_calculator_input.lua` physically navigates Menu 7, selects
Calculator and computes `12 + 3 = 15`. The result frame is pinned in
`run_noki8210_acceptance.py`; no firmware arithmetic or UI state is edited.
This is a representative app check, not exhaustive coverage of all apps.

Run any focused scenario from a new directory:

```sh
.venv/bin/python tools/run_noki8210_acceptance.py RUN \
  --scenario registration
```

Available scenarios are `registration`, `incoming-call`, `outgoing-call`,
`incoming-sms`, `outgoing-sms`, `calculator` and `phonebook`. Each seeds
only the unchanged acquired base-record snapshot into a fresh persistent
flash image. Incoming events use copied external network configuration;
all UI interaction uses physical key fields. `phonebook` executes save
and cold readback as separate processes sharing only persistent storage.
Successful checks produce `acceptance.json`, console/log evidence and
captures. Existing directories are refused. The normal machine remains
unchanged; native DSP completion and speech are explicitly not claimed.

The same scenarios are standard gates: `make verify-8210-registration
RUN_DIR=/tmp/8210-registration`, with corresponding `incoming-call`,
`outgoing-call`, `incoming-sms`, `outgoing-sms`, `calculator` and `phonebook`
suffixes. Use a distinct, nonexistent `RUN_DIR` for every gate. The runner
pins both acquired input hashes before creating storage; wrong-product inputs
are rejected rather than silently provisioned. Incoming configuration is
copied into the run, never modified in the tracked fixtures.

The shared observer retains staged-DSP/self-test, decoded-key and readiness
acceptance records only. PMM copy/cache dump, column-mask and input-lifecycle
investigation probes are retired. The journal replay and checksum tools/tests
retain their conclusions: the original journal is a failing negative control,
and omitting its later low records is a diagnostic snapshot fixture, not an
authentic factory-default reconstruction.

Readiness observations (`8210_startup_post`, `8210_readiness_input` and
`8210_readiness_flags`) are passive. Neither report `15`, checklist bytes nor
task-resume results are injected by acceptance.

## Inputs and hardware

The stock MCU/PPM-C composition exactly matches the canonical MAME flash.
The canonical product-local PMM tail is also acquired; provenance and hashes
are in `roms/README.md`. No sibling provisioning image is enabled.

Nokia's [NSM-3 System Module, issue 1 12/1999](https://www.eserviceinfo.com/preview_html.php?fileid=5456&previewid=3011)
identifies MAD2WD1, CCONT, CHAPS and COBBA-GJP, a 13 MHz system clock and
flash-backed product data. The stock service package assigns the same MCU
to ROM5 and ROM6; [service bulletin SB-050](https://www.eserviceinfo.com/downloadsm/225117/NOKIA_NSM3-050.html)
documents the later ROM6 board revision. The current profile selects ROM6.
Internal DSP ROM images remain absent. Uniform-fill audit members are not
executable DSP evidence.

## Established contracts

- GENSIO control `0x22` selects CCONT. The byte write at `0x302d32` is followed
  by receive-ready polling at `0x302d36`; control bit 2 is clear. NSM-3 therefore
  uses byte-write-triggered ready, independently of other products.
- `0x2cac80` parks shared offsets 2/4/6 at `0xffff`; `0x2cad46` accepts silicon
  PROM version 6 or 5 from offset 4. The HLE peer publishes version 6 after that
  physical sentinel write. It does not overlay reads.
- `0x2cad60..0x2cad9a` transfers 115 blocks of 512 halfwords; the final block
  has 510 halfwords and two `0xffff` terminators. Source starts at `0x200040`
  and advances by `0x20`, so this is sparse ARM-flash input, not DSP program
  code. The trace independently observes 58 alternating ownership pairs.
- `0x2cadce` waits for shared offset 2 to leave `0xffff`, then copies offsets
  2/0 into product-local bootstrap state at `0x135774 + 0x0e/0x0c`. The HLE
  does not publish the unvalidated final results.

The boot descriptor pointer at `0x135808` resolves to flash `0x31bcf0`.
Its six-word descriptor is `0f00 0000 00df 0f00 00dc 0000`; the 223-word
program at flash `0x31bcfc` is independently observed byte-for-byte at shared
offset `0xe00` before the sparse transfer. Its SHA-1 is
`6646da3c5be9c70deda7e0b5b9f257d5d2ace815`. The `00dc` descriptor field's
meaning remains unassigned. Execution outside that staged image must not be
filled with guessed mask-ROM instructions.

The isolated `nsm3verify` core fixture loads this program at `0x0f00`, supplies
the observed MCU buffer descriptors (`087b=0100`, `087c=0300`,
`087d:087e=0000:e800`, `0881=0200`), and sends blocks only at the program's
alternating ownership polls. It consumes all 116 blocks without leaving the
staged program. Its default case writes ports `000e=1387`, `0000=000d`,
`000c=0010` and stops fail-closed on the first port `002d` read (reported PC
`0f9f`, after the PORTR operands).

Comparison cases attach the existing COBBA register model. The verifier reads
register F, waits on register D's `bits 1:0=0`/`bits 3:2=3` status handshake,
then reads F again. Supplying F as 0 or `0016` changes only the companion
publication at data `0800`. `0016` is a metamorphic fixture value, not an
8210 hardware identity. The model's reset value 0 remains uncharacterized for
this product and is not claimed to be a measured COBBA identity.

The absolute-MVPD operand order independently agrees with GNU binutils 2.43.1:
`7cf8 0801 ff87` copies program `ff87` to data `0801`. Likewise,
`7df8 0803 ff87` attempts a write to program `ff87`. The acquired ROM4 PROM
has value 4 at that address. The verifier sets `PMST=ffa8`; the
[TI CPU reference, table 4-3](https://www.ti.com/lit/ug/spru131g/spru131g.pdf)
defines `MP/MC` as bit 6, clear here, enabling on-chip ROM. `OVLY` and `DROM`
are set. Do not mistake the high interrupt-vector-pointer bit for `MP/MC`.
The fixture explicitly supplies version 6 (the selected NSM-3 board contract),
or version 4 as a negative family comparison, and ignores the program's
attempted write of 6. Publications at data `0801` and `0802` follow that input;
data `0803` retains the program's constant 6. Treating all program space as RAM
would incorrectly turn the attempted write into a version source. No ROM6
mask image has been recovered. The fingerprint is stored at data `04f7:04f8`
under the current core; its production semantics remain unresolved. With the
pinned stock flash it is `c2e0:6006`, unchanged across both COBBA inputs and
the version-4/6 comparisons, with final `PMST=ffa8`. Neither half equals the
collaborator-reported `1eff`. This separates the fixture's flash calculation
from its version-dependent shared completion; it does not establish where
ROM6 hardware maps either result.

These tests establish the publication semantics under the current clean-room
core and explicit peripheral inputs, not silicon equivalence, a measured ROM6
version-cell layout, or a valid handset completion. The collaborator bridge's
`DSPB_HLE_VERIFY` comment reports `1eff` from a physical 8210; that is a lead
requiring its raw trace, ROM revision and memory mapping, not an input to copy.
The operand-order audit is closed; the unresolved evidence is ROM6's mapping
at `ff87` under these PMST settings and the matching physical trace/program.
The generic CPU core stores PMST at data register `001d`, but does not switch
ROM/RAM mappings for `MP/MC`, `OVLY` or `DROM`: instruction fetch uses the
configured program cache, MVDP/MVPD use the configured program space, and
ordinary data accesses use the configured data space. Thus the fixture's
read-only `ff87` handler is an explicit test input, not an implementation of
the PMST-controlled ROM6 memory map. The existing ROM4 board backend separately
implements its evidenced OVLY alias (`0080..27ff`) and HPI DARAM mapping;
that backend is not selected by this isolated fixture and is not a ROM6 map.
Before a core-backed handset promotion,
establish which physical memories cover program `ff87`, data `04f7:04f8`
and shared data `0800:0803` in this mode, including aliases and write protection.
Do not implement a generic C54x overlay from the ROM4 image alone: the MAD2
revision's memory layout is part of the missing contract.
No matching raw verdict capture or ROM6 mask image was found in the checked-out
collaborator repository. Its hardware-bridge source reports the value but
does not include the capture. A code comment is not a substitute for those inputs.

The independent assembler input and output are:

| Assembly | Encoded words |
| --- | --- |
| `mvdp *(0803), ff87` | `7df8 0803 ff87` |
| `mvpd ff87, *(0801)` | `7cf8 0801 ff87` |
| `portw *(0008), 000e` | `75f8 0008 000e` |
| `portr 002d, *(0009)` | `74f8 0009 002d` |

The analysis tool was built outside the repository from
[GNU binutils 2.43.1](https://ftp.gnu.org/gnu/binutils/binutils-2.43.1.tar.xz),
SHA-256 `13f74202a3c4c51118b797a39ea4200d3f6cfbe224da6d1d95bb938480132dfd`,
target `tic54x-coff`. Its code is not part of the MAME implementation.

The existing independent emulator clears the final sentinel at read time.
That is a compatibility behavior, not a captured NSM-3 verification result,
and is not imported. Packet/service, SIM, keypad and radio contracts remain
unpromoted behind this boundary.

## Acceptance

`make normalize-8210` reconstructs the stock flash and checks PMM identity.
`make verify-8210-bootstrap` validates the ROM6 publication, exact upload
handoff order and fail-closed final wait. It is a frontier gate, not boot/UI
acceptance. Existing product profiles are unchanged.

`make verify-8210-verifier` extracts the pinned staged image, executes it in
an isolated run directory and checks the unsupported-peripheral case plus
COBBA and immutable-version sensitivity cases. Unsupported peripheral reads
are fatal rather than synthetic responses.

## Evidence needed to resume

The software-accessible stock upload, operand decoding, existing COBBA model
and explicit memory-input comparisons do not establish the final silicon
publication. A useful physical or independently captured reference must include:

- NSM-3 board/DSP ROM revision, MCU/PPM hashes and the staged-program hash;
  the current reference is v5.31 PPM C and the 223-word program above.
- Ordered reset/release, upload and ownership exchanges, followed by actual
  DSP writes to shared offsets `000`, `002`, `004` and `006`. Distinguish raw
  hardware reads from bridge-HLE substitutions and retransmission repairs.
- Program `ff87` read/write behavior under `PMST=ffa8`, the memory backing
  data `04f7:04f8` and `0800:0803`, and COBBA register-F/status-D readings.
- The final result pair and its ordering relative to the last buffer
  acknowledgement and any reset, with the raw trace retained and hashed.

A matching raw capture can justify a narrowly declared HLE bootstrap contract;
a ROM6 memory image/map can support core execution. Neither is currently
available in the acquired collection. UI, SIM, network, call/audio and
handset save/load acceptance remain unvalidated behind this boundary.
