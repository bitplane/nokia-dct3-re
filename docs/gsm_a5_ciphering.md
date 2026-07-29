# GSM A5 ciphering

The emulator has a standards-owned A5 boundary; it is not a Nokia product
compatibility response. `gsm_a5` generates a burst keystream from the selected
algorithm, 64-bit `Kc`, 22-bit GSM `COUNT` and link direction. `COUNT` is
derived from the saved TDMA frame number as `T1 || T3 || T2`. The implementation
is stateless per burst.

## Supported policy

- A5/0 is the explicit unciphered mode and produces a bit-for-bit zero
  keystream.
- A5/1 is implemented and checked against the published
  `Kc=1223456789abcdef`, `COUNT=0x134` 114+114-bit known-answer vector.
- A5/2 remains a typed but unsupported identifier. The API rejects it rather
  than reusing A5/1 or silently returning clear data. No retained DCT3 trace
  currently proves an A5/2 negotiation, so the laboratory network does not
  advertise it.

`make verify-gsm-a5` additionally proves wrong-key, wrong-COUNT and
wrong-direction separation. It also rejects a premature, keyless, malformed,
wrong-SAPI, A5/0 or unsupported-A5/2 activation transition.
`make verify-gsm-xcch-l1` carries a 184-bit xCCH
block through FIRE/convolutional coding, 456-bit formation, four-burst
interleaving, A5, normal-burst packing and an independent receive half. Its
negative compositions fail the FIRE check.

## Activation and ownership

The laboratory network policy selects A5/0 or A5/1 through `CIPHERCFG`.
Selecting A5/1 also requires organic MM authentication. A successful SRES
commits the independently derived network `Kc` to the saved GSM session. The
network then sends `06 35 01`; the handset loads its SIM `EF_Kc`, publishes its
product-owned DSP type-`0x14` control and organically returns RR Cipher Mode
Complete. Only that completion activates the session cipher. A cold preserved-
NVRAM service with no live network security context reauthenticates; it does
not manufacture a retained key.

The generic session owns algorithm, key-valid, pending and active state.
`nokia_radio_peer_device` owns frame timing and applies A5 only after coding and
interleaving:

- SDCCH/LAPDm uses the TS 45.002 SDCCH/8 subchannel-0 mappings: downlink
  `B(0..3)` and uplink `B(15..18)` in the 51-frame multiframe.
- TCH/F, FACCH/F and SACCH use their existing scheduled normal-burst payloads.
- exactly 114 data bits are XORed; tail bits, training sequences and stealing
  flags are never ciphered.

Nokia DSPIF grammar, SIM authentication, LAPDm, xCCH coding, TCH/F coding,
speech, PCM and host media remain separate owners.

## Save state and diagnostics

Save states retain the selected algorithm, `Kc`, key validity, pending/active
transition and TDMA frame number. They do not serialize an LFSR because each
keystream is regenerated from that state. The encrypted active-call replay gate
reproduces its digital speech interval exactly, including degraded bursts and
FACCH interruption, then tears down normally.

`Kc` is never printed by semantic traces. Nonzero DSP type-`0x14` payloads are
redacted, and raw reads/writes of the DSP transmit packet ring are suppressed
because those words cannot be classified safely until packet assembly is
complete. Logs expose only algorithm, direction and public frame/COUNT
metadata.

## Acceptance

The principal Nokia 3210 and 3410 gates and focused 3310/3330 gates prove
organic A5/1 command/completion and bidirectional ciphered xCCH and traffic
bursts. The 3210 additionally proves incoming and outgoing calls, non-silent
GSM-FR, SACCH, FACCH, degraded-frame concealment/recovery, active-call
save/load and clean physical teardown. These gates supplement rather than
replace every A5/0 registration, paging, call, SIM, media and power oracle.
The pending-SDCCH state gate saves after Cipher Mode Command but before Cipher
Mode Complete, proves byte-identical replay through activation and encrypted
SDCCH, and observes the restored call return to PCH. The outgoing promotion
gates prove physical dialing and encrypted call/media teardown on all four
products; the 3330 gate starts from separately provisioned, preserved PMM.
The host release gate replays encrypted CC/RR release exactly without
serializing its socket, and the two-call host gate requires two separate
command/completion/activation sequences with an intervening PCH return.

Relevant commands are:

```
make verify-gsm-a5
make verify-gsm-xcch-l1
make verify-radio-a5-1-incoming-call RUN_DIR=run-a5
make verify-radio-a5-1-outgoing-call RUN_DIR=run-a5-outgoing
make verify-radio-a5-1-sdcch-state RUN_DIR=run-a5-sdcch-state
make verify-radio-a5-1-state RUN_DIR=run-a5-state
make verify-radio-a5-1-degraded RUN_DIR=run-a5-degraded
make verify-3410-radio-a5-1-incoming-call RUN_DIR=run-a5-3410
make verify-3410-radio-a5-1-outgoing-call RUN_DIR=run-a5-3410-outgoing
make verify-3310-radio-a5-1-incoming-call RUN_DIR=run-a5-3310
make verify-3310-radio-a5-1-outgoing-call RUN_DIR=run-a5-3310-outgoing
make verify-3330-radio-a5-1-incoming-call RUN_DIR=run-a5-3330
make verify-3330-radio-a5-1-outgoing-call RUN_DIR=run-a5-3330-outgoing
make verify-radio-a5-1-host-release-restore RUN_DIR=run-a5-release
make verify-radio-a5-1-host-two-calls RUN_DIR=run-a5-two-calls
```

The channel placement and burst boundary follow 3GPP TS 45.002 and TS 45.003.
The A5/1 known-answer test records its complete key, COUNT and both 114-bit
directions in the source so it can be reproduced without the production
receiver. A5/1 was not published as a normal 3GPP algorithm specification;
the retained vector is the independently published Table III vector from
*Cube attack on Trivium and A5/1 stream ciphers*. This limitation is why A5/2
is not inferred from the A5/1 work.
