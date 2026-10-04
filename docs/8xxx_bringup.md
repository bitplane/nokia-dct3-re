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

The associated 6250 v5.03 cold scout, still using `PRODUCT_DEFAULT`, parks
at `0x4e7dca..0x4e7dce` polling CTSI reset-ready bit 4. Its release contract
requires independent recovery before inheriting any NPE-3 configuration.

## Verification

The initial product-only GENSIO correction preserved the 3210 semantic
baseline and coherent frontier. The SELECT gate's missing records were an
ownership-filtered logging omission: retained board latches are not GENSIO
endpoint registers. Separate passive `gensio_select` records restore coverage
without changing register ownership or behavior; `verify-gensio` now passes
both 3210 firmware revisions. After adding the separate live staged-code
composition, the normal 8250 boundary, 3210 baseline and coherent frontier
still reproduce. C54x core conformance passes, including the new BLEQ cases.
The tool suite passes; all 11 MAME overlay patches apply to the pinned
upstream commit.
