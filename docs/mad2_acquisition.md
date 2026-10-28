# MAD2 hardware acquisition references

Software-accessible MADos and physical capture methods, their exact scope and limitations. An acquisition route or an external annotation is not an admitted peripheral contract. [The hardware atlas](hardware_atlas.md) owns physical-part identities.

## MAD2 hardware-acquisition routes

## Retained MADos source reference

The local source archive is
`roms/research/nok-mados/Nok-MADos-c4b9591e243a74484e27becf343655bb459f99b7.tar.gz`,
from [Nok-MADos](https://github.com/fandigunawan/Nok-MADos) revision
`c4b9591e243a74484e27becf343655bb459f99b7`, with SHA-256
`e568f33b16afecc05a9ae3cbb94590bf4eff2db0173d81ef568ad0ea27546802`.
This LGPL source is a research reference, not code imported into the devices.

`include/dspblocks/block00.h` through `block1B.h` preserve 28 upload
descriptors, 20 with nonzero declared sizes. `hw/dspblocks.c` exposes blocks
`00`, `01` and `14` as initialization, MDI setup and secondary initialization.
These are upload payloads, not resident mask-ROM captures. Their product and
firmware provenance must be established by byte comparison before any use in
a handset configuration; archival MCU addresses alone do not establish it.

A complete-payload comparison against 58 collected `.bin`, `.fls`, `.mcu`
and `.mcusw` files of at most 16 MiB found all 20 nonempty payloads in
`roms/noki3310/3310f639e.fls`, as big-endian 16-bit words. Parsed payload
lengths equal the declared descriptor sizes for all 20 blocks. The three
exposed blocks match at these **file offsets**, not MCU program addresses:

| Archival block | Words | 3310 v6.39 file offset |
| --- | ---: | --- |
| `00` | 638 | `0x1020c0` |
| `01` | 2986 | `0x1025cc` |
| `14` | 629 | `0x108128` |

All 20 matches also have the archival five-word header immediately before
the payload, within the firmware's six-word descriptor layout (12 bytes).
The sixth word is zero in every case. For blocks `00`, `01` and `14`, the
descriptor file offsets are respectively `0x1020b4`, `0x1025c0` and
`0x10811c`. This checks both payload identity and descriptor metadata; it
does not establish which blocks a particular runtime boot requests.

The own 3310 v6.39 image also contains the complete ordered pointer catalogue
at file offset `0x0f35bc` (MCU address `0x2f35bc`, image base `0x200000`).
All 28 entries index the matching six-word descriptor and complete payload,
including the eight zero-length blocks. Verify this stronger correspondence
by supplying only that image to the matcher with
`--catalogue-offset 0xf35bc --image-base 0x200000`. This is a static catalogue
contract, not proof of its MCU reader or the runtime block-request sequence.

The v6.39 boot initializes a RAM copy at `0x1107c0`: passive catalogue
reads at PC `0x200194`, beginning at 0.007424 s, show source `0x2f35bc`
and destination `0x1107c0` advancing together. The RAM-table literal has
three statically decoded consumers:

| MCU site | Observed instruction contract |
| --- | --- |
| `0x2b93bc..0x2b93cc` | Loads the RAM-table base, selects `base + index*4`, reads descriptor destination and size, and skips the payload path for size zero. |
| `0x2bcad4..0x2bcaea` | Loads entry zero, reads size at `+4`, and forms payload address `descriptor+12` and staging field at `+6`. |
| `0x2bcfbc..0x2bcfce` | Selects an indexed descriptor and copies its header halfwords into another structure. |

The base literals are at `0x2b974c`, `0x2bcb6c` and `0x2bd1f4`.
`tools/noki3310_upload_catalogue_observe.lua` records only existing memory
reads and CPU registers, with a per-PC log cap and an uncapped total. In
the 16-second research-HLE boot, flash-table reads also occur in a strided
reader at `0x2bc904` and a bulk reader at `0x2d292c`; neither observation
alone is evidence of a DSP block request. Runtime selection at the RAM
consumers is separately observed below. No resident-ROM compatibility follows
from this MCU-side mapping.

The passive RAM-table tap records exactly three reads in a fresh 16-second
v6.39 research-HLE boot:

| Time (seconds) | MCU reader | Table entry | Descriptor |
| ---: | --- | --- | --- |
| 0.106164 | `0x2bc890` | `1b` at `0x11082c` | `0x3091d8` |
| 0.220730 | `0x2bcad6` | `00` at `0x1107c0` | `0x3020b4` |
| 2.156058 | `0x2b93be` | `15` at `0x110814` | `0x308628` |

The entry-`1b` access adds a fourth consumer to the three base-pointer sites:
`0x2bc88e..0x2bc8a4` loads the fixed entry pointer, reads size at `+4`,
and forms the payload address at `+12`. Entry `15` is selected with index
offset `0x54` and enters the previously decoded indexed path. Blocks `01`
and `14` are not selected through this RAM table in this window. This
bounded observation does not establish the complete request lifecycle,
successful native execution, or the absence of other selection mechanisms.

The own v6.39 entry-`1b` extraction is supported by
`tools/extract_nsm3_verifier.py --product 3310`: 223 words, SHA-1
`6646da3c5be9c70deda7e0b5b9f257d5d2ace815`, from descriptor file offset
`0x1091d8`, guarded by the complete own-image hash. Static initialization
at `0x2bc8ae..0x2bc8d8` writes the tuple
`0100 0300 0000 e800 0001 0001 0200` beginning at MCU HPI `0x100f6`.
The verifier's flash feeder starts at MCU `0x200040`, samples halfwords
at stride `0x20`, sends 115 full 512-word chunks followed by 510 sampled
words and two `ffff` words. Thus the own MCU establishes the 116-chunk
geometry independently; it is not inferred from the matching 8210 payload.
`verify-3310-native-verifier` executes this own-input payload in standalone
native C54x configuration `nhm5verify`, without a donor mask ROM. It requires
all 116 ordered chunks and fails closed at the first unsupported peripheral
read: port `0x2d`, PC `0x0f9f`. Explicit comparison variants supply the
existing COBBA model and PROM-version inputs; they publish at PC `0x0f64`
with own-flash calculation fingerprint `ce0f433f` and PMST `ffa8`. Changing
the supplied register-F value changes the published result, and the ROM4
variant changes the supplied version result. These are peripheral
sensitivity checks, not identification of the fitted DSP's PROM version,
COBBA register values, a native handset boot, RF acquisition or speech.

This establishes byte identity, not unique archival provenance or a compatible
resident DSP ROM. Other products share complete individual payloads: for
example, 3210 v6.00 matches blocks `05`, `06`, `0f` and `16`. Raw images were
searched in both 16-bit byte orders; transformed/container offsets require
their own loader mapping. No handset configuration changes follow from this
comparison. The useful next boundary is the matching firmware's descriptor
and upload consumer, rather than executing these blocks against an unrelated
mask ROM.

Reproduce complete-payload matches with
`tools/dsp_upload_reference_match.py tmp/nok-mados-reference/include/dspblocks roms/noki3310/3310f639e.fls roms/3210f600a.fls`.
The JSON includes image and payload hashes, all occurrences in both byte
orders, and explicit file-offset labeling; malformed or size-inconsistent
source arrays fail rather than silently contributing partial signatures.
Each occurrence includes its preceding six-word descriptor candidate and
an explicit comparison with the archival five-word header. A payload match
without a matching descriptor is retained as such, not promoted to a
matching upload contract.

Inspection of `hw/dsp.c`, `hw/m_dsp.c`, `hw/dspblocks.c` and their DSP headers
does not identify the 8810 port-`0x36` completion-bit owner or its timing.
The MCU SIMI offsets in `include/hw/ioports.h` are a separate address space
and do not supply a DSP port mapping. This bounded negative result does not
exclude other historical sources.

## Capture boundary

A September 2026 public-source census found no complete, reproducible MAD2 DSP
proxy build. The current external emulator contains a remote-DSP transport and
describes a work-in-progress bridge to a custom MADos handset image, but its
public tree contains neither that phone-side image nor DSP-proxy wiring or
firmware. Its published ESP32 design is a SIM-card reader and is unrelated to
DSP register capture.

The preserved
[`Nok-MADos`](https://github.com/fandigunawan/Nok-MADos) tree nevertheless
contains the useful historical foundation. `hw/dsp.c` includes AlexD-attributed
C54x bootstrap words, uploads code through the ordinary MAD2 HPI window, and
contains a `dsp_readmem()` path that copies DSP-visible results into phone
flash. `apps/dspread.c` repeatedly releases and resets the DSP with cycle-offset
timing to recover protected program-ROM contents into MCU-visible RAM. This is
the documented security-hole family behind the ROM4 dump, not an operational
register capture. The existing payload initializes its own architectural state
and does not report the pre-operational `IMR` value required here.

That code makes a future no-solder experiment plausible: flash a custom MADos
image through an ordinary DCT3 flash/service cable, upload a small DSP payload,
and return captured words over MBUS. It does not eliminate the need for a real
ROM4 handset. Capturing the load-bearing boundary also requires arranging the
payload before the operational ROM performs its `IMR` OR-mask sequence; dumping
registers after a separately reset diagnostic payload would answer a different
question.

Two lower-level alternatives are documented but weaker:

- A published 5110 forensic experiment identifies MAD2 `JTRST`, `JTCLK`,
  `JTDI`, `JTMS` and `JTDO` test pads and uses fine-wire soldering for ARM-side
  JTAG access. That can observe MCU memory and the HPI window, but does not by
  itself expose the embedded C54x architectural registers.
- The NSE-1 service manual identifies `DSPXF` at test point J222 and documents
  phase transitions for DSP initialization, synchronization and registration.
  A logic analyser can validate phase timing, but the one-bit flag cannot
  recover `IMR`.
  The [NHM-3 manual](6250_bringup.md#product-documentation-boundary) supplies
  an additional board mapping, `DSPXF` at J314, with initialization/download,
  synchronization and registration diagrams. Its embedded captures predate
  the manual and are not independently established 6250 timing measurements;
  retain the phase vocabulary without importing numerical delays.

The historical
[`5110 DSP ROM v4` discussion](https://nokiafree.org/forums/archive/index.php/t-39175.html)
also rules out a generic ICE or Code Composer Studio recipe: the authors state
that the on-chip protection blocks ordinary external reads and that the dump
used a ROM-code security hole. No public service-tool command, NokiX script,
JTAG recipe, ESP32 design or archived binary was found that captures MAD2 DSP
MMRs at the required boundary.

The smallest credible future hardware task is therefore not a new electrical
bridge. It is a MADos-derived ROM4 capture image plus an MBUS result protocol,
run on a real 5110 with a conventional flash/service cable. Before attempting
it, obtain or reconstruct the unpublished proxy details and verify that the
capture executes before operational receiver initialization.
