# Acquired 8xxx software frontiers

## Current boundary

The stock 8250 v5.02 PPM K, 8850 v5.31 PPM C and 8890 v12.20 PPM C
execute with their own acquired flash/PMM inputs. None is promoted to graphical
boot, interactive UI, registration or calls. Input hashes and acquisition
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
fabricating a DSP completion is not justified by this condition. Next trace
the execution/dispatch prerequisites of this routine and its other writers.
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
fault bytes `0f/10/11=00/00/00`. Firmware publishes later calibration/config
traffic and reaches its idle loop; the eight-second frame is blank white.
SIM/card, product application registration, keypad mapping and interactive
UI acceptance remain unproved. Do not interpret the loop as a missing wake
event or supply another unsolicited application body without its contract.
Next recover the unattended UI/startup dependency on this composition.

Reproduce using `nsm2hle`, the same observer/private directories and
`-verbose`; run `noki8850_staged_trace_check.py <run>/error.log --runtime-hle`.
This validates the upload/handoff/self-test sequence, not graphical boot.

### 8850 Startup/Input Frontier

Current runtime-HLE composition completes startup readiness organically
and delivers a raw physical matrix press through keypad IRQ/ack, but its
eight-second LCD is still blank. Graphical boot and semantic key mapping
remain unproven. The next boundary is display/application initialization
after task 1 reaches state 4, not the readiness retry described below.

`PRODUCT_8850` uses the existing BLB-2 nominal ADC tuple
`000/3ff/2c0/150/140/000/200/000`, replacing the conservative full-scale
placeholders. Nokia's [NSM-2 system-module manual](https://www.eserviceinfo.com/preview_html.php?fileid=5444&previewid=2990)
identifies BLB-2, its 68 kohm BSI resistor and battery temperature circuit.
The raw tuple is a shared calibrated board input, **not measured NSM-2
voltage scaling**. In one fresh isolated comparison, firmware posts `14`
from `244cb6`, consumes it in state `0d`, reaches `power=06 reports=0f`,
then enters state 4. It unmasks keypad columns (`6b=60`) before the physical
press, which raises IRQ and is acknowledged. No charger was asserted,
firmware state changed, PMM repaired or peer message added by the fixture.
Reproduce with the startup observer and `-verbose`, then check using
`noki8850_staged_trace_check.py <log> --runtime-hle --startup-readiness`.
This gate establishes readiness and physical IRQ delivery only.

The live GENSIO stream carries LCD initialization at `30461c..3046fc`:
command `24`, bank/column addressing, zero-fill data, then extended setup
and normal-display command `0c`. Thus the blank frame is not evidence of
missing serial routing. The own-ROM channel-map handler is `2429a6`:
command `70` invokes `242970`, requires the length field above `42`, and
passes a 64-byte map to `304a72`; command `71` disables it. Map enable is
stored at `13fe78`. Availability reader `304a4a` checks this byte before
using a class-indexed bit map. Neither handler/application nor availability
probe is observed in the fresh eleven-second readiness run. This locates a
candidate application boundary, **not proof that its absence blocks UI**.
The research composition still supplies no unsolicited application map.
Next trace the post-readiness display/application request producer and its
consumer before defining any product-local peer application contract.

The post-readiness tail takes `2a1d64` and invokes `2d1140` at `2a1d98`.
Fresh passive CPU probes observe its catalogue input `0731` entering
`3014b6`, followed by `0735` from `2d1120`. `3014b6` treats `r0` as a
packed scalar input, allocates a 16-byte message, stores its low halfword,
and posts to task 5 through `2885bc`; it is not a descriptor-pointer API.
The corrected observer run exits normally and passes startup readiness.
Next trace task 5's reception and transition selection for `0731/0735`.
The UI-start producer itself is therefore present; neither a missing
readiness report nor an assumed missing channel map justifies injecting
another UI-start event.

Task 5's RTOS receive caller is `30134a` (return `30134f`), proven by
the RTOS task byte `1115d2==5` at entry `2886c0`. Its catalogue wrapper
`301348` reads the packed message halfword and copies optional arguments
according to its top two bits. A fresh run observes queued `0731` and
`0735` at `30134e`; both startup publications reach task 5. The next
unresolved boundary is callback/transition selection after this wrapper,
not missing publication or RTOS delivery. Mid-routine probes that did not
fire were retired rather than used as absence evidence.

`noki8850_startup_observe.lua` installs CPU debugger probes directly on
`:maincpu` and performs one raw column-3/host-bit-4 press at five seconds.
Invoke with `-debug -debugger none`. ARM debugger actions use `r14`, not
the Lua state alias `LR`; an invalid debugger expression can hide a probe's
output. The working reply probe observes class `74`, command `0d`, status
`00`. Queue publication at `28846c` is now observable and bounded to 200
records. Treat absent earlier PC-tap/debug-script records as instrumentation
limitations, not evidence of an unexecuted path.

With the conservative placeholder ADC tuple, the raw press changes physical columns `1f -> 17 -> 1f` but generates no
keypad IRQ because firmware masked all five columns. A write observation
locates `6b=3f` at store `3054ec`, 1.285520 seconds. Entry `3054ac` ORs the
five mask bits after updating its software state. Its complete direct-BL
caller scan finds `2a1ac2`, `2a1bc2`, `2a1dea`; runtime takes `2a1bc2`.
Do not bypass that firmware-owned mask or claim a validated Menu mapping:
the reused host label does not establish this product's semantic key.

The selected startup branch tests byte `137fe0` at `2a1a7e..2a1a8c` and
enters `2a1b30` on value 2. This byte is initialized from helper `2ff716`;
its inputs include boot-state byte `13fec1` (reader `2f6a84`), power-state
byte `13ff00`, two analog samples and a decoded-key predicate. Fresh samples
show boot-state `0a` throughout; startup selector becomes 2 and power-state
becomes `06`. An explicit physical Power hold from script start to 1.5
seconds changes the later selector to 5, but still observes the same
`2a1bc2` mask caller and blank frame. That does not prove the initial branch
changed. Reproduce this comparison with `noki8850_power_start_observe.lua`
instead of the startup observer. No analog values, software state or peer application body were
altered by these observations.

The task-1 post probe at `2885bc` and receive probe after `2886c0` observe
reports `17`, `16` and `15` in a fresh runtime-HLE run. The receive wrapper
`2a0dd8` explicitly returns these three reports, and `14`, unchanged. Only
`37/c8/33/32` reach the separately observed continuation at `2a1be8` in this
run; publication or receipt is therefore not proof that a particular
startup continuation consumed a report. Both probes are capped at 200
records, not an exhaustive producer census.

The comparisons at `2a1bea..2a1bfe` select handlers for
`10/14/16/15/17`. Other inputs call `2a105c`, an interior entry of the wider
startup dispatcher, not a standalone generic handler. Its effects must be
decoded before describing this branch as a five-report wait. The later
tests at `2a1c96..2a1caa` require low nibbles 6 at `13ff00` and `f` at
`137fdd`. Report `14`'s stub `2ff870` has one direct-BL caller, `244cb6`;
neither that stub nor the preceding `244caa` probe is observed in the fresh
eight-second run. This is bounded runtime evidence, not absence of an
indirect producer.

The wider receive loop at `2a1a0c` dispatches through the 14-entry BE32
table at `2a1a28`, using the state halfword at `138070+4`. State `0d`
selects `2a1bea`, so the reports absent from the direct `2a1be8` probe
still reach that continuation through the table. A fresh state trace proves
`17/16/15` accumulate `08/0a/0e` at `137fdd`; input `10` also arrives and
the power nibble becomes 6. Startup remains in state `0d` because readiness
bit 0 (report `14`) is missing, not because `15..17` were lost. The direct
return probe alone was insufficient to identify the consumer.

Report `14`'s owner is the dispatcher entered at `24481c`. Its receive
boundary `2463a4` stores the event at `1376c0+1a`; the 21-entry BE32 table
at `2463bc` selects continuations using `1376c0+1c`. In the fresh run this
state changes `11 -> 4 -> 3`, with repeated event `26` and occasional
`49` (also `4a/4b/4c` earlier). State 3 selects `245be4`; this is an active
firmware lifecycle, not an absent owner. All observation probes are capped
at 200 records. The same run passes the native-upload/runtime-self-test
checker with verbose transport logging.

Next decode the state-3 owner's completion/initialization conditions and
the routes to its report-`14` publication. Establish that contract before
changing analog values or adding a peer response.

The extended 39-second fresh run rules out merely ending before the retry
window: timeout event `49` reduces the state-3 counter at `1376c0+4` from
4 through 0, without report `14`. Its `+a` initialization byte remains 1
and `+d` flag remains 0. The status read at `245c22` uses encoded field
`00009004`: field index `10`, mask `04`, normalized to one bit by
`3030de..3030ee`. The own-ROM field table `33f688` maps index `10` to
command base `70`, hence CCONT register `0e` bit 2; all four observed reads
return zero. This is the modeled charger-present/reset bit, not an ADC
sample. At zero retries the path waits again rather than fabricating a
completion.

Two literal-argument direct posts send event `21` to task `13` at
`245284` and `2457c0`; the latter requires event `42` in owner state 7.
Those are routes to the report-publication branch, not permission to inject
the event. Next establish the no-charger startup/owner initialization
contract and why the current composition selects this lifecycle. Do not
assert charger presence merely to satisfy the observed bit test.

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
`0x2cb30e..0x2cb314`. The 8850 and 8890 instead reach software flag loops;
their ownership and prerequisites are not yet classified. Similar GENSIO
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
