# Acquired 8xxx software frontiers

## Current boundary

The stock 8250 v5.02 PPM K, 8850 v5.31 PPM C and 8890 v12.20 PPM C
execute with their own acquired flash/PMM inputs. Normal machines remain
unpromoted. The separate 8850 research composition has graphical, physical
UI, registration, call-signaling and SMS acceptance below; those results do
not establish the 8250 or 8890 contracts. Input hashes and acquisition
provenance remain in `roms/README.md`.

The normal 8250 remains at its fail-closed verifier boundary. The separate
`nsm3dr6` research composition executes the MCU-uploaded verifier and two DSP
loaders using a product-flash bootstrap fragment. This is not a fitted-mask dump;
at the unavailable call it halts only the DSP for a silent-peer observation.
Graphical boot and phone functionality remain unproved.

The separate `nsm3dhle` diagnostic composition explicitly yields transport
ownership at that missing call. It completes parameter acknowledgements and
request-derived discovery, computes a candidate family-83 identity reply,
and returns transformed original PMM records through the real RX path.
The MCU rejects those records at `28d3c4 -> 28d56e`, before format selection;
the final self-test request remains unanswered and the LCD remains blank.
This composition is not normal 8250 support or proof of fitted ROM6 behavior.

The unresolved contracts are the original chip/provisioning pairing and
the missing ROM6 initialization/self-test/record-processing behavior.
ROM4 arithmetic, published codec tables and the acquired MCU consumers
constrain them but do not establish them. Useful new evidence would be a
compatible ROM6 mask image, an independently captured 8250 peer exchange,
or a verified original chip identity with its record preprocessing contract.
No default unlocked record, inferred success flag or donor profile is an
acceptable substitute. Software-only identity constraints remain explicitly
inconclusive; feature acceptance cannot begin until a coherent boot advances.

### 8250 missing-evidence boundary

Fresh stock and runtime-comparison runs reproduce the protected upload and
record-rejection contracts. Selector ownership, direct MCU input copying,
both codec directions and the actual PMM extent do not provide a correction
that preserves the acquired provisioning. Graphical/UI/SIM/phonebook/radio/
call/SMS acceptance for this product remains absent, not implied by 8850.

The public [codec author's protocol discussion](https://nokiafree.org/forums/archive/index.php/t-38381.html?s=905ef8616030d0920e8836236655ced0)
supplies family tables and an identity-dependent record model, not this
dump's original chip pairing. The independent emulator's
[model inventory](https://github.com/djr-747/nokia-dct3-emulator/blob/main/docs/MODELS.md)
reports 8250 v6.02 HLE success; its
[architecture description](https://github.com/djr-747/nokia-dct3-emulator)
explicitly includes identity provisioning. Neither is an original v5.02
PMM/physical-peer oracle. A bounded public search found no usable ROM6 mask
or matching physical record exchange; that is a search result, not proof
that none exists.

Resume on an independently verified original COBBA identity and matching
record transformation, a product-matched service/peer capture, or executable
ROM6 resident code. Generated open-lock data, another product's PMM, a
candidate satisfying only local byte constraints or a successful sibling
screen are not substitutes. The normal machine and research comparison
remain fail-closed; no new success or record verdict is installed.

## 8250 staged verifier

The stock descriptor at flash `0x3188c0` is
`0f00 0000 00df 0f00 00dc 0000`. Its 223-word program at `0x3188cc`
has SHA-1 `6646da3c5be9c70deda7e0b5b9f257d5d2ace815`, identical to
the independently extracted 8210/6210 program. A read-only live capture at
0.5 seconds reproduces every word at MCU `0x11e00` and these supplied fields:
DSP `087b..0881 = 0100 0300 0000 e800 0001 0001 0200`.
The final observation records 58 ownership transitions on each buffer,
strictly alternating, with shared results `0000 ffff` still unpublished.

`tools/nsm3d_verifier_observe.lua` and
`tools/nsm3d_bootstrap_trace_check.py` protect that boundary; the latter also
pins the stock flash before comparing the runtime program. Execute the Lua
observer with the same private-directory options as the scout, without
`-verbose`, and check its log against `roms/noki8250/8250-502mcuppmk.fls`.

The `nsm3dverify` core-only instrument executes those stock bytes with the
observed geometry and sparse input stream. Reproduce with:

```sh
.venv/bin/python tools/nsm3_verifier_check.py mame/mame \
  roms/noki8250/8250-502mcuppmk.fls run_8250_staged_core --product 8250
```

The boundary case consumes 116 blocks before requiring port `002d` at PC
`0f9f`. Existing COBBA-model comparisons select F/status/F and publish the
supplied register-F value in word 0, the selected PROM version in words 1/2,
and 6 in word 3. Stock-input fingerprint `f3a3625c` differs from NSM-3's
`c2e06006`. The ROM4 and alternate-F BIOSes are sensitivity fixtures only;
none identifies fitted NSM-3D silicon, and no fixture publication is injected
into `noki8250`.

Once shared offset 2 changes, `0x2cb316..0x2cb31e` copies shared word 1
to RAM `0x12f028` and word 0 to `0x12f026`, then returns without validating
them locally. A halfword-aligned literal Thumb-BL scan of the complete stock
flash finds one candidate call to `0x2cb226`, at `0x2f33d0`; its decoded
caller continues initialization after return. The direct literal reader of
`0x12f026`, at `0x2de1ca`, formats a three-character COBBA identifier; it is
not a verdict check. Indirect consumers through aliased structure pointers
are not closed by the scan. The remaining execution boundary is the second
loader's call to `2c75`, not an assumed result-validation gate.

### Live uploaded-code composition

`nsm3dr6` combines the existing MCU/HLE devices with a bounded C54x staged
executor. MCU-uploaded shared memory is its executable program; native code
owns buffer acknowledgements and result publication while active. HLE
bootstrap acknowledgements are suppressed during that ownership. No result
words, transfer-count completion or MCU state are manufactured.

The authentic service package `nsm3d_604.exe`, member `nsm-3d.ini`, names
`Rom6ImageFile=nsm3dx_6.040`. This supports a ROM6-family experiment, not a
fitted-mask identification for v5.02. The composition supplies immutable
PROM version 6 from the product-flash fragment, nominal COBBA register-F/status inputs, and a
declared 13 MHz execution clock. It retains the staged code's CTSI writes
to ports 0/2/0c/0e without fabricating timer interrupts; other ports and execution
outside the recovered program fail closed.

On a product-local cold run, native execution publishes `0000/0006/0006/0006`
at 0.996196 seconds. MCU `0x2cb328` has retained `0000/0006` after exactly
58 ordered ownership pairs per buffer. The next initialization clears shared
memory, loads descriptor `0x311d14` with fields
`fd00/ff80/027e/0500/0078/0000`, and releases DSP reset at `0x2cb4c0`
at 1.014471 seconds. Its loader executes the acquired bootstrap fragment,
then drives descriptor requests through the ordinary MAD2 IRQ4 path.

The descriptor's 638-word upload has SHA-1
`1250a9e17ce44ec8cc373f222a817f99f505bcdf`. A read-only capture reproduces
every word at MCU `0x10a00` (DSP DARAM `0d00..0f7d`); the final 126 words
are executable loader code at `0f00..0f7d`, SHA-1
`5bcd6f091b23730b2484eb844841361ef7a12889`. The earlier words contain a
branch table and padding, not a complete DSP ROM.

GNU tic54x disassembly independently decodes the prefix: preserve fingerprint
words `04f7/04f8`, clear work memory, then repeat `MVPD ff80,*AR2+` for
104 words into data `0780..07e7`. The needed source range is `ff80..ffe7`;
catalogue entry `0f` supplies those exact 104 words. Its descriptor is
`ff80/ff80/0068/0200/008c/0000`, payload at `0x31770c`, SHA-1
`440bf49f1eba4cadb12f7f7581c992b0025807d6`. This is a firmware-contained
bootstrap template, not proof of its identity to the physical mask. The
research composition maps the template explicitly and no longer synthesizes
the version word. The read-only observer verifies every mapped word.
Later code installs 422 uploaded branch-table words into program `0590..0735`
using `MVDP`, posts selectors `14` then `01` at `0871`, strobes bit 3 at
MMR `29`, and waits on `0872`. It copies input chunks from `087e+0800` to
destination `087b`, decrementing the remaining `087d` count, then branches
to `0a00` when done. These paths now execute natively: the first `14` request
is followed by 133 `01` chunk requests. The MCU supplies descriptor 14's
613-word loader2 at `0a00`, then descriptor 1's `0a5c` words in chunks of
`0014`, ending with 12 words. The second loader is byte-compared with its
product-flash payload before execution; SHA-1
`b1df4b301d67c6c4421ae346478c465a7fd20ff0`. At 1.023949 seconds it enters
`0a00`, initializes retained CTSI registers, and calls `2c75` at `0a40`.
That routine is outside the acquired code and remains fail-closed. No ROM4
instructions or guessed helper return values were imported.

The second loader's early direct calls also include `938c`, `4007` and
`9ddd`. None lies in any of the 28 initialized descriptors' declared
destination ranges. This is bounded upload coverage, not an absence proof
for relocation or an identification of the fitted mask. Reproduce the
catalogue, payload hashes and half-open range queries with:

```sh
.venv/bin/python -m tools.nsm3d_catalogue \
  roms/noki8250/8250-502mcuppmk.fls \
  --address 0x2c75 --address 0x938c --address 0x4007 --address 0x9ddd
```

The next software question is whether the loader relocates acquired code
to those addresses or calls resident mask routines. A public sibling HLE
boot claim for 8250 v6.02 is not an execution oracle for this v5.02 image;
its advertised model/version and provisioning must be matched before
using it to justify a peer response.

A same-input reference run with the stock v5.02 flash, its acquired virgin
PMM at `0x3d0000` and checksum repair disabled reaches a Security-code
frame under the sibling HLE. This is a software-path lead, not mask-ROM
execution evidence or proof that its responses match real NSM-3D hardware.

The bounded native run installs exactly 422 program words at `0590..0735`
before reaching `2c75`, with PMST `07ac` and zero in the research data
backing at `2c75`. Other program writes outside the known RAM and immutable
fragment are rejected, not silently discarded. This excludes a preceding
`MVDP` relocation to the call target in the executed stream; it does not
establish the complete MAD2 ROM6 overlay geometry. TI documents the effect
of OVLY as device-specific, so a generic C54x RAM map is not a substitute
for the fitted ASIC's map ([TI SPRU131G](https://www.ti.com/lit/pdf/spru131)).

The verifier at `0f0f` deliberately attempts `MVDP D0803,Pff87`, with
`D0803=6`, before reading the same resident cell. The explicit read-only
fragment rejects that write; its acquired word, not the attempted value,
supplies the result. This probe must not become a writable version latch.

The second loader also contains a ten-word transaction at `0c08`: it
copies input through `AR2`, calls unavailable helper `8513`, copies six
words XORed with `a5a5` to `04ef..04f4`, compares paired input words, writes
`abba` to `0906` and branches to unavailable `45ba`. The main loop at
`0aa7` waits for that cookie before branching to `2000`. Thus neither
`abba` nor a generic "upload complete" notification can be manufactured
as a replacement for the unrecovered transaction. Recover the MCU-side
request/response contract before choosing a declared HLE alternative.

The MCU handler at `0x2cb874` consumes the selector from shared byte offset
`e2`, indexes the relocated catalogue, copies at most the declared input
chunk length, and acknowledges through `e4`. The flash initialization record
at `0x309560` copies `0x74` bytes to RAM `0x12f040`: 28 descriptor pointers
and a null terminator. Its source table is `0x309568`; selectors 0, 1, 0f,
14 and 1b resolve respectively to `0x311d14`, `0x312220`, `0x317700`,
`0x31782c` and `0x3188c0`. This bounds the catalogue independently of a
literal-call scan. Selector 14 is not ROM4's loader2 selector 12.

Native execution exposed missing `BC BLEQ` opcode `f84f`; the core now
implements the signed 40-bit comparison with taken/not-taken cycle tests.
The expanded observer also roots its tap userdata, preventing garbage
collection from leaving an invalid callback during the larger upload capture.

`nsm3dr6` now suspends native execution at `2c75` and retains transport
ownership while the MCU continues. This is diagnostic isolation, not
execution of missing code or an emulated helper return. The MCU then issues
control command `32`, argument `3fff`, commit `1`, through the encoder at
`0x2cb518` and doorbell site `0x2cb838`. Pending word `0x100e0` stays `1`
through eight seconds; the native PC stays `2c75`. The first request's
semantic effect and legitimate completion are the next peer contract.
Do not advance it with a guessed acknowledgement.

The encoder's 53-entry command table maps `32` (and alias `2a`) to
MCU shared address `0x100b8`, DSP data word `085c`. It stores the complete
16-bit argument, then sets pending `0x100e0` and rings the doorbell when
commit is requested. The observer's `900f` is the independent word at
`0x100a8`, not command 32's payload. A scan of all 28 acquired catalogue
payloads finds no literal `085c` and five `085d` occurrences; this does not
exclude indirect accesses or establish the absent resident handler.
The acquired ROM4 mask uses `085c` as a multiplication coefficient in two
80-iteration routines, making gain a useful comparison hypothesis, not a
ROM6 semantic identification. The next contract is the resident consumer
of this parameter and its completion, rather than an inferred IRQ mask.
The fresh silent-peer run confirms `0x100b8=3fff` and saved caller
`0x2b6175`. This is the retry/deferred-parameter wrapper at `0x2b6114`:
commands `31..34` have four retained halfwords and a dirty-bit mask, and
an unavailable encoder is retried before its parameter is queued. Thus
the first request belongs to an MCU parameter-update lifecycle, not the
loader's IRQ4 upload acknowledgement. Its command-32 setter at
`0x2b61e6` clamps its argument before calling the wrapper; physical units
and the resident DSP consumer remain unresolved.
The setter at `0x2b61d0` uses a signed comparison against `0x8000` and
substitutes `0x7fff` for larger inputs. Its initialization call at
`0x2b627e` loads `0x3fff` from the product-local literal at `0x2b65f8`.
Runtime saved return addresses confirm encoder caller `0x2b6175` and
wrapper caller `0x2b61f1`. This establishes a bounded coefficient-like
parameter lifecycle, not its physical units or an evidenced ROM6 consumer.

The acquired ROM4 mask supplies a bounded comparison for control completion:
at `377b` and `38c3` it tests DSP word `0870` (MCU `0x100e0`), calls the
parameter consumer at `a51b`, pulses MMR `29` bit 3, then clears `0870` at
`379e` or `38d0`. The consumer reads neighboring parameter `085d` and
updates configuration; `085c` is consumed directly by the multiply loops.
This corroborates the MCU-side distinction between stored parameters,
busy ownership and IRQ4 notification. None of the 28 acquired ROM6 upload
payloads contains literal `0870`, so these ROM4 instruction addresses and
handler implementation are not a recovered ROM6 resident routine.

`nsm3dhle` is a separate, explicitly selected hybrid research composition.
It executes the same native verifier and loaders, suspends before resident
call `2c75`, then transfers transport ownership to runtime HLE. The native
CPU remains suspended; no helper return, loader `abba` cookie or firmware
state is fabricated. `nsm3dr6` retains its silent ownership policy, and the
normal `noki8250` configuration remains fail-closed.

The hybrid accepts the MCU's 14-word parameter bank at shared offsets
`a8..c2` into saved HLE state before clearing busy. This is declared opaque
configuration consumption, not recovered ROM6 instruction behavior, analogue
gain emulation, or a measured completion latency. Its fresh eight-second
run observes commands `32/31/33/08/09/2f/2f`, retains parameter `3fff`,
clears pending, and leaves the native PC at `2c75`. The MCU reaches
`0x2f349a`; graphical boot and phone features remain unproved. The next
boundary is the MCU's post-control packet/service initialization, which
must be decoded from this firmware rather than importing another product's
ring layout or replies. Continued native execution still needs the fitted
mask.

The hybrid now enables request-derived D0 discovery transport only, with
no unsolicited application/channel-map profile or radio protocol selected.
The MCU emits type `05`, payload `1eff00d000030101e000`. The peer derives
type-`8e` responses `1e0002d000030101e000` and
`1e0002d000030401c100`; the firmware consumes both and emits its own
follow-up `1e0200d0000305014100`. TX and RX rings are drained at eight
seconds (`a4/a6=0002/0002`, `1c8/1ca=008c/008c`). This validates the
shared discovery grammar against this MCU, not a donor application setup.
The independently captured LCD is still blank. Subsequent organic type-70
requests start with primitives `13/14/15/16` and `0d00`; their resident
service completion is the next boundary. The research HLE answers the
identity query and returns a candidate decoded record; the final self-test
reply remains unimplemented.

The producer at `0x28cb68` supplies product-local inputs, not an arbitrary
challenge: primitive `13` reads the firmware checksum at `0x3cfffc` through
`0x2ffb22` (the alternate branch reads `0x200038`). Primitives `14`, `15`
and `16` read logical PMM offsets `14` (12 bytes), `00` plus `0c` (20
bytes total), and `20` (24 bytes). The acquired PMM's low-record body at
file offset `0x10026` reproduces all three captured request bodies exactly.
The runtime checker accepts `--pmm` to enforce this ordered provenance.
This proves the forwarded inputs, not their compatibility with a modeled
COBBA identity or the missing ROM6 transforms.

The receive path is recovered independently from ring observations:
`0x2cb0c0` counts queued words, `0x2cb150` builds a class-18 message,
and task-4 code `0x3029fe` dispatches its type byte. Types `70..7f` are
reframed by `0x2e2a54` and posted to task 2, retaining the compact payload
at message `+8`. The service decoder at `0x244a5c` selects primitive `0d`
at `0x244acc`; result byte `+9` bits 0 and 1 record failures in two
service-result slots. Therefore emitting `0d00` without recovering the
checks would assert success, not merely acknowledge transport.

The ROM4 comparison exposes more of this family, but is not a ROM6 spec.
Its type-70 parser `4951` indexes a **data-ROM**, not program-ROM, table
at `b0a6`: primitives `13/14/15/16` select `4ac7/4b1f/4b3e/4b73`.
Primitive 13 reads COBBA values and calls transform `7f05` before emitting
a type-74 frame starting `340e`; primitive 15 calls `7f17`, stores six
words XORed with `a5a5`, and compares paired words. This resembles the
acquired ROM6 loader's transaction structurally, without establishing
identical transforms or silicon inputs. ROM4 primitive `0d` at `4a16`
reports resident word `06f9`, rather than echoing the request's zero.

A fresh native NSE-1 reference run with the existing product-local EEPROM
profile observes three writes to `D06f9`, all zero, at PCs `0f10`, `0e34`
and `0e6c` between 0.133283 and 0.133907 seconds. At three seconds the
word is still zero, identity flags `D1f11` are `0007`, and upload completion
`D0880` is `1074`. Thus an identity bitfield is not the primitive-`0d`
self-test payload. The literal mask scan finds its reader at `4a1c`, but
the observed writes occur in uploaded startup code; that scan alone is
not a writer census. `tools/nse1_selftest_status_observe.lua` reproduces
the passive observation with `noki5110`, a fresh `make_5110_eeprom_profile.py`
fixture, `-autoboot_delay 0` and a four-second run. No device or firmware
state is changed by the observer. These ROM4 startup results do not prove
the missing ROM6 routines pass on 8250; its final `0d` remains unanswered.

The existing native-observed word codec now has a mathematically derived
inverse in `tools/nse5_transform_trace_check.py`. It inverts the recovered
96-bit linear helper by GF(2) elimination and the nonlinear three-bit
permutation explicitly, then reverses the round order and rotations. Tests
cover every linear basis vector, both nonlinear word groups, 100 randomized
full round trips and recorded native vectors. The acquired ROM4 MSID tables
at data `b6e5/b6f7` reproduce the native reply and decode it to the same
checksum/signature/hash as the existing independent byte codec. This is
codec machinery, not evidence that ROM6 uses those tables or that the
8250 PMM belongs to the ROM4 modeled signature.

`tools/dct3_msid_codec.py` now supports explicitly selected `82` and `83`
MSID families using that independently derived word transform/inverse.
The numeric `83` decoder table is published in the original
[DCT3 MBUS discussion](https://nokiafree.org/forums/archive/index.php/t-38381.html?s=905ef8616030d0920e8836236655ced0).
A separate [RAE-3 service-tool capture](https://forum.gsmhosting.com/vbb/f550/9110-contact-service-951428/)
provides an external `83` vector: the codec reproduces all twelve reported
plaintext bytes, including its checksum, chip ID and signature. This is
independent codec-family validation, not a donor identity for this machine
or proof that the 8250 selects `83`. The encoder requires all plaintext
bytes and the family explicitly; it never chooses a passing identity.

The `nsm3dhle` research composition now explicitly selects a candidate
family-`83` identity-query model. It encodes the checksum actually sent by
the MCU, the existing COBBA register-5/6 packing (`00160010` with current
calibrated reset inputs), the published `ac ad ab` family marker and an
explicitly unmeasured revision input of zero. Its compiled C++ inverse is
checked against 101 independent Python word-model vectors. The ROM4
register packing and family selection remain declared ROM6 HLE hypotheses,
not recovered physical reset values or an assertion of PMM compatibility.

On a fresh coherent run, the device queues type `74` payload
`340e0083cf9a70aea6ab6dc5febbd33c` in response to the organic `1304`
request. Firmware handler `0x28d02c` retains all 13 MSID bytes at `0x12da5c`
and sets its ready flag at `0x12da3f`. The decoded reply reproduces the
request checksum and modeled chip inputs; the low-record requests still
match the acquired PMM. This validates the computed query/receive contract,
not the identity record verdict. No `0d00` success,
ABBA cookie, PMM rewrite or firmware-state change is synthesized.
`--identity` checks the encoded inputs and firmware-owned retention and
rejects a fabricated final success. The strict `nsm3dr6` control remains
silent at command 32 with native transport ownership retained.

The candidate codec also answers the organic `1618` request with a short
`3532` envelope: format zero, two computed inverse blocks and the original
24 input bytes. Its key is the published family-`83` lock table XORed with
the modeled chip packing; it does not substitute default/open-lock data.
The private final word of each decoded block is stripped, as in ROM4
`4b9e/4ba1`, without claiming that either marker passed a validity test.
The current original markers are `5146/f343`. Firmware receives all 52
response bytes at `0x28d250`, then takes its invalid-field branch at
`0x28d56e`; the acquired records are not accepted by this candidate
codec/key/format combination. This does not prove defective PMM or the
correct ROM6 table/padding selection. Original chip identity and any
product-specific preprocessing still require evidence. RX drains at
`00b0/00b0`, MCU remains at `0x2f3496`, and the LCD is still blank.
`--records` independently recomputes both inverse blocks, checks marker
stripping/original-byte retention and requires identical MCU receipt.

A fresh passive decoder-path run localizes this rejection before format
selection: `28d350 -> 28d356 -> 28d37e -> 28d3be -> 28d3c4 -> 28d56e`.
Message byte `+15` is `ee`, bypassing the special `60..6f` and `78..7f`
cases. The ordinary structural branch then requires message byte `+21`
in `78..7f`; its computed value is `06`, so the `blt` rejects it.
These are byte 9 of each decoded 12-byte block, not stripped marker words.
Changing the format selector cannot repair this observed rejection: its
code at `28d476` is never reached. This narrows the next investigation to
the decoded content's chip/key/preprocessing contract, not envelope format.
The read-only Lua observer retains the last 16 decoder PCs to reproduce
the path; it does not modify the response or CPU state.

The preceding selector at `28d356` reads bit 7 of `13fdd9` (literal
`28d6b4`). Clear selects the path at `28d434`; set selects the structural
checks above. Own initialization unconditionally ORs `80` at `2447f2..f6`,
after setting `40`, and the passive runtime trace observes `c0` before
record receipt. Its later value zero follows teardown and must not be
mistaken for the startup input. `noki8250_record_context_observe.lua`
reproduces these writes without modifying them. Bypassing bit 7 is not a
provisioning fix or a legitimate decoder alternative.

The acquired sector begins at file `10000`, with initial record header
`00ce/8000/0000` at `10020`: an extended 32768-byte body starting `10026`.
It crosses multiple 8-KiB sectors. The bounded NSE-5/NHM-3 journal replayer
therefore cannot establish this product's latest-record selection: its
single-sector extent contract does not apply. Existing runtime request
checks establish the selected low-record bytes directly; a generic journal
replay must not silently replace those observations.

The primitive-16 builder at `28cc32..28cc64` allocates the own 30-byte
object, reads logical PMM `20/18` through `306448` and sends it directly
through `2890c4`. Unlike the NSE-1 conditional transform path, there is no
additional caller-side preprocessing between read and send. The existing
live check independently matches all 24 transmitted bytes to the acquired
PMM. An offline direction comparison with the declared candidate chip/key
gives second-block byte 9 `a6` forward versus `06` inverse; neither lies in
the required `78..7f` range. `test_nsm3d_record_query.py` pins both complete
results. This closes a direction-only correction, not unknown ROM6 keys,
chip identity or DSP-side preprocessing.

An offline comparison uses only the two published `82`/`83` lock tables,
the same original PMM bytes and the same nominal chip packing. Before
marker stripping, family `82` gives byte-9 values `43/bc` and markers
`c875/28f7`; family `83` gives `ee/06` and `5146/f343`. Both therefore
fail the ordinary `+21 in 78..7f` structural branch. This is not a chip-ID
search, and does not exclude either family with the original physical
chip identity. Merely switching the current HLE's family is unsupported.
The numeric tables are the author's published `Lock_Enc_82/83` values in
[the DCT3 MBUS discussion](https://nokiafree.org/forums/archive/index.php/t-38381.html?s=905ef8616030d0920e8836236655ced0).

The own-ROM helper at `28bf30..28c018` constructs a separate primitive
`17`, length `30`, combining caller fields with original 24-byte PMM data
(or reversed 12-byte halves when its stack argument is one), then posts
it through `2890c4`. Its observed caller is downstream of successful
primitive-35 validation at `28d55e`. That helper is a record-update
transaction, not evidence of an IMEI-derived pad on the current `1618`
read request. Neither a donor chip identity nor a default unlocked record
has been substituted to make these checks pass.

The acquired PMM's provenance imposes another limit: its filename's
"virgin EEPROM" wording is not a factory-state guarantee. The file has one
`EEPROM` header at `10006`, and contains non-erased user data outside the
low identity records (personal contents are not reproduced here). The ten
24-byte slots at logical `20 + 18*n` contain only two distinct ciphertexts:
slot 0 SHA1 `064d3eacf4fa384d2ecb2e2cdd83e99b88a37723`, slots 1–9 SHA1
`1c9f0bd909e352d04013573cf9d16b4ae12b287d`. Repetition does not prove a
default/unlocked plaintext or provide nine independent key observations.
The reviewed header, filename and record structure supply no authenticated
original COBBA serial. The existing registers 5/6 are explicitly calibrated
inputs, not measured chip identity; see [the COBBA boundary](cobba_control_boundary.md#remaining-boundary).

`tools/dct3_record_identity_constraints.py` is an optional offline research
instrument, not a provisioning tool or acceptance gate. It pins this PMM's
SHA1 and models the explicit family-83/raw-input/24-bit-chip hypothesis,
requiring both unstripped final words to equal ROM4's `54c2` marker. Its
symbolic byte inverse is cross-checked against the independent native word
model, including randomized keys and inputs. It requires a separately
installed `z3-solver`; it does not add a runtime emulator dependency.
Timeout means **unknown**, not absence of a compatible chip, and even a
satisfying candidate would need independent record/identity validation.
No solver output is connected to a device setter, PMM writer or HLE verdict.
The acquired first record returns `unknown (timeout)` with both 10-second
and 120-second limits. This avenue has not recovered a serial or falsified
the unknown-chip hypothesis; neither a candidate nor an absence proof exists.
The `--all-distinct` experiment constrains both distinct records with the
same symbolic chip and returns `unknown (timeout)` at 60 seconds. It adds
independent ciphertext conditions, not extra observations from repeated
slots. No further identity is inferred from these inconclusive results.

The 8250 decoder independently routes primitive `34` to `0x28d02c`
(13-byte retention), `35` to `0x28d250`, and `36` to `0x28d0d0`.
The `35` handler's envelope branch (`0x28d286..0x28d2c6`) accepts size
`32` directly, or size `34` after summing 25 big-endian halfwords from
message `+0a`, truncating to 16 bits, XORing `ffff` (literal at
`0x28d5e8`), and comparing the stored halfword at message `+3c`.
This is an envelope check, not the complete context/identity acceptance
contract. `tools/nsm3d_service_contract.py` checks that distinction offline;
its tests cover byte order, overflow, malformed extents and corruption.
The byte at message `+0a` is a format selector, not an unused success byte:
`0x28d476..0x28d4a6` maps 1 to internal flags 3, 2 to flags `0b`, and
0 to flags 5 (or 7 when context `+0d` is `81`); other values invalidate
the record. The candidate HLE's zero is the observed ROM4 format choice,
not proof of the fitted ROM6 response format.
The `36` handler stores whether message `+0a` is zero to `0x12da46`
and invokes `0x288c84(2)`; it does not decode a transformed record.
Recover these MCU-side producers/consumers and compare their transformation
contract with the acquired mask before implementing a result-producing
HLE. The generic compact-success profile remains disabled here.

Use the same private-directory invocation below with machine `nsm3dhle`
and validate its log separately:

```sh
.venv/bin/python tools/nsm3d_runtime_hle_check.py RUN/error.log \
  roms/noki8250/8250-502mcuppmk.fls --discovery --identity --records \
  --pmm 'roms/noki8250/8250 virgin eeprom 003d0000.fls'
```

Use `-verbose` for discovery transport observations. Setting fixture variable
`NOKIA_DCT3_SNAPSHOT_DIR` captures the native LCD at eight seconds without
modifying guest state. The hybrid checker verifies acquired native uploads,
exclusive ownership, ordered control acceptance, firmware discovery follow-up,
drained rings and the retained native stop. It is not a boot
or UI acceptance gate. Both 3210 gates, the silent native fixture, the
tool suite and the patch-stack check pass with this composition present.

HLE callbacks and queued service/packet/response/keepalive/speech work must
not mutate the transport while native ownership is active. In particular,
the generic HLE doorbell clear of `0x100e0` is suppressed: otherwise the
MCU proceeds to later requests despite the explicitly silent native peer,
invalidating the observation.

Run `nsm3dr6` with the private-directory options below and
`tools/nsm3d_verifier_observe.lua`. Validate native publication, MCU retention,
complete loader upload, mapped fragment, organic loader2 delivery and the
silent command-32 boundary; successful process exit is not phone boot:

```sh
.venv/bin/python tools/nsm3d_live_verifier_check.py RUN/error.log \
  roms/noki8250/8250-502mcuppmk.fls
```

## Recovered GENSIO contract

### 8890 Support And Native Boundary

The acquired v12.20 PPM C and product-local PMM have verified research-HLE
acceptance for the following workflows. Normal `noki8890` is deliberately
conservative; select `nsb6hle` explicitly. No donor PMM, firmware-state
injection or borrowed product verdict is part of these workflows.

| Requirement | Acceptance Evidence |
| --- | --- |
| Own native upload and explicit HLE boundary | `noki8890_staged_check.py --runtime --selftest`; acquired verifier/loaders execute before suspension at absent resident `2c75` |
| Own service consumer | `noki8890_selftest_contract.py`; request-correlated runtime reply and fault-byte updates |
| Graphical boot, SIM and physical menus | `noki8890_ui_check.py`; SIM/APDU coverage, key decode and reviewed menu pixels |
| Durable phonebook | `noki8890_phonebook_check.py save/readback`; exact A/123 storage and separate cold read without rewriting it |
| Application input | `noki8890_calculator_check.py`; physical 12+3 and result 15 |
| GSM900 and PCS1900 registration | `noki8890_registration_check.py`, with `--pcs1900` for strict carrier-600 acceptance |
| Incoming/outgoing calls | Own incoming/outgoing call checkers; physical Answer/Send/End, correct called number and CC/RR closure on both bands |
| Incoming/outgoing SMS | Own SMS checkers; physical composition/read, CP/RP closure, persistent hello and reviewed body pixels on both bands |

Native resident DSP execution, speech/media, identity/security-record
replies, neighbour/handover contracts and normal-machine promotion remain
separate unproved work. Calibrated board inputs and the declared runtime
HLE are not measurements of real NSB-6 silicon. The current graphical and
signaling acceptance must not be described as complete native emulation.

#### Stock Bootstrap Contract

An isolated v12.20 PPM C run with its acquired PMM renders `CONTACT
SERVICE`. The sampled loop `2f0d3e..2f0d44` is not a blank-display result.
`tools/noki8890_bootstrap_observe.lua` passively records the first shared
memory access sites and captures the screen through the common scout.

Own bootstrap `2c2d94..2c2eaa` reads word offsets `4/6` through `r5`.
At `2c2dbc/2c2ddc` offset 4 remains `ffff`; the bounded retry expires and
clears its readiness byte at `2c2de6`. Although both words match at
`2c2dee`, zero readiness takes the failure exit `2c2e8c` before the sparse
upload loop. The existing conservative product composition therefore does
not establish a verifier verdict or absent post-upload completion.

The own upload payload at file `11624c` is byte-identical to the mapped
223-word staged verifier; its descriptor at `116240` is
`0f00/0000/00df/0f00/00dc/0000`. The own 104-word program fragment at
file `11508c` is byte-identical to the acquired 8250 fragment, while the
loader and loader2 payloads are not identical. Their product-local catalogue
and acquired bytes are used for execution before the runtime HLE handoff.
Shared-fragment identity is not a physical chip-identity measurement or
permission to copy a sibling's record/self-test verdict.

`nsb6stage` is a separate native-upload research composition. Its explicit
pre-upload ROM input is 6 from the acquired fragment, not a measurement of
physical NSB-6 silicon; normal `noki8890` is unchanged. The 28-entry table
at `307610` is initialized by `307608` into `134c78`. Selector 0 contains
638 loader words (SHA1 `a5f4f14638f0cbfd0bab29cb4995a8e05641af22`),
selector 14 contains the own 613-word second loader (SHA1
`7fc1c5a9435664f15b7064de1cf129f764ab21ac`), and selector 1 contains
`092f` words. Own observed loader release uses control word `0880=0078`.

Native verifier execution publishes `0000/0006/0006/0006`, followed by
loader requests `14` and 118 requests `01`. The executor byte-verifies the
second loader before running it; 422 installed program words precede its
call to absent `2c75`. It suspends with native ownership retained, without
a guessed helper return or runtime reply. Reproduce in fresh private
directories with machine `nsb6stage`, the bootstrap observer above, nine
simulated seconds and verbose logging; then check:

```sh
.venv/bin/python tools/noki8890_staged_check.py RUN/error.log \
  roms/noki8890/8890_12.20_ppmc.fls
.venv/bin/python -m tools.nsm3d_catalogue \
  roms/noki8890/8890_12.20_ppmc.fls --product 8890 --address 0x2c75
```

The latter reports declared descriptor coverage, not a proof against
dynamic relocation. Native upload acceptance is not graphical/UI acceptance.

`nsb6hle` is a separate runtime transport comparison: it suspends native
execution at that same absent routine, then enables shared parameter
acknowledgement and request-derived external-service discovery. These
settings belong to the product configuration so reset preserves them.
It does not implement identity or security-record verdicts. Its compact
self-test HLE response is selected by the own consumer contract below.
The observer's bounded debugger probes at `2c307c/2c3398` record the own
service encoder and shared pending-word commit without changing firmware.

The coherent comparison commits command `32` with argument `3fff`, emits
discovery frame `1eff00d000030101e000`, receives its echo and discovery
response, then organically acknowledges with `1e0200d0000305014100`.
It subsequently transmits type-70 requests `13/14/15/16` and `0d00`.
Those acquired-product requests are evidence for the next consumer audit,
not permission to import sibling reply semantics. This short transport
check alone does not establish interactive acceptance. Reproduce with
`nsb6hle`, the same observer and twelve
simulated seconds; check with the staged checker's `--runtime` option.

Own dispatcher `243706..243746` decodes class `74` by a subtract cascade
(`03+02+0c+02+2d+02+32=74`), then calls `240938` except command `32`.
The own handler's command cascade `240952..240960` selects command `0d`
at `2409ac`. Flag `13fde1` bit 2 arms the wait; timer `19` is cancelled,
fault array `13fbe0` offset `0f` cleared, and reply byte `+9` bits 0/1
select clear/failure values for offsets `10/11`. These own-ROM facts
justify the existing request-correlated compact HLE self-test contract,
not a copied silicon verdict or execution of absent resident code.

With that contract, TX `70:0d00` at 1.247515 seconds receives RX
`74:0d00` at 1.247615. The own consumer sees armed flag `c4` and returns
with fault bytes `0f/10/11=00/00/00`. A subsequent `70:0a09` request
appears. Self-test acceptance alone does not establish graphical boot.
Identity and security-record requests remain unanswered; neither is required
for the graphical and physical-input result below.

```sh
.venv/bin/python tools/noki8890_selftest_contract.py \
  roms/noki8890/8890_12.20_ppmc.fls
.venv/bin/python tools/noki8890_staged_check.py RUN/error.log \
  roms/noki8890/8890_12.20_ppmc.fls --runtime --selftest
```

#### 8890 Graphical and Physical Input

Nokia's [NSB-6 user manual](https://fcc.report/FCC-ID/ljpnsb6ny/85827.pdf)
specifies a BLB-2 pack. The research composition uses the existing nominal
BLB-2 CCONT tuple rather than full-scale conservative placeholders.
These are calibrated board inputs, not measured NSB-6 electrical units.
Firmware retains voltage, pack and temperature decisions. With no card,
that input correction yields the German **SIM-Karte einsetzen** frame;
no identity/record result is synthesized.

Own scanner `2fb850` drives row pins 1..4 and returns row*5+column.
IRQ0 handler `2fb9a4` reads pending columns at `2b` and posts scan event
`41` through `2fef6c`. Decoder `2ff006` indexes the 25-byte matrix at
`339f4c`; the own table matches the shared five-column host layout.
The research machine selects that layout, with Menu `19`, Names `1a`
and Send/End `0e/0f`, rather than the generic 3310 layout.

With the physical SIMI controller and ordinary removable laboratory card,
the firmware organically reads ICCID, phase, SST, IMSI and all 50 ADN
records, then renders **Sicherheitscode / OK**. Physical `12345` followed
by Menu decodes `01/02/03/04/05/19` and dismisses the prompt into the
Nokia startup graphic. Further physical Menu presses acknowledge the
clock-not-set notice, open **Mitteilungen**, then its message submenu.
The own acquired PMM remains unchanged; no donor provisioning is used.
Native DSP completion and normal `noki8890` promotion are not implied.

Reproduce with `nsb6hle`, fresh private cfg/NVRAM, verbose logging,
`-debug -debugger none`, `tools/noki8890_security_input.lua`, autoboot
delay zero and 35 simulated seconds. Acceptance combines SIM/APDU
coverage, physically correlated decoded keys and stable reviewed
title/list pixels (excluding animation and scrollbar):

```sh
.venv/bin/python tools/noki8890_ui_check.py RUN/error.log RUN/snap
```

Physical phonebook Add entry additionally stores A/123 through
`A0 DC 01 04 20`, receiving `9000`. The exact persisted GSM 11.11 ADN
record is validated, not just the German SIM-save confirmation screen.
A preserved-storage cold process reads record 1, then physical
Names/Search/Detail displays number 123 without any UPDATE RECORD.
Fresh and preserved boots have different clock-notice dismissal paths;
the save/read fixtures deliberately keep those sequences separate.

Run `noki8890_phonebook_input.lua` for 40 seconds from fresh storage;
run `noki8890_phonebook_read.lua` for 32 seconds with that preserved
NVRAM directory. Check each stage:

```sh
.venv/bin/python tools/noki8890_phonebook_check.py save SAVE/error.log \
  SAVE/nvram/nsb6hle/sim_card SAVE/snap/8890_phonebook_save.png
.venv/bin/python tools/noki8890_phonebook_check.py readback READ/error.log \
  SAVE/nvram/nsb6hle/sim_card READ/snap/8890_phonebook_contact.png
```

Physical calculator navigation reaches menu 7 (**Rechner**) and computes
12+3=15 through the application's Options/Add/Result sequence. Reproduce
with `noki8890_calculator_input.lua`, fresh storage and 44 seconds, then
check decoded physical inputs and the arithmetic-area pixel oracle:

```sh
.venv/bin/python tools/noki8890_calculator_check.py RUN/error.log \
  RUN/snap/8890_calculator_result.png
```

#### 8890 Radio Contract

The own RX dispatcher at `30168e` selects type `80` at `301750`, calling
`2dae9c`. Its thirteen-entry `83..8f` table at `3016bc` maps `8b` to
`301710 -> 2db270`, which posts to task 12 through `28190c`.
Type `89` maps to `301720 -> 2daffc`; `2db01a..2db024` compares body
bit 0 against the pending channel context. These are independently decoded
NSB-6 consumers, not reused sibling addresses.

Own startup directly emits `56/160` with ARFCN `003c` in its candidate
window. `RADIO_NSB6` therefore uses candidate-window acquisition, not the
8850's initial `55` band-scan continuation. The request-derived experiment
organically constructs candidate and assigned CHANNEL_CONFIGURE, accepts
their confirmations, then sends Location Updating Request with its own
capability octet `23`. The contention UA echoes that request; the handset
acknowledges Location Updating Accept and Channel Release, writes both
EF_LOCI LAI/status fields and returns to paging/BCCH after own
deconfiguration `040000000000001a6000003c0000000f00000000`.

This is the independently verified GSM900 contract. Separate PCS1900
acquisition and user-service acceptance are documented below.
Physical End after a connected outgoing call publishes traffic-release
parameter `14`; that own observation now selects the release contract.
Handover, neighbour and speech contracts remain unset until product
observations justify them. Native missing DSP execution
remains distinct from these HLE transport/network results.

Reproduce with the research machine and security input fixture for 65
seconds, from fresh cfg/NVRAM; then run:

```sh
.venv/bin/python tools/noki8890_registration_check.py RUN/error.log
```

Physical outgoing Send produces CM Service Request/Accept, SETUP,
Call Proceeding, traffic assignment/configuration, Assignment Complete,
Alerting, Connect and Connect Acknowledge. Physical End produces
Disconnect, Release/Release Complete and RR release; own deconfiguration
`040000001117001a6000003c0000001400000001` accepts the idle confirmation
and returns to paging. This is signaling, not speech acceptance.

Physical outgoing dialing now preserves the intended `1234567` in both
the displayed editor and SETUP. Reproduce with
`noki8890_outgoing_call_input.lua`, fresh cfg/NVRAM and 60 seconds, then
run `noki8890_outgoing_call_check.py RUN/error.log` with its default
expected number. The fixture directly cancels the initial clock editor
with End, dismisses the notice with Names/C, then enters the number.

Avoid confirming the empty clock editor before cancellation: that creates
an invalid-input saved position, which the next editor can consume. Own
clock dispatcher `236130` reaches `236182` on its validation input;
`2357f0` parses the clock fields and returns the failing field position.
On failure, `2361bc..2361c4` passes `{0, failing_position}` to `25ee74`,
which stores it at `1324a0/1324a2`. These positions survive cancellation.
First-digit insertion at `2603bc` advances context `1348cc + 1c` to 1,
but construction through `23c23c..23c242` restores saved position 0 via
`25eed4` when descriptor `1324ad == 64` matches. Subsequent digits then
insert before the original digit. Direct cancellation does not create
that failing-position record and passes correct dialing without device
changes, cursor pokes or reordered test digits. Extra settling, Scroll
Down and a shorter first-key pulse do not repair the invalid-input path.

`noki8890_dial_observe.lua` is a read-only diagnostic for the saved record
and restore sites. Clock-error cancellation behavior remains a separate
UI lifecycle observation, not a missing keypad or radio contract.

Incoming-call signaling separately passes IMSI paging, Paging Response,
contention UA, cipher/MM-information exchange, incoming SETUP, Call
Confirmed/Alerting, own traffic configuration and Assignment Complete.
Physical Answer (`0e`) produces Connect, and physical End (`0f`) closes
Disconnect, network Release, Release Complete and RR release before the
same own deconfiguration and return to idle paging. Call indicators are
visible, but the foreground clock notice after release is not an idle
presentation oracle. Neither signaling run proves speech media.

Copy `fixtures/noki8890_incoming_call/nsb6hle.cfg` into a private cfg
directory (do not run with the tracked fixture directory writable by MAME).
Use fresh NVRAM and `noki8890_incoming_call_input.lua` for 50 seconds:

```sh
.venv/bin/python tools/noki8890_incoming_call_check.py RUN/error.log
```

Incoming ordinary text SMS is verified on the research composition: IMSI
paging, SAPI-3 segmented delivery, handset CP/RP acknowledgements, network
CP acknowledgement and RR release complete. Physical input opens the
message and displays `hello`; the SIM retains the exact read SMS record.
The own DSP cipher-control publication is `0076ffffffffffffffff0000`.
Copy `fixtures/noki8890_incoming_sms/nsb6hle.cfg` into a private cfg
directory and run `noki8890_incoming_sms_input.lua` with fresh NVRAM,
verbose logging and 36 emulated seconds. Validate with:

```sh
.venv/bin/python tools/noki8890_incoming_sms_check.py RUN/error.log RUN/nvram/nsb6hle/sim_card RUN/snap/8890_sms_read_2.png
```

Outgoing SMS also passes through physical UI input: Write message, `A`,
Send, recipient `5551234`, confirmation. The emitted SMS-SUBMIT is exactly
`390118000100069121436587090d11010781551532f40000a70141`;
the laboratory network accepts it, CP/RP acknowledgements close and the
handset returns to paging. Run `noki8890_outgoing_sms_input.lua` for 52
seconds with fresh NVRAM, private cfg, verbose logging and no incoming
scenario enabled; check with `noki8890_outgoing_sms_check.py RUN/error.log`.
The SMS recipient editor preserves digit ordering; the direct-call editor's
rotation is not a general keypad wiring or numeric-entry defect.

GSM900 and PCS1900 laboratory registration are verified separately;
clock-notice settlement and invalid-clock error cancellation remain
presentation/lifecycle questions. Call signaling and SMS are verified on
both bands; speech/media remains unproved.

### 8890 PCS1900 Acquisition Contract

Current result: fresh own PMM reaches PCS1900 registration on ARFCN 600,
writes EF_LOCI, acknowledges release and returns to paging. This uses the
explicit runtime research HLE, not a completed native DSP. The handset
builds its candidate window and PCS channel configuration organically.

`fixtures/noki8890_pcs1900/nsb6hle.cfg` selects laboratory carriers 600/601
and the stable two-cell topology. The latter is essential: legacy single-cell
mode treats unlisted ARFCNs as receivable, so merely changing its configured
carrier can still yield GSM900 registration on the handset's initial ARFCN
60. That run is not PCS acceptance.

The network emits SI1 Rest Octets `6b` for PCS, rather than DCS `2b`:
absent NCH followed by H rather than L in the band-indicator position.
This is the standards-level distinction described in
[ETSI TS 145 014 section 4.1.6](https://www.etsi.org/deliver/etsi_ts/145000_145099/145014/07.01.00_60/ts_145014v070100p.pdf),
not a Nokia packet label. Existing defaults remain DCS/GSM900.

With fresh own PMM and the explicit topology, firmware emits its initial
`56/160` candidate window for `003c`, then `55/4` `01140000` and
`04080000`. The own protocol enables separate GSM and PCS scans: the first
has no receivable GSM cells; the second reports 600/601. Firmware then
publishes a `56/160` window beginning `02580259`, selects 600, emits
PCS configuration prefix `0412`, consumes SI1 with band indicator `6b`
and publishes its Location Updating Request with power-class byte `20`
(GSM900 uses `23`). No state or callback is injected.

Reproduce by copying the fixture into a private cfg directory and running
`noki8890_radio_observe.lua` with fresh NVRAM, verbose logging and 90
seconds. Check the resulting log with
`tools/noki8890_registration_check.py --pcs1900 RUN/error.log`.
The check requires ordered scan requests, PCS measurements, firmware
candidate selection, carrier 600 configuration, SI1 band indication,
correlated registration, EF_LOCI writes, release and paging. Its negative
tests reject a GSM-only candidate, DCS SI1 indication, reversed scans and
the wrong power-class field. Default and coherent 3210 gates remain green.

Own construction is now mapped: state-1 path `21ef44..21ef74` (embedded
firmware diagnostic name `PH_1050`) calls `2aed52` with mode 1 and parameter
index 0. It allocates eight bytes, creates a four-byte type-55 message,
encodes it through `2aecf2`, and posts it to transport task 3 via `28190c`.
The encoder implements five distinct mode branches, preserving the mode in
wire byte 0 and selecting wire byte 1 from a three-byte table:

| Firmware Mode | Parameter Table | Bytes |
| --- | --- | --- |
| 1 | `3398d0` | `14 05 03` |
| 2 | `3398d3` | `07 04 03` |
| 3 | `3398d9` | `03 05 03` |
| 4 | `3398d6` | `08 04 03` |
| 5 | `3398dc` | `05 03 03` |

`noki8890_radio_observe.lua` passively captures constructor mode/index/caller
and final wire bytes. The strict topology reproduces mode 1/index 0 from
caller `21ef75`, then mode 4/index 0 from caller `21ee1b`.
Do not infer units, bands or result counts solely from the table values.

The generic terminal branch's phase label does not establish the meaning
of a four-byte type-55 request. NSB-6 modes 1 and 4 are decoded as band scans
before that branch, replacing the stale previous-candidate response with
measurements of the corresponding receivable topology. Its protocol also
permits the firmware-owned explicit candidate window after a band scan.

The own receive path is `301710 -> 2db270 -> task 12 -> 29d010`.
`29d010` passes the first report to `2809fc` and subsequent reports to
`280b0a`; these are measurement-list builders, not an opaque reply queue.
The first builder consumes up to forty four-byte records from message
offset 6: a big-endian ARFCN at offsets 6/7 and signed RSSI at offset 9,
advancing four bytes per record. RSSI below -104 terminates the list.
Accepted records are stored in sixteen-byte internal entries and counted
by the band classification returned by `2c06bc`.

`280904` tests those counters according to the constructor mode: mode 1
requires thirty class-1 entries, mode 2 forty class-2 entries, mode 3 both,
mode 4 forty class-3 entries, and mode 5 class 1 plus class 3. This is a
receive-side distinction between modes, not a meaning inferred from the
three-byte parameter tables. In `2c06bc`, the enabled class-3 branch is
the 512-and-up range behind capability 5; overlapping channel numbers
must still be interpreted using the handset's enabled band capabilities.
Report-count limits in `28097e` provide another completion condition;
they are firmware policy, not permission to replay arbitrary packets.

The strict-topology observer records mode 1 completing with state 4, zero
counters and one report: its first RSSI is `81` (-127), so parsing stops
before accepting a record. The next organic request is mode 4
(`04080000`). That result accepts ARFCNs `0258` and `0259`, with signed
RSSI -61 and -71, and completes with class-3 count 2. At 8.966 seconds
firmware selects carrier 600; Location Updating Accept is acknowledged
at 14.575 seconds. These timings and signal levels are laboratory HLE
observations, not measured silicon latency or calibrated RF units.

### 8890 PCS1900 Call And SMS Acceptance

Fresh strict-topology runs also complete incoming and outgoing call
signaling and incoming/outgoing SMS. The network's existing assignment
encoder derives the non-hopping traffic carrier from the serving ARFCN;
firmware configures TCH/F with
`041202000271012fc10002580000000400000000` and releases to carrier 600
with `041202001117001a600002580000001400000001`. Physical Send/Answer/End
close the CC/RR lifecycle. Outgoing SETUP contains the intended `1234567`.
These are signaling proofs, not audible speech or a native DSP claim.

Use fresh private NVRAM and cfg directories, verbose logging, and 60-second
runs. Each PCS checker first requires the independently checked scan,
carrier-600 registration and SI1 band indication; a GSM900-only run cannot
pass by reaching the same UI or sending the same Layer-3 messages.

| Workflow | Fixture Directory | Physical Input Script | Checker |
| --- | --- | --- | --- |
| Outgoing call | `fixtures/noki8890_pcs1900` | `noki8890_outgoing_call_input.lua` | `noki8890_outgoing_call_check.py --pcs1900 RUN/error.log` |
| Incoming call | `fixtures/noki8890_pcs1900_incoming_call` | `noki8890_incoming_call_input.lua` | `noki8890_incoming_call_check.py --pcs1900 RUN/error.log` |
| Outgoing SMS | `fixtures/noki8890_pcs1900` | `noki8890_outgoing_sms_input.lua` | `noki8890_outgoing_sms_check.py --pcs1900 RUN/error.log` |
| Incoming SMS | `fixtures/noki8890_pcs1900_incoming_sms` | `noki8890_incoming_sms_input.lua` | `noki8890_incoming_sms_check.py --pcs1900 RUN/error.log RUN/nvram/nsb6hle/sim_card RUN/snap/8890_sms_read_2.png` |

Scripts and checkers are under `tools/`; copy the selected fixture's cfg
file rather than allowing MAME to rewrite the tracked fixture. Incoming
SMS acceptance requires the physical Read transaction, persistent read
record containing `hello`, its reviewed body pixels, and CP/RP closure.
Outgoing SMS acceptance requires physical composition of `A` to `5551234`,
the exact SMS-SUBMIT and its network acknowledgements. The default checker
options retain the independently verified GSM900 contracts.

### 8850 stock-input runtime boundary

A fresh v5.31 PPM C run with its acquired PMM renders `CONTACT SERVICE`
at eight seconds. This is a graphical failure frame, not interactive boot.
The former `2f6e44` PC-only observation was insufficient to classify the
firmware as blocked: routine `2f6da0` polls byte `1381ec`, but Timer-0
compare values advance throughout the observation (`0047` at 0.5 seconds,
`00a5` at 1 second, `0497` at 8 seconds). FIQ mask/control are `e3/05`.
FIQ dispatcher `302eec` loads the same flag from literal `302fb4` and clears
it at `302ef2`. The observed loop is therefore not a proven missing wake
or final DSP publication wait. Its complete entry/exit cadence remains to
be observed; investigate the service-failure verdict rather than injecting
a flag clear.

`tools/noki8850_frontier_observe.lua` records bounded PC/register and
side-effect-free MAD2 status/counter samples plus screenshots. Its flag
write tap produced no records in this run; that negative result is not a
writer-absence proof. The firmware-owned clear is independently decoded.
Use the private-directory invocation below with this observer for nine
seconds. No guest state is written.

The own-ROM application checksum routine is `2408c8`; startup compares its
result with logical NV `0254` at `240b8c..240b94`. Failure branch `240c02`
writes `0c` to fault-array offset `0c`. Literal `240da8` independently
identifies that array as `13fbe0`. Fresh runtime samples from 0.5 through
8 seconds contain `ffff00ff00ffff00ffffff0000ff0e0fffff00000000ffff`:
offset `0c` is clear, while offsets `0e/0f` contain `0e/0f`. The latter
stores occur at `240b4c/240b50` when readiness helper `2f6aa4` does not
return 1. Several other slots retain `ff`, so do not reduce this screen to
one completed self-test failure or import the 6250 checksum repair path.
The next contract is the helper's firmware-owned readiness byte and the
remaining startup-test completion paths. The read taps on checksum entry
and failure produced no records; those negatives are not execution proofs.

Helper `2f6aa4` reads byte `135664` (literal at `2f6d9c`). A
halfword-aligned literal-reference scan finds four pool copies and fourteen
PC-relative loads; this is not an exhaustive indirect-writer census.
One decoded setter is `2cb43c..2cb450`: if that byte and shared-memory
halfword `100e4` are both zero, firmware stores 1 to the byte. Fresh passive
samples at 0.1, 0.5, 1, 2, 4 and 8 seconds show both values remain zero.
Thus the sampled shared-memory predicate already permits this setter;
fabricating a DSP completion is not justified by this condition. The native
upload research composition below supplies its genuine IRQ4 prerequisite.
The samples do not prove the predicate held at every intervening instant.

The readiness setter belongs to DSP service IRQ4: IRQ dispatcher `302eb6`
calls entry `2cb418` when unmasked pending bit 4 is set. A second direct
caller is `308446`. A fresh conservative-profile transport trace shows no
service-pending transaction before two seconds. It writes shared identity
`10004=ffff`, which the conservative HLE does not answer; own-ROM
`2cae26..2cae34` consequently skips the upload unless identity is 5 or 6.
This precedes the readiness failure and supersedes a missing-IRQ hypothesis
as the immediate frontier.

`PRODUCT_8850` now explicitly selects the ROM6 HLE identity (6) and
alternating upload acknowledgements. This is a declared silicon selection,
not evidence of the fitted mask revision. Final verification is deliberately
unmodelled. A fresh nine-second run reaches `2caeae..2caeb4`, polling shared
result `10002` against `ffff`, with startup tests unfinished and no accepted
graphical boot. The former CONTACT SERVICE frame describes the conservative
profile, not the new upload frontier. The next software avenue is executing
the acquired flash's verifier/loader through the staged DSP composition;
the 104-word bootstrap fragment at flash offset `11ad54` exactly matches
the already decoded NSM-3D fragment, while candidate loader sources must be
checked independently before use.

### 8850 Native Upload Boundary

Research machine `nsm2stage` executes the acquired NSM-2 code instead of
supplying a verifier verdict. Its flash bootstrap payload at `11ad54` is
104 words, SHA-1 `440bf49f1eba4cadb12f7f7581c992b0025807d6`; the preceding
descriptor is `ff80/ff80/0068/0200/008c/0000`. This recovered template is
not a fitted mask dump. The native verifier publishes shared words
`0000/0006/0006/0006` at 1.179761 seconds. Loader release fields independently
identify control word `0880=0078`, unlike NSM-3D's `087f`.

The native loader requests selector `14` once and selector `01` 133 times
through MAD2 IRQ4. Its second loader at flash offset `11ae80` is 613 words,
SHA-1 `f543a5807a4abf19e2429d48c8690d5137dbaba9`. The staged device compares
every uploaded word against this product-local source before entry `0a00`.
At `0a37` it reads I/O port `001c`, ORs `0200`, and writes the value back;
the composition now retains that register for the RMW only. Its physical
bit meaning and reset value remain unverified; no completion is generated.

At 1.208090 seconds the loader calls absent mask routine `2c75`; observation
suspends the DSP with transport ownership retained. MCU readiness byte
`135664` subsequently becomes 1 organically, resolving that prerequisite.
The eight-second frame is blank and startup fault slots remain unfinished:
this is native upload validation, not graphical boot or phone acceptance.
The next software experiment may select runtime HLE after this verified
loader boundary, but must derive subsequent requests from NSM-2 traffic.

Run `nsm2stage` with the private-directory invocation and observer described
above, using `noki8850` as the parent ROM directory. Then check the captured
log with `python3 tools/noki8850_staged_trace_check.py <run>/error.log`.
The checker requires ordered native publication, loader verification,
missing-mask suspension and firmware readiness; it rejects an HLE handoff.

### 8850 Runtime Self-Test Contract

Research machine `nsm2hle` preserves native verifier/loader execution, then
suspends native execution at `2c75` and assigns subsequent transport to HLE.
No missing instruction, return value or verifier cookie is fabricated.
It enables request-derived external discovery without an unsolicited
registration/channel-map application contract.

Own TX at 1.293367 seconds is type `70`, body `0d00`. Dispatcher
`243a0e..243a4a` selects class `74` (subtract cascade totals `74`) and
calls `240dbc` except command `32`. Handler `240dd2..240de0` selects `0d`
at `240e2c`: flag byte `13fde1` bit 2 arms the wait, timer `18` is cancelled,
fault-array offset `0f` is cleared, and reply byte `+9` bits 0/1 clear or set
fault slots `10/11`. These own-ROM facts select the existing compact
request-correlated `74:0d00` HLE contract. This models a successful peer
self-test; it does not execute the missing DSP self-test implementation.

Fresh runtime observes the response at 1.293467 seconds and subsequent
fault bytes `0f/10/11=00/00/00`. The stronger graphical and physical-input
acceptance below uses the same runtime boundary. The eight-second blank
frame precedes text output and does not establish an unmet startup event.

Reproduce using `nsm2hle`, the same observer/private directories and
`-verbose`; run `noki8850_staged_trace_check.py <run>/error.log --runtime-hle`.
This validates the upload/handoff/self-test sequence, not graphical boot.

### 8850 Graphical and Physical-Input Contract

The separate `nsm2hle` research composition reaches an interactive idle
screen using native uploaded bootstrap/verifier/loader execution followed
by declared runtime HLE. Normal `noki8850` remains fail-closed at its final
DSP-result poll; this milestone does not promote the normal machine.

With no card, firmware renders **Insert SIM card** after 8.05 simulated
seconds. With the laboratory SIM and its own acquired PMM, it reads
ICCID, phase, SST, IMSI and all 50 ADN records, then displays **Security
code / OK**. Physical `12345` and the left softkey dismiss the prompt,
show the Nokia startup graphic and reach Menu/Names idle. Further physical
softkeys open Messages, Inbox and the Names menu (Search/Add entry/Erase).
Physical Add entry stores A/123 in SIM ADN record 1, and a preserved-storage
cold restart displays both the name and number through Search/Detail.
Calculator also computes 12+3=15 through physical inputs and its Options
menu. The research composition also completes laboratory Location Updating,
persists EF_LOCI and returns to steady paging/BCCH. Physical outgoing-call
and incoming-call signaling plus registered-operator pixels are verified.
Incoming SMS delivery, physical reading and cold-boot persistence are also
verified. Outgoing SMS has separate acceptance below; speech media remains unverified.

#### Radio Contract

Own-ROM RX assembly at `2cac20` produces a four-byte envelope whose type is
at byte 3. Dispatcher `307346` routes type `80` to `2df198`; its table at
`307380` has thirteen entries for `83..8f` (32-bit entries require the
swap16 halfword rotation). In particular, `8b` reaches `2df56c`, which posts
the packet to task 12, and `89` reaches `2df2f8`, where body bit 0 must match
the pending channel context. This is independent product evidence, not a
copied sibling address map.

The initial organic `55:03050000` scan accepts a laboratory RSSI result and
immediately constructs its own `56/160` candidate window containing ARFCN
1. `RADIO_NSM2` therefore explicitly permits that completed-band-scan to
candidate-window continuation. Release/neighbour/handover calibrations not
yet recovered for this product remain unset. The normal machine is unchanged.

Run `noki8850_radio_observe.lua` for 60 simulated seconds with the isolated
research invocation above, then check:

```sh
.venv/bin/python tools/radio_registration_trace_check.py RUN/error.log --profile nsm2
```

Acceptance requires the handset's Location Updating Request, contention UA,
Location Updating Accept acknowledgement, Channel Release acknowledgement,
both EF_LOCI writes, assigned/released channel confirmations and sustained
post-release paging/BCCH. Own-ROM handler probes additionally show `80`,
`8b` and `89` entering the recovered consumers. This proves laboratory
registration with the declared radio HLE, not RF or missing native mask code.

#### Reproduction and Acceptance

Use fresh private working, configuration and NVRAM directories with
`-noreadconfig -debug -debugger none -verbose -log -video none -sound none`.
Run `nsm2hle` for 36 seconds with
`tools/noki8850_security_input.lua` as the autoboot script, delay zero.
The script changes physical input fields only; all CPU probes are passive.
Then run:

```sh
.venv/bin/python tools/noki8850_staged_trace_check.py RUN/error.log \
  --runtime-hle --startup-readiness --sim-reads \
  --physical-navigation RUN/snap
```

The checker requires native/HLE ownership order, organic readiness, SIM
identity/service/ADN reads, physical inputs correlated with decoded keys,
and stable top-left 72x16 pixel crops from Messages, Inbox and Names.
The crop excludes the animated menu icon and scrollbar. Raw IRQ delivery
alone cannot pass this acceptance. Stock absent DSP-mask warnings are not
evidence of native mask execution.

Use `noki8850_startup_observe.lua` for a passive startup/one-softkey run.
The ten-second and end-of-run frames are authoritative for the first
text: the eight-second sample precedes it. `--display-transfer` checks the
no-card Insert SIM card framebuffer signature, not the SIM/security UI.

For durable phonebook acceptance, run `noki8850_phonebook_input.lua` for
46 seconds in fresh storage, then `noki8850_phonebook_read.lua` for 40
seconds in a new private working/configuration directory while preserving
the first run's NVRAM directory. Do not reseed the SIM or copy a donor PMM.
The first script presses Names, Add entry, A, 123, Save. The second uses
Search/Detail and must not issue any UPDATE RECORD. Check both stages:

```sh
.venv/bin/python tools/noki8850_phonebook_check.py save WRITE/error.log \
  WRITE/nvram/nsm2hle/sim_card WRITE/snap/8850_phonebook_save.png
.venv/bin/python tools/noki8850_phonebook_check.py readback READ/error.log \
  WRITE/nvram/nsm2hle/sim_card READ/snap/8850_phonebook_contact.png
```

The checks require exact A/123 bytes, the other 49 ADN records erased,
organic UPDATE RECORD/commit for Save, a cold READ RECORD without writes
for readback, physical input markers and reviewed display pixels including
the full number. A saved NVRAM file alone cannot establish UI readback.

Run `noki8850_calculator_input.lua` for 52 seconds in fresh storage.
Its own firmware menu order places Games at 6 and Calculator at 7.
The fixture opens Calculator, enters 12, selects Options/Add, enters 3,
and selects Options/Equals. Add
`--calculator-frame RUN/snap/8850_calculator_result.png` to the staged
checker above. It requires correlated decoded inputs and stable arithmetic
pixels for 15, not mere app entry. The keypad star produces a decimal
point on this firmware and is not used as an assumed add shortcut.

#### Board Readiness

`PRODUCT_8850` selects the existing BLB-2 nominal ADC tuple
`000/3ff/2c0/150/140/000/200/000`.
Nokia's [NSM-2 system-module manual](https://www.eserviceinfo.com/preview_html.php?fileid=5444&previewid=2990)
identifies BLB-2, its 68 kohm BSI resistor and temperature circuit.
The tuple is a calibrated board input, **not measured NSM-2 voltage
scaling**. No charger is asserted to satisfy startup.

Task 1 dispatches through the 14-entry BE32 table at `2a1a28`, using
state `138070+4`. State `0d` reaches `2a1bea`; reports `17/16/15`
accumulate readiness `08/0a/0e` at `137fdd`. Report `14` is published
at `2ff870` from `244cb6`, completes readiness `0f` with power nibble
6 at `13ff00`, and permits state 4 and keypad unmasking.
The earlier conservative-input retry is not the current frontier:
owner `24481c` / receive `2463a4` / table `2463bc` uses state at
`1376c0+1c`; its state-3 path `245be4` retries event `49`.
Encoded field `9004`, index 10 in `33f688`, reads CCONT register
`0e` bit 2 (charger presence), not an ADC sample. Do not assert that
bit or inject task-13 event `21` to manufacture readiness.

#### Keypad

Own scan `3015c0` drives row pins 1..4 and returns raw row*5+column.
Decoder `30540a` uses selector `13803b` and table `33f504`.
For the observed selector zero:

| Row pin | Column 0 | Column 1 | Column 2 | Column 3 | Column 4 |
| --- | --- | --- | --- | --- | --- |
| 1 | 11 | 19 | 01 | 02 | 03 |
| 2 | 0e | 17 | 04 | 05 | 06 |
| 3 | 0f | 18 | 07 | 08 | 09 |
| 4 | 10 | 1a | 0c | 0a | 0b |

Physical call evidence identifies `0e` as Send and `0f` as End (column 0,
row-pin bits 2/3). Entries `11/10` do not initiate/end calls and remain
unmapped rather than carrying misleading Send/End labels.

#### Outgoing Call Acceptance

`noki8850_outgoing_call_input.lua` completes the physical security/menu
sequence, returns to idle, dials `5551234`, presses Send and later End.
The own-ROM traffic configuration is
`041202000271012fc10000010000000400000000`. Physical End emits
`041202001117001a600000010000001400000001`, independently establishing
release parameter `14` for `RADIO_NSM2`.

Run the fixture for 60 simulated seconds in fresh isolated storage, then:

```sh
.venv/bin/python tools/noki8850_outgoing_call_check.py RUN/error.log --frames RUN/snap
```

The gate requires Send/End decoded as `0e/0f`, CM Service Request/Accept,
exact Called Party BCD digits, SETUP/Proceeding/Assignment/Alerting/Connect,
handset Connect Acknowledge, physical Disconnect, Release/Release Complete,
RR release, the product-local channel transaction and confirmation, and
return to paging. Reviewed operator-only pixel crops show numeric `001 01`
before dialing and `DCT3 LAB` after release, excluding animated indicators.
The wire also contains `860b`/`840a` speech-control commands, but no NSM-2
speech-media contract is selected; this acceptance does not prove audio.

#### Incoming Call Acceptance

Copy `fixtures/noki8850_incoming_call/nsm2hle.cfg` into a private run's
configuration directory. Run `noki8850_incoming_call_input.lua` for 45
seconds with fresh NVRAM. The fixture completes physical security input
without continuing menu navigation during ringing, answers with Send at
20 seconds and hangs up with End at 28 seconds. Check:

```sh
.venv/bin/python tools/noki8850_incoming_call_check.py RUN/error.log
```

Acceptance requires IMSI paging, Paging Response, cipher/MM exchange,
incoming SETUP, own 11-byte Call Confirmed, Alerting, TCH assignment,
physical Send/Connect and network acknowledgement, physical End/Disconnect,
CC/RR release and idle paging. Captures show caller `5551234`, connected
call presentation and return to `DCT3 LAB`. No additional firmware or
device behavior change was required; this remains signaling-only acceptance.

#### Incoming SMS Acceptance

Copy `fixtures/noki8850_incoming_sms/nsm2hle.cfg` into a private run's
configuration directory and run `noki8850_incoming_sms_input.lua` for 43
seconds with fresh NVRAM. The laboratory delivery occurs during startup;
the fixture subsequently opens Messages, Inbox, the sender entry and Read.
It does not claim a post-startup new-message notification banner.

The handset's own no-cipher publication is
`14:0080ffffffffffffffff0000`. SAPI 3 receives segmented CP-DATA, firmware
persists `hello` from `5551234` in EF_SMS record 1, and CP/RP acknowledgements
close before RR release and return to paging. Physical Read displays the
text and changes record status from unread `03` to read `01`.

```sh
.venv/bin/python tools/noki8850_sms_check.py WRITE/error.log \
  WRITE/nvram/nsm2hle/sim_card WRITE/snap/8850_sms_read_4.png
```

Repeat the same physical input fixture in a fresh working/configuration
directory with **no incoming-SMS config**, preserving only the previous
NVRAM directory. Its Inbox/Read path again shows `hello`, without a new
delivery or UPDATE RECORD. Check that run with the same storage path and
`--preserved`. Both modes verify exact SMS-DELIVER bytes/read status, physical
Read decoding and reviewed text-body pixels. The fresh mode additionally
requires the full delivery, CP/RP closure and paging return. Outgoing SMS
is not established by these checks.

### Outgoing SMS

`tools/noki8850_outgoing_sms_input.lua` uses physical keys to open
Messages/Write messages, enter `A`, select Send, enter `5551234` and confirm.
Run it in a fresh private directory on `nsm2hle` for 60 seconds with the
same ROM paths and no incoming-message configuration. Validate the log:

```sh
.venv/bin/python tools/noki8850_outgoing_sms_check.py RUN/error.log
```

The firmware emits exact CP/RP/SMS-SUBMIT bytes
`390118000100069121436587090d11010781551532f40000a70141`:
GSM-7 `A`, destination `5551234`, SMSC `1234567890`. The laboratory peer
accepts one submission; network CP-ACK/RP-ACK and handset CP-ACK close the
exchange, followed by acknowledged RR release and paging return. The final
captured frame returns to the text editor, not a claimed success banner.
This verifies signaling against the declared HLE network, not delivery to
an external carrier or speech media.

### Capability acceptance boundary

On the final radio-enabled `nsm2hle` composition, fresh physical-input runs
verify Calculator `12+3=15` and exact SIM phonebook Save; a separate cold
run retaining only NVRAM verifies Search/Detail without rewriting ADN.
Together with the independent registration, incoming/outgoing call and SMS
checks above, these establish the requested research software capabilities.
They do not promote `noki8850`: native completion still requires the absent
resident DSP routine at `2c75`. Speech media is also unverified. The runtime
HLE, synthetic SIM and laboratory network remain explicit substitutes,
not evidence for a recovered DSP mask or real-carrier interoperability.

IRQ0 handler `301714` reads pending column bits at `2b` and posts
event `41` through `305370` before acknowledging IRQ0. KBGPIO now owns
that optional pending-column register and the row-pin shift. Defaults
remain unchanged for existing products. Pending columns are accumulated
only for unmasked physical changes, cleared on acknowledgement and saved.
The independent row/power status at `29` is not yet modeled for NSM-2.

Runtime checks observe raw `07/08/09/0c/0d` mapping to digits 1..5,
raw `06` to softkey `19`, and right-softkey `1a`.
Use branch-target probes: `305410` is the raw scanner return and
`2a12e0/2a12fe/2a1358` observe decoded returns. Mid-block return
instructions `30170c/305440` are not reliable hook locations.
ARM debugger actions use `r14`, not the Lua alias `LR`.

#### UI Rendering and Closed Misreadings

Post-readiness `2a1d64` invokes `2d1140` and `2d1120`, posting
scalar catalogue inputs `0731/0735` through `3014b6`.
Task-5 receive loop is `30134a..3013fc`. The eight ordered filters are
`300aae, 2fce32, 27bd84, 270f00, 2c3d38, 271bd8, 266730, 2bd40c`;
a direct-BL census misses the active-context `bx r1` at `2fd09c`.
Context 2f invokes `2ad608`, with mode `13fc6c`, phase `13fc72`
and flag `13fc4b`. Its organic `05dc/05e1` transaction constructs
class-5a content `32ecf6` at object `114604`. Object +4 is the prior
list node, not a missing display parent. Dirty work flushes at `25f514`;
renderer command 6 sets up layout, while command 5 executes text handling
`23e9dc`. Text resolves through `23ea62` to `12fe50`, UTF-16
prefix `0049006e` (In). Missing text and an uninitialized UI task are
not supported explanations.

Glyph `27ed6c` / draw `27e7f8` / blit `27e662` writes framebuffer
`1304d8`. Physical transfer `27f550` reads it, optionally masked by
`1302e0` while flag `13029e` is zero; sampled glyph pixels are nonzero
and mask bytes zero. It transfers 84 columns across six banks through
GENSIO `2e`, then normal-display command `0c` through `6e`.
Resource `7305` also attempts service forwarding through
`304a3c/304a0a/30491c/304944`, but a channel map is not needed for
physical LCD output. The own MSB-first permission grammar (class 73,
bitmap bytes 14/46 mask 10) can enable forwarding, but the no-map
composition renders identically. No unsolicited map is retained.

Timer 51 at `111b18` is a delta-linked queue record, not an independently
decrementing counter. `287950` subtracts preceding intervals; an unchanged
stored delta does not establish a stalled clock. Organic expiry near
31 s follows `02 -> 03 -> 01` and posts `023f` through descriptor
`331ae4+51*8` (`0033e0c8 03050000`). Handler `268dec` invokes
`2f1d9c(0,1)`. Its indexed class-74 refresh checks enable bytes
`137e94+index`; `2f1a28` activates and `2f1e9c` removes entries.
Zero enables on this path are not evidence of a missing hardware peer.

All three independently select CCONT with control `0x22`, write the command
at `0x2c`, poll status `0x6d` bit 2, and read the response at `0x6c`:

| Product | Command setup | Receive-ready loop before correction | PC at eight seconds after correction |
| --- | --- | --- | --- |
| 8250 v5.02 | `0x2feb1c` | `0x2feb2c..0x2feb32` | `0x2cb314` |
| 8850 v5.31 | `0x3030ac` | `0x3030bc..0x3030c2` | `0x2f6e44` |
| 8890 v12.20 | `0x2fd0d8` | `0x2fd0e8..0x2fd0ee` | `0x2f0d40` |

The command byte must initiate receive-ready without requiring control bit 2.
`PRODUCT_8XXX` now configures that existing device contract. No DSP version,
completion, identity or provisioning was added by this correction.

The 8250 writes each ownership word (`0xfe`, `0x100`) to zero 58 times in
alternating order. It then polls shared offset 2 against `0xffff` at
`0x2cb30e..0x2cb314`. The normal 8850 and 8890 machines instead reach
software flag loops. The separate 8850 native/HLE research boundary is
documented above, not inherited by either normal machine. Similar GENSIO
code does not establish identical DSP families or completion semantics.

## Reproduction and limits

Use `tools/dct3_model_scout.lua` with `-autoboot_delay 0`,
`-seconds_to_run 9`, `-log`, `-noreadconfig`, and private working, NVRAM,
configuration and snapshot directories. The script captures CPU PCs and six
screens without changing firmware state. Use fresh NVRAM seeded only from
the acquired product-local PMM. `-verbose` additionally records transport
writes, but the tight final-result poll can produce very large logs.

The declared legacy `dsp_prom/drom/pdrom` uniform-fill audit members are
needed to satisfy the current ROM declarations; they are not executed by this
HLE composition and are not fitted mask evidence. No missing boot ROM was
fabricated. A zero process exit and a PC sample are not phone acceptance.

The 6250's independently recovered reset/GENSIO contracts and current DSP
publication boundary now live in [6250_bringup.md](6250_bringup.md).

## Verification

The initial product-only GENSIO correction preserved the 3210 semantic
baseline and coherent frontier. The SELECT gate's missing records were an
ownership-filtered logging omission: retained board latches are not GENSIO
endpoint registers. Separate passive `gensio_select` records restore coverage
without changing register ownership or behavior; `verify-gensio` now passes
both 3210 firmware revisions. After adding the separate live staged-code
composition, the normal 8250 boundary, 3210 baseline and coherent frontier
still reproduce. C54x core conformance passes, including the new BLEQ cases.
The tool suite passes; all 13 MAME overlay patches apply to the pinned
upstream commit.
