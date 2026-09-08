# MBUS controller contract

`nokia_mbus_device` owns MAD2 PUP offsets `0x18..0x1a`, byte timing, status
generation, RX/TX holding state, and FIQ2/FIQ3 outputs. It exposes a received
byte input and a transmitted byte callback; no external service peer is
attached by default.

## Recovered firmware contract

The v6.00 initializer at `0x2aafb4` aligns with the v5.01 initializer at
`0x2a8040`. Both perform the same sequence: reset control through bit 7, clear
the low status conditions, initialize the byte register, then leave control at
`0x4c` in receive mode. A bounded one-second trace records ten accesses to
`0x18..0x1a` and no transmitted bytes in either ROM.

The v6.00 FIQ handler at `0x2b56cc` establishes the data-path predicates:

- status bit 4 with control bit 5 calls the TX step at `0x2aae2a`;
- status bit 5 with control bit 6 calls the RX state machine at `0x2aae76`;
- RX reads the byte from `0x1a` and acknowledges MAD2 FIQ2 (`0x04`); and
- the low three status bits feed an error/reset branch.

`make verify-mbus` checks the identical two-ROM initialization and the negative
result that ordinary boot has no MBUS counterparty. Its external-input fixture
presents byte `0xa5` only after receive mode is active. Firmware observes
status `0xe7`, reads the byte through its real RX handler, and acknowledges
FIQ2. The later incomplete-frame behavior is deliberately outside the
controller acceptance contract.

## Timer and remaining uncertainty

The controller uses the physical 9,600-baud rate with a ten-bit character time
(approximately 1.042 ms) for byte completion. The public MADos interrupt map
independently identifies FIQ3 as `FIQ_MBUSTIM` at 423.1 Hz, distinct from the
FIQ2 receive/transmit event. Products with an independently exercised timer
contract therefore run that source while firmware leaves FIQ3 unmasked;
status bit 7 only resets the serial bit counter. The 3210 profiles retain the
established no-FIQ3 ordinary-boot behavior until their clock gate is identified.

M2BUS is a single-wire half-duplex bus, so a transmitted byte is also sampled
by the receiver. The controller now exposes each completed TX byte through its
RX holding state; NAM-2 consumes and compares that echo through its ordinary
FIQ2 handler before sending the next byte. Public protocol documentation also
specifies 3 ms of idle bus before an ordinary frame, 2.5 ms before an ACK and a
200 ms ACK timeout. The precise oscillator phase, representation of a collision
when another endpoint drives a different line value, framing errors, overrun
behavior, and multi-byte buffering remain unmodeled.
The lower service/test protocol behind task 7 is mapped separately; ordinary
boot provides no evidence that it is an always-present MBUS peer. A future
tool or peer must attach through the byte callbacks and may respond only to
organic transmitted frames.

NAM-2 v5.84 supplies the first organic startup use of this controller. Its
initializer at `0x2f7f90`, start routine at `0x2f7c24`, FIQ3 handler at
`0x2f7d2a`, and byte-event state machine at `0x2f7d86` establish the
FIQ3-timer/FIQ2-byte-completion split independently of the ROM4 firmware.
The shared controller model therefore transmits
`1f ff 00 d0 00 01 01 01 31` with a valid zero XOR over the frame. No peer is
modeled: a trial using the superficially similar DSP-service D0 state-1/state-4
reply was consumed byte-for-byte but rejected by firmware. This closes the
controller behavior while leaving the external MBUS protocol explicitly open.

Public [Gammu/Gnokii Nokia protocol documentation](https://docs.gammu.org/protocol/nokia.html)
closes the wire-level application vocabulary. M2BUS uses terminal node `0x1d`,
requires a type-`0x7f` transport acknowledgement for normal frames, and defines
terminal startup request `1f 00 1d d0 00 01 04 <seq> <xor>` followed by phone
response `1f 1d 00 d0 00 01 05 <seq> <xor>`. A diagnostic trial delivered that
documented `D0/04` frame through the real RX/FIQ2 path and NAM-2 organically
returned `D0/05`, proving the application semantics. It did not settle the
startup transaction reproducibly. Adding physical transmit echo makes firmware
compare every byte but, correctly, does not manufacture the missing terminal
acknowledgement. The trial code is not retained.

The remaining wire evidence is the MBUSTIM oscillator phase and differing-line
collision representation. Closing NAM-2 startup additionally requires evidence
for the external terminal or service fixture expected during this transaction;
a peer must attach at the byte callback and follow the public timing and ACK
grammar rather than schedule firmware-specific state changes.
