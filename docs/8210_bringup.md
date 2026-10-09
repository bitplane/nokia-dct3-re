# Nokia 8210 bring-up

## Current boundary

The normal NSM-3 v5.31 PPM C machine remains at the final sparse-flash
verification wait, `0x2cadce`. The separate `nsm3stage` research composition
executes this product's uploaded verifier and loaders, then retains native
ownership and stops at absent resident DSP routine `0x2c75`. Physical-verdict
equivalence and complete native DSP runtime are not claimed.

The separate `nsm3hle` composition explicitly hands off at that missing-code
call. With the acquired unchanged base-record storage fixture, fresh
isolated acceptance scenarios pass: graphical/physical security and menus,
calculator, SIM initialization and cold-persistent phonebook, registration
and operator idle, both call-signaling directions, and both SMS directions.
Identity and record success replies remain unselected. This is research-HLE
phone-service acceptance, not a promotion of the normal machine, complete
hardware fidelity, native DSP speech, or authentic factory-default provisioning.

### Documented PCM bus

Nokia's NSM-3 System Module, issue 1 (12/1999), printed page 24,
independently specifies COBBA-GJP-generated PCMDClk at 1 MHz (13 MHz / 13)
and PCMSClk at 8 kHz (PCMDClk / 125). The inspected timing diagram shows
a one-data-clock active-high sync pulse, MSB-first 16-bit words with bits
15..13 extending the sign of the 13-bit sample, and data transitions at
rising clock edges (falling-edge sampling). This establishes the product's
physical PCM contract, not native DSP speech. The product configuration records
those independently evidenced bus fields. A fresh rebuilt physical
outgoing-call gate (`run_8210_pcm_profile_regression`) preserves own-number
dialing and CC/RR release. That profile-only signaling regression does not
establish audio. Own speech-request selection and endpoint routing have
separate compiler, runtime and physical-audio evidence below; neither is
inferred from the PCM diagram alone.

The primary-authored PDF is acquired from
[the service-manual mirror](https://www.eserviceinfo.com/downloadsm/228901/NOKIA_03sys.html)
and retained as `roms/research/nsm3/nsm3-system-module.pdf` (ignored source
collection), SHA256
`dbd9e70549b6de726be9667a2451bda74b7317f84956b50e5c91fce3124939c0`.

Own compiler `0x2cafd8` has 53 selector entries at `0x2cb00c`.
Selector `0x11` reaches `0x2cb254`, constructs bit `0200`, and combines it
with keep-mask `fdff` into field shadow `0x135776`. Selector 8 reaches
`0x2cb29c`, prefixes the low twelve argument bits with `8000`, retains a
separate command shadow `0x135778` and publishes DSPIF `0x100a8` at
`0x2cb2ea`. Do not conflate the two shadows. The independent parameter
publisher at `0x2c6cf4` masks its constructed word with `fdff` from
`0x337db8`, then conditionally adds a halfword from table `0x337db0`.
The four entries preceding that keep-mask are `0200`; runtime index-domain
coverage is not claimed. Its command-selector table at `0x337da4` starts
`08 09 30`, and `0x2c733e` publishes the first constructed word through
the own compiler. This is a decoded producer connection, not an inference
from shadow adjacency. The fresh physical outgoing trace changes
command 8 to `860b` after Send and back to `840a` after End, independently
corroborating call-correlated bit `0200`, not delivered audio.
`noki8210_speech_control_check.py ROM LOG` pins the own
ROM, selector instructions/literals and ordered physical/control events.

The command-8 mask/value `0200` is now selected only in the explicit
`nsm3hle` composition. Fresh rebuilt `run_8210_speech_request_regression`
passes the physical outgoing lifecycle and own control checker; HLE retains
`060b` during the call and `040a` at release. Its PCM-derived cadence executes
400 uplink frames and zero downlink frames before stopping. That run preceded
audio endpoint configuration, so its uplink frames are not evidence of
captured microphone audio. It is not an answered SIP/media gate. The normal
machine and native-DSP limitations are unchanged.

### Own audio endpoint evidence

The complete three-volume NSM-3 service archive from
[this mirror](https://www.eserviceinfo.com/downloadsm/5456/Nokia_8210.html)
is acquired and passes `unrar t`; its PDFs are retained under
`roms/research/nsm3/`. Do not infer the product from the misleading archive
filename mentioning 8810: the inspected document headers identify NSM-3.
`04ui.pdf`, UI Module issue 1 (12/1999), printed page 15, explicitly connects
the built-in microphone to COBBA MIC2. Its table maps X300/2 MICP to MIC2N
and X300/1 MICN to MIC2P; preserve that documented polarity rather than
silently swapping labels. Printed page 4 identifies EARP/EARN receiver
connections. These support MIC2/EAR as declared HLE endpoints, not decoded
COBBA register policy. The page-15 32 dB gain is an electrical test condition,
not a recovered runtime gain. No gain is promoted from that figure.
SHA256 of `04ui.pdf`:
`684b896015d1531f552d224742d957286d7869ecda2517910f1c5bfa88062c5e`.

The explicit `nsm3hle` composition now selects those declared MIC2/EAR
endpoints and neutral host microphone/receiver routes, without inherited
gains or synthesized COBBA register writes. Fresh rebuilt
`run_8210_audio_endpoints_final` preserves the physical outgoing call gate;
five focused NSM-3/NPE-3 control tests pass. Actual answered SIP/RTP and
non-silent physical audio remain unvalidated. Endpoint wiring does not
promote native DSP execution or factory provisioning.

`make verify-8210-sip-outgoing-media RUN_DIR=NEW_DIRECTORY` validates one physical
outgoing SIP-200 call on `nsm3hle`, with own PCM timing and endpoints.
It invokes `tools/run_noki8210_sip_cancel.py --outgoing-media` and requires
the optional PJSIP 2.16 build documented in `external_call_bridge.md`.
Fresh run `run_8210_sip_outgoing_media_verified` passes the shared media,
full own-product CC/RR lifecycle and reviewed presentation checks. Bridge
counters are uplink 365, downlink 353, transmitted PCM 365 and received PCM
400 (39 dropped); handset sequences 0..351 are accepted in order.
Physical End emits DISCONNECT `036502e090`, network RELEASE, then handset
RELEASE COMPLETE `032a`, RR release and return to paging. This is not the
handset-RELEASE direction assumed by the older products' SIP fixtures.

The adapter publishes correlated `media_closed` when the connected session
ends, before draining queued media; final `ended` follows CC/RR completion.
The bridge stops downlink immediately on `media_closed` but retains the
dialog until `ended`. A queued terminal packet may be rejected safely only
after that explicit boundary, with correlated completion and radio release.
Wrong requests, unclassified or active-media rejections, sequence
gaps and accepted packets after closure remain failures. Adapter rejection
reason fields are observational; no admission behavior changed. The common
checker accepts exact PJSIP status 200 with reason `OK` or `Normal call clearing`.

This establishes outgoing HLE media transport, not native DSP speech. Provisioning remains the declared
base-record comparison, not validated factory data.
The outgoing runner accepts `--record-media --sound pulse` for explicit
audio observation; recordings alone are not waveform acceptance.

`make verify-8210-sip-outgoing-waveform RUN_DIR=NEW_DIRECTORY` adds isolated
virtual PulseAudio endpoints and restores server defaults on exit. Fresh
`run_8210_sip_waveform_probe/handset` passes the full outgoing call plus
at least two consecutive seconds of 440 Hz in the SIP remote microphone
recording and 660 Hz in the MAME earpiece recording. Both stream routes were
observed. The tones traverse the configured physical audio ports and HLE
codec/PCM paths, not firmware-state injection. This validates non-silent
synthetic audio in both directions; it does not establish native DSP speech,
analog gain calibration or real RF service. Incoming audio is tested separately.

`make verify-8210-sip-incoming-media RUN_DIR=NEW_DIRECTORY` waits for a fresh
registered-idle frame before a local SIP INVITE, then uses the existing own
physical Call/Send and End fixture. It requires decoded keys, paging,
assignment, CONNECT/acknowledgement, DISCONNECT/network RELEASE/RELEASE
COMPLETE, return to paging, own loader/self-test/provisioning checks, ordered
media, and reviewed ringing/post-release UI. Call restoration acceptance is
separate, not inherited from these media tests.
Fresh `run_8210_sip_incoming_media_verified` passes with bridge counters
uplink 396, downlink 380, transmitted PCM 396 and received PCM 401 (13
dropped). This is independently executed incoming HLE media transport,
not native DSP or non-silent incoming-waveform proof.

`make verify-8210-sip-incoming-waveform RUN_DIR=NEW_DIRECTORY` independently
adds the isolated virtual microphone/earpiece tone fixture to incoming calls.
Fresh `run_8210_sip_incoming_waveform_probe/handset` passes sustained 440 Hz
microphone and 660 Hz earpiece recordings, full physical Answer/End, release
and UI checks. Bridge counters are uplink 396, downlink 379, transmitted PCM
396 and received PCM 400 (13 dropped). This verifies non-silent synthetic
audio in both directions on an incoming HLE call, not native DSP speech.

`make verify-8210-sip-outgoing-restore RUN_DIR=NEW_DIRECTORY` saves a connected
physical outgoing call, verifies exact CPU/SP/RAM/time restoration using the
existing architecture observer, then requires the external dialog to close
with SIP BYE. The fresh epoch republishes the own `1234567` request; the bridge
must clear it exactly once without redial and complete firmware CC/RR release.
Fresh `run_8210_sip_outgoing_restore_probe` passes, restoring architecture at
34.000 s and ending the epoch-2 call at 34.130 s. SIP dialog restoration,
waveform continuity across load is
not claimed. This fixture tests restoration, not media-quality acceptance.

`verify-8210-sip-pending-outgoing-restore` uses SIP 180 and the same exact
architectural save/load before connection. Fresh
`run_8210_sip_pending_restore_probe` passes real CANCEL/487, one fresh-epoch
clear and complete firmware release without redial, CONNECT, or accepted
media. The physically dialed `1234567` is checked both in request identities
and decoded SETUP; another product's digits cannot substitute for it.

`verify-8210-sip-incoming-restore` holds the existing physical incoming
Answer fixture, saves/restores exact CPU/SP/RAM/time at 42.000 s, closes
the real SIP dialog with BYE and clears the restored handset once under
epoch 2. Fresh `run_8210_sip_incoming_restore_probe` passes, ending the
restored call at 42.130 s. The checker requires the own decoded physical
Answer before connection, complete firmware release, and no dialog replay.
Waveform continuity across load remains a separate unvalidated case.

`verify-8210-sip-alerting-incoming-restore` holds before physical Answer and
saves/restores exact CPU/SP/RAM/time at 36.000 s. Fresh
`run_8210_sip_alerting_restore_probe` passes real INVITE rejection and one
fresh-epoch handset clear, ending at 36.110 s, without Answer, CONNECT,
accepted media or dialog replay. Connected and alerting restoration are
independently exercised in both call directions; external dialogs are always
discarded, not restored. None of these gates establishes native DSP speech.

Fresh matrix run `run_8210_sip_media_closed` passes with explicit media
closure at 37.030 s and completion at 37.040 s, with no rejected downlink.
The separate boundaries replace the falsified same-poll assumption; no
arbitrary rejection-time tolerance is used. A WebSocket transition test
verifies closure stops downlink without prematurely terminating the dialog.

## Physical power lifecycle

`make verify-8210-power-cycle RUN_DIR=NEW_DIRECTORY` starts with private fresh
storage, the declared base-record comparison and the configured ARFCN4 lab
cell. Physical security entry reaches idle, a four-second Power press drives
CCONT rail-off, and a later physical Power press wakes through PWRONX `02`.
The gate requires a blank off screen, at least eight seconds of continuous
CCONT RTC ticks and no DSP/radio transport activity during the off interval.
It independently checks both own native-upload sequences, explicit HLE
handoffs, compact self-tests and coherent registration; it does not infer the
second boot from the first.

After wake the firmware reads retained `EF_LOCI` before sending warm Location
Updating with body `05087200f110000133080910101032547698`. It rewrites the LAI
after acceptance but does not redundantly rewrite the already-valid location
status byte. The security editor reappears; physical `12345` and Menu decode
organically and return to reviewed operator-idle pixels. The RTC runs while
off, but firmware writes seconds register `07=00` after wake. Cold-process RTC
persistence, measured wake latency, all-peripheral rail gating and native
speech remain unproved. No charger pulse, donor storage or firmware-state
write participates in this fixture.

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

SIM PIN is distinct from the phone security editor.
`verify-8210-slow-pin-registration` enters physical `1234`/Menu at 8..12 seconds,
then the phone code `12345`. It requires enabled CHV1, unchanged PIN, restored
retries, the `57/03050000` background request, a receivable ARFCN 4 measurement,
correlated own-ROM route/completion and the full registration/EF_LOCI contract.
Type `8b` selects task 12, not NPE-3/NHM-3's task 14: completion
`21ef4c -> 2a2250 -> 28755e` consumes forty four-byte records, ARFCN at offsets
6/7 and signed RSSI at 9. The static radio-contract tool checks those own-ROM
instructions. The configured ARFCN 4/5 laboratory network supplies the response;
no phone PMM or firmware state is forced. Ordinary phone-code-only registration
remains a separate regression, and native speech is not established.

`verify-8210-pin-host-incoming-call` and `verify-8210-pin-host-incoming-sms`
compose that authentication lifecycle with the existing coherent host fixture.
The call gate checks caller pixels, physical Answer/End, own CC/RR signaling
and registered-idle recovery. The SMS gate checks CP/RP closure, physical Read,
reviewed message-body pixels and persistent read-status storage. Host inputs
are retained when selecting the PIN fixture's carrier pair; conflicting cell
configuration is rejected rather than silently rewritten.

`verify-8210-pin-host-outgoing-call` retains the physical `1234567` number,
host connect decision, exact own CC/RR grammar and release/idle UI checks.
`verify-8210-pin-host-outgoing-sms` retains physical `A` to `5551234`, the
correlated host submission decision, CP/RP closure and reviewed success UI.
Both require the same PIN/phone-code and coherent-registration evidence as
the incoming gates; no native speech claim follows from call signaling.

`verify-8210-pin-phonebook` saves `A / 123`, starts a fresh MAME process using
the saved card, repeats physical PIN and phone-code entry, and searches the
contact through ordinary keys. It requires own upload/self-test evidence,
one successful VERIFY per process, byte-identical card storage on readback,
retained-LAI registration without rewriting valid location status, and the
exact reviewed `Number: 123` image. The same image check also protects the
ordinary no-PIN save/readback gate.

The `verify-8210-pin-state-idle`, `-call` and `-sms` gates independently
restore authenticated sessions. They require exact CPU/RAM/emulated-time,
matched protocol replay and identical saved/restored pixels. Physical Menu,
End or Read must work after load; the call gate requires an established-call
save boundary and registered idle after release, while SMS checks delivered
storage before save and persistent read status afterward. Enabled CHV1/PIN
and exactly one successful startup VERIFY are retained. Native DSP restoration
is outside these research-HLE gates.

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
Incoming calls and SMS have the separate acceptance contracts below;
Mobility remains unproved; no-PIN DCS1800 registration is separately
covered below, without inheriting call/SMS acceptance.

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
`incoming-sms`, `outgoing-sms`, `calculator`, `phonebook`, `idle-state` and
`call-state`, `sms-state`, `host-incoming-call`, `host-incoming-sms` and
`host-outgoing-sms`, `host-rejected-sms` and `host-silent-sms`. Each seeds
only the unchanged acquired base-record snapshot into a fresh persistent
flash image. Incoming events use copied external network configuration;
all UI interaction uses physical key fields. `phonebook` executes save
and cold readback as separate processes sharing only persistent storage.
Successful checks produce `acceptance.json`, console/log evidence and
captures. Existing directories are refused. The normal machine remains
unchanged; native DSP completion and speech are explicitly not claimed.

### DCS Host SMS Acceptance

All four host SMS outcomes pass without PIN and with early physical PIN
entry (`--pin-enabled --pin-start 7`). Standard targets use prefix
`verify-8210-dcs-`; supply a new `RUN_DIR` for each:

| Outcome | No-PIN Suffix | Early-PIN Suffix | Additional Required Evidence |
| --- | --- | --- | --- |
| Incoming | `host-incoming-sms` | `early-pin-host-incoming-sms` | Physical Read, SIM read-status persistence, reviewed `hello` pixels |
| Submitted | `host-outgoing-sms` | `early-pin-host-outgoing-sms` | Physical `A` to `5551234`, reviewed success pixels |
| Rejected | `host-rejected-sms` | `early-pin-host-rejected-sms` | RP error, reviewed failure pixels, physical recovery |
| Timed out | `host-silent-sms` | `early-pin-host-silent-sms` | RP silence/timeout closure, distinct failure pixels, physical recovery |

Each requires own DCS registration, persisted EF_LOCI, correlated host
outcome and firmware CP/RP closure. ARFCN4 cannot satisfy DCS823 checks.
PIN fixtures additionally require the correlated measurement/VERIFY/registration
sequence. Fresh authenticated failure evidence is
`run_8210_early_pin_dcs_rejected_sms_01` and
`run_8210_early_pin_dcs_silent_sms_01`. The default eight-second PIN sequence
remains a failing negative control; these gates do not establish its recovery.

The original seven scenarios are standard gates: `make verify-8210-registration
RUN_DIR=/tmp/8210-registration`, with corresponding `incoming-call`,
`outgoing-call`, `incoming-sms`, `outgoing-sms`, `calculator` and `phonebook`
suffixes. Use a distinct, nonexistent `RUN_DIR` for every gate. The runner
pins both acquired input hashes before creating storage; wrong-product inputs
are rejected rather than silently provisioned. Incoming configuration is
copied into the run, never modified in the tracked fixtures.

`make verify-8210-ussd` independently exercises physical `*123#` and Send,
the exact GSM 04.80 processUnstructuredSS request and network response,
clean RR release, the firmware's `Nokia test network` screen, and physical
Back to registered idle. It also requires persisted laboratory EF_LOCI.
The NSM-3 normal decoder at `307dbe..307df4` reads pointer `33ee78` from
literal `307e40`; its last row is `10 1a 0c 0a 0b`. Thus Star occupies
column 2 and Hash column 4. The own NSM-2 table at `33f504` and NSB-6 table
at `339f4c` contain the same 25 bytes, so these research compositions share
the corrected port labels without a product-local override. Matrix bits and
controller behavior are unchanged. This is
physical firmware input and laboratory supplementary-service coverage,
not native DSP or factory-PMM promotion.

`make verify-8210-call-divert` independently enters physical `*#21#`/Send,
requires the exact service-`21` InterrogateSS and inactive network result,
RR release, the transient `Service not active` screen and physical Back
to registered idle with persistent EF_LOCI. Its early result capture is
intentional: the firmware automatically dismisses this result before the
later USSD capture time. Both fixtures share input mechanics and boundary
checks, not message payloads or frame oracles. This verifies interrogation
of an inactive service, not activation, forwarding or delivery of a call.

`verify-8210-sim-toolkit` independently enables the card-owned Phase 2+
DISPLAY TEXT fixture on the same acquired base-record comparison. The own
firmware reads EF_PHASE=3, publishes a nine-byte TERMINAL PROFILE, observes
STATUS `91 16` and fetches `A0 12 00 00 16`. The reviewed 84x48 `DCT3 SAT`
frame has pixel SHA256
`0c609d0fc7f1f59534f7e29ccaa995d441ff52f701b76093661b2458380ff558`.
Physical Menu/OK produces successful TERMINAL RESPONSE
`810301218002028281030100`/`90 00` and restores numeric-PLMN idle (pixel
SHA256 `d6d4b05af24a06c42a97e31c3134f7e497e149f4420f87f3d243613f06926300`).
The gate requires own staged/self-test/base-record predicates and persisted
laboratory registration. These Toolkit gates establish the default-cell,
PIN-disabled composition are established; no DCS/PIN Toolkit combination,
factory PMM or native DSP promotion follows. Accepted protocol ordering is
shared through `dct3_toolkit_check.py`; startup, provisioning and frames
remain product-owned.

`verify-8210-sim-toolkit-inkey` selects the existing card-owned GET INKEY
sequence after DISPLAY TEXT. The first terminal response advertises the
next command with `91 15`; firmware fetches its 21-byte payload and renders
`Press 5`. Physical digit 5 enters the editor, and physical Menu/OK completes
TERMINAL RESPONSE `8103022200020282810301000d020435`, followed by `90 00`
and registered idle. Digit entry alone does not complete this NSM-3 UI
transaction. The gate retains the own staged/self-test/base-record and SIM
location requirements and checks exact DISPLAY TEXT, GET INKEY and idle
pixels. This proves GET INKEY in this composition, not other proactive
commands or a change to the native-DSP boundary.

`verify-8210-sim-toolkit-input` extends that same sequence with GET INPUT.
The GET INKEY response advertises `91 1a`; firmware fetches 26 bytes and
renders `Enter 42`. Physical digits 4, 2 and Menu/OK produce
`8103032300020282810301000d03043432` and `90 00`, returning to registered
idle. The extended gate checks both preceding commands, all three physical
input actions, the GET INPUT prompt and the final idle pixels, along with
unchanged own-product startup and persistent-location predicates. Its
accepted response is two GSM-default-alphabet digits; arbitrary text,
UCS2, cancellation, timeout and PIN/DCS combinations are not established.

`idle-state` uses `noki8210_state_idle.lua` after physical security-code
acceptance and laboratory registration. At 32 seconds it saves registered
idle, compares a one-second reference interval with the restored interval,
then presses physical Menu and captures Messages. `noki8210_state_check.py`
requires exact PC/SP/mapped-RAM digest/emulated-time restoration, a nonempty
ordered radio/SIM replay with identical payloads, identical full idle pixels,
reviewed `DCT3 LAB` text, and post-load decoded Menu/Messages presentation.
Missing protocol logs are a failure; use the runner's verbose logging.
This is research idle/UI restoration under the declared base-record PMM
comparison, not active-call/SMS replay, factory provisioning or native DSP
completion. No handset state or message is injected.

`call-state` uses the same exact-state/replay harness after physically dialing
`1234567` and Send. It saves at 34 seconds, after Connect Acknowledge and
before any Disconnect, then requires the same nonempty one-second protocol
replay and identical active-call pixels. After load, physical End completes
the own NSM-3 CC/RR release grammar and returns to reviewed `DCT3 LAB` idle.
The physical composer schedules no End while this restoration fixture owns
the continuation. Check with `noki8210_state_check.py LOG SNAP --call`;
ordinary outgoing-call and idle-state fixtures remain independent. This
verifies active signaling/UI restoration, not native speech or delivered-SMS
restoration.

`sms-state` delivers the laboratory `hello` once, stores it and closes
CP/RP/RR before saving at 21 seconds. It requires exact architectural
restoration, identical saved-screen pixels and nonempty one-second ordered
radio/SIM replay; only after load does the shared physical Read fixture
open the message. Acceptance requires exact persistent read-status content,
reviewed `hello` body pixels, exactly one page and exactly the delivery plus
read-status writes. Redelivery or rewriting the message cannot repair a
restore. Run `run_noki8210_acceptance.py RUN --scenario sms-state`, or check
`noki8210_state_check.py LOG SNAP --sms --storage SIM_NVRAM`. This completes
the independently executed idle, active-call and delivered-message
restoration set for the research composition; it does not prove save/load
within an unfinished SMS transaction or native DSP runtime.

`verify-8210-host-incoming-call` uses the shared configured GSM900 host
fixture, not automatic incoming calls. The physical startup
fixture produces registered idle at 32 seconds; only then does
`run_host_incoming_signaling_gate.py` queue caller `5551234`. Physical Send
and End complete the own NSM-3 CC/RR lifecycle. The runner requires matching
epoch/request identity and all five host phases; handset acceptance also
requires reviewed caller and post-release `DCT3 LAB` pixels. ARFCN4 SCH,
candidate/release parameters, host registration and persistent EF_LOCI are
checked alongside own native uploads, explicit HLE handoff and self-test. The
exact configured-cell traffic words are
`041202000271012fc10000040000000400000000`, with teardown
`041202001117001a600000040000001400000001`. Default-cell call comparisons
retain their separate words; the two variants cannot satisfy each other's
matchers. Run from a new
directory with `run_noki8210_acceptance.py RUN --scenario host-incoming-call`
(`--port` selects the private HTTP endpoint). The manifest records both MAME
and host runner commands. This proves external host signaling through the
research composition, not native speech or external-network calls.

`verify-8210-host-outgoing-call` physically dials `1234567` and Send, then
requires one correlated host request, rejection of wrong-ID and duplicate
decisions, one accepted Connect, firmware CC/RR assignment and Connect
Acknowledge, and physical End followed by complete release. It uses the
same exact configured-cell traffic grammar and mandatory own-stage,
registration and persistent-SIM checks. Reviewed dialed-number, Call 1 and
post-release operator regions prove presentation; the connected/idle crops
agree with the independent own-product state oracles. Neither host-call
gate exercises media payloads or proves native speech.

`verify-8210-sip-cancel` separately proves a real local PJSIP INVITE followed
by caller CANCEL while alerting, 487, exactly one correlated host termination
and complete firmware CC/RR release with zero accepted media. Physical
security entry settles registered idle before admitting the call. The own
SETUP carries caller `5551234`; physical Names/C decodes `1a` and dismisses
the reviewed `1 missed call` screen. The final idle frame displays numeric
`001 01`, not the initial `DCT3 LAB`; both are separately reviewed, with no
forced network-name repaint and no established cause for that presentation
difference.

This runner pins the acquired MCU and PMM and uses the same explicitly
labelled base-record comparison, not reconstructed factory data or donor
provisioning. Its host fixture independently selects a GSM900 laboratory
cell pair 4/5. Acceptance requires ARFCN4 SCH, recovered `041202` candidate
and release parameters, host registration on 4 and persistent EF_LOCI.
Legacy default-cell gates tune MCU carrier 4 while their default network
reports 1; they establish their documented signaling contracts but not this
stronger carrier-coherence claim. This gate does not promote those gates,
factory PMM validation, native resident DSP execution or speech/media.

`verify-8210-sip-idle-restore` saves settled registered idle at 32 s and
requires exact PC/SP/mapped-RAM checksum/time restoration, identical full
idle pixels and nonempty ordered one-second radio/SIM replay. Only after
that replay completes does the fixture publish a fresh readiness frame at
35 s and admit one external SIP call under host epoch 2. The same coherent
ARFCN4, CANCEL/487, zero-media, CC/RR, SIM-location and physical Exit checks
remain mandatory. No external dialog is active at save or restored, and no
SIP request may precede the successful idle restore. The ordinary idle gate
retains its independent physical Menu/Messages continuation. Provisioning
and native-speech limitations remain unchanged.

The four `verify-8210-host-*-sms` gates independently use fresh storage and
`fixtures/noki8210_host_gsm900/nsm3hle.cfg`, shared with the SIP fixtures.
It enables the host adapter and an external GSM900 cell pair 4/5, not an
automatic call/SMS scenario. Every outcome requires ARFCN4 SCH, recovered
candidate/release parameters, host registration on 4 and persistent EF_LOCI
and the own native uploads/explicit HLE handoff/self-test in addition to
its message-specific checks. Default-cell comparison gates
and factory-provisioning limitations are not promoted.
The incoming host supplies
GSM-7 `hello` from `5551234`; acceptance requires queued/delivered request
identity, handset CP/RP/RR closure, exactly one page, delivery/read-status
SIM writes, persistent text and reviewed physical Read pixels. Outgoing
physical Write/Send submits exact GSM-7 `A` to `5551234`; the host accepts
the correlated request once while wrong-ID and duplicate decisions are
rejected, and handset acknowledgement/release returns to paging. Reviewed
`Message sent` text is required independently of the animated envelope. Each
runner checks both the host protocol and the own-product handset contract.
These tests establish external software SMS transport, not external carrier
delivery or native DSP execution. `--port` and a new run directory isolate
each case; the manifest retains the exact host command.

`host-rejected-sms` selects explicit RP-ERROR: the handset closes CP/RP/RR,
shows **Message not sent this time**, then physically decoded End/Menu
reopen Messages. `host-silent-sms` supplies no RP result and observes for
125 seconds. At about 104.5 seconds the handset sends main-link DISC;
UA/channel deconfiguration ends the correlated host request and resumes
paging. Its transient error is **Message sending failed**, not an RP result.
Only afterwards do physical End/Menu test recovery. Both outcomes require
their distinct reviewed English error text and Messages title, and reuse
the evidenced 8850 protocol/presentation checker with NSM-3's own key-trace
format. Silence fails if any RP-ACK/RP-ERROR appears. These are failure and
recovery tests, not delivery; no handset timeout or release is injected.

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

## Research-HLE DCS1800 registration

`verify-8210-dcs-registration` selects external laboratory carriers 823/824
through ordinary network configuration, retaining the acquired NSM-3 MCU
and declared own base-record PMM comparison. No firmware or peer change is
needed. The own scan publishes `56:03370338`, selects SCH on carrier `0337`,
uses recovered `041202` channel parameters and capability byte `30` in its
Location Updating request (GSM900 uses `33`). Acceptance requires the ordered
Location Updating/release sequence, carrier-823 paging and updated EF_LOCI.
Media and native DSP on this band are not promoted.
`verify-8210-dcs-idle-state` extends the no-PIN DCS profile through an
idle save/load round trip. `run_8210_dcs_idle_state_01` passes explicit
DCS registration and persisted EF_LOCI, exact saved/restored PC/SP/RAM
and emulated time, transport replay, matching registered-screen pixels,
and post-load physical Menu decode plus reviewed menu presentation.
The runner rejects DCS PIN idle-state and unsupported DCS service scenarios
pending their own evidence; this gate does not weaken the late-PIN boundary.
`verify-8210-dcs-incoming-sms` composes the same topology with the external
incoming-SMS scenario without discarding either configuration. Run
`run_8210_dcs_incoming_sms_01` passes DCS registration and EF_LOCI,
ordered paging/CP/RP/LAPDm closure, delivery and read-status writes to
EF_SMS, persistent read `hello` content, and physical reading with reviewed
body pixels. No-PIN coverage only; call coverage has separate gates below.
`verify-8210-dcs-sms-state` uses that same external SMS/band composition
and saves after delivery and release. `run_8210_dcs_sms_state_01` passes
exact architecture, transport and pixel replay, persistent read content,
and physical message reading after restoration, with DCS registration
verified separately. This remains no-PIN research-HLE coverage.
`verify-8210-dcs-outgoing-sms` validates physical `A`/`5551234` entry,
exact SMS-SUBMIT, laboratory acceptance, network CP/RP acknowledgement,
release/resumed paging and reviewed Message-sent pixels. Run
`run_8210_dcs_outgoing_sms_01` also passes explicit DCS registration and
persisted EF_LOCI. This is no-PIN laboratory signaling, not external
SMS delivery or native DSP validation.
`verify-8210-dcs-outgoing-call` independently checks registration, physical
`1234567` entry/Send/End, CC setup/connect/disconnect/release and reviewed
dialed/active/released screens. Capture `run_8210_dcs_outgoing_call_contract_01`
established traffic word `041202000271012fc10003370000000400000000` and
release word `041202001117001a600003370000001400000001`; fresh run
`run_8210_dcs_outgoing_call_01` passes those exact contracts. GSM and DCS
patterns reject each other's carrier words. No peer behavior changed;
this is no-PIN laboratory signaling, not native speech or an external call.
`verify-8210-dcs-call-state` saves inside the connected call and verifies
exact architectural, protocol and pixel replay before physically ending
the restored call. `run_8210_dcs_call_state_01` passes the DCS channel
contract and separate DCS registration/EF_LOCI check. No-PIN signaling
restoration only; no media/native-speech claim follows from this gate.
`verify-8210-dcs-incoming-call` validates paging, caller presentation,
physical Answer/End, exact DCS traffic/release words, CC/RR closure and
returned registered-idle pixels. `run_8210_dcs_incoming_call_02` passes.
The caller crop `(40,8,80,16)` contains the reviewed `5551234` text and
is byte-identical to fresh GSM control `run_8210_gsm_incoming_call_pixels_01`;
the retired wider crop included changing signal/battery/animation pixels
and failed for both bands. Blank-caller negative tests remain enforced.
This gate proves no-PIN laboratory signaling only. DCS host services and
early-PIN acceptance have separate gates above; late-PIN recovery, native
speech and external end-to-end speech calls remain unproved.
`verify-8210-dcs-phonebook` creates contact `A/123` through physical
inputs, checks initial DCS registration/EF_LOCI, cold-boots preserved
storage with the same own base-record comparison and carrier-823
configuration, then physically reads the contact. Run
`run_8210_dcs_phonebook_01` passes record persistence and reviewed
contact pixels. This does not add PIN-timing or host-backend coverage.

**Current frontier:** DCS with physical PIN entry starting at eight seconds
does not register; entry starting at seven seconds does. The passing
no-PIN DCS control also lacks `07f0`, `1587` and type 57: none is a universal
DCS prerequisite. It receives `03ec` before serving-channel completion;
PIN DCS receives it afterwards. At completion, the passing control has
`03ec` queued alongside current `03eb`; PIN DCS has no queued replacement.
The later request is retained, but its argument-3 selector returns 4 and
returns to the receive loop. Successful early PIN enters receive state 15
before the acknowledgement; default late PIN is already in state 13.
State 13 can reevaluate the queue on a subsequent `8b` measurement via
the argument-0 selector, but ordinary `03f9` neighbour notifications do
not do so. The alternate `0413` route requires timer `81`, whose descriptor
never changes after initialization in the failing run; its setup owners
are separate receive states 23/24. The concrete open contract is the
ordinary post-acquisition measurement/request-continuation lifetime.
The peer currently replaces one pending type-`56` measurement completion
on channel configuration, but its cancellation semantics are unvalidated.
Simply retaining that completion at the current cadence would deliver it
before late PIN readiness, not prove a late recovery. The own producer/
consumer anchors below are verified; they do not justify periodic `8b`
or acknowledgement replay. Do not make DCS reproduce GSM's reference
chain. No firmware state or peer response is forced.

The default-timing `--dcs1800 --pin-enabled` experiment remains unsuccessful:
the early `55:03050000` response reaches the correlated task-12 completion
at 5.54 seconds, before physical PIN acceptance at 12 seconds. Unlike the
verified GSM900 PIN lifecycle, no later type-57 request is emitted. The
passing no-PIN DCS control shows that request is not required on this band;
Location Updating acceptance is still missing and the strict runner rejects
the PIN run. Its measurement contract now expects the observed type 55,
with correlated delivery, physical PIN acceptance and Location Updating;
the late-request recovery lifecycle must be understood before
timing-independent authenticated DCS coverage can be claimed. The
explicit seven-second fixture is bounded authenticated coverage, not a
replacement default. No response, firmware state or timer is forced.
Continuous carrier-823 SI1/2/3/4 and paging packets remain present after PIN,
excluding a one-shot-broadcast explanation. Passive completion observation
finds the same context `00113264`, selector `03` and zero word at `+4` in
both GSM900 and DCS1800 runs. Both therefore select parser `28755e`, not
alternate `287664`; parser selection alone does not explain the divergence.
The selected-cell lifecycle spans SIM acceptance; the measurement envelope
and periodic SI transmission are present.

### Mapped GSM reference chain
Own firmware diagnostic strings narrow this further: successful GSM reaches
`PH_1300`, whereas authenticated DCS enters `PH_9000b2`. Both call decision
helper `2a1380`, whose halfword state is at `137f5a`. The GSM `PH_1250`
calls observe states 4 then 6; DCS observes state 5 and subsequently calls
the same helper with argument 3 from `21f5c8`. These are observed internal
states, not yet named protocol semantics. Decode the state-5/argument-3
contract and its `2a0eb8` dependency before adding any peer response.
The observed state-5/argument-3 rejection reaches `2a194c`: its measurement
outcome is `4`, which fails the first `outcome & ~2 == 0` predicate. The
record is `07 08 01 00`, and the two selected-cell links are nonzero/zero;
those later predicates are not reached. The outcome is written organically
by parser `28755e` at `287640` into the object referenced by `13722c`.
GSM also receives outcome `4`, so it is not a DCS-specific failure code.
The post-rejection `21b8ea -> 21bb8c` path repeatedly enters `PH_9000`
while broadcasts arrive: the task remains live, not blocked inside the
decision helper. Compare the lifecycle that establishes the selected-cell
record/links before treating the outcome as a missing peer reply.
The link roots have six/four aligned literal references respectively. Runtime
writes identify `287470` scan-link creation, `287218` scan-link clearing and
`2873a4` selected-link publication. The working GSM path additionally uses
`28725c` at measurement completion; these are MCU-owned transitions.

The decisive post-PIN inputs differ: DCS receives `03ec`, while GSM receives
`03ed`. Both are constructed by `2aede0`, called from `258afc`, and posted to
task 12 by `2af11a`. The producer's status cascade maps `07d4 -> 03ec` and
`07da -> 03ed`; enumerate the producers of those status values next. The raw
byte at message `+3` differs too, but this constructor does not demonstrably
initialize it as a class, so its observed `c7`/`3b` bytes have no assigned
semantics. Do not substitute `03ed` or alter the selected-cell links.

The own-ROM status mapper at `209b90` provides a concrete upstream candidate:
its subtract cascade starts at `09c6`, maps `09c8 -> 07d4` and
`09cc -> 07da`, and publishes through `20925c` with argument zero.
The cascade arithmetic, branch targets and aligned pool literals are checked
by `noki8210_radio_contract.py`. This is a static mapping, not proof that this
routine supplies the statuses in the paired runs: other literal references
exist. The next observation should establish whether `209b90` receives those
inputs, its callers, and the producer of the differing input before assigning
protocol semantics or changing peer behavior.
A linear Thumb decode finds direct `bl 209b90` candidates at `20cc9a`,
`210fd2` and `211048`. These are investigation anchors, not an exhaustive
producer census: linear decoding can include data and misses indirect calls.
Paired passive runtime observation confirms `210fd2` is the selected caller:
DCS supplies `09c8` at 12.695703 seconds, followed by `03ec` construction
at 12.695866; GSM supplies `09cc` at 12.696790, followed by `03ed` at
12.696956. Both earlier supply `09c7` through the same caller at 3.181184.
The caller loads the input from `[sp,#4]`; its message dispatch and input
producer are the next boundary. GSM additionally invokes the mapper with
`09fc`/`09fa` through `211048`; those are observations, not assigned protocol
semantics. Evidence: `run_8210_pin_{dcs,gsm}_mapper_01`; GSM registration
passes, DCS is rejected by the unchanged strict acceptance checker.
The paired `run_8210_pin_{dcs,gsm}_upstream_constructor_01` runs identify
the actual input producers. Constructor `2252cc` receives DCS `09c8` from
`22923e` and GSM `09cc` from `2280c8`; its common `225366` mailbox send
targets task 15. These distinct producer branches precede the common mapper.
The working GSM branch tests context `+60`, byte `+19` and helper `2c2986`
before constructing `09cc`; the observed DCS branch constructs `09c8`
after clearing the object referenced by its `229222` pool load. Context
ownership and the entry conditions of these branches remain unresolved.
Trace those branch selectors next, without assigning radio/SIM semantics
from message numbers alone. GSM acceptance passes; DCS still fails.
Paired producer-context observations identify the first selector: at `22803e`,
context `136aec` byte `+11` is zero on DCS and one on GSM. Zero calls
`229224` through `228052`, emitting `09c8`; one branches to `228080` and
subsequently emits `09cc`. At that selector DCS also has byte `+19=0`,
word `+5c=0`, word `+60=4`; GSM has `+19=1`, `+5c=113594`, `+60=1`.
Both have `+0a=0` and `+30=10` (decimal). The successful branch clears
`+11` before constructing its message, so inspecting it only at the
constructor would hide this difference. The next question is the writer
and meaning of `136afd` (context `+11`), not a replacement radio reply.
Evidence: `run_8210_pin_dcs_readiness_context_02` and
`run_8210_pin_gsm_readiness_context_01`; strict GSM acceptance passes and
DCS remains unregistered. Direct-store candidates in the local subsystem
are `226ff6`, `22703a`, `22707a`, `227108`, `227126`, `227aca`, `22806e`,
`228086` and `2281d0`; this syntactic scan is not exhaustive writer coverage.
The paired `run_8210_pin_{dcs,gsm}_selector_writers_01` write watches find
the concrete successful setter: GSM writes one through `227126` at
10.795097 seconds, then clears it through `228086` at 12.694759. DCS
only has the initial zeroing writes, not a later set or clear in this run.
The setter loop calls input retrieval `225688` and requires `1587` before
storing one. Own-ROM checks pin this predicate and store; the watch reports
PC `227124`, the preceding instruction, not a different store site.
Paired `run_8210_pin_{dcs,gsm}_readiness_prerequisite_01` observations
confirm the predecessor: GSM posts `09fc` at 10.793668, maps it through
`209b90`, then posts `1587` at 10.794179 before the selector write.
DCS posts neither in this run. The raw send observation reports target
arguments 15 and 17 respectively; these are not subsystem names.
`run_8210_pin_gsm_prerequisite_constructor_01` identifies `258c6e` as the
original `09fc` constructor call. Its status cascade selects this input
for `07f0` (`7f << 4`) at `258c1a`; the other branches construct
`09f8`, `09f9` or `09fa`.
The `07f0` source is now observed: constructor `2a20b0` is called through
`2a21f4`, and its common send is `2a213a`. Dispatcher `2a21a4` reads the
stored input object at context `137f58 +8`; `03eb` selects `07f0`.
In paired `run_8210_pin_{dcs,gsm}_status_dispatch_01`, GSM calls this
dispatcher from `21bffa` at 10.793016 with stored `03eb`, state 5; DCS
does not call it. Both originally receive the same `03eb` request at
3.182437 in task-12 state 8. Therefore the original request is present in
DCS: the missing transition is the completion path reaching `21bffa`,
not construction of the request or the final PIN readiness selector.
GSM subsequently calls it again with `03ed`, state 0, at 12.896008.
The independent `run_8210_dcs_no_pin_completion_01` passes full registration
while following `03ec`, with no `07f0`, `1587` or type-57 request. Its
`03ec` arrives at 6.422718 seconds, before candidate/serving changes; in the
PIN run it arrives at 12.696057, after the serving acknowledgement.
`run_8210_{pin_dcs,dcs_no_pin}_pending_requests_01` observes the queue at
`2a0dc8`: both retain current `03eb`, but at the serving completion the
passing control has queued `03ec` while PIN DCS has zero at context `+0c`.
The helper returns 0 with no queued request, 1 for the first current request,
or 2 when promoting a replacement. That result controls the state-5 tail
through `2a1484`. Gate byte `13721e` is `05` in both runs, excluding a
different value of that gate as the explanation. The remaining question is
how the late `03ec` should settle the already-completed initial request.

### Physical-input ordering control

`run_8210_pin_dcs_early_valid_01` passes the strict PIN and full DCS
registration/release/paging/EF_LOCI checks with `--pin-start 7`. Its captured
screen shows the PIN editor before the first key; VERIFY returns `9000`.
At 12.262178 seconds, `2a0dc8` has queued `03ec`, and the publisher selects
it at 12.550787; Location Updating is accepted at 12.890599. The MCU,
PMM, card contents, radio peer and radio timing are unchanged. This proves
an authenticated DCS path, not timing-independent PIN support: the default
eight-second fixture remains failing and is not replaced by the favorable
control. A four-second attempt captured a blank pre-editor screen and
VERIFY `9804`; it is not evidence about registration recovery.

Reproduce the valid control with
`tools/run_noki8210_acceptance.py RUN --pin-enabled --dcs1800 --pin-start 7`.
The option affects physical input only, is restricted to PIN registration,
incoming-call, host call/SMS, phonebook or idle/call/SMS restoration fixtures,
and is recorded in successful run manifests. Next decode recovery when
`03ec` arrives after the serving acknowledgement, rather than adjust peer
latency or choose an earlier default input.

Early-PIN DCS also passes host-triggered incoming call signaling:
`tools/run_noki8210_acceptance.py RUN --scenario host-incoming-call --pin-enabled --pin-start 7 --dcs1800`.
`run_8210_early_pin_dcs_host_incoming_03` verifies PIN, own DCS registration,
persisted EF_LOCI, physical Answer/End and CC/RR release. The host waits for
the registered-idle capture before calling; a fixed automatic call can collide
with security-code input and is not an equivalent fixture. Host registration
validation selects the DCS823 contract rather than the GSM900 ARFCN4 contract.
Speech and late-PIN recovery remain unproved.
The corresponding standard target is
`make verify-8210-dcs-early-pin-host-incoming-call RUN_DIR=NEW_DIRECTORY`.
Outgoing DCS host signaling separately passes without PIN and with early
PIN entry: `run_8210_dcs_host_outgoing_call_01` and
`run_8210_early_pin_dcs_host_outgoing_call_01`. Both require physical
`1234567` dialing, reviewed number/connected/idle crops, own DCS traffic
and release parameters, correlated host Connect, physical End, registration
and persisted EF_LOCI. Use `verify-8210-dcs-host-outgoing-call` or
`verify-8210-dcs-early-pin-host-outgoing-call`; speech is not tested.
Early-PIN DCS idle, active-call and delivered-SMS restoration also pass
(`run_8210_early_pin_dcs_idle_state_02`, `run_8210_early_pin_dcs_call_state_01`,
`run_8210_early_pin_dcs_sms_state_01`). They require the correlated
measurement/VERIFY/registration sequence, exact CPU/RAM/time restoration,
ordered protocol replay, reviewed pixels and physical Menu/End/Read
continuation. Standard targets are `verify-8210-dcs-early-pin-state-idle`,
`verify-8210-dcs-early-pin-state-call` and
`verify-8210-dcs-early-pin-state-sms`. These do not establish late-PIN recovery.
Early-PIN DCS contact persistence separately passes a two-process Save and
cold readback (`run_8210_early_pin_dcs_phonebook_01`). Both processes require
own-ROM/base-record boot and authentication. The cold process also requires
own carrier-823 registration, unchanged persisted SIM bytes, the retained
EF_LOCI in its location-update request with DCS capability `30`, no invalidation
write, and reviewed contact pixels after physical Search/Detail.
Use `verify-8210-dcs-early-pin-phonebook` with a fresh `RUN_DIR`.
Paired `run_8210_pin_dcs_{late,early}_queue_result_01` confirms the late
request is not lost: `21f5aa` calls `2a1eaa`, which copies `03ec` into
context `+0c`. Both physical timings then return 4 from the argument-3
selector, retaining current `03eb` and queued `03ec`, and continue through
`21b8ea` rather than the result-3 path. The distinction is the next event:
early PIN still receives the serving acknowledgement, whose argument-0
selector returns 3 and promotes `03ec`; late PIN already consumed it with
result 0 and an empty queue. Replaying the acknowledgement is not a fix.
Determine the ordinary post-acknowledgement recovery event/continuation.
The result-4 continuation itself is passive: `21b8ea` logs
`PH_9000_ENTER`, branches directly to `21bb8c`, disposes the consumed
message through `288e44`, and receives again through `2886b0`. It does
not issue a radio request or arm a retry on this path. Therefore trace
subsequent receive events and their queued-request consumers; do not
interpret the PH9000 label alone as an active recovery operation.
One concrete subsequent-event candidate is decoded: the state-`0d`
dispatch entry at `21bbe4 + 13*4` points to `21ef30`. Input `1802` with
type `8b` takes `21ef44`, parses the measurement through `2a2250`, then
calls `21f900`, which invokes the argument-0 queue selector. This is the
same selector that promotes the early queued request. The late run's
subsequent traffic contains `83` and `03f9`, not a later `8b` completion.
The open transport question is whether an already-issued measurement
command requires such a later completion; the consumer alone does not
authorize unsolicited periodic `8b` packets.
The existing peer has a specific cancellation boundary to audit. The
updated type-`56` window at 5.542080 produces SCH (`80`) at 7.897080;
firmware then sends type `02` at 7.898223. With one window report remaining,
`handle_acquisition_packet` replaces the candidate-measurement phase and
pending `8b` with the candidate-channel-change `8f`/`89` transaction.
There is no late type-`57` request in the failing run. Thus the missing
completion is not an unanswered late request: establish whether type `02`
legitimately cancels the earlier window's terminal measurement or whether
that measurement remains independently owed. Current passing early-PIN
and no-PIN runs do not decide this contract, and must not be used as proof
that cancellation is correct.
Reference scope: the recovered NHM-5 trace-name catalogue in the sibling
project (`tools/symbols/trace-names-nhm5.txt`) labels `1855`, `1856`, and
`1857` as `INVALID_MDI_MSG`. Its useful names for `188b` and `1802` do not
specify the ROM6 type-`55`/`56`/`57` command lifetimes. Do not import a
cancellation or periodic-measurement rule from that catalogue. Resolve
the own-ROM request/control lifecycle or obtain a matching ROM6 trace.
Own type-`56` construction is anchored at `2b2fe0`: it allocates `a4`
bytes, sets payload length `a0`, message halfword 2 and type byte `56`,
and initializes all 160 payload bytes to `ff`. Producer `286c48` calls
this constructor, selects list population through `2869bc` or `286bc0`,
then sends via `2b300e` to task 3. These are ROM-checked producer anchors,
not a proof of measurement lifetime or cancellation semantics. Follow
their caller's state/control transitions to settle that remaining question.
Read-only caller-return taps in
`run_8210_pin_dcs_candidate_owner_02` identify `21f390` as the initial
publication (return `21f394`, 3.183910), and `21faaa` as the updated DCS
publication (return `21faae`, 5.542089). The latter's receive loop tests
`8b` at `21fb78`, parses it at `21fb86`, and invokes the argument-0
selector at `21fba2`. Result zero branches back to `21fa68` to publish
another list. This establishes an own-firmware consumer/retry contract
for that window, but not whether channel configuration cancels it.
The initial function-entry tap produced no observations despite the
transport publications; caller-return taps replace it. No absence claim
is based on the silent entry tap. The normal PIN run still fails the
strict registration gate with the same empty-queue acknowledgement and
later retained `03ec`.
Own scan-control constructor `2b2db2` creates type `55` with four payload
bytes and sends through task 3. Its first two bytes come from helper
`2b2d52`: the first preserves selector 1..5; the second is indexed by a
separate option in a selector-specific ROM table. Selector 3 uses the
three-byte table `05 03 03` at `33e8a9`, so option 0 produces the observed
`03 05`, while options 1/2 produce `03 03`. Direct caller candidates are
`21f3d8` and `21fa52`. This mapping does not establish continuous versus
one-shot measurement or an implicit cancellation command; those meanings
remain unassigned pending lifecycle evidence.
Both type-`55` callers select option 0 when the request object's byte
`+9` is zero, otherwise option 2 (`21f3c4` and `21fa3a`). Type-`57`
constructor `2b31a8` uses the same helper `2b2d52` at `2b31d6`; thus the
option byte cannot by itself distinguish acquisition from background
measurement. No explicit stop packet occurs between the updated list
and channel configuration in the captured late-PIN run. Neither that
absence nor the shared encoding proves that the DSP must keep measuring.
Cancellation is not yet a direct late-PIN remedy: under the current peer
cadence, the pending terminal follows the 7.897080 SCH on the next poll,
before the 12.696057 readiness request. Retaining it would not itself
supply a post-PIN event. An indirect cell-state effect remains possible
and must be shown rather than assumed. A full-image aligned direct-BL
candidate scan (950,270 offsets) finds only two calls to parser `2a2250`,
at `21ef4c` and `21fb86`; both decoded consumers are above. This is not
an indirect-call absence proof. The channel-acknowledgement loop forwards
other events through `21bdc4`/`21f48a`, whose special `8b` branch at
`21f504` leads to a flag setter (`21bb54`), not either parser call.
That generic branch sets byte `137db0` to one. Its byte-zero consumer
at `21d952` passes the value to `2865dc`, which uses outcome root
`13722c` and, only when enabled, clears outcome 1 or 2 to zero. It leaves
outcome 4 unchanged and publishes no event. The read-only write-watch in
`run_8210_pin_dcs_measurement_flag_01` observes zero during RAM initialization
and one at 7.908521 and 12.261757 (writer PC `2eafbc`, channel-acknowledgement
times). The late request still returns 4. Thus a missing set of this byte
is not the late-PIN defect. The address is shared by other offset-bearing
consumers; it is not assigned a general subsystem name from this one use.
Outcome 4 is not an empty-candidate diagnosis in this run. Read-only
`run_8210_pin_dcs_outcome_terminal_01` observes terminal branch `287638`
with object `11324c`, halfword counts at `+4/+6/+8` equal to `0,2,0`,
skip flag `sl=0`, and final signed RSSI `fp=-127` (`ffffff81`). The
parser's `sample + 104 < 0` test selects this branch; with no skipped
entries it stores 4 at `287640`. With a skipped entry the same terminal
branch stores 3. A separate route can also select 4, so do not generalize
the enum name beyond this decoded branch. Here two entries were counted
before the sentinel: the persistent outcome reflects this completed scan,
not absence of radio candidates. Late readiness must be explained through
the subsequent lifecycle rather than by synthesizing a stronger RSSI or
changing the outcome value.
The record storage is seven rows, not a three-row table. Accessor
`286218` indexes `137248 + 9*index`, booleanizes its nine bytes, and
derives returned byte `+9` as true exactly when source byte `+8` is
nonzero. Helper `2a0eb8` examines only rows 0..2. The complementary
helper `2a1170` examines rows 3..6; selector state 2 selects it when the
**current** request is `03ec` (`2a15b2 -> 2a168e -> 2a1694`). These
are structural row indices, not assigned band/subsystem names.
Read-only `run_8210_pin_dcs_record_banks_01` observes identical rows at
the serving acknowledgement and late readiness result: rows 0..4 are
zero, row 5 is `000000000000000001`, and row 6 is
`010000000100000000`. Thus the first-bank accessor sees no flagged
record while a complementary row does satisfy its derived byte-9 test.
The late call still has current `03eb`, queued `03ec`, and selector state
5; it does not take state 2's current-`03ec` selection. Explain the
firmware-owned promotion/continuation that changes request ownership;
do not strengthen signal levels or rewrite either record bank.
The full-image aligned direct-BL scan finds four candidate calls to queue
promoter `2a0dc8`: `2a13d0`, `2a1484`, `2a193a`, `2a1988`. The last
belongs to selector argument 2 (`2a187a -> 2a197c`), unlike argument 3's
outcome/record predicates. Its sole direct selector caller is `21d886`.
This is not an unconditional recovery entrance: `21fdd8` selects input
`0413` through `220198`, requires timer-`81` remaining-duration query
`287b38` to be nonzero,
and proceeds only with nonzero context `138140` or stored request `03ed`
at root `137f64`. It then enters `21d84a` unless controller byte
`137968+8` is `1a`. Own diagnostic labels are `PH_2000a2`, `PH_2500`,
and `PH_2500a2`. The failing cell-message trace does not observe input
`0413`; that is not a static producer-absence proof. Trace the event's
producer and timer/context lifecycle before treating it as a late-`03ec`
recovery contract.
The timer query returns a duration halfword, not a boolean armed flag.
Its descriptor array starts at `11174c` with 12-byte entries, placing
timer `81` at `111d58`. Descriptor state byte `+8` selects the queued
calculation; the other branch reads duration halfword `+4`. The queued
calculation accumulates timer deltas and subtracts elapsed ticks with a
zero floor before returning the halfword. The static contract checker
pins the descriptor roots, stride, state/duration reads and return.
An expired timer and an absent event are therefore separate questions;
neither justifies injecting `0413`.
Two explicit setup candidates are `21bca6` and `21bcf6`; both pass
timer `81` and duration `075a` from pool `21c018`. The first only starts
it when the preceding remaining-duration query returns zero. Adjacent
`2d5ae8` calls format diagnostic output; they are not event producers.
In `run_8210_pin_dcs_recovery_timer_02`, a direct write-watch covering
all 12 descriptor bytes sees cold clearing at 0.004714s and initialization
at 1.117176..1.117178s, but no later descriptor writes. Initialization
sets duration zero, byte `+6=0c`, byte `+7=03`, state byte `+8=01`,
and timer id halfword `+a=0081`. Neither setup return tap is observed.
The physical-PIN DCS registration check still fails as expected. This
does not close indirect setup/event producers, but it establishes that
the observed boot never changes this descriptor after initialization.
Find the lifecycle selecting the setup sites before treating timer
expiry as a missing late-request recovery event.
The receive-state table at `21bbe4` selects state 23 at `21e93a`
and state 24 at `21e786`. State 23 calls the conditional timer setup
helper `21bc88` at `21e94a`, only after its message-word comparison
and class-byte `89` test succeed. State 24 calls the unconditional
helper `21bcec` at `21e906`, after four comparisons of stored fields
against extracted incoming bit fields (`21e8d2..21e904`). These are
state-owned continuations, not generic effects of every type-`89`
acknowledgement. The aligned direct-call scan found these two helper
callers; indirect entrances remain unclosed. Next establish the
ordinary transitions selecting states 23/24 and the compared fields,
rather than replaying `89` into the existing state.
The receive-state context is `138038` (pool `21bbd4`), with its state
halfword at `13803a`; helper `21bdc4` stores its argument there before
entering `21f48a`. Direct write-watch run
`run_8210_pin_dcs_receive_state_01` observes cold initialization to
state 8, then `8 -> 11` at 7.903565s (caller `21ee0f`),
`11 -> 17` at 8.408569s (caller `21ef61`), and `17 -> 13`
at 12.553509s (caller `21ee3d`). All later writes retain state 13
through the 46-second run, including the late readiness input.
No state-23/24 write occurs in this observed lifecycle. This bounds
the timer route without closing static/data-driven entrances: the
current failure is in state 13's request continuation, not an observed
state-23/24 timer transaction waiting for its reply.
The paired control `run_8210_pin_dcs_receive_state_early_01`
(`--pin-start 7`) passes the full registration/release/paging and
persisted-EF_LOCI check with the same passive taps. It shares the
initial `8 -> 11 -> 17` chronology, then enters state 15 at
11.696859s (caller `21ef5b`) before the serving acknowledgement.
That acknowledgement returns selector result 3 at 12.262718s with
current `03ec` and an empty queue; subsequent receive states are
`15 -> 2` at 12.557164s and `2 -> 7` at 12.888496s.
State 15's handler `21f8e2` checks input `1802` and class byte `89`
before entering `21f900`'s argument-zero selector. This is the
observed successful promotion owner, distinct from timer states
23/24. The standard late-PIN run reaches state 13 before readiness,
and never observes this state-15 acknowledgement pairing. The
control does not establish timing-independent PIN coverage or
justify delaying/replaying the radio acknowledgement.
Ordinary neighbour notifications `03f9` do not retry this queue in
receive state 13. Its non-`1802/8b` route reaches `21ee30`, where
only `0411` selects the special branch; other inputs re-store state
13 through `21bdc4` and enter shared forwarding `21f48a`.
That cascade has no `03f9` case and reaches default handler `21b954`.
The default handler's selectors are `03fd`, `03fc` (constructed),
`03fb`, `03fa`, `0422`, `041d`, and `0421`; unmatched `03f9` reaches
`21b9ee -> 21b8ea`, the receive-loop tail. This explains the repeated
state-13 writes accompanying observed neighbour notifications without
queue promotion. It does not establish what a real DSP must publish:
the remaining request-promotion candidate in this observed state is
the separate `1802/8b -> 2a2250 -> 21f900` measurement continuation,
whose post-acquisition production/lifetime contract is still unproved.
Do not set that selector or inject `1587` to obtain authenticated DCS coverage.

The bounded request-lifetime watches in `noki8210_security_input.lua` cover
root `137f58..137f67` and the initially selected heap context `113264..11326b`.
Paired current-build runs `run_8210_late_pin_lifetime_02` and
`run_8210_early_pin_lifetime_02` reproduce failure at eight-second PIN entry
and full registration at seven seconds, respectively. Both initialize selector
`03` at `2a1e78` and alternate pointer zero at `2a1e7c`, at 3.181635s.
In the successful run, queued pointer `1135f4` arrives at 11.696917s;
`2a0df4/2a0df8` promote it to current and clear the queue at 12.262181s,
before ordinary teardown clears the context roots at 12.557286s. In the
late run, the same queue pointer arrives at 12.696127s, after the serving
acknowledgement; no subsequent promotion store occurs in the 46-second window.
These observations support an unclosed request-lifetime/continuation contract,
not selector corruption or an unanswered late type-57 request. The next
static target is the promotion call at `2a1484` and its event ownership
(`2a1489` is its Thumb return address, not a separate entrance).
The initially watched heap address is recycled after successful teardown:
later allocator and packet writes there are not measurement-context changes.
Do not infer permanent object ownership from a fixed-address watch.
The own selector table at `2a139c` maps states 4/5/6 to
`2a1468/2a1456/2a1444`. Each requires argument zero to reach shared
`2a1474`; byte `13721e` must be nonzero before `2a1484` calls the
current/queued helper `2a0dc8`. State 5 with argument 3 instead branches
through `2a1464 -> 2a1872` and the previously decoded rejection checks.
Thus late readiness itself is not a queue-promotion trigger. These branches,
table entries, pool pointer and call boundary are checked by
`noki8210_radio_contract.py`. The unresolved producer question is which
ordinary post-acquisition event invokes argument zero after late readiness;
the known state-13 measurement continuation is eligible, but its production
and request lifetime remain unproved. No repeat measurement is justified by
the existence of this consumer alone.
The aligned direct-BL census over the complete acquired image finds selector
sites `21d886`, `21f356`, `21f5ca`, `21f90a`, `21fba2`, `2a1e28`,
`2a1e62`, `2a224a`, and wrapper site `21ef50`. The three setup/teardown
sites in the `2a` region use argument 1, while `21d886` uses 2 and
`21f5ca` uses 3. Direct argument-zero sites are `21f356`, `21f90a`
and `21fba2`; the latter follows the candidate-window measurement parser.
This is direct-call candidate coverage, not exhaustive event ownership:
receive state 10 additionally reaches wrapper `21f900` by tail branch
`21f2c0`, after its message comparison and class-`89` predicate. The observed
late lifecycle remains state 13, so that state-10 entrance is not established
as its recovery. Tail/data-driven/indirect entrances require separate closure;
a missing BL cannot prove absence of a producer. The next decode is the
shared state-13 forwarding route and ordinary events selecting the alternate
argument-zero owner at `21f356`, rather than replaying acknowledgement packets.
That alternate owner is now bounded to receive state 21: table entry
`21bbe4 + 21*4` selects `21fc40`, which treats `03ed` specially and otherwise
calls `21f2ec`. Its literal comparisons select `03eb`, `03ec` (`fb<<2`)
and `03ea` for `21f314`; after the selected-object predicate, `21f348`
queues the incoming request and `21f354/21f356` invoke argument zero.
State 13's shared forwarding selects `03ec` through `21f542/21f54c`,
then `21f552 -> 21f5aa`: it queues the request but evaluates argument 3
at `21f5c8`, not this alternate path. Thus the argument-zero readiness owner
exists, but is not selected by the observed late-PIN receive state.
The next unresolved contract is the ordinary transition into state 21
or a post-readiness state-13 measurement continuation; neither can be replaced
by changing the callback/state selector in the emulator.
An aligned direct-BL scan finds 19 candidates calling state setter `21bdc4`.
Only one is immediately preceded by literal `movs r0,#21`:
`21f30a -> 21f30c`, the unmatched-input tail of this same alternate handler.
This is not an independent boot entrance. The scan does not cover computed
arguments, tail branches, direct stores or indirect calls, and therefore does
not prove state 21 unreachable. Resolve those entrance classes before claiming
complete state ownership.
The complete aligned direct-BL candidate scan also finds only `21fc4a`
calling the alternate handler `21f2ec`, from receive state 21 itself;
there are no four-byte-aligned ARM/Thumb literal pointer candidates to that
handler. Complete aligned 16-bit unconditional and conditional branch
candidate scans also find no target of `21f2ec`. Its `03eb/03ec/03ea` comparisons and unmatched-input state-21
assignment are machine-checked. Thus this route has no independently
identified direct or short-branch entrance from state 13. Computed calls,
aliases and other instruction modes remain outside this negative result; it is not a total
producer-absence proof or justification for forcing state 21.
All 19 direct setter candidates have an immediate literal predecessor;
the argument sequence in address order is
`7,5,26,4,6,2,23,24,25,11,13,15,17,12,9,10,21,28,20`, now checked
against the image. No aligned 16-bit unconditional branch or aligned literal
pointer (ARM/Thumb forms) targets `21bdc4`. These negative scans do not close
direct stores: an inline `movs r0,#1` at `21bdc2` shares the same store, and
raw store-opcode scans also match embedded data. Context aliases `138038`
have literal loads at `21b8b8` and `21f484`; direct-state ownership requires
boundary-aware control flow from these roots, not counting every apparent
`strh [r4,#2]` in the task interval as executable.
Boundary-aware branch decoding establishes additional inline state-store
continuations at `21f482`: `21f39e` supplies 22, `21f424` supplies 19,
`21f45c` supplies 14, `21f46a` supplies 16, and `21f480` supplies 18.
The shared setter's preceding `21bdc2` supplies state 1. These exact
instructions are machine-checked; none supplies 21. Runtime initialization
to state 8 occurs through generic copy code `2001a4`, not this setter.
This bounds the known direct/inline routes without proving arbitrary aliases
absent. In the failing run none of the alternate state routes is observed
after state 13; the strongest current completion boundary remains its
`1802/8b` measurement continuation and the unanswered lifetime of the
original scan, rather than an assumed missing transition to state 21.
State 13's special `0411` path is also an eligible argument-zero entrance.
At `21ee52..21ee5e` it reads the measurement outcome from the object rooted
at `13722c`, excluding outcomes 0/1/2. The observed outcome 4 satisfies
that first predicate. It then performs candidate bookkeeping through
`287272`, clears context byte `+2`, and calls `21fb8c` at `21ee84`,
which reaches the existing argument-zero selector at `21fba0`.
This does not establish that `0411` should arrive or that later predicates
will succeed. Producer ownership is unclosed: aligned four-byte `00000411`
values occur at ten image locations, including non-task-12 candidates
`275e90`, `2ff570`, `30a12c` and data `33f0c6`. Decode their consumers
and any constructed/data-driven producer before treating `0411` as a missing
peer event. Do not inject it as a recovery shortcut.
The `275e90` literal is a return producer, not a posting site:
decoder `275ac0` reads two message bytes, and its `275b42` branch returns
`0411`. Caller `275dc8` retains that result in `r5`; dispatcher `275c6c`
selects this call for internal input `03f7` (base `03ea`, subtract 5,
table slot 8). Passive input/return/consumer taps in
`run_8210_late_pin_special_status_03` observe none of these three boundaries
through 46 seconds; the registration check still fails. This is a bounded
runtime observation, not global producer absence.
The `03f7` literal at `306048` is used by constructor `305ec4`: if its
flag argument is not 1, message byte `+4` is not `b0`, the tracked byte
comparison succeeds, and message byte `+5` is not 1, `305ef4/305ef6`
replace the message ID with `03f7` and `305efc` posts it to task 24.
There are no aligned direct-BL candidates to `305ec4`; Thumb pointer
`305ec5` is a callback literal at `2e1150`, loaded by `2e0fae` and registered
through `2b98e4` at `2e0fb0`. It is not an indexed dispatch-table entry.
The registrar stores the callback at RAM root `12bab4`. Consumer
`2ba054..2ba078` loads it and invokes it with `r1` pointing to the packet,
`r0=packet[2]+4`, and `r2=packet[5]&1`. This is a transport callback; its
upstream packet family and forwarding ownership remain unclosed.
The other non-task-12 `0411` references
at `2ff402` and `30a086` are comparisons, not direct literal producers.
Resolve the registered transport-callback family before assigning GSM semantics
to `03f7` or synthesizing a peer message.
The upstream packet family is DSP RX **9c**: dispatcher `306fae` subtracts
`80`, then 3 at `306fb6`; after the `83..8f` table range, `306fbc`
subtracts `17` (selecting 9a), then `306fc2` subtracts 2 (selecting 9c).
The latter branch reaches `307008 -> 2ba010`, the sole aligned direct caller
of the registered packet worker. The dispatch arithmetic and worker call are
machine-checked. The current late-PIN trace has no type-9c publication.
This establishes `9c -> registered callback -> 03f7 -> decoder -> 0411`
as an eligible chain, not that ordinary DCS camping requires it or that it
has a particular GSM message meaning. Packet lifetime, payload meaning and
task-24 forwarding remain evidence requirements before implementing it.
The callback has a separate marker/data lifecycle. Its tracked tag is byte
`136caf` (root `136cac + 3`). A changed packet byte `+4` updates that tag,
posts the descriptor at `33ea20` (ID `03fa`) to task 24, and disposes the
original packet; a failed post restores the previous tag. A callback flag
of 1 (`packet[5]&1`), tag `b0`, or exact packet byte `+5 == 1` instead posts
the descriptor at `33ea14` (ID `03f8`) and disposes the packet. Only the
stable-tag, non-marker route writes ID `03f7` into the original packet and
forwards that packet. These predicates, marker IDs, tag root, rollback and
disposal instructions are checked against the acquired image by
`noki8210_radio_contract.py`. Thus the first packet of a changed-tag sequence
does not reach the `03f7` decoder as data. This bounds the forwarding grammar;
it does not name either marker's GSM meaning, authorize a `9c` publication,
or prove that this sequence supplies late-PIN recovery. The legitimate sender
remains to be identified before any peer implementation. The shared
consumer `275c6c` uses a ten-entry table at `275cdc`, with input origin
`03f1`: `03f7 -> 275da8`, `03f8 -> 275d7a`, `03f9 -> 275e82` (return),
and `03fa -> 275d04`. This origin is derived from the complete subtract
cascade, not from the table's location alone. The `03f8` arm calls `2b9a18`,
clears context bytes `+24/+25`, and sets `+26=1`; the `03fa` arm updates
tracked context and conditionally enters the same constructor. Constructor
`2b9a18` creates internal message halfword 2, payload length byte `+2`,
type `50` at `+3`, and copies its supplied context bytes after the header.
The table, flag mutations and constructor header are own-ROM checked.
Thus markers can have an outbound transaction effect; they are not
interchangeable with the forwarded data packet. No type-`50` or type-`9c`
publication is recorded in the retained 46-second late-PIN forwarding run.
That is a bounded observation, not proof of a universal request/response
pairing or justification for publishing either packet unsolicited. The
constructor's payload semantics and legitimate activation remain open.
The expanded passive run `run_8210_late_pin_status_forward_04` also watches
constructor `305ec4`, its `03f7` store at `305ef4`, and the six shared-decoder
returns `2ff4d4`, `303cb6`, `30511c`, `30951a`, `30a0d8`, `30a576`.
None is observed through the completed 46-second failing run. This lowers
the route's priority for the current lifecycle but does not prove its GSM
semantics or global irrelevance. Task numbers/aliases from the 3210 map must
not be imported to identify this NSM-3 task-24 callback. The primary remaining
boundary is still post-acquisition measurement/request lifetime; synthesizing
9c solely to reach an eligible software continuation is unsupported.

## Evidence needed for native DSP completion

The post-acquisition type-`11` instruction independently carries a full
16-bit carrier. Own constructor `2b304a..2b3052` serializes descriptor
halfword `+4` into message bytes `+0a/+0b` (payload `+6/+7`). The mode-4
DCS request `040000000000033800000000` therefore targets ARFCN 824, not
56. The product-selected peer decoder now preserves both bytes; other
products retain their own encoding. Static constructor checks and fresh
early/late PIN runs verify the correction. Early PIN still registers;
late PIN still fails, and the corrected request is rejected by the separate
neighbour-list eligibility predicate. This does not establish that mode 4
must populate that list or which reply it requires.

The original constructor bypasses timing/BSIC serialization for mode 4.
An aligned direct-BL scan identifies nine constructor candidates: six
ROM-descriptor sites (`21c320`, `21c96e`, `21cc1e`, `21d236`, `21d41a`,
`21de2e`) and three stack-descriptor sites (`286d04`, `286d42`, `286d88`).
This is direct-call candidate coverage, not closure of indirect/data-driven
entrances. Helper `286d4a` selects modes 4/2/3 from context bits 16..19
equal to 2/3/4 respectively; mode 4 takes carrier halfword `+6` only.
Passive runtime capture observes caller `21ef07`, argument zero, context
`1196a0`, carrier `0338`, control `ffc22664` at 12.262144s, then constructor
caller `286d8d` at 12.262148s. This precedes the late `03ec` queue arrival.
Unused stack bytes do not become packet timing or BSIC fields: the constructor
zeroes its allocated message before taking the mode-4 shortcut. This bounds
the observed packet's provenance but does not name it as an acknowledged
measurement, cancellation or recovery request. Decode the mode-4 DSP consumer
or an independent protocol trace before changing eligibility or supplying a
reply. The queued-request continuation remains the boot frontier.

The observed caller's own eligibility is narrower than the helper's full
mode selection: `21eee2..21ef06` invokes `286d4a(0)` only for context
selectors 2 or 3. Other selectors bypass this invocation. Consequently this
entrance can emit mode 4 (selector 2) or mode 2 (selector 3), not the helper's
mode-3 branch (selector 4). The hash-pinned radio contract verifier checks
both comparisons and the helper call. This is an MCU lifecycle distinction,
not a recovered DSP cancellation or acknowledgement contract.

Immediately before this selection, the caller runs received-block helper
`286e9a`, which calls updater `286e58`. That updater dereferences pointer
slot `137224`, whereas mode-selection helper `286d4a` dereferences `137228`.
The updater tests received-block payload error byte `+4`: zero selects the
record's control nibble 3; a nonzero error selects nibble 1 when its byte
`+b` counter remains nonzero. Neither branch directly selects nibble 2.
These are distinct pointer slots, not proof of distinct runtime objects.
Observe both pointers and their target control words at the failing call
before attributing its mode-4 selector to this receive update. The own-ROM
contract verifier pins the roots and update instructions.

Public-reference coverage is bounded. Gammu's
[NHM-5 v5.87 trace catalogue](https://github.com/gammu/gammu/blob/master/gammu/depend/nokia/dct3trac/nhm5_587.txt)
names `1811` as `NMEAS_INSTRUCTIONS`, but its
[passive MDI decoder](https://github.com/gammu/gammu/blob/master/gammu/depend/nokia/dct3trac/wmx.c)
hex-dumps this packet rather than interpreting modes. The catalogue's `48xx`
family distinguishes measurement-request discard, neighbour initialization,
synchronization selection and system-parameter selection. These labels are
cross-product search clues, not NSM-3 state identities or proof that mode 4
expects a reply. Neither this decoder nor the Osmocom decoder establishes
that missing contract. The remaining software route is to correlate these
lifecycle distinctions with the own-ROM mode-4 producer and its consumers;
do not promote an NHM-5 trace label into an NSM-3 semantic claim.

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
available in the acquired collection. Native UI, SIM, network, call/audio and
handset save/load acceptance remain unvalidated behind this boundary. The
separate research-HLE profile has the product-local acceptance listed above;
those results do not establish the missing native silicon publication.
