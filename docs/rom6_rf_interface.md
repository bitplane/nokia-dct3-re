# Native ROM6 radio interface

## Current boundary

The acquired-ROM6 **8210 NSM-3 v5.31** composition `nsm3r6` already issues
native radio-facing work. Compare-edge serial output and partial HAGAR
command-state reception are modeled; the separate ADC-pin laboratory source
delivers independently scheduled FCCH and coded SCH samples. Native RSSI,
bounded FIR/mixer/correlation arithmetic and finite-FCCH **control acceptance**
have focused checks. These do not qualify a physically timed RF receiver.
[Native boot acceptance](rom6_dsp_bringup.md) remains separate.

The own-upload 6210 reference-clock [P09-start laboratory candidate](6210_bringup.md#whole-sequence-p09-capture-candidate)
recovers exact FN 704 / BSIC 42 through the native SCH frontend/CRC and
publishes its own success report. It remains legal and interactive for 65 s.
The separate [raw-BCCH candidate](6210_bringup.md#whole-phone-raw-bcch-laboratory)
now decodes three complete SI3 blocks in a coherent 24-second UI run: exact
184-bit information/40-bit parity bus transactions, native FIRE success and
23-octet ring publications. This is a repeated-SI3 decoder experiment, not
complete cell broadcast acquisition, camping or registration.
The separate full-SI candidate qualifies all four broadcast types, their MCU
acquisition-success return, organic random-access submission and native
RA_INFO in a legal 24-second physical-Menu run; see
[the exact scope](6210_bringup.md#complete-si-broadcast-candidate).
The unchanged immediate-gate control still captures only partial SCH.
Neither result establishes fitted MFI timing or RF tuning. An explicit
laboratory block-request latch prevents interrupt coalescence from stranding
queued blocks: the P09 zero-source control now passes 24 s, while the coded
control retains its 65-second SCH result, both without transport faults.
No physical acknowledgement mechanism or FIFO capacity is claimed.

The hardware frontier remains **capture-edge ownership**: which P09/P0a/P0b
events start conversion, admit samples and assert MFI INT0, and how those
events relate to the DSP I/O counter/reload epoch. Under the current declared
P03-level laboratory gate, the NSM-3 first SCH window contains zeros; the
6210 window is nonzero but partial. Both fail native decoding.
Mode-1 product/history and mixer checks do not expose an
arithmetic defect; the P03 bit-8 poll does not prove a missing hardware clear.
Physical SCH/BCCH reception and camping remain unqualified. This is now
distinct from laboratory decoder execution: isolated native SCH and BCCH paths recover
independently generated payloads from filtered I/Q samples.
The separate P05-start candidate clocks complete native buffers at the
declared counter match without generating any reply. The raw-AGCH profile
additionally decodes an external access response and makes the MCU configure
SDCCH and submit SABM/Location Updating. With the explicit synthetic record
family,the native P27 stream now contains independently valid xCCH blocks,
including that exact SABM. An external exact normal-burst decoder now
returns contention-resolution UA as raw I/Q;the native dedicated receiver
publishes the matching successful class80 result. Raw Location Updating
Accept also reaches the native receiver;the MCU acknowledges it and persists
updated test-cell EF_LOCI. A raw Channel Release/DISC/UA exchange returns
the MCU to common control. Native no-identity paging also decodes after release,
and a complete60s run retains updated EF_LOCI and legal execution. Stable
camping/source-frame binding remain open. The acquired PMM still parks at the validation
gate,described below. These laboratory
results do not retire the physical request/acknowledgement or tuning contract.

| Receive scope | Current evidence | Not established |
| --- | --- | --- |
| Isolated SCH | Raw P27 input through native ISR/FIR, frontend, Viterbi and CRC recovers exact FN/BSIC; zero-input control rejects | Physical interrupt/capture timing, organic synchronization |
| Isolated BCCH | Four independent raw GMSK bursts traverse native ISR/FIR, frontend, deinterleaving, weighting and Viterbi to exact SI3/parity; another payload passes, zero samples reject | Hardware FIRE completion, RF tuning/camping |
| Immediate-gate ADC baseline | Native FCCH control acceptance; independently checked raw samples and fault-free provisional staging; first SCH aperture misses its source burst | Physical capture/compare epoch and first-valid-sample ownership |
| Own 6210 P09-start candidate | Complete source burst, native SCH CRC/information/parity and FN704/BSIC42; legal 65-second UI run; zero-source 24-second transport control | Physical capture policy/capacity and acknowledgement; BCCH/camping |
| Own 6210 raw-BCCH candidate | SCH plus three four-burst SI3 blocks, exact FIRE input/result and native publications; coherent 24-second Menu/transport acceptance | Complete broadcast schedule, MCU acquisition/camping, fitted timing/tuning |
| Own 6210 full-SI candidate | Legal 24-second physical Menu, SI1..SI4 native decoding, MCU availability `0f`, observed success return, organic IDLE_RA/RA_INFO and independently checked P21/P27 access bits | Physical release timing/access grant, camping, registration and fitted timing/tuning |
| Own 6210 P05-start candidate | Complete native buffers clocked at the projected P05 matches; same receive/UI checks and exact port-word preservation | Physical pin/timing identity, external network decode/AGCH, camping and registration |
| Own 6210 raw-AGCH candidate | Exact successful native assignment decode; MCU assigned-SDCCH configuration and SABM containing Location Updating Request | Native dedicated transmission, validation-gate completion, camping/registration and physical timing/tuning |
| Synthetic-record raw-AGCH run | Ten-slot validation cycle;combined CCCH;32 normal bursts/8 exact xCCH blocks;native UA/LU Accept and RR acknowledgement;updated SIM EF_LOCI;raw normal release back to common RX | Stable camping/registered presentation and physical timing/tuning |
| Idle-paging source | All three combined CCCH blocks carry exact no-identity fill;25 native CRC-success publications in a complete60s capture;MM storage and Menu retained | Continuous idle reception,source-FN binding and physical carrier selection |
| Addressed paging | The uplink IMSI determines combined PCH phase1/block6;two raw pages decode natively and pass the MCU's paging-group admission | Native Paging Response,stable camping and physical carrier selection |

The CPU additions needed by the normal-burst frontend have generic focused
tests. The rebuilt native ROM6 menu/storage/save-state scenarios, ROM4
coherent boot/RF gate and HLE coherent frontier pass. These regressions guard
existing behavior; they do not turn a laboratory waveform into received RF.

### Native access serialization

The full-SI run produces four P21 start/commit transactions (`control & 0e00`
equal to `0a00` / `0600`), each containing fifteen P27 words. The resident's
12-bit shifts identify the low-12-bit, LSB-first lane. All four streams have
one access-burst header at bit offset 9, correct trailing zeros and exact
36-bit codewords for RA `0b,08,06,06` with BSIC 42. Their separately decoded
octets match native RA_INFO reports for frames `2614,2671,3726,3782`.
The checker never obtains its decoded octet from those reports.

The independent reference implements the CRC6/BSIC colouring/convolutional
rules in [TS45.003 4.6](https://www.etsi.org/deliver/etsi_ts/145000_145099/145003/05.08.00_60/ts_145003v050800p.pdf),
and access tails/training from [TS45.002 5.2.7](https://www.etsi.org/deliver/etsi_ts/145000_145099/145002/15.00.00_60/ts_145002v150000p.pdf).
Only exact noiseless codewords are accepted; no RF demodulation or error
correction is claimed. Missing, partial, aborted and malformed transactions
and report/data mismatches fail the full-SI gate.

`gsm_rach.h` supplies an external exact decoder for the finite P27 format,
without RA_INFO or private firmware-state inputs. Its standalone test checks
all 256 access octets at BSIC 0, 42 and 63 against the independent Python
encoder, the captured native RA `0b` transaction, wrong-BSIC rejection and
every single-bit corruption of that transaction's coded payload. Only the
separate raw-AGCH profile connects it to a response; no-reply profiles are unchanged.

The absent serializer was a generic CPU DAGEN dependency: immediate ST
postdecrement followed by MVDD used the old destination. Own writes at
`7c5d/7c5f` overwrote the buffer pointer at `2657` with `00e0`, leaving
readiness `2656` zero. Generic red/green instruction tests now preserve the
older pointer update before younger Access, without moving data bus clocks;
the native ready word and complete serialized buffers then appear. The
temporary firmware-RAM probes were removed. No missing peer completion or
private-memory repair was supplied.

P05/P06/P07/P08 and P0f are separately programmed around the burst. Buffer
commit is not evidence of their physical pin meanings, a timed RF emission
or a network decode. Their physical meanings remain unresolved even though
the separately named P05 laboratory candidate now completes raw AGCH reception;
never return an assignment merely on RA_INFO.

All four native attempts program P05/P06/P07/P08 as
`071f/08aa/067c/090e` before serialization; P2f is `3a98`. P06 has exactly
four writes in the complete capture. Outside these attempts the idle path
repeatedly writes out-of-range `3a98` to P05/P07 and zero to P08. The first
P21 buffer commit precedes the subsequent modeled P0f `09c4` match, but
that ISR compare is not independently identified as an RF release edge.
The P05..P08 tuple has nested start/end-like ranges within the 5000-count
period; interpreting them as modulation/power comparators is a hypothesis
to test against their writers and the existing counter, not a recovered pin
specification. Preserve the actual control timestamps when testing it.

The read-only full-SI observer now joins each P21 commit to the existing
backend counter and independently projected P05..P08 deadlines. All four
commits occur at counter `062c`; the first is at 12.066136505 s, before the
projected P07/P05/P06/P08 positions at
12.066209846/12.066360308/12.066724923/12.066817231 s. All four targets share
one fractional counter phase. P05's absolute source-frame projections are
`2614,2671,3726,3782`, agreeing with the separately observed RA_INFO fields.
The gate binds the logged controls to actual I/O writes and rejects missing
windows, changed phase, frame disagreement and partial transactions. It
does not assert that any projected event fired, or that P05 drives TX.

This supports a **separate P05-start laboratory candidate**. It must
consume only complete native P21/P27 buffers, preserve
the actual counter deadline, reject/reset/cancel partial work, decode the
actual access bits outside the DSP and derive any response frame from that
timed endpoint rather than RA_INFO or private descriptors. Only subsequent
raw AGCH waveform reception may establish a native assignment. The existing
broadcast-only composition remains the no-reply control; no physical clock,
pin function or fitted ROM identity is promoted by the candidate.

The separate `npe3tp5` composition implements this experiment:

```sh
.venv/bin/python tools/run_npe3_native.py --reference-clock --seconds 24 --adc-source fcch-sch-broadcast --capture-start p09 --tx-release p05
```

It arms from the actual P05 write, not buffer completion or RA_INFO. The
finite P21/P27 collector admits only the observed complete fifteen-word
transaction; partial, overflowed, aborted or replaced loads cannot release.
Invalid P05 values and backend reset cancel pending candidate work. Counter
reloads re-project the compare using the existing model. At a matching edge
the complete buffer is consumed once and emitted as a laboratory event;
neither a firmware completion nor network response is produced.

Four native events preserve every original port word and match the projected
P05 times exactly, with target and counter both `071f`. The checker requires
the explicit candidate identity and rejects missing/duplicate, early/late,
wrong-counter or changed-buffer events. `npe3sp9` remains a no-release,
no-reply control. Logical framing/replacement/reset and mid-load restoration
have executable C++ tests; runtime buffer/control state and its MAME timer
are registered, but a timed handset save/restore is not yet qualified.
Fifteen words is a bounded observed transaction, not a claimed hardware FIFO
capacity; other transmit lengths/formats remain rejected.

### Native raw AGCH candidate

The separate `npe3ap5` profile attaches an external exact access decoder to
the P05 endpoint. It derives RA only from the released P27 bits and derives
the reception frame from the endpoint time at the declared source epoch.
Neither RA_INFO nor private DSP/MCU state enters the response. A valid request
schedules a standards-derived Immediate Assignment in the next complete
combined CCCH block, encoded through FIRE/convolution/interleaving,
normal bursts and GMSK into the existing raw ADC source. No decoded packet,
completion or interrupt is supplied directly.

```sh
.venv/bin/python tools/run_npe3_native.py --reference-clock --seconds 24 --adc-source fcch-sch-broadcast --capture-start p09 --tx-release p05 --raw-agch
```

The earlier non-combined-source control's native RA `0b` at frame2614 schedules frames2617..2620. At12.094406923s
the native type80/class60 publisher carries successful status00, frame2620
and the exact23-octet assignment
`2d063f002000230b09ae00002b2b2b2b2b2b2b2b2b2b2b`.
The independently checked response echoes RA and request reference and assigns
SDCCH/4 subchannel0, timeslot0, non-hopping ARFCN35. Descriptor20 denotes
SDCCH/4,not SDCCH/8,per
[TS44.018 table10.5.2.5.1](https://www.etsi.org/deliver/etsi_ts/144000_144099/144018/05.21.00_60/ts_144018v052100p.pdf).
The raw-AGCH profile now advertises combined CCCH (`CCCH-CONF=1`,SI3 byte10=41)
and restricts AGCH to blocks ending at9,15,19. This matches the assigned
SDCCH/4 on TS0;no-reply broadcast profiles retain their non-combined layout.
The mapping follows
[TS45.002 tables3 and5](https://www.etsi.org/deliver/etsi_ts/145000_145099/145002/18.00.00_60/ts_145002v180000p.pdf)
and TS44.018 table10.5.2.11.1. The earlier acquired-storage24-second control retains
legal execution and physical Menu, but produces only the first access attempt
and no subsequent native RX packets after this assignment. The MCU accepts
the assignment as established by the ordered submissions below, but native
dedicated-channel service, camping and completed Location Updating remain
unqualified.

The source remains untuned at the ADC attachment: broadcast ARFCN35 is a
declared laboratory cell, not evidence of coherent physical carrier selection
or fitted P05 pin semantics. `npe3sp9` and `npe3tp5` remain no-reply controls.

### Assigned-channel submission and validation boundary

The complete read-only MCU ring window (0..24s,56 commits,zero invalid indices)
places two own submissions after the successful AGCH publication:

- At12.098916s, type02 configures a24-byte assigned-channel record with
  channel80 and non-hopping ARFCN35.
- At12.156672231s, type1b submits a28-byte SEND_BLOCK. Its channel prefix is
  `0080`; the23-byte LAPDm block is
  `013f4905087000f000fffe330809101010325476982b2b`.
  Address01/control3f/length49 encode SABM with18 information bytes, whose
  L3 header0508 is a Location Updating Request. Submission is not transmission.

The resident SEND_BLOCK handler calls34b4, where34c6 compares data04fa with7.
The first fetch of3500 has executing branch owner34ca,04fa=0000 and the
actual SABM atAR2=083d. The native error tail sets0866 bit6 and enters IDLE3;
terminalPC350a is this error loop, not ordinary waiting. Therefore an
`illegal=0` terminal alone cannot qualify dedicated service.

The startup write-watch observes04fa cleared at0.609381143s and written zero
by all three validation writers45b4/45bc/45e6 at0.741934681/0.742544549/
0.747797758s. These are actual data writes; speculative program fetches and
stop-time program snapshots must not be counted as executed setters. Static
45b2/45ba/45e4 clear bits0/1/2 and conditionally set them from equality tests;
34c6 requires all three. No validation bit or branch is overridden.

Early own type70 inputs are13/4-byte seed`e1906dcf`,14/12-byte record
`92af475b873dfecf1ec705d7`,15/20-byte identity record and16/24-byte record.
The seed is byte-identical to the final four bytes of the acquired MCU image.
The native MSID response`832445fa5f92b9f7d13be70b52` independently decodes via
`dct3_msid_codec.py` to`e1906dcf00160010acadabda`: requested seed, modeled
COBBA serial00160010 and family83 signature. This validates that reply's
arithmetic, not the persistent security records or every later transform.
At the bit0 setter the preserved startup fingerprint is`f65a:0d46`, while
the six-word transform's first pair is`0dfd:5edc`. Their mismatch, the
bit1 equality and the bit2 record validation are the next bounded contracts.

The offline`dct3_rom6_record_codec.py` now reproduces all six native FAID
words`0dfd5edca196a316f99c5f96`, not just the rejected checksum pair. From
the acquired DROM it uses the explicit B74F key, XORs chip00160010 into its
first four bytes, and applies the resident8555 key preconditioning
(`reverse_words(linear_mix(key))`) before the twelve-round transform with
B761 schedule. B755 likewise reproduces the full decoded identity
`c21e6d4c90c54e5c649e096a`; XORa5a5 yields the observed04ef..04f4 contents.
B749 reproduces both lock blocks from the native3532 reply, including final
words294d/e5cd which differ from the B779 marker54c2. This is independently
checked arithmetic evidence for the actual stored-record/model mismatch,
not permission to force04fa or to copy a donor provisioning repair. The
inverse helper accepts explicit plaintext/table/chip only and edits no storage.
The declared external record fixture below isolates that validation contract
from the acquired store's unknown donor identity. No additional AGCH or UA
can release the acquired-input error loop.

### Synthetic record-family fixture

`--provision-records` adds an explicit, separate storage experiment to the raw
AGCH run. It never changes the acquired PMM or the machine defaults. The
fixture declares example IMEI14`49015420621001` and the already observed model
chip00160010, not fitted donor identity. All tables come from the pinned DROM.
FAID plaintext uses the existing factory-fixture convention: flash seed XOR
17ca6089, its complement and four zero bytes. Here that checksum equals the
independently computed native startup fingerprintf65a0d46 exactly. The identity
record contains the explicit example BCD bytes and duplicates them in the
MCU-readable field. All ten lock slots explicitly carry the same existing
unlocked-fixture convention. These policy bytes remain a laboratory input,
not a factory claim or an inference about the donor records.

Storage obligations are independent of those decrypted-record checks. The own
PMM journal has a2500-byte base payload at26,295 writes ending1bb0,and no
later write to cache0..10f. MCU387fb4 compares sum16(cache[0:11c])
against the BE32 field at11c;4a18b0 is a byte-sum,not CRC32. Failing to update
it produces fault slot18/value12 and clears host17fd99 bit6 at3028dc,
leading to CONTACT SERVICE even with all DSP validation bits set. The fixture
therefore recomputes this low-block sum and the base record's checksum byte,
preserving all unrelated cache bytes and journal entries. Its manifest records
source/derived PMM hashes and the synthetic identity.

The early program snapshot taken at the actual45b4 data write establishes the
own upload at0c01..0c47 (MCU file24f7e,142 bytes,
SHA256`e89cb6d0a3d680a9d0a321549a6f6f9583cbaf7c3b975c44f521f655a724752c`).
0c0d..0c16 compares the decoded FAID pair with04f7:04f8. The identity arm
0c30..0c46 compares decoded words1/2/3 and word4's high byte with the seven
unencrypted BCD bytes before45ba. A terminal program snapshot is unsuitable
for this startup code because later acquisition uploads replace it.

The MCU itself reads offsets20,38,50,68,80,98,b0,c8,e0,f8 and20 again.
The initial call returns to387471;the ten follow-ups return to387f3f.
Own387ef6..387f50 increments the index,wraps at10,rereads slot0 and marks
the final cycle. No index,request or completion is supplied by the fixture.
Eleven native validations each clear then restore bit2 through45e6/45e9;
04fa settles at7,with zero writes by the3507 error tail. Qualifying only the
initial7 is insufficient:retaining the acquired next-slot placeholder makes
the second validation fail and leaves3. All records and both outer checksums
must agree.

The complete24s gate retains legal execution and exact physical Menu. The
longer startup changes the first decoded SCH fromFN704 to715 and the first
SI cycle starts withSI4,not SI3. The provisioned run therefore checks the
actual source-correlated SCH and every successful broadcast through a complete
four-type cycle;the no-reply fixed-profile expectations remain unchanged.
Its earliest FIR prefix is correctly zero;the later fully source-correlated
SCH proof qualifies progress,not a claim that this initial prefix was nonzero.

With combined CCCH,native accessRA0b atFN3422 receives raw AGCH
at3423..3426. After the generic
circular-logic admission and the FIRE encoding contract below,40 normal P27
bursts form10 independently exact xCCH blocks in the no-UA control. The first is the MCU's SABM/LU
byte-for-byte;later blocks include retransmissions,SACCH reports and idle UI.
Without raw UA or Location Updating Accept,retransmission/return
to search is expected. Port-level coding is qualified,not physical emission
or registration. The first SCH remains fully source-correlated atFN715.
The combined-cell run records three successful SCH attempts,including the
initial result,and three failed attempts: the checker matches every attempt's status,
information words and order to its actual native class40 publication;only
successful attempts qualify acquisition. Failure rows cannot be dropped,
promoted to success,or accepted before the initial source-qualified result.
The raw UA extension is described below;Location Updating is still incomplete.

### Native raw contention-resolution UA

The external BTS accepts only exact fifteen-word normal serializer buffers
with unique tails/training and xCCH stealing flags. Four consecutive timed
SDCCH/4 subchannel0 uplink frames37..40 are deinterleaved and checked using
the shared convolution/FIRE decoder,with corrected-bit count required to be
zero. The port adapter uses LSB-first air-interface octets explicitly;the
older traffic helper's MSB-first packed-block convention is not reused.
Independent Python vectors test this distinction,including walking bytes,
SABM,wrong training,corrupted coding and malformed LAPDm headers.

For a complete SAPI0 SABM with P=1 and valid length,the BTS echoes its
information unchanged in UA with F=1. The first request's information is
retained;different subsequent contention requests are ignored. Padding is
not part of the contention identity. This follows
[TS44.006 sections6.3.2,6.8.8 and8.4.1.4](https://www.etsi.org/deliver/etsi_ts/144000_144099/144006/04.00.00_60/ts_144006v040000p.pdf).
The reply is encoded with assigned TSC0 and scheduled at the next TS0
SDCCH/4 downlink22..25,not an AGCH block. Only raw ADC I/Q is supplied;
neither private memory nor decoded MCU/DSP messages feed the BTS request.

The no-idle-paging24s control decodes SABM at uplink3454..3457 and schedules UA at
3490..3493. Native type80/class80 successfully publishes the exact UA
`01734905087000f000fffe330809101010325476982b2b` at16.123703494s,
with frame3493,BSIC42 and ARFCN35. The acceptance checker independently
joins every BTS uplink to timed port decoding,verifies the response schedule
and echo,and requires a unique successful native publication. Native uplink
then contains idle UI rather than SABM retries;the UA-only control has48
normal bursts/12 valid xCCH blocks. UI/Menu,complete record validation and legal execution
remain intact. This proves dedicated native reception,not MCU link completion
or Location Updating by itself. The UA-only control submits idle UI at
16.158123769s and later reconfigures common control at17.632165538s.

### Native Location Updating acceptance

The explicit test-cell policy sends plain,unciphered Location Updating Accept
after UA,using the existing network message builder and only the decoded
air-interface request as subscriber input. Authentication/ciphering are not
claimed. The IMSI is echoed as a Mobile Identity IE,with advertised
PLMN001-01/LAC1. This follows
[TS24.008 sections4.4.4.3,4.4.4.6 and9.2.13](https://www.etsi.org/deliver/etsi_ts/124000_124099/124008/03.20.00_60/ts_124008v032000p.pdf).
LAPDm is a BS command,I-frame N(S)=0/N(R)=0;the first next SDCCH/4 block
after UA carries
`030045050200f1100001170809101010325476982b2b2b`
at3541..3544 in the no-idle-paging control. Native class80 publishes the exact block at16.359095143s;
the MCU submits RR `032101` at16.393637846s and the timed native uplink
3556..3559 independently decodes to that acknowledgement.

The fresh fixture directory contains no pre-seeded SIM file. Its saved
EF_LOCI changes from the card's unregistered initialization to
`ffffffff00f1100001ff00`:no allocated TMSI,advertised LAI,and update status00.
The runner retains fixture storage,checks this exact record,and leaves all
acquired ROM/PMM files untouched. This qualifies native MM acceptance and
SIM persistence,not stable camping. The LU-only control returns to common
control without a network RR Channel Release;normal release is qualified
separately below. SACCH system information is still absent.

The Messages Menu oracle is deliberately re-banked only for the provisioned
LU composition. The reviewed raw pixel hash is
`5adef93609b763a40d0e3c9dca356d388a910bb393a518e99a53eec2c7184202`;
comparison with the preceding UA frame changes only envelope-animation
pixels25..48,33..39. Ordinary native and no-reply profile hashes are unchanged.

### Native normal channel release

The external BTS waits for exact timed `RR N(R)=1` after LU Accept before
transmitting RR Channel Release,cause0,in network I-frame N(S)=1/N(R)=0.
In the no-idle-paging control,uplink3556..3559 supplies that acknowledgement. The raw
reply `03020d060d002b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b`
occupies downlink3592..3595 and native class80 publishes exact success at
16.594479846s. Native uplink3607..3610 acknowledges N(R)=2;uplink3658..3661
then carries DISC `015301`. Only this real command triggers empty UA
`017301` at3694..3697,decoded natively at17.065241846s. The MCU reconfigures
common control at17.067163462s,immediately after UA rather than the LU-only
control's later17.632s return. DISC P=1/UA F=1 follow
[TS44.006 section8.4.4.2](https://www.etsi.org/deliver/etsi_ts/144000_144099/144006/04.00.00_60/ts_144006v040000p.pdf).

The checker binds all replies to independently decoded timed uplinks,checks
both native downlink publications,and requires the actual MCU common-channel
configuration within100ms of closing UA. Missing/changed RR,DISC,coding,
downlink status/sequence or common configuration reject. New observed RACH
starts a new single-MS laboratory connection;it does not modify handset
registration state. Physical Menu retains the LU-phase exact hash,EF_LOCI
remains updated,and all32 normal bursts form8 exact xCCH blocks. The idle
frame displays `DCT3 LAB`,but the UA-only control also displays that SIM name:
the label alone is not a registration/camping oracle. Idle paging and the
longer compatibility run are qualified below;stable camping and fitted RF
tuning remain separate contracts.

### Native idle paging and extended observation

The raw laboratory BTS now fills each combined common-control block
(6..9,12..15,16..19) with Paging Request Type1 containing a one-octet
"no identity" Mobile Identity
([TS44.018 9.1.22](https://www.etsi.org/deliver/etsi_ts/144000_144099/144018/05.21.00_60/ts_144018v052100p.pdf),
[TS24.008 10.5.1.4](https://www.etsi.org/deliver/etsi_ts/124000_124099/124008/03.20.00_60/ts_124008v032000p.pdf)). The23-octet source is
`1506210001f02b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b`.
This is the existing network `paging_fill()` grammar,not a request addressed
to the emulated subscriber. TSC2/FIRE/convolution/interleaving/GMSK use the
same source path as BCCH;an actual access grant overrides its scheduled
common-control block. No samples are selected from MCU messages or RAM.
The independent reference checks all three block positions,latched I/Q and
off-window silence;native class60 publications must carry the exact fill or
the independently checked actual assignment. Failed common blocks are counted,
not treated as successful paging.

The60s capture retains the exact Messages Menu oracle,all ten record slots,
SCH715/BSIC42,the complete SI set,one native access,raw LU/normal release,
and updated EF_LOCI. Its later traffic is not another registration attempt.
Adding valid common traffic changes acquisition chronology without changing
clock/sample knobs:access2708 gets AGCH2709..2712;SABM2740 gets UA2776;
LU Accept starts2827,RR acknowledgement2842 triggers Release2878,DISC2944
gets closing UA2980,and the MCU returns to common RX at13.771456385s.
The source produces25 CRC-successful paging-fill publications from6.389976857s
through37.931345934s;reported receiver keys include35 and46. There are33
failed common-block publications. This is an untuned ADC composition,not
evidence that one physical carrier is received on both channels.

Two limits are explicit. First,the repeated identical fill cannot uniquely
identify an individual source block. Six publications precede the nominal
time of their reported block-ending FN,including FN1386 at6.389976857s and
FN8220 at37.931345934s. The checker preserves every discrepancy and leaves
source-frame binding unqualified;no clock/tolerance is fitted to conceal it.
The formatter's origin and conditional adjustment are now established below;
that is not unique source-block provenance. Second,sparse MCU publications
do not imply a stalled receiver:the firmware intentionally filters some
decoded no-identity fills. The recurring host4a/body011b is a scalar update,
not a missing-reply request. Stable camping still needs addressed paging/DRX
evidence rather than an idle label or a count of forwarded empty pages.

Extended observation covers the requested horizon,not just the first24s:
MCU and DSP ring captures use cap2048 for longer raw-profile runs,and the
passive write journal scales its bounded24s budget (maximum120s). Both
footer and complete-row checks still reject truncation. The complete60s
journal has2,199,755 writes. An initial capped capture was rejected;after
correcting both producer and analysis bounds,the completed uncapped capture
was reassessed through the production checks without re-executing firmware.
The native8210 Menu,state and cold phonebook controls,the ROM4 coherent/RF
boundary,and the HLE semantic/coherent frontier controls also pass.

```sh
.venv/bin/python tools/run_npe3_native.py --reference-clock --seconds 60 --adc-source fcch-sch-broadcast --capture-start p09 --tx-release p05 --raw-agch --provision-records
```

### Native idle metadata and forwarding contract

Formatter4ee0 selects primary frame0912/0913,BSIC250a and receiver250e when
2f1e=0;otherwise it uses secondary frame2456/2457,BSIC2616 and receiver2618.
If3eb5 bit9 and0586 bit0 are both set,4f27 copies the low frame and4f28
adds2. Otherwise4f2c copies it unchanged. A bounded24s write-observer run
records10 such two-instruction adjustments across61 formatted class60 blocks.
For example,the primary low word0568 becomes published056a at6.38997s.
This explains the nominally early reported FN without changing any emulated
clock. It does not identify which repeated air block supplied the samples.

The61 blocks pass through native gate50b2. Non-idle/acquisition modes bypass
the paging-pattern filter. In idle mode,the filter uses051d bit6 and patterns
2625/2626 (set by host46),with264e controls permitting an override. Its
return is tested at4d6e;zero suppresses publication. The trace joins every
actual gate return to its formatter and actual committed RX payload:56 are
forwarded and5 correctly decoded no-identity fills are filtered. There is no
unexplained publication loss in this window. Some filtered successes occur
after the last forwarded empty fill,so MCU message silence is not a receive
failure oracle. Addressed paging has the separate receive/admission contract
below;it does not yet qualify a native Paging Response or stable camping.

### Native addressed paging

The research-only `Laboratory BTS page` input requests an external network
action after the raw closing UA. The BTS retains only the IMSI decoded from
the actual timed uplink SABM/Location Updating Request;it does not inspect MCU
subscriber state. The ordinary raw-AGCH control leaves this input inactive.
The input constructs a standards-valid IMSI Paging Request Type1 and supplies
only its coded GMSK I/Q. It neither posts a decoded packet nor changes handset
state. The plain test-cell policy remains unchanged.

[TS45.002 sections6.5.2/6.5.3 and table5](https://www.etsi.org/deliver/etsi_ts/145000_145099/145002/18.00.00_60/ts_145002v180000p.pdf)
give three PCH blocks for the advertised combined CCCH with no AGCH reservation.
With BS_PA_MFRMS=2,the transmitted IMSI ending789 selects group3:odd
51-multiframes,block starting at offset6. The nine-block non-combined HLE
schedule must not be reused here. The shared arithmetic has standalone C++
tests over every IMSI suffix,reservation count and allowed multiframe count;
the independent Python checker derives the same slot from the actual IMSI
and checks the source SI3 configuration.

In the24s run,the external input is serviced at18.016666667s. Actual sample
reads encounter addressed blocks starting4035 and4953. Native class60
publishes the exact23 octets at18.629975 and22.866722s (rounded),with native
paging-pattern gate result2 rather than the empty-fill result0. These sparse
sample observations are not proof of a complete DRX listening schedule.

Both objects reach the MCU's457db8 path. Own helper489c7a derives phase1
and paging-block index0 from live control4100. The completed decision at
457e80 hasR6=1,and457a32 receives the original bytes with status0 and the
accepted-group argument1. That forwarder constructs message0623 for task15.
Thus timing/group admission is not the missing boundary for these pages.
Task15 consumes0623,strips the L2 pseudo-length and posts05eb to task18.
The ordinary RR parser copies the on-air IMSI and returns event083e. No new
access or Paging Response is observed within24s;follow that controller
decision rather than inventing a DSP wake/completion. Unique source-FN
binding,physical tuning and stable camping remain unqualified.
The inactive-input control's entire radio-write journal remains byte-identical
to the preceding no-page checkpoint. Temporary heap-read probes were retired
after locating the consumer;the retained observations are bounded entry/post/
parser-return taps,not watches that can misattribute later heap reuse.

### Paging controller continuation

The parser stores083e at172fa8. Helper3cd1bc reads and returns it;the next
receive iteration clears the field at3cceb0. This is ordinary event delivery,
not disappearance of the parsed page. Actual return315450,selector315472
and continuation31543e are joined for both addressed pages.

Controller context169be8 has continuation word169c14=4. Its selector treats
083e,083f and0840 alike:315472 sets byte+0b (169bf3) to1,frees the decoded
object at+28,and commits continuation4 before receiving again. The measured
context byte+6 is1. A separate pending-page drain at315816 clears+0b and
recreates083e;its path to3133b8 additionally requires29fe00(1)==1,byte+3=0
and byte+6!=1. Entry3133b8 sets the request field+0c to80. These are recovered
field/control-flow contracts,not established semantic names for the current
procedure or a complete census of its exit inputs. No field is changed by
the observer or BTS source.

Read-only snapshots at12,14,16,18,20 and24seconds find the channel-confirmation
pointer173fc4 zero each time. This excludes an outstanding confirmation object
at those sampled points only,not transient objects between them;absence of
DSP type89 is not evidence to synthesize a confirmation. The snapshot probe is
retired after recording this bounded result.

A complete bounded field-change trace through24seconds shows byte+6 set at
2.437741846 (315272),cleared at12.046145538 (3153c0,received07ef),and set
again at16.288393385 (315272). Thus an initial missing clear/completion is
disproved:the condition re-enters before paging. The first page sets+0b at
18.632392000 (31547a),and the second leaves the already-set latch unchanged.
The common31525e tail publishes its report,sets continuation4 and writes1
to byte+6;it is shared by multiple status branches,not exclusively07d4.
The trace label `procedure` denotes this byte only,not a recovered procedure
name. The17 observed field changes also distinguish diagnostic selector+1e
from continuation+2c;312c58's selector store is not a mode transition.
The native acceptance checker pins both flag writers and validates continuous
field histories,clear/re-entry ordering and the first-page latch. The full
DSP journal remains byte-identical. Why the ordinary lifecycle re-enters the
condition,and how an RF-selective source changes that lifecycle,remain open;
do not skip the second set or force a pending-page drain.

The re-entry source is an ordinary handset report chain,not a missing DSP
acknowledgment. Constructor30f62c posts07f2 to task18 with a BE32 result at+4.
The native run posts result2 at15.102925077 and result4 at16.271560692,both
through30f660. Result4's observed constructor caller is2d314c;the constructor
deliberately skips30d908's cell-data copy in this case,so remaining object
bytes are not valid cell information. RR receives the second07f2 and publishes
09f7 through314fe4/3ccbec. Handler2a3a18 consumes09f7 and dispatches1580;
the upper controller constructs09c8,translator2a086c maps it to07d4,and the
RR selector reaches the shared byte+6 setter. `check_reentry_source` joins
these actual boundaries with pinned constructor/report instructions;the full
DSP journal retains its baseline hash.

Result4 does not imply a generally unsuitable or missing cached cell. Paired
constructor snapshots retain one cached key46/BSIC42/state4 record. At result4,
its signed fields+0c/+10 are-87/+18;these numeric fields alone do not qualify
all cell eligibility. The candidate list is rebuilt with key46,then cursor
30c9d6 advances to1. Crucially,this advance precedes the selector's final key
test and is not evidence that the cell was accepted.

Selector configuration16839c is mode10 in both snapshots. Its30c998 routine
accepts a looked-up key only if it differs from selected key17251c in mode10;
mode8 has a different acceptance rule. The only cached key46 equals17251c=46,
so mode10 deliberately excludes it. The actual null return is stored at
2d2c94 into168358. This path enters2d0818,not the separate chooser path at
2d3104. Signed criterion16856e changes from-2 in the result2 snapshot to+18
in result4;the negative branch2d0834 produces result2,while the nonnegative
mode10 branch2d0898 sets lower-controller word172518=4 and long-branches to
the shared2d314a report tail. A BL to that tail is not proof that its earlier
candidate-return checks executed. Do not conflate selector mode16839c=10
with lower-controller word172518 changing3 to4.

After the report,the first list is exhausted,the alternate count1683b2 is0,
and fallback lookup key1683a4=00ff has no cached match. The pinned guards of
30f690,30c998 and30e6e4 consequently imply early-null returns on this captured
post-report state without external stubs. These are postconditions,not the
original rejection reason or an absence proof. `check_candidate_snapshot`
pins the actual guards,joins the native null-return write,and checks paired
RAM snapshots. The full DSP journal remains unchanged. Zero-hit interior-PC
probes are retired;do not replace this selection policy with a fabricated
hardware completion or overwrite the list/current key.

The next question is why mode10's current-cell-excluding selection runs after
raw LU/release,and how its completion settles ordinary paging. Do not
inject083e again or clear the context byte. Broad heap watches used to locate
the consumer are retired;bounded return/selector/context observations retain
the contract. The fresh native capture reproduces UI,SIM,LU and raw release,
and the3210 HLE semantic/coherent frontier also passes. The legacy6210 staged
HLE composition does not currently reach runtime in a fresh-storage42s quiet
control;it is not a usable positive paging oracle until its verifier/loader
handoff is requalified.

```sh
.venv/bin/python tools/run_npe3_native.py --reference-clock --seconds 24 --adc-source fcch-sch-broadcast --capture-start p09 --tx-release p05 --raw-agch --provision-records --page-at 18
```

Host4a uses the DROM table atb041+4a=b08b,which selects35c5. That handler
copies one word to0578,acknowledges the host ring through2e2f,and returns.
The captured two-byte011b commands produce exactly two matching scalar
stores. All six aligned resident literal references are enumerated:one store
operand and five read operands. Readers use0578 with0579/057a or0585/3eb8
in optional signal-processing corrections and shadow-change handling. This
establishes a calibration-scalar path,not its physical quantity or units,and
does not close indirect writers. No reply-generating path is established by
this handler;do not invent one to satisfy an idle-wake hypothesis.

`rom6_idle_contract.py` pins the relevant dispatcher,handler,formatter and
gate instruction fragments and checks the bounded observations against both
native rings. The actual MVMD/MVDK writes expose Read-slot5168/35c5 while
EXECPC can be invalid;generic bus-time context PCs are not instruction-owner
proof. The curated passive observer ends at24s even in longer runs,retains
no frame-history flood,and never selects firmware work or writes state.

```sh
.venv/bin/python tools/run_npe3_native.py --reference-clock --seconds 24 --adc-source fcch-sch-broadcast --capture-start p09 --tx-release p05 --raw-agch --provision-records
```

The reference project's registration notes independently associate04fa with
record/identity validation and report a6210 FAID/provisioning repair. That is
a hypothesis source, not an imported fix: its failed profile had04fa=6,
whereas this acquired PMM/model combination has04fa=0. Resolve the actual
inputs and algorithms before changing storage or a hardware identity.

The original [NSM-3 System Module manual, Issue 1 12/1999](https://www.eserviceinfo.com/preview_html.php?fileid=5456&previewid=3011)
identifies the 13MHz COBBA clock and separate IData/QData connections
(p.31), and specifies direct conversion without an intermediate frequency
(p.37). It describes serial high-speed baseband signalling (p.9), not the
older handset's parallel data/strobe interface. These facts constrain a
future physical trace but do not establish the DSP receive-register edges.

The missing independent anchor is a receive-register specification or a
hardware trace joining a programmed receive window to its first sample/data
availability and interrupt. Register-value formulas and decoder success are
not substitutes for that physical timing evidence. Keep any additional
software inference explicit about which edge it establishes; do not select
a window by moving the waveform or by replaying decoded results.

### Evidence needed to resume physical capture modeling

The remaining dependency is timing evidence, not another decoded packet or
an acquired DSP program image. A useful MAD2/MFI programming specification
would identify the receive-compare, capture/reload and data-ready semantics.
Alternatively, a measured receive transaction should retain:

- Handset/ASIC and firmware identities, clock reference and reset epoch.
- Timestamped P03/P09/P0a/P0b programming and counter/reload observations,
  with issuing DSP instructions distinguished from delayed bus contexts.
- The first valid ADC/MFI sample and first block-ready/INT0 event on the
  same time axis, distinguishing production from later CPU reads.
- At least two requested positions on different sides of the current
  counter, plus close/cancel behavior, to discriminate a next-match event,
  an explicit next-cycle event and a completed-block deadline.

A phone need not camp successfully for that trace to answer the timing
question. Internal register observations and physical IData/QData clocking
must be related explicitly; a pin trace without the programming sequence,
or a RAM snapshot without sample timestamps, is insufficient. Another
emulator's calibrated scheduling policy is not an independent measurement.
The P03-level, P09-start and P09-completed-block experiments below constrain
specific candidates but do not select a physical implementation.

The unchanged-source post-decoder-check run
`tmp/rom6-adc-lab/nsm3afs-3a_ocphy` checks 4096 source words across five
capture epochs and observes 903584 total P27 reads, no transport faults and
peak staging 69. It still publishes class-40 error status 1 with zero decoded
words. This closes decoder qualification separately from the timing question:
neither moving the source nor choosing a compare delay merely to obtain a
successful CRC would recover the missing physical contract. The separate
FIRE accelerator's physical timing and correction modes remain unimplemented;
the functional check subset below does not establish those hardware behaviors.

## Reproduction and coverage

```sh
.venv/bin/python tools/run_rom6_native.py --scenario radio
```

This repeats the qualified 60-second physical unlock/menu scenario with passive
I/O taps. The runner checks native composition, own uploads, verifier/self-test,
program coverage and reviewed early/late menu frames before accepting capture.
`radio-census.json` summarizes `primary/rom6_radio_writes.tsv`; the latter
contains every write, timestamp, mask and bus-time CPU PC. Per-port read
summaries retain counts and first/last times, with explicit caps on distinct
values (16) and CPU contexts (32). These contexts are not issuing pipeline-slot
identities. Missing/overflowed streams and mismatched per-port counts fail.
This is a manual research instrument, not an RF acceptance gate. Retire its
broad overlay/port capture when focused serial-device and receiver tests cover
the recovered boundary; keep only the normalized contract and useful controls.

The reference unattached-receiver census has a complete **342,473-write** stream. There
are 13 observed read addresses and 38 write addresses, not an exhaustive map
of all reachable firmware paths or physical MAD2 registers.

| Observed operation | Count | Boundary |
| --- | ---: | --- |
| P27 sample reads | 1,245,280 | Every value is zero; first 0.535343582s, last 60.014753494s. COBBA's unattached callback supplies deterministic silence, not measured receiver noise. |
| P31 / P32 writes | 30,354 each | Backend retains words; compare-edge projection reaches the partial HAGAR command-state device, without applying RF tuning. |
| P04 writes | 35,855 | Existing comparator raises transfer pending independently of the raw command-state receipt. |
| P30 writes | 30,789 | Raw values 0000/00bb/0100/0103; their physical meanings are not established here. |
| P0e frame-length writes | 11 | Values 0546/1387; inherited quarter-symbol/frame notification policy still needs ROM6-specific validation. |

The report has 545 distinct *issued* P30/P31/P32/P04 tuples. Only 30,348 P04
writes follow fresh writes to all three payload registers; the others lack a
complete fresh payload and their meanings are not established by this capture.
Do not treat every P04 write as a new RF
command. Delivery, bit order, latch timing, IRQ ownership and samples remain
separate obligations.

## Own-code attribution

The first-P31 snapshot preserves the live low program overlay before later
reuse. Its words `0b20..0b6f` exactly match 160 bytes at MCU offset `117070`,
SHA256 `206cc4a2a0352fea4ff5dcfdf037bea65c7eedc15faacdc243accde5035ebb46`.
The uploaded routine contains `PORTW *AR2+,P31` at `0b54` and
`PORTW *AR2,P32` at `0b58`, selecting data words `3f04` or `3f06` upstream.
Its observed bus-time PCs are `0b55/0b59`. This is one writer, not the sole
source: eight P32 CPU contexts occur in this run. A terminal overlay dump is
not evidence of bytes executed earlier at the same low address.

| P32 bus-time CPU PC | Writes |
| --- | ---: |
| `00ae3b` | 9 |
| `001a95` | 19,357 |
| `0048ac` | 3 |
| `003d2b` | 2,749 |
| `001614` | 2,747 |
| `0038c1` | 2,743 |
| `000b59` | 3 |
| `0023e0` | 2,743 |

The acquired resident routine `388c..38c8` has a separate AR2==31 destination
branch: `38bb` sends mapped AL to P31 and `38c0` sends AH to P32. Other branches
build words in RAM for later serializers, including the observed `1a8c..1a98`
P30/P31/P32/P04 path. A pair can therefore be halves of one computed word,
not two independently addressed RF registers.

## Scheduling ownership

The descriptor list belongs to firmware RAM, not a recovered hardware FIFO:

- `296a..2973` acknowledges request `1000` through P01 before testing firmware
  state. `2993..2995` reloads descriptor/payload cursors from `265e/2660`.
- `2a8d..2a99` publishes one P30/P31/P32/P04 group. `2a9d` advances the
  descriptor cursor; `2ac6..2ac8` retain both cursors for the next interrupt.
- `2a63..2a6f` cancels the modeled compare using target `3a98` and updates mirror
  `058e`. The target exceeds both observed counter periods (5000/1351).
- The alternate branch `2ae7..2af4` writes P30, then P04, then P31/P32. It
  is static evidence against assuming one universal write-P04 snapshot rule;
  this branch is not among the run's eight observed P32 contexts.

The current backend has one replaceable compare target and a pending bit,
not a packet queue. Reprogramming replaces its deadline; writing an out-of-range
target stops future matches without acknowledging an already-pending request.
P01/P02 bit `1000` acknowledges that independent pending bit; reset cancels it.
These are the existing modeled contract, not a complete SCU specification.

`tools/run_rom6_native.py --scenario serial` captures all **24,914** modeled
compare callbacks, correlated with the complete write stream by
`rom6_timed_output_check.py`. Every callback uses the latest in-range target,
reads the counter at that target and projects the live P30/P31/P32/P04 tuple.
There are 10,995 preceding target reprograms. Arm/edge payloads agree throughout,
so **even this complete run cannot distinguish write-time latching from
live-register sampling**. All24,913 intervening intervals contain a firmware
`1000` acknowledgement; every callback observes pending clear beforehand.
No one-ACK-per-packet hardware FIFO follows from this; delayed service can
coalesce pending bits.

All observed compare-edge controls are `0100`; raw `0000` writes do not reach
a compare in this run. At the edge, tags `4400`, `4100`, `4a80` occur 2,750,
5,506 and2,749 times respectively. The raw callback does not interpret these
families, declare a physical serial completion or supply an RF response.
Its bounded verbose capture rejects truncation rather than weakening coverage.

`make check-c54x-serial-compare` independently exercises the backend without a
handset ROM: target replacement, live-register projection, an armed save/restore
with exact callback time and tuple, both acknowledgement ports, periodic matches,
out-of-range cancellation, pending retention, raw zero control and reset abort.
These are executable **model conformance** tests, not a silicon latch proof.

## Counter-phase contract

The backend's declared 13 MHz/12 counter retains one reference epoch through
wraps. Frame, slot and transfer compares are scheduled at absolute reference
ticks, rounded upward to the first representable attosecond. They are not
relative delays from a fractional-phase register write. A compare cannot fire
while its own counter still reads one tick short. The older relative/floor
calculation violated this in 20 of 256 captured callbacks; the strict checker
rejects that evidence rather than allowing a one-tick tolerance.

Independent literal C++ tests cover fractional writes, equality/next-wrap
policy, nonzero base, short reload, long elapsed time and rational rounding.
Existing registers, pending bits and reference epoch remain saved/reset by
the backend; no new firmware state or forced counter publication is added.
The 13 MHz/12 physical rate, equality-at-write policy and RF serial latch phase
remain separately unmeasured for ROM6.

P0e reload writes also retain the divider's fractional phase, including
unchanged-value writes. The executable backend fixture writes at half a
quarter, observes the next counter increment on the original clock, and
requires the original exact compare deadline; a changed-modulus write is
checked separately. The former implementation re-anchored to each write and
fails this fixture. P0d explicitly loads the counter and still establishes
a new epoch. These are internal consistency rules for the declared model,
not a recovered silicon reload specification.

The 6210's corrected run retains seven FCCH confirmations, residual `-2`,
native Messages UI and a failed SCH report. Its first receive tick is now
`42243300` (frame 704, offset 3300), only 84 reference ticks earlier than
the pre-correction capture. It remains 137.5 complex-sample intervals into
the SCH envelope. Fractional reload loss was real, but not the acquisition
blocker; no source shift or rate change is justified by this result.

## Cross-reference limits

Port numbers here are hexadecimal. Check literal I/O operands before importing
external labels: the stable-read helper `3ee1..3ef2` reads `000d`, which may be
called decimal "PA13" elsewhere; it is not a separate P13 input.

Dan's snapshot `dd356ff7c5fc62529ab5843ce737593ebff51f73`, especially
`rom6-rf-acquisition-current.md`, `rom6-pll-unmatched-o3x.md` and
`rom6-frame-int0-ownership.md`, supplies hypotheses, not an 8210 specification.
Our own run reproduces setup pair `81c0/4000` 17 times, `9044/4400` 7 times and
`48ce/4400` 11 times. His 3310 trace's `48c8/4400` channel interpretation must
not be transplanted onto our different word. Zero P30, unmatched tuning and
frame-versus-block notification remain distinct questions; none justifies
forcing the laboratory carrier or synthesizing a received report.

The retained Nokia NSM-3 System Module manual (issue 1, 12/1999, printed 41)
identifies HAGAR, a serially programmed PLL, 64/65 prescaling, 200 kHz reference
and `M=N*64+A`. Printed 25 names MAD2's SCU and MFI separately. These primary
constraints do not decode the DSP port packet or prove which block owns each
edge. Source/hash: [hardware atlas](hardware_atlas.md#cobba-part-identity-does-not-select-pcm-timing).

## Qualified RX word arithmetic

`tools/run_rom6_native.py --scenario tuning` adds bounded, read-only Execute
observations to the same UI/radio fixture. It checks the resident formatter and
key normalizer at `388c..3937`, SHA256
`37dc843e29d036630964d06e8d310074afd2734095f1036d9dc39d2e9243e22b`.
All **2,743 formatter calls and actual P31/P32 pairs** match an independent
numeric reference; the run covers all **548 scan keys** (`0..124`, `512..885`,
`975..1023`). Thirty-nine selected paths additionally check normalized input,
addend, conditional doubling, quotient, remainder, return and destination.
This is producer/word qualification, not received-sample or RF-delivery proof.
The complete timestamped342,473-write stream is byte-identical to the plain
radio observer's capture on the same binary/input composition; the added
Execute observations do not change that checked output stream.

For the observed RX branch (`3e7e & 0x6 == 0x4`, `3ec8 & 0xa2 == 0xa0`,
`051f.bit7 == 1`), define a neutral **formatter scalar** `s`:

```text
GSM/E-GSM: k' = k for 0..124, otherwise k - 1024 for 975..1023
           s = 2 * (k' + 4675)
DCS:       s = k + 8514, for 512..885
q, r = divmod(s - 38, 64)
P31/P32 combined = 0x44000000 + ((q << 7) | r) + 38
```

The live GSM addend is `1243` (4675); DCS selects `2142` (8514). The direct
GSM addend `1162` (4450) belongs to the alternate constructor and is not TX
qualification here. E-GSM normalization is signed: key975 enters the adder as
-49, not the unsigned word `ffcf`.

For inverse arithmetic, subtract 38 **before** checking the packing gap or
extracting fields. For `u = P31 - 38`, require `u >= 0`, `u & 0x40 == 0` and
exact re-packing, then `s = 64*(u >> 7) + (u & 0x3f) + 38`. Applying the ROM4
gap test directly to the biased P31 word would reject valid ROM6 words.
`tools/rom6_tuning_contract.py` admits this positive `4400` branch only;
`6000` and unrelated control formats remain unsupported, not invalid silicon.

| Pair | Scalar | Numeric RX interpretations |
| --- | ---: | --- |
| `48c8/4400` | 9352 | GSM1 at 935.2MHz **or** DCS838 at 1870.4MHz |
| `48ce/4400` | 9358 | GSM4 at 935.8MHz **or** DCS844 at 1871.6MHz |
| `47e4/4400` | 9252 | E-GSM975 at 925.2MHz **or** DCS738 at 1850.4MHz |
| `9044/4400` | 18500 | Canonical arithmetic, no matching finite RX scan key |

There are 74 GSM/DCS collisions across the observed domain. The inverse returns
all candidates and leaves a valid unmatched word unmatched; it never chooses
the laboratory carrier. The Nokia frequency plan is numerically consistent
with `f_RX = s*100kHz` in GSM, `s*200kHz` in DCS and `f_VCO = s*400kHz` in
both. Relative to the manual's 200kHz reference, that implies `M = 2*s`;
**it does not establish that the DSP word's bit fields directly encode M**.
Port-to-wire serialization, reference-divider controls and settling remain open.

The own run issues `4a80` payloads `0402` and `0208`, not the sibling's
documented whole-word families. The firmware configuration-class contract below
is qualified; physical band-field ownership is not. Do not whitelist imported
words or arbitrary payloads sharing their low bits.

## Own configuration-class contract

`tools/run_rom6_native.py --scenario config` composes the qualified tuning
fixture with passive data-store and Execute observations. It qualifies
**2,749 key classifications, RAM selections and actual issued configuration
pairs**, covering all548 scan keys. Every configuration is staged before its
P31/P32 issue, without a device reply, RAM poke or RF-source selection.

The words originate in an upload, not a live threshold calculation:

- The native prefix at data `3e7e..3f10` matches294 MCU bytes at unique file
  offset `116b08`, SHA256
  `69d876c8a91b4807e48edeb77b2effabecb01d8c6dca05c4946cc1a9b673f5c2`.
  Individual `0402/0208` literals have another flash occurrence; the complete
  prefix distinguishes them.
- The collector records the published146-word prefix plus the incoming
  `3f10` store value. That is a transaction artifact, not a claimed post-store
  RAM snapshot and not an emulated-memory write.
- Initialization clears both slots at bus-time PC `0f15`; resident
  `22ad..22ae` copies data through AR2 into AR3, publishing `0402` at `3f0f`
  and `0208` at `3f10` (bus-time context `22af`). No further slot write occurs
  in the qualified run.

The key classifier `3d81..3d90` sets `263b.bit2` for0..124 and975..1023,
clearing it for512..885 while preserving other class bits. The **active**
configuration selector is different: uploaded `1994..199d` compares the prior
payload at `1274` with data `3efc`, loads `3f0f` on equality or `3f10` otherwise,
and stages the selected word with tag `4a80` at `1274/1275`. Its60 uploaded
bytes at `1990..19ad` match unique MCU offset `1161b0`.

| Key class | Class bit2 | Prior payload | Compared data3efc | Selected tagged word |
| --- | --- | --- | --- | --- |
| GSM/E-GSM | Set | `1054` | `1054` | `4a800402` |
| DCS | Clear | `082a` | `1054` | `4a800208` |

The run has877 GSM/E-GSM and1,872 DCS selections. The classifier, old-payload
comparison, selected slots and issued pairs agree on every event. The static
`29c3..29cc` bit-based selector remains a separate route, not the active producer
in this run. Do not replace the equality test with a class-bit claim.

`rom6_config_contract.py` provides an **analysis-only** whole-word class
interpretation. Together with the RX scalar inverse it uniquely recovers the
requested key for all2,743 native tuning pairs. Unknown words, including sibling
`0c22/2018` values or arbitrary words with the same low bits, stay unsupported.
A canonical unmatched PLL word remains unmatched under either configuration.

This is not permission to retune at a port write. At11 class transitions the
last issued configuration still describes the previous class when the native
PLL pair is issued. The first subsequent configuration matches the requested
key in every case. Raw issuance order is therefore not a coherent radio-state
update. Moreover `1054/4100` and `082a/4100` co-vary with these classes; the
current evidence does not isolate which physical command owns band selection
or what other fields the `4a80` payload controls.

## Next implementation boundary

Do not add a second descriptor queue. RX arithmetic and configuration-class
selection are qualified, and the partial device below receives raw compare
projections. The next boundary is physical interpretation of the co-varying
`4100`/`4a80` controls and receive-clock/sample ownership. The complete
stable-payload capture does not uniquely choose a physical latch edge; the
device's compare-edge policy is explicit, not a measured silicon conclusion.
A clocked sample source with provenance follows; no carrier fallback is permitted.
Native FCCH/SCH/BCCH, camp and registration come after those boundaries.

## Partial HAGAR command state

The `nsm3r6` and raw-laboratory `npe3ap5` research compositions attach
`nokia_hagar_device`. It receives
the raw P30/P31/P32/P04 tuple on actual compare callbacks, not port reads or
firmware-PC hooks. It owns the last `4100`, `4400` and `4a80` words, their
validity and receipt count. All are saved/resettable. No second descriptor queue,
completion IRQ, PLL-lock indication or received sample is introduced.

The device exposes a **conditional arithmetic candidate**, not RF readiness:
both `1054/4100 + 0402/4a80` or `082a/4100 + 0208/4a80` must agree, and the
PLL word must have a unique matching key in that class. Disagreement, unknown
control, unsupported transactions and unmatched `9044` have no fallback.
Unsupported transactions invalidate the known-family set; all three families
must be received again. Interleaved `4680` retains the conditional arithmetic
candidate without assigning gain semantics or authorizing sample reception.

Independent executable tests cover all 548 keys, the two interpretations of
`48c8`, class disagreement, unknown words/control, unmatched PLL, snapshot and
reset. The MAME compare fixture additionally saves/restores an actual configured
device and checks repeated delivery and reset. Holding the DSP in reset stops
new compare receipts but does not invent an off-chip HAGAR reset; board-level
reset-pin ownership remains unproved. The serial acceptance checks every device
receipt against independent bus/word replay. This is partial stateful hardware
modelling, not qualification of physical divider fields, RF gain, settling,
reference-divider control, serial clocks or a working receiver.

The qualified 60-second serial run checks all 24,914 device receipts: 19,205
have a conditional candidate and 5,709 remain unresolved, covering all 548 scan
keys. Its complete timestamped write stream retains the baseline SHA256 above.

The6210 24-second addressed-page run independently pairs64,928 command receipts
with their exact compare-edge tuples. All remain unresolved by the NSM-3
contract: its6,463 configuration writes are `4a800502` (2,406) and `4a800308`
(4,057),not the qualified `0402`/`0208` payloads. No bit-masking equivalence,
frequency candidate or RX-readiness follows. The observer supplies no samples;
the full1,468,878-row DSP write journal remains byte-identical to the prior
page run (SHA256 `e6f4b9c05d50f08663cc7cbc7fc1c5166a51b1d84c805dd43573c0dbd9118074`).
Native UI,SIM,raw LU/release and two addressed-page admissions still pass.
Recovering the6210 configuration producers is required before reusing the
NSM-3 frequency interpretation or introducing source tuning.

Own6210 provenance is now established independently of that interpretation:
the294-byte data prefix3e7e..3f10 uniquely matches MCU offset2083c. Its route
literals3efc/3efe remain1054/082a,but3f0f/3f10 are0502/0308. The runtime
clear writes are owned by read-stage0f14;the two upload stores by22ad.
Selectors1990,1a5e and29b9 read this pair under their respective route/flag
conditions. Their staging writes need not have a valid read-stage PC;an
`ffffffff` observation does not assign instruction ownership. Across the
24-second page run,all6,481 staged words match subsequent P31/P32 configuration
writes from the common output routine (P32 context1a89),without changing the
full bus journal. `npe3_rf_config_check.py` checks unique upload identity,
producer coverage,uncapped staging and ordered port delivery. This proves a
product-specific source pair,not that bit8 is harmless or that either word
can be aliased to the NSM-3 interpretation. The next RF contract is the6210's
own key-to-PLL/configuration relationship and receiver-state interpretation.

The6210 resident388c..3937 formatter is byte-identical to the independently
decoded ROM6 routine (SHA256
`37dc843e29d036630964d06e8d310074afd2734095f1036d9dc39d2e9243e22b`).
Read-stage388e/389e observations distinguish RX/TX inputs;direct instruction
fetch taps do not provide entry coverage and are not an absence test. The
qualified24-second capture checks3,539 RX and674 TX invocations,covering283
observed RX keys,not all548 possible keys. All4,212 buffered invocations
produce the exact independent low/high pair at the entry's destination;one
RX invocation writes P31/P32 directly. The buffer-store receipt's38c7 is
pipeline context for both writes,not separate ownership attribution.

With the observed flags,the normalized key is `key-1024` for975..1023 and
otherwise unchanged. RX scalar is twice `(key+4675)` for GSM900 or
`key+8514` for DCS1800;TX uses4450 or8039 respectively. The unique own MCU
prefix supplies these addends. Both directions use the same biased64/65
numeric packing described above and tag4400. Buffered calls must not be
mistaken for direct port writes,nor TX calls interpreted as RX tuning.
`check_formatter` validates every captured input against its output and
rejects missing/extra/reordered destination stores. The full DSP journal
retains the same hash;native Menu,SIM,LU/release and addressed pages pass.
This closes numeric formatter reuse,not physical divider-bit semantics,
configuration-word equivalence,RF enable/gain/settling or tuned reception.

## Receive interrupt and clock boundary

`tools/run_rom6_native.py --scenario receive` adds a passive 1,024-read prefix
and a first-read program view to the qualified serial/UI fixture. The complete
342,473-write stream remains byte-identical to the earlier observers (SHA256
`40f8ae09eb4e7a3cc124bf2d7f9c4e197e831fc122e3cd58830cb24f82f32b6f`).
All 1,245,280 P27 reads still consume unattached silence, not a clocked buffer.

The prefix contains 32 groups of 32 words. Group starts are 4.589913–4.615385ms
apart, compatible with the inherited frame-driven INT0 policy; this is not a
measurement of the physical COBBA sample clock. The first read occurs with
last-written P03=`0000` and P09=`3a98`, before an in-range P09 position is
programmed. Therefore these reads cannot qualify a P09-driven producer.

Own-code ownership is concrete:

- PMST=`07ac` selects INT0 vector `07c0`, which far-branches to `22da`.
  That entry calls uploaded hook `05da`, branching to the live `1845` routine.
- The uploaded routine repeats four P27 reads eight times. Observed bus-time
  contexts `18a9/18af/18b5/18bb` accompany instructions at
  `18a4/18aa/18b0/18b6`; do not identify instruction addresses from bus PCs.
  Live `1845..18d1` matches 282 own MCU bytes at `115f1a`, SHA256
  `bce951612776b11c87f4beda52b0ee527056fc0f9e8767174b626f64c6b786c3`.
- P09's position producers and the receive reload are qualified below; these
  remain separate from physical sample production and interrupt ownership.

This supports investigating P09 as a receive-position comparator, but does not
prove its interrupt route, recurring cadence, buffer ownership, sample rate or
signed-position rule. The backend currently stores P09 without scheduling it;
its frame callback independently asserts INT0 while the DSP is released and
external-RF ownership is not selected. CCONT RTC control does not gate CTSI.
Do not transplant the sibling's explicitly diagnostic PA09 cadence as measured
hardware. A receive model must separate sample production, data-available
notification and frame timing before a native decoder result can qualify it.

### Qualified receive positions

The same receive fixture captures every data 1844 store and P09 write, joined to
the independent complete I/O stream. All 5,495 observed P09 writes are classified:

| Issuing instruction / bus context | Count | Value source |
| --- | ---: | --- |
| `3e31` / `3e32` | 2,749 | Current AR1, including zero |
| uploaded `198b` / `198c` | 2,743 | Latest stored data 1844 |
| `3f17` / `3f18` | 3 | Out-of-range `3a98` sentinel |

Data 1844 has two initialization clears and 2,749 writes at instruction `140c`
(bus context `140d`). Every calculated value matches this exact reference:

```text
position = AR1 - 32
if position < 0: position += data036a
store low16(position) at data1844
```

This is one conditional addition of the **raw firmware modulus**, not arbitrary
modulo reduction and not addition of reload+1. Nine calculations use `1387`
(4999); 2,740 use `0546` (1350). The first-read code view qualifies the uploaded
producer and deferred writer against own MCU ranges, and the checker joins
every deferred read to the latest observed store. No unseen writer is assumed.

The short receive-loop reload is intentional own code: uploaded `09ed..09f3`
sets data 036a=`0546` and writes it to P0e. Its 56-byte enclosing range
`09d8..09f3` uniquely matches MCU offset `116de0`, SHA256
`3bc200ec0b2f0dc4e405da8bc274db0a660220a3ef8dc11c192c3e53cdf62825`.
All 11 reload writes are independently bus-correlated. Do not replace this with
a fixed GSM-frame interval or infer CPU corruption from the shorter scan cycle.
Physical reload/compare semantics remain a distinct question.

At the first P27 read, the eight live data 3ebd coefficients are
`0387,11a3,2ad6,4000,4000,2ad6,11a3,0387` (sum 65536). The uploaded reader
consumes four scalar inputs per repeated iteration. These constrain a later
signal/filter experiment; coefficient shape alone does not establish the ADC
sample rate, I/Q wire packing, uninterrupted capture or RF interrupt cadence.

### Capture-position programming

The receive fixture independently joins all **11,001** writes to P0a, P0b and
P11 against the complete bus stream. These are register-value contracts, not
proof of the corresponding physical capture edges:

| Port / issuing instruction / bus context | Count | Value source |
| --- | ---: | --- |
| P0a / `3e23` / `3e24` | 2,749 | AR1 minus data 3ea7; add raw data 036a once if negative |
| P0a / `3edb` / `3edc` | 2,747 | `3a98` sentinel |
| P0a / `3f26` / `3f27` | 3 | `3a98` initialization |
| P0b / uploaded `15cf` / `15d0` | 2,747 | Low A after stable P0d read, addition of data 3eb6 and one conditional subtraction of data 036a |
| P0b / `3e2b` / `3e2c` | 2,749 | `3a98` sentinel |
| P0b / `3f37` / `3f38` | 3 | Zero initialization |
| P11 / `3fe4` / `3fe5` | 3 | `01fe`; field meaning unknown |

Every observed start lead is `00a3` (163), and every observed stop tail is
`0020` (32). The checker verifies each start calculation and stop accumulator,
but does not independently capture each preceding P0d read. The stop formula
is statically decoded; its counter-input chronology is not yet runtime-qualified.
The uploaded `15c4..15d1` region uniquely matches MCU offset `115a18`, SHA256
`820291aa03aec557fd522ba30fc3d54adc3689b0e4bafb6f6ff37c3b68d65453`.
Do not assign these writes to the similar resident `3e7a` instruction.

The start routine writes P0a, defuses P0b, programs P09 and then sets P03 bits
0/2. The stop routine clears P03 bit 0 before programming P0b; the common tail
then defuses P0a. This ordering is compatible with an armed, counter-timed
capture lifecycle rather than immediate P03 level gating. It does not identify
which compare controls ADC clocks, transfer clocks or interrupt generation.
In particular, P11=`01fe` is not evidence for a 512-word FIFO.

### P03 lifecycle status boundary

P03 bit 8 is not merely an unused preserved control bit. Resident
`6867..6870` reads P03 into AL, masks `0100`, and returns when nonzero;
only a clear bit publishes `263a.bit7`. Routine `ac51..ac55` tests that
flag before its initialization branch. Firmware sets P03 bit 8 at
`4899..489f` and `ac77..ac7d`; `67b2..67b8` can explicitly clear it in a
different branch. These are code-derived bit operations, not established
names for the underlying hardware state.

Do not infer a missing hardware clear merely from the poll. Its predecessor
loads the scheduling countdown through AR5 (`2fde`) and takes this poll
when the countdown is at most one (`679f..67a2`, also `6850..6853`).
The longer-wait path explicitly clears P03 bit 8 at `67b8` **before**
waiting on P0f at `67d7` and `680c`. In the fresh four-second
`status-goo333vx` trace, all ten poll calls have AR5=`2fde` and countdown
zero. Thus the observed retained bit is compatible with the immediate-work
branch; the poll alone does not establish a stuck completion latch.

The passive `rom6_adc_status.lua` census retains per-consumer counts rather
than every read. In twelve-second `status-du8pcopi`, all 7,060 reads fit
seven consumer contexts with zero overflow. The poll's bus context `686d`
occurs ten times from `3.298898132` to `3.341567451s`, always with bit 8
set and value `0104`. Thus those ten calls do not publish the flag. The
subsequent initializer at `20eb` has 1,548 clear-bit reads; the model does
not leave the bit set across every firmware lifecycle.

The backend still treats P03 as stored state. The sibling's completion
rule at P0f=`0ed8` is a candidate, not a hardware specification; no such
clear is implemented here. Whether any hardware-owned clear exists, and
whether the skipped publication matters to receive timing, remain unresolved. Retire
this dedicated observer once that completion contract is qualified.

## ADC-pin transport experiment

`nsm3adc` and `nsm3ad0` are explicit **laboratory compositions**, not promoted
handsets or RF acceptance gates. They attach `nokia_mfi_rx_device` between a
clocked COBBA ADC callback and P27. The former applies an analytic periodic
67.708333kHz I/Q tone of amplitude 1024; the latter supplies matched zeros.
Neither source uses HAGAR's candidate, an ARFCN or a firmware PC. This is
direct ADC-pin stimulation, **not a tuned over-the-air signal**.

```sh
.venv/bin/python tools/run_rom6_adc_lab.py --machine nsm3adc
.venv/bin/python tools/run_rom6_adc_lab.py --machine nsm3ad0
make check-c54x-serial-compare
```

The candidate clocks scalar words at 13MHz/12, not from P27 demand. This is
the two-complex-samples/symbol composition matched to the native accumulator
below, not a measured silicon clock. The laboratory now uses a 96-word staging
limit derived from the service-gap bound below, **not a measured MFI capacity**.
The component's default 64-word limit remains a logical negative control.
Words, transfer ticks,
queue state and the next clock deadline are saved. Block notifications
exclusively own INT0 throughout these laboratory compositions, including
inactive capture periods. The non-laboratory compositions retain their existing
frame source. Experimental enable wiring requires
P03.bit0, P21.bit10 and clear P21.bit12. This is a hypothesis, **not a recovered
truth table**; P09/P0a/P0b capture boundaries are not implemented here.

The oscillator latches simultaneous I/Q at the I transfer and serializes Q twelve
reference ticks later. Logged `source_tick` is the **scalar interface-transfer
timestamp**, not necessarily analogue conversion time. The independent checker
evaluates sine/cosine from these timestamps and rejects skipped or future words.
Periodicity is intrinsic to the analytic source, not finite-capture fallback.

Exclusive ownership removes pre-capture warm-up reads caused by synthetic
frame interrupts. In the divider-6 qualification, both laboratory inputs first read P27 at
2.537874286s (bus context `2446`, in the native discard loop). All **40,320**
P27 reads reconcile with consumed clock-produced words across activation
counter resets; none is an inactive retained-register read. The checker also
qualifies 2,752 actual word values across three activation prefixes.

The original 64-word transport result is **negative**. Matched tone/silence trials reject
the sufficiency of each exact combination below, not its individual physical
rate, comparator or capacity hypothesis:

| Scoped trial, all with 64-word buffering | Scalar divider | First overrun |
| --- | ---: | --- |
| P21-only enable | 6 | 0.803263846s, before RF work |
| P03 gating, frame INT0 outside capture | 6 | 2.548358769s |
| P03-armed one-shot P09 start / P0b stop | 6 | 2.548362462s (21,600 produced / 21,536 consumed) |
| P03 gating, cadence-only comparison | 12 | 2.548639385s (11,680 / 11,616) |
| P03 gating, exclusive MFI INT0 | 6 | 2.548358769s (22,752 / 22,688) |
| P03 gating, exclusive MFI INT0 | 12 | 2.548639385s (11,680 / 11,616) |

The timed-gate implementation was removed. The slower divider was initially
restored after its transport-only test, then selected by the independent
post-filter signal test below; it still does not qualify losslessness. Exact
binary hashes, fault contexts and retained raw-capture locations for those
rejections are in [the falsification ledger](../evidence/falsifications.json).
These failures do not establish physical buffer capacity or justify a capacity
sweep. The runner rejects every fault across counter resets.
The header distinguishes oscillator phase divisor from MFI scalar divisor;
the checker rejects disagreement with the device summary. Cadence comparisons
must not retime the laboratory oscillator.
Word/block diagnostics have per-process budgets of 4,096/128, independent of
the number of capture activations. Reset and save/load do not replenish these
host-only budgets. The checker joins every P27 read to full queue accounting
while explicitly bounding value qualification to logged prefixes; it does not
equate a small trace with a complete value census.

At both overflows the passive fault observer sees IMR=`035f`, IFR=`0021` and
ST1=`c903`/`4903`: INT0 is enabled and pending, but global INTM is set. This
identifies native critical-section backpressure at those events, not a missing
IRQ request. It does not establish the physical buffer capacity or clock gate.

Logical tests cover ordering, provenance, peeks, partial blocks, overrun,
underrun and reset. The MAME component gate saves a partial buffer and checks
its exact first restored block, transfer ticks and completion time. An
independent second backend provides an interrupt-ownership control: with the
same independent CTSI frame clock, the default backend receives frame INT0, the
external backend receives none while idle, and its explicit data-available
input does assert INT0 without a P03 write. This is composition conformance,
not a claim about the fitted MAD2 interrupt multiplexer. An
in-callback stream load does not rewind the test timer's scheduler-local
relative deadline; its later callback is not an equal-time endpoint. Neither
this component test nor the good source prefix qualifies a native ADC session.
No RSSI, FCCH, SCH, BCCH or camping result is claimed.

### Qualified native FIR prefix

`rom6_adc_filter_check.py` joins the first 1,024 clock-delivered scalar words to
all 1,024 writes into native history `0200..020f`. It then independently checks
the first 128 output words stored at `00c8..00d7`: tone produces 128 nonzero
words, silence produces 128 zero words. The 32-word discard, subsequent
four-input/two-output ordering and signed arithmetic are checked, not inferred
from a changed screenshot or merely nonzero RAM.

The reference is an eight-tap signed convolution over the observed circular
history, with Q16 coefficients and half-up high-word rounding. Native output
order is **Q,I**, with complex decimation by two. A separate constant-lane
test uses the coefficients' unity DC gain as its oracle rather than generating
expected values with the convolution under test. Actual native runs agree
word-for-word; no CPU arithmetic change was needed.

The checker pins the resident image, live `2468..24d3` filter/mixer bytes, own uploaded
entry and the complete own `0590..0735` payload. The capture is intentionally
bounded: its counter sees 496 filter stores within the first 1,024 inputs, but
only the first 128 output **values** are qualified. It does not validate RF
timing, an RSSI result, FCCH detection or later data after an overrun.

### Qualified quarter-rate accumulator prefix

The resident `249f..24d2` accumulator multiplies successive complex samples
by `1,-j,-1,+j`, then sums groups of four (observed mode 0), or eight (static
mode-1 branch). Eight live mode-0 blocks, all 32 output words, match an
independent complex rotation and the native 72-word ring-pointer updates.
Mode 1 has a logical reference test, not a native execution qualification.

For the same physical +67.708333kHz tone:

| Scalar divider | First adjacent native accumulator outputs (signed A,B) | Adjacent-group dot product |
| --- | --- | ---: |
| 6 (four complex samples/symbol) | `(444,-2232)`, `(-444,2232)` | -5,178,960 |
| 12 (two complex samples/symbol) | `(804,-1936)`, `(804,-1936)` | +4,394,512 |
| 12, zero input | `(0,0)`, `(0,0)` | 0 |

This selects divider 12 for the **laboratory baseband** on signal evidence,
not because it hides an overrun. With the proved decimation by two, a quarter-
rate FCCH tone becomes stationary at the native quarter-rate mixer only when
its output runs at the GSM symbol rate. The known FCCH relationship is also
described in the primary [FCCH detector patent US7590419B2](https://patents.google.com/patent/US7590419B2/en).
The inference assumes unshifted complex baseband at the ADC fixture; it does
not identify an actual MFI wire clock or exclude an undiscovered hardware NCO.

The original 64-word six-second divider-12 runs reconcile 975,264 tone and 1,340,800 zero
P27 reads to clocked queue consumption; each independently checks a bounded
4,096-word prefix. Both retain the initial overrun at 2.548639385s. Changed
measurement activity after that fault is not an accepted RSSI/FCCH result.
MFI bank/acknowledgement semantics
(notably P21), COBBA control gating, capture edges and P11=`01fe` remain the
next boundary. Retire the direct-ADC compositions and field probes when a
focused clocked-air gate covers the recovered contract.

The native read-side control sequence sets P21 bits 10/11 at uploaded
`1863..186a` and clears bit 11 at resident `25bc..25c3` on exit. These live
bytes are checked against the own upload and resident image. Bit 11 therefore
brackets the software read interval; its physical meaning (bank ownership,
acknowledgement, clock control or something else) is not established. Do not
treat the current opaque P21 latch as a recovered MFI handshake.

### Service-gap staging bound

A passive first-window trace has 11,648 P27 reads, 794 control transactions
and 362 read gaps above half a block period, without hitting its cap. The
fault lies in a **55.934us** read gap, from 2.548587824s (after read 11,616)
to 2.548643758s (read 11,617). It includes COBBA register-8/9 transactions
while INTM is set; P21's software read-interval bit is clear. The program-fetch
mask observations are samples, not exact instruction-ownership or idle-state
proofs.

For the declared uninterrupted producer, first source tick 32,992,152 and
scalar divider 12 imply 11,685 produced words just before that next read:

```text
backlog before next read = 11685 - 11616 = 69 words
smallest 32-word-rounded staging limit covering this bound = 96 words
```

`continuous_queue_bound` independently accounts for nanosecond log rounding;
both bounds are 69. No new 32-word notification would occur between the old
overflow and this read (the next boundary is 11,712), so this local demand
calculation does not assume an extra interrupt or a changed CPU path. A
different physical capture gate or banking policy could change that demand;
69 is not a lower bound on silicon RAM independent of this producer model. A
P21.bit11 service-hold/re-notification candidate with the original 64-word
queue fails at the identical first overflow and was removed; it establishes
no physical acknowledgement semantics.

The single pre-registered **96-word laboratory staging** trial completes
matched three- and six-second tone/zero runs with no raw transport faults,
and reports a peak of **69** in every run, matching the independent bound.
No samples are overwritten, discarded, padded or demand-generated; all P27
reads still reconcile with clock-produced queue consumption. The 96-slot
backing is saved/resettable, and focused tests exercise full wrap/provenance
and snapshot copies at that limit as well as the default 64-word overflow.

This unblocks native signal-processing experiments, **not hardware receiver
qualification**. It neither proves that silicon has a 96-word FIFO nor that
all future workloads fit it. The fixture reports provisional limit/peak and
keeps `physical_buffer_capacity_qualified=false`; a later overflow is a new
contract question, not permission to keep increasing storage. Current MFI
bank/acknowledgement and capture gating remain open. The raw first-window
trace and its read-only observer are retained at
`tmp/rom6-adc-lab/window-nuza6oxg`.

### Qualified raw logarithmic measurement

The optional read-only measurement observer joins Execute entries in resident
`4315..4347`, the return to `242f` and the subsequent raw `022b` store at
`2431` (bus context `2432`). The live code and own return hook are pinned;
the observer neither calls the routine nor changes any firmware value.

```sh
.venv/bin/python tools/run_rom6_adc_lab.py --machine nsm3adc --measure
.venv/bin/python tools/run_rom6_adc_lab.py --machine nsm3ad0 --measure
```

The 64-word data-ROM table at `b09e` independently equals
`floor(640 * log10(index))` for indices 1..63, with zero at index 0. For a
positive 40-bit input `P`, the routine uses its six leading significant bits:

```text
e = max(0, bit_length(P) - 6)
m = P >> e
log_q16 = (floor(640*log10(m)) << 16) + e*(1541 << 13)
```

Nonpositive input follows the decoded passthrough branch, not a logarithm of
zero. Calibration either subtracts a signed direct value scaled by 64, or
selects a signed Q16 correction from `0474`/`049c` plus the caller's index.
The native arithmetic is therefore in **1/64-dB units relative to firmware
calibration**, not independently established physical dBm.

In paired fault-free six-second runs, all 32 captured conversions per input
match the full 40-bit results. Each run also has **15** joined raw measurement
stores. For those calls, ADC epoch ticks determine the analytic tone phase;
independent convolution predicts exactly the native input powers
`274576`, `274657` or `275282` (zero for silence), not merely a range or a
nonzero result. The observed table-calibrated raw words are signed **-551**
for the tone and **-7020** for zero. Only the `0474` calibration branch is
native-qualified by these captures; the other branches have logical vectors.

### Qualified frequency-correction prefix

The same observer captures 16 calls to resident `71a3..71d7`, joining all
15 qualified raw RSSI stores to their subsequent correction. Each paired
tone/silence run covers channel keys 0..15: eleven stores (including the
zero-delta boundary at key 10) and five interior no-store returns.

The signed descriptor at `3ea1..3ea4` contains lower/upper channel boundaries
and left/right slopes: `(10, 90, 472, 722)` in these runs. Outside the strict
interior, the routine adds `(signed_delta * signed_slope + 128) >> 8` to the
signed raw word, wrapping the result to 16 bits. The first tone value changes
`fdd9` to `fdeb`. Flags `051f.bit7` select alternate-band handling: keys
125..974 use descriptor `3ed6..3ed9`; keys 975..1023 are interpreted as
negative extended channels relative to 1024 and use the main left slope.
Those alternate/right branches have logical vectors, not native coverage in
this bounded prefix. The checker pins the correction code, checks arithmetic
mode, and requires the predicted write or no-write behavior at each return.

This qualifies the **observed corrected RSSI-field prefix**, not its final
response. The MCU-bound measurement reply is the next unresolved join.
Response-packet, physical-dBm, FCCH and camping flags remain false. The
direct-ADC source still does not establish a tuned RF carrier or physical
MFI capacity.

### Native MCU-bound publications under ADC-pin stimulation

The measurement observer also records actual DSP writes to receive-ring
producer `08e4`, retaining the preceding cursor, consumer, and committed
words. The checker validates cursor continuity, wrapping and packet lengths;
it never drains or fills the ring. In the paired six-second trials there are
five commits, all from bus context `2d08`: four type-`74` packets with payload
prefixes `340e`, `3532`, `0d00` and `0a090006`, then type `8a` with payload
`0004`. The last packet arrives at 2.687767604 seconds for the analytic tone
and 3.143782044 seconds for silence. Their packet bodies are identical.

There is **no type `83` or `8b` RSSI-results publication in this bounded
capture**. Internal corrected RSSI fields must therefore not be presented as
a qualified MCU RSSI response. Type `8a` belongs to the `NO_PSW_FOUND` family
in the mapped MCU decoder; the observed negative result is not FCCH success.
Whether and how the corrected fields affect the native search decision is
the next join. The capture does not yet independently qualify MCU packet
consumption, nor prove that RSSI replies cannot occur in another lifecycle.

### Native search correlation products

`run_rom6_adc_lab.py --detect --machine nsm3adc` (and `nsm3ad0`) uses a
separate bounded observer for resident `4146..423a`. Its first initialization
window computes seventeen complex products via `422b..423a`. All seventeen
captured calls match independent signed dot/cross products of their four
actual input words, with the resident code, own entry hook and arithmetic
mode checked. In observed mode 0, for `(x0,y0,x1,y1)`:

```text
A = x0*x1 + y0*y1
B = x1*y0 - y1*x0
```

The tone supplies `(804,-1936,804,-1936)`, yielding `(4394512,0)` every
time; silence yields `(0,0)`. This extends the qualified FIR/mixer prefix
into the native search computation without substituting a detector verdict.
The separate read-only `rom6_adc_mode1.lua` observer checks fifty nonzero-input
mode-1 calls against the same signed dot/cross equations, including varied
SCH inputs. Both arithmetic delay slots at `4235..4236` execute before the
branch to `4238`; mode 1 does not omit either term. The two delayed `MVDD`
instructions leave `(x1,y1,x1,y1)` in `2f52..2f55`, independently checked
alongside the products. This qualifies products/history, not SCH decoding.
The same observer also retains sixteen consecutive later mode-1 mixer
blocks, starting at the first nonzero FIR block. All 256 observed FIR words
at `00c8..00d7` produce exactly the 32 published words under the independent
eight-pair quarter-rate rotation/sum; circular output pointers and producer
context `24d7` also match. The bounded filter observation has zero overflow.
This joins actual FIR outputs to mixer outputs, not an independent check of
these later FIR inputs against the waveform. It excludes a mode-1 summation
or output-pointer error in that window, not a capture-timing error.
Sixteen nonzero-correlation mode-1 metric returns also match the independent
integer ratio below, with numerator multiplier **256** rather than **1024**.
Both delayed branches at `41ca`/`41d1` execute their shift/square slots;
mode 1 skips only the following two-bit scale. The checker covers the
observed nonsaturating input domain, not all later search metrics.
Sixteen subsequent normalized-metric returns per input also match an
independent integer ratio of the observed correlation sums and energy. For
the observed nonnegative-energy, nonsaturating mode-0 domain, normalize all
three inputs with `s = 31 - bit_length(energy)` (zero uses `s=0`), take signed
bits 32..16 after shifting by `s-9`, square them, and compute:

```text
denominator = max(1, normalized_energy_squared & 0xffff)
numerator = 1024 * (normalized_real_squared + normalized_imag_squared)
quotient, remainder = divmod(numerator, 2 * denominator)
native_accumulator = (remainder << 15) | quotient
```

The tone has correlation `(74706704,0)`, energy `19775304`, denominator
`1369`, numerator `20647936`, and native accumulator `01531d75`: metric word
**7541**, above both stored thresholds `0405` (**1029**). Silence returns
zero through the denominator-one guard. This validates ratio arithmetic for
these actual inputs, not independent generation of the entire energy window.
Physical FCCH timing remains unqualified; finite-burst control acceptance
is qualified separately below. A continuous
tone alone is not a complete GSM FCCH/SCH waveform and cannot establish
decoder success.

The detector observer reserves debugger
temporaries independently of the measurement observer; the base observer's
`temp9` is reserved for ARM self-test pointers and must not be reused as a
DSP counter.

### Uploaded search threshold and continuous-tone rejection

The actual consumer is the handset's uploaded `0b40..0d7f` code, not the
resident `40bc` wrapper. Its 1152 bytes match MCU offset `1170b0` and SHA256
`245f696206a75730ab96ea480d5927b1b194533947d8fb732f057d4dbb3ecfb0`.
The `0b92` call returns the metric to the `0b94` comparison against data
`3eaf`, whose observed value is **`0574` (1396)**. Read-side observations at
bus context `0b9a` join all sixteen qualified metric returns to the actual
threshold operand. Tone `7541` is above it; silence `0` is below it.

The uploaded positive path increments `3044` at `0b9d`; `0bc3..0bc7` branches
to failure `0d68` when that count reaches **500**. The continuous-tone run
has exactly **500** threshold reads, while silence has **15938** before its
later negative packet. Thus continuous tone already clears the actual
metric threshold, but triggers a duration/count guard. This supports testing
an independently timed **finite FCCH burst**, not raising signal amplitude,
changing thresholds or injecting a success result. The bounded first-32-read
capture does not claim that every later metric was individually recorded;
the guard interpretation combines the pinned uploaded code with total read
counts and the observed native negative packets.

### Finite FCCH ADC-pin fixture

`nsm3afb` is a separate **laboratory** composition, not another supported
handset or a tuned RF source. Its ideal rectangular tone follows
[GSM 05.02 v5.1.0 section 5.2.4 and table 3](https://www.etsi.org/deliver/etsi_gts/05/0502/05.01.00_60/gsmts_0502v050100p.pdf):
148 zero bits including tails, equivalent to a +1625/24-kHz offset, in TS0
of frames 0/10/20/30/40 of each 51-frame multiframe. With the declared 13-MHz
reference, a frame is 60000 ticks and the active interval is 7104 ticks.
The waveform's epoch is absolute zero, independent of firmware requests,
capture enable and FIFO reads. Q remains latched from the preceding I
conversion even across an envelope boundary.

A second, bounded raw-read window starts at the first nonzero P27 word,
not at a request or a chosen detector result. Independent epoch/counter
joins validate reads 655..1678 at source ticks 33000000..33012276:
592 active words (296 complex samples) followed by 432 off-slot words.
This is the scheduled frame-40 FCCH at source frame 550. The earlier
128-output FIR window is silent because it precedes the burst; the full
4096-word raw prefix is not all silence. This qualifies delivered waveform
provenance, not FCCH detection or physical receive timing.

```sh
.venv/bin/python tools/run_rom6_adc_lab.py --machine nsm3afb --seconds 6
```

Other slots contain zeros: no SCH, BCCH, BTS power envelope, RF gain/noise or
HAGAR-selected carrier is implied. An isolated C++ test checks all five
bursts, 296 complex samples per active interval, exact edges and multiframe
periodicity. The independent Python trigonometric reference checks delivered
sample provenance, including the latched-Q boundary. Existing continuous
tone and silence compositions are unchanged.

The finite waveform advances beyond the continuous-tone path. After the
independently tested CPU alias fixes, it reaches uploaded sample-processing
helper `1cc5..1cff`. Those 118 bytes match MCU offset `11681a` and SHA256
`5d7bf683dab9d7869f16ff09ae114740d11c7c694b9f5e678e11433fac6ed3ba`.
The helper deliberately uses mapped AR6/AR7 addresses as scratch extrema:
`1cc6/1cc8` initialize AR2=`0016`, AR3=`0017`; its conditional stores update
these registers before computing their sum.

The mapped AR7 store at `1ce7` now uses the independently tested generic
Xmem-register path. The current six-second trace has 702272 total native
reads, peak queued 69 and no transport fault; all 4096 retained source
words match the independent waveform.

### SCH training-correlation identification

The finite stimulus now reaches resident `76b0..76cf`. Its program words at
`7737..773a` are **`b962 040f 2d45 761b`**, exactly the 64-bit extended
training sequence in GSM 05.02 section 5.2.5, encoded MSB-first. This is an
independent protocol anchor, not a sibling-emulator label. The routine reads
each mask through READA into AL, then tests its high bit and shifts it to
choose the sign of successive complex sample components over four groups
of sixteen iterations.

The routine and its later processing use generic, independently tested CPU
paths: mapped accumulator BIT, repeated paired addition, block-interior
RPTZ, BRC-to-AR copies and CMPS. Their operand/admission contracts and
focused save/replay tests live in `c54x_execution_contracts.md`.

[TI SPRU172C](https://www.ti.com/lit/ug/spru172c/spru172c.pdf),
CMPS description (4-35), identifies `8e/8f` as compare,
select and store maximum: compare the signed high/low sixteen-bit halves
in **Read**, retain the larger half for the memory store, and update
TRN/TC in Execute (low wins ties). This timing rule is explicit in the
manual, not inferred from this firmware. The paired `8e93`/`8f94` calls
and circular counterparts follow dual-lane arithmetic at `9015..902a`;
this alone is not proof of a decoded SCH payload.

The unchanged six-second finite trial completes without unsupported CPU
execution or transport faults: **1464672** native input words, all 4096
retained source words independently checked, terminal idle PC `3999`,
and provisional queue peak 69. Its fifth native RX-ring publication at
`3.344663736s` is type `80`, payload
**`40000100000b0004000000000000`**, instead of the continuous-tone/silence
type-`8a` negative packet. Raw accepted trial:
`tmp/rom6-adc-lab/nsm3afb-qa8mlp5u`; continuous-tone control
`nsm3adc-e2gbzpod` remains fault-free with independent metric checks.

### Native failed-SCH publication

The own-ROM producer now establishes the failed-block status, rather than
borrowing a sibling's packet label. The `4ba8` class-`40` branch calls
decoder `9268`, normalizer `4d9d` and check routine `4f3a`. The latter
masks its remainder to ten bits, compares it with `03ff`, and writes
`2f20=1` on mismatch through the return-delay MVMD at `4f5d`. In the
SCH-absent trial all three decoded words are zero, the remainder is zero,
and the captured status write is one at `3.344658769s`. Its displayed
context `4bc8` is the caller, not the owning instruction.

Formatter `4ee0` packs `2f20 << 8` into word `2f5c`: this is byte 2 of
the received-block payload. All seven observed `2f5b..2f61` writes join
the source metadata and the exact fourteen published bytes. The branch
at `4bcd..4bd3` supplies type `80`, length `0e` and source `2f5b` to
publisher `2c88`; its ring commit writes through `2d03` before PORTW1.
The packet's frame words come from `0912/0913`, BSIC from `250a`, ARFCN
from `250e` and shift from `2615` on the observed selector-zero branch.
They are metadata, **not decoded-frame or valid-BSIC evidence** in a
failed block.

`check_fcch_only_sch_failure` pins the relevant own-ROM instruction
fragments and joins zero-codeword/remainder/status, formatter sources,
seven writes and the actual packet. It deliberately qualifies only this
negative control, not the CRC algorithm for arbitrary words or a valid
SCH decode. Fresh evidence: `tmp/rom6-adc-lab/nsm3afb-xqpil8w6`.

### Isolated positive SCH soft-input decoding

```sh
.venv/bin/python tools/run_rom6_receive_kernel.py
```

The runner first makes a passive four-second native entry capture with
`rom6_receive_entry.lua`, then executes a **separate CPU-only fixture**.
`--entry-dir` can reuse that capture; `--frame` and `--bsic` select the
independently encoded laboratory message. The fixture is explicitly selected
with `run_c54x_section.py --fixture-header`, not included in ISA suite discovery.
Acquired program/data files remain private inputs, not embedded source arrays.

The native class-40 path copies 78 soft words into `2f80..2fcd`, calls
Viterbi routine `9268`, normalizes its packed output at `4d9d` and checks
the SCH polynomial at `4f3a`. The decoder pads its terminal history with
positive `127`; positive soft input represents encoded zero. The captured
failed-SCH frontend actually supplies 78 **+1** words. This uniform-input negative control
reproduces three zero decoded words and CRC status 1; zero-valued soft inputs
would instead introduce metric ties and are not the captured contract.

For independently convolutionally encoded input with soft magnitude 32,
the native decoder/normalizer produces the exact expected 35 information/parity
bits, packed least-significant-bit first into `2f80..2f82`, and the native
CRC checker writes status 0. Frame/BSIC cases `(1,42)`, `(11,0)` and
`(2715638,63)` exercise distinct information fields, including the hyperframe
boundary. No decoded bits or CRC verdict are supplied to executing code.

The fixture clears all RAM below `b000`, retains acquired DROM tables, gives
the routine a normal return stack and stops at the original caller's CRC
continuation, before MDI formatting. The only program adaptation is that
isolated stop instruction; decoder, normalizer and CRC bytes are checked
against the acquired ROM6 image. No MCU, radio peer or handset peripheral
participates, and unexpected I/O fails. Both negative and positive cases have
a 1,000-cycle snapshot and 30,000-cycle finish with replay. Inputs and results
live under `tmp/rom6-receive-kernel/`, including per-run `kernel.log`.

This qualifies the soft-input Viterbi/packing/CRC path, **not** the raw-sample
training/equalizer frontend, organic receive timing, BCCH or camping. The next
step is to recover the preceding sample-buffer contract and drive it from
the independent GMSK source without touching the booting phone's state.

`--capture-frontend` adds passive class-40 snapshots at `743a` (sample-stage
entry), `7937` (first filter builder), `76a7` (training correlation) and
`7790` (equalizer), plus up to 1,024 actual P27 read values. All eight entry
stages are observed in `sch-zg5wqe7t`. Its complete 960-word SCH transaction
matches the independent source at ticks `43445256..43456764` with zero
differences; 116 words are nonzero, all within its initial FCCH tail ending
at word154. The later zero correlator window must not be described as an
entirely zero ADC transaction. The uploaded `18a4` preprocessing reader,
resident `2439` discard path and `2592` sample-storage path all participate;
the initial RSSI FIR-prefix test alone does not qualify this whole path.

### Isolated native control-channel bit decoding

Both receive runners also accept `--raw-adc`. This supplies words through
the P27 I/O endpoint and invokes native INT0 vector 16 using laboratory
software interrupt instructions. The actual `07c0` vector, uploaded `1845`
hook and resident `257d..25d8` ISR perform all FIR and buffer writes.
The uploaded hook is checked against the own MCU bytes at `115f1a`.
Ordered I/O stimuli and their cursor replay are test harness facilities, not
register behavior added to the CPU or a decoder verdict.

The declared entry contract is zero history/DC offset, coefficients
`0387/11a3/2ad6/4000/4000/2ad6/11a3/0387`, `022a=0`, `022c=2`,
and a non-expiring isolated block countdown. SCH consumes 23 blocks of
32 raw words to fill `0080..01ef`; BCCH consumes 19 per burst to fill
`2667..2796`, 76 blocks/2432 raw words in total. No filtered buffer or
decoded decisions are supplied by the host. SCH verifies exact FN/BSIC and
CRC including zero-input rejection; BCCH verifies exact SI3 and all-ones
payload/parity, with zero raw inputs failing the expected SI3 comparison.
Save/restore replays the same stream and results. This closes isolated
ISR/FIR execution, not asynchronous block production or physical INT0 timing.

`tools/run_rom6_xcch_kernel.py --entry-dir <passive-entry-directory>`
reuses the acquired resident program/DROM and runs native `924c`, followed
by the actual caller's normalizer at `4d06..4d12`. Low live RAM is cleared;
456 independently encoded signed soft words are the declared input.
The fixture stops **before** `3a26`, not after a fabricated FIRE verdict.
Known SI3 bytes and an all-ones control block both reproduce all 184 payload
bits and 40 parity bits exactly, including save/restore replay. The caller
complements the 40 FIRE bits before its hardware transaction, which is
included in the expected packed output.

`tools/gsm_xcch_vector.cpp` reuses the existing channel encoder. Its output
is independently checked by `tools/gsm_xcch_signal.py` against the FIRE
polynomial, convolutional code, four-burst interleaving, training and radio
octet bit order specified by [GSM 05.03 sections 4.1 and 4.4](https://www.etsi.org/deliver/etsi_gts/05/0503/05.00.00_60/gsmts_0503v050000p.pdf).
Radio octets are LSB-first; the existing library's packed-control helper is
a different, MSB-first internal representation and is deliberately not used.

Remaining boundary: native `3a26` writes 14 packed words to port `39` under
port `3c` control, and `3a81/3ade` consume status bits `60` from port `38`.
The backend implements the functional224-bit check subset below; the
request-only fixture still does not exercise a completed peripheral check.
The fixture's `--fire-transaction` option verifies the complete observed
request and save/restore replay, halting at `3ade` before status polling:
`3c <- 3,5`; fourteen LSB-first packed words to `39`; `3c <- fffc,2`;
port `00` read/OR `60`/write; port `38` read/AND `10`/OR `5`/write.
Its declared zero initial I/O bank supplies no status-success bits.
This establishes the request format, not the meaning or latency of the
accelerator's status. The consumer accepts status `20` directly and uses
status `40` for a correction path reading ports `3a/3b`; other results
produce the failure flag. The request's `5` is not itself a successful check.

### Functional FIRE coding subset

`nokia_dsp_fire_state` owns the observed input grammar at P3c/P39 and the
computed check result at P38. The supported sequence is `3,5`, exactly14
input words, `fffc,2`, then P38 command `5` (optionally retaining bit4).
Words are consumed LSB-first. The caller has complemented the transmitted
40 parity bits, so polynomial division of the resulting224-bit block by
`(x^23+1)(x^17+x^3+1)` must leave zero. Only then is status `20` published.
Incomplete/overflowed inputs, invalid sequencing, nonzero syndrome and
unsupported operations cannot publish success. Result bits written by
software are masked out. P3c=`3` restarts the input stream without inventing
a reset of unrelated P38 command fields; cold device reset clears the state.

For encoding,the same3/5 loading grammar admits exactly12 input words,
of which only184 bits are information. Afterfffc/2,P38 command3 computes
the uncomplemented40-bit polynomial remainder. Native3ab6 selects P3c3/7
and reads three P39 words;807f complements them and packs the first32 bits
LSB-first followed by the third word's high byte. Its unused low byte is
not physically qualified. P39 no longer returns the last input word in
this observed result-read phase;unknown/unselected phases retain the opaque
register fallback. Output,cursor and validity are saved,with side-effect-free
peek and partial-read restoration tests.

This is synchronous **functional** completion, with no busy interval,
interrupt latency,cipher result or error-correction claim. Status
`40` and its P3a/P3b correction data are not fabricated. A corrupted block
is rejected even if real silicon might correct it. Other operation contracts
remain unresolved, not successful no-ops.

`tools/test_dsp_fire_cpp.py` checks independently generated zero, all-ones
and SI3 codewords and all224 single-bit corruptions of each, plus partial
input, overflow, unsupported command, reset and state-copy replay. The
native-backend BSP gate separately exercises the actual MMIO binding and
MAME save/load midway through an input block. This qualifies the functional
peripheral subset,not physical accelerator timing. Encoder vectors cover
zero,all-ones,walking bytes and native SABM;partial/overflow/reset and
ignored upper padding bits are checked independently. In the whole-phone
run,the previous opaque P39 projection produced correct convolutional data
but invalid FIRE bits (`d4 ff d4 ff ff` on the first block). Modeled readback
makes all10 serialized xCCH blocks independently FIRE-valid.
### Isolated normal-burst receive chain

The `--bursts` option instead supplies the four independent normal bursts'
114 soft nibbles, excluding tails/training/stealing flags, to native
`5905/58e0`. Their interleaver writes are checked against every one of the
114 packed words at `02c2`. The routine starts each half-burst at
`57*burst` and `57*(burst+4)`, then advances 64 modulo 456, matching the
independent GSM control-channel mapping. The adjacent `5b26` path and its
`dcc2` table are a different layout, not this four-burst control path.

The isolated caller then enters `4cf7` with the actual caller's unpack
count `AR1=71`, and executes `5b18`, `4da7`, `924c` and the normalizer.
Equal, nonzero per-burst confidence is an explicit laboratory precondition;
positive/negative decisions use nibbles `3/c`. Known SI3 and an all-ones
block reproduce all payload/parity bits and save/restore replay. No handset
RAM or native decoder code is changed; the small laboratory caller is not
part of the booting phone.

`--frontend` replaces the supplied nibbles with four independently generated
GMSK normal bursts at TSC2, filtered using the separately checked three-tap
FIR. Each 304-word I,Q buffer represents 152 complex samples, including the
zero history/trailing padding. The native `743a` normal-burst frontend owns
training correlation, channel estimation, equalization and soft decisions;
its output flows directly into `5905`, not back through a host decoder.
Both SI3 and an all-ones block reproduce the exact payload/parity with
restore replay. `--frontend --zero-samples` fails at the payload comparison
(`2f80=0`, expected SI3 `0649`), after reaching the same halt without an
illegal instruction. The signal, not retained phone RAM, supplies the result.

Normal-burst entry uses class `50`, product mode `30`, TSC selectors
`250b=2616=2`, filter coefficients `df85/6d1b/df85`, and completed buffer
pointer `0231=2797`. The caller supplies a complete buffer and equal nonzero
confidence; these are isolated contracts, not an emulated arrival schedule
or measured analog gain. Without `--raw-adc`, ADC interrupt/FIR execution
is not included.

No hardware CRC acceptance, organic BCCH reception or camping is qualified
by these isolated fixtures.

### Isolated positive SCH filtered-sample frontend

```sh
.venv/bin/python tools/run_rom6_receive_kernel.py --frontend
```

This starts at native sample routine `743a`, not the Viterbi entry. Its
input is 184 complex samples (368 words) in **I,Q** order at `0080..01ef`.
The native `2592..25af` FIR/storage path writes I then Q, unlike the earlier
RSSI observer's Q,I output ordering. The test supplies a complete buffer
(`0231=01f0`) as an isolated entry precondition, not a state write in a
booting handset.

The independent BT=0.3 GMSK generator produces two complex samples/symbol.
An independent eight-coefficient FIR/decimation calculation produces the
one-complex-sample/symbol input in that buffer, with the declared zero
history and no receiver-selected waveform shift. Native `743a` then runs
the three-tap/quarter-rotation builder, extended training correlation,
channel estimation/equalization, soft decisions and the previously qualified
Viterbi/packing/CRC chain. Exact decoded information/parity words and status
0 pass for FN/BSIC `(1,42)`, `(11,0)` and `(2715638,63)`.

All live RAM is cleared except five explicit captured configuration words:
`0524..0526=df85,6d1b,df85`, `250f=0030`, `2f02=0040`. Input buffer,
available-buffer count, return stack and a non-verdict status sentinel are
initialized by the testbench. Acquired DROM tables remain intact, and the
entire resident frontend program matches the acquired ROM6 image. The
zero-sample control must produce zero decoded words and CRC status 1.
Both cases have a 1,000-cycle checkpoint and a bounded 200,000-cycle finish;
all six case/replay pairs pass without any I/O.

This is a positive **native filtered-I/Q kernel** result. The preceding
ADC ISR/FIR is calculated independently, not executed in `--frontend` mode;
its full SCH preprocessing path and real sample arrival/interrupt timing
remain separate obligations. No MCU-visible result, carrier selection,
boot synchronization, BCCH or camping is implied.

### Correlator input and capture timing

The following zero-aperture observations are NSM-3-specific. The own
[6210 trial](6210_bringup.md#reference-clock-raw-adc-trials) instead checks
192 nonzero native builder stores and a final 960-word receive epoch beginning
141 complex samples into the independently generated SCH burst. Neither
aperture qualifies synchronization; do not import one product's failure cause
into the other or retime the independent source to fit the decoder.

The first correlator pass is not reading stale reset data. All 192 words
at `00d8..0197` were written by builder `7937..7966` at
`3.342868352..3.342924879s`, immediately before the first mask-table
selection at `3.342932440s`. The builder uses the existing complex FIR
and quarter-rotation operations with coefficients at `0524`; its source
pointer is three or four words ahead of each observed destination. All words are
zero in the current coded-SCH fixture. Thirty-three correlation positions start
at `00d8,00da,..0118`, without another P27 read between them.

A twelve-second unchanged-source run (`nsm3afs-eliz4fsi`) retains exactly
these 33 correlation positions and one type-80 failed SCH publication at
`3.344087736s`. The six captured native ring commits also include a later
type-8b RSSI survey at `6.862295780s`; receive activity continues, but merely
extending this run does not produce another SCH attempt. Its raw transport
remains fault-free with the same provisional 96-word staging and peak 69.
This bounds the waiting experiment, not the physical sampling contract.

The shared read count 702272 identifies capture 5: 701312 words were consumed
in the four closed captures, followed by 960 in the current capture. Its
declared source interval is ticks `43445256..43456764`: it begins within
frame 724's FCCH envelope but ends before frame 725's SCH. The later
correlator window is zero. This is a measured laboratory aperture, not an established
physical COBBA/MFI acquisition window.

The final control sequence is P0a=`117e`, P0b=`3a98`, P09=`1221`,
then P03=`0105`. A passive P0d read returns `1095` at `3.341935429s`;
P03 is written at `3.341942747s`. Advancing that counter at 13MHz/12,
with its unobserved sub-tick phase bounded by one count, places P09
388..389 quarter ticks ahead of the immediate lab start (about 358..359us).
The source's absolute epoch must not be confused with the separately
loadable SCU counter epoch. These observations expose the unresolved
arm-versus-compare boundary; they do not establish which compare should
clock ADC conversion, serial transfer or INT0, or prove that a timing change
would produce a valid SCH.

Bounded observation: `tmp/rom6-adc-lab/nsm3afs-mpd0rspg` after the mapped
READA correction described below. The observer
retains only the last 128 relevant controls and the last writer per sample
address, and freezes them at the first training-mask selection. It neither
changes registers nor schedules a response.

### Independent SCH source coding

`tools/gsm_sch_signal.py` implements the source bit layers from GSM 05.03
section 4.7 and GSM 05.02 section 5.2.5: 25 information bits, ten parity
bits giving polynomial remainder `03ff`, four terminating zeros, 78
convolutionally coded bits and the 148-bit synchronization burst. The
64-bit training sequence is placed between the two 39-bit coded halves.

The independently published
[Osmocom coding test vector](https://github.com/osmocom/libosmocore/blob/master/tests/coding/coding_test.ok)
for packed information `01020300` agrees bit-for-bit on all 78 output bits.
Separate polynomial long division checks zero, all-one and each information
basis vector; tests also check burst placement and reject malformed inputs.
The implementation is derived from the standards, not the handset decoder.

BSIC/frame packing follows
[GSM 04.08 v5.0.0 section 9.1.30](https://www.etsi.org/deliver/etsi_gts/04/0408/05.00.00_60/gsmts_0408v050000p.pdf),
GSM 05.02's T1/T2/T3' definitions, and the least-significant-bit-first
octet mapping also specified by
[TS 44.004 v4.0.0 section 7.10](https://www.etsi.org/deliver/etsi_ts/144000_144099/144004/04.00.00_60/ts_144004v040000p.pdf).
An independently indexed field decoder reconstructs BSIC/FN, including T1
and hyperframe boundaries; malformed or unscheduled SCH positions are rejected.

The ideal GMSK source follows
[GSM 05.04 v5.0.1 sections 2.2-2.6](https://www.etsi.org/deliver/etsi_i_ets/300900_300999/300959/01_60/ets_300959e01p.pdf):
dummy ones outside the burst, differential precoding, BT=0.3 Gaussian
filtering and modulation index 1/2. The Gaussian/rectangle pulse is
integrated analytically; phase at burst start is chosen as zero. Numerical
integration of the filter and instantaneous modulation frequency independently
checks the implementation, alongside constant-envelope and known-tone tests.
There is no BTS power-ramp, noise or channel model.

Four source samples per symbol represent either quarter-bit phase at which
the two-complex-samples/symbol lab interface can first be enabled; they do
not double the interface clock. Q must still be latched from the same I
conversion. These Python functions do **not** write DSP memory or messages.
The separate `nsm3afs` ADC laboratory machine supplies the corresponding
C++ waveform at absolute frame epoch zero, with BSIC 42 and frame-derived
SCH information. Its first-timeslot SCH frames are 1/11/21/31/41 in each
51-frame multiframe; FCCH remains at 0/10/20/30/40. Other slots are zero.
Independent Python comparison checks every quantized I/Q pair for four
frame/BSIC cases. The original `nsm3afb` remains FCCH-only, including its
failed-SCH negative acceptance. Neither machine is a tuned-air receiver.
Source unit tests are not RF, synchronization or camping results.

The six-second `nsm3afs-3tq6lnj4` trial independently checks delivery of
1024 consecutive raw words starting at global read 5655. Source ticks
33060000..33072276 cover frame 551's SCH: 592 active scalar words (516
nonzero) and 432 off-slot words. Every value agrees with the Python encoder
and modulation reference, including simultaneous-I/Q latch ownership.
The full run consumes 1464672 words without transport faults (peak staging
69 of the provisional 96 words). Nevertheless, the later 192-word
correlator window remains zero and the native failed-SCH packet is unchanged.
Thus adding a correctly delivered SCH source does not by itself correct the
unqualified acquisition aperture. Do not tune its absolute epoch to the
firmware request or interpret packet publication as synchronization.

Reaching this standard-pattern correlator and its later processing does
not prove FCCH acquisition, SCH synchronization, decoded frame number or
camping, particularly for the FCCH-only negative stimulus. Native receive
validation of the separate coded SCH source remains necessary. Use its
known training/data positions to discriminate capture timing;
do not retime the source to a request or guessed decoder expectation.
No firmware result or decoded radio message is synthesized.

### Native finite-search acceptance boundary

The own uploaded `0b40..0d7f` search has a positive control path before
the failed SCH operation. Passive writes in `nsm3afs-mpd0rspg` show seven
`2f65` updates at issuing instruction `0d26`, from `fffa` through zero.
Each occurs in source frame 30/40/0/10/20/30/40, shortly after its FCCH
envelope ends (source-frame offsets 7519..7803 ticks versus end 7104).
The following branch sets bit 7 of `2627` at instruction `0d5d`, observed
at `3.296011297s`; it follows the zero confirmation count. The independent
checker pins the complete own uploaded search code and checks chronology,
counter progression and absolute-source burst alignment. This establishes
**native search-control acceptance**, not independently correct frequency,
frame phase, physical capture gating or SCH synchronization.

At this boundary the observed words are `2f78=fff7`, `2f7b=06f0`,
`2f6c=fffe`, `2f6d=0000`, `2f68..69=0000,0000` and
`247a..7b=0000,0001`. The timing residual has an independent arithmetic
check below; these are not yet qualified physical measurements. The next question is whether their derivation and later
capture selection agree with the known source epoch. Keep that check
separate from merely observing the acceptance flag; do not shift the
source to make the selected aperture appear correct.

### Frame-adjustment readback boundary

Own routine `56de..56fb` adds the pending correction at `2658` to its
argument, accumulates it into `053b`, records P10 into `0557` for a nonzero
adjustment (otherwise `ffff`), and writes `4999 + adjustment` to P0e.
The finite-source run applies `2477=04e8` (1256), producing reload `186f`.
The zero-adjustment reload `1387` follows on subsequent scheduler entries.
This is firmware arithmetic, not proof of the physical reload/latch edge.

P10 is still opaque in the backend. In the four-second
`nsm3afs-lhizd1pj` run all **11** observed P10 reads return zero: one at
issuing instruction `56ec`, ten at `a2e1`. The passive trace's total is
below its cap of 32. The latter routine stores its read into `265a` and
then overwrites A with unrelated initialization constants. Both saved
values have identifiable consumers: `0557` is compared with a P15-derived
tuple field at `2b29`, and `265a` is compared with a fresh P10 read at
`a49a` behind the `262c`/`2615` gates. **Neither saved value is read in
this bounded run before the failed SCH operation.** Therefore its zero
readback cannot explain the current failure through either mapped consumer.
This does not qualify a physical P10 value or exclude its later lifecycle.
Do not add a free-running-counter answer solely because the port is missing.

The search computes eight accumulated differences at `0d0b` from the old
signed residual, unsigned peak/base indices and a table word selected through
READA into AL followed immediately by LDM AL,A. A generic CPU bypass defect
formerly supplied the table address rather than its contents. The standalone
72-case mapped-program-read fixture reproduces and guards the correction.
Native A now matches the actual selected words, and every 40-bit difference
matches `signed(previous) + peak - base - table_word`; the final residual is
`-2`, not the invalid accumulated address differences. The 20 table entries
represent successive 10/11-frame FCCH offsets in 32-quarter-symbol units,
within one unit; no uniform nearest-rounding rule is assumed (entry 16 is
three quarters of a unit below the exact value).

The resulting arithmetic is
`((trunc(-2/7) + 1776 - 18) * 32) mod 5000 = 1256`.
The frame adjustment and later capture shift, but SCH remains invalid with a
zero correlator window. The bounded peak/source-clock join below establishes
capture-local sample accounting. It does not establish the physical counter
epoch or prove that the reload/capture boundary implements the silicon clock.

### Peak indices and the first receive block

`check_peak_clock` joins the bounded peak-write prefix to actual capture
epochs and consumed P27 words. In `nsm3afs-9lqtp5ki` all 128 retained
peak updates (of 198 observed writes) satisfy exactly:

```text
peak_index = capture_local_consumed_words / 32 + 1
last_consumed_source_tick = first_tick + (capture_local_consumed_words - 1) * 12
```

Every joined read count is a block multiple. The result holds in both
search modes: native mode 0 publishes four mixed scalar words per block
and the metric consumes four; mode 1 publishes two and consumes two.
Both therefore advance their index once per 32 raw words, matching the
32-quarter-symbol timing units in the peak-offset table under the declared
word clock. The checker derives source time from the queue epoch, never
from the delayed CPU read timestamp; mutation tests guard capture accounting,
index bias, source-before-use and the two observed store owners.

The immediate P03 gating does **not** align its first completed block with
P09. The two FCCH starts program P09=`1368` (4968). Passive P0d samples
and the existing 13MHz/12, 5000-quarter counter model bound the first-word
counter at 19..20 and 3886..3887 respectively. Their first completed
32-word blocks consequently occur around counter 50..51 and 3917..3918,
not at 4968. These are laboratory-clock observations, not physical edge
measurements. A **first completed-block deadline** hypothesis is distinct
from the previous producer-start hypothesis, but was also insufficient in
the bounded trial below. Neither result establishes the silicon truth table.

The removed completed-block candidate used the same source, scalar divider
12 and 96-word staging. It selected a future P09 match with enough lead for
32 real samples, rounded the deadline up to the scalar clock and placed
the first word 31 clocks before it. Latest P09/reload writes replaced a
pending start; P03/MFI disable cancelled it. No samples were backfilled.
In `nsm3afq-79w8cr8i`, all 4096 retained raw words match the independent
waveform and 912608 total reads have no transport faults (peak staging 69).
Five retained first-word clocks equal their scheduled values, each 372
reference ticks before its completed-block deadline. Native FCCH accepts
with residual zero, yet the SCH result remains three zero words, remainder
zero and error status one; its native type-80 packet at `3.343457692s` is
`40000100000b0004000000000000`.

The control's peak-index invariant does not automatically transfer to a
different start lifecycle: the candidate's retained peaks all have bias
zero instead of one. The existing checker correctly rejects that mismatch;
it was not weakened to accept the experiment. The failed native SCH result
independently rejects the candidate's sufficiency. Its machine, clock API
and registration changes were deleted rather than promoted into the driver.

Next resolve **which frame** the firmware selects for class-40 acquisition:
the FCCH frame adjustment into `0912..0913`, subsequent frame advances,
and the scheduler's preparation-versus-sampling boundary. Do not compensate
for an unresolved frame-selection error by shifting the laboratory source.

### Frame records and the unmodeled timing-tuple path

The complete 24-write journal up to the first SCH mask in
`nsm3afs-2b3tr0fm` distinguishes the owners of `0912..0913` through the
CPU's exported Execute PC, not only its later bus-context PC:

| Time | Completed low-word value | Execute owner |
| --- | --- | --- |
| 0.535307451 | 0 | `2085`, initialization |
| 3.298844110 | 1 | `4272`, normal increment helper |
| 3.298862440 | 2 | `5714`, FCCH offset application |
| 3.304613934 | 3 | `4272` |
| 3.341538242 | 11 | `4272`, following eight more regular increments |

The offset routine `56fc..573a` adds `247a..247b` to the existing record,
then recomputes the modular fields. `57fe` copies the record to `091c`
and increments that copy through `425f`, so current/next records must not
be conflated. A pre-SCH software frame count is not itself a decoded GSM
FN: its preparation-versus-sampling convention remains to be established.

`tools/rom6_frame_schedule.lua` adds a bounded read-only join after 3.2s,
stopping at the first class-40 reset (132 events of a 256-event cap in
`tmp/rom6-receive-kernel/frame-2by95i1x/primary`). Incoming write values
are logged separately from pre-write RAM snapshots. At `3.341945824s`,
instruction `463f` publishes class `40` with **current frame 11 and next
frame 12**; these records are not aliases. P09 receives `1221` at
`3.341941495s`, and P03 enables receive at `3.341942747s`, before that
class publication. Thus observing `2f02=40` is too late to identify the
physical capture-start edge by itself.

The frame adjustment writes P0e=`186f` (6255) at `3.298860945s`, then
updates the current frame from 1 to 2. The next normal increment is at
`3.304613934s`, followed by ordinary 5000-quarter periods. Class-40's
start lies in independent source frame 724 (FCCH), not 725 (SCH).
The next question is the capture compare's relationship to these frame
records and the one-frame reload adjustment, not another decoder payload
guess. This observation neither establishes a physical epoch nor warrants
delaying capture or shifting the source by a frame.

There is also a distinct timing-tuple interrupt path. The resident
dispatcher at `281d..281f` tests P01 bit `0080` and branches to `2afc`.
That handler acknowledges `0080`, reads P13/P14/P15 into a ring selected
by `0553`, masks the middle word to 14 bits, and compares the third tuple
word (P15) with saved P10 at `0557`. The postmodified pointer reaches the
third word before the load at `2b26`; it is not comparing P13. On equality it adjusts the saved `053a`
correction by `4999 - P0e`. The current backend's P01 projection produces
only slot `0008`, transfer `1000` and host-ring `0002`; it cannot reach
this handler. Passive counts confirm zero P01-bit-80 observations and zero
P13..P15 reads before the first SCH operation.

This is an identified **unmodeled contract**, not proof that the event must
occur during ordinary receive, nor evidence for a particular tuple value.
P10's observed non-use therefore does not close the wider timing subsystem.
Resolve the handler's event origin and record ownership before synthesizing
an interrupt or exposing a guessed free-running readback.
