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

## Remaining uncertainty

The controller now uses the physical 9,600-baud rate with a ten-bit character
time (approximately 1.042 ms) for byte completion and the currently coupled
FIQ3 event. The precise FIQ3 source/phase, collision, line echo, framing
errors, overrun behavior, and multi-byte buffering are not modeled.
The lower service/test protocol behind task 7 is mapped separately; ordinary
boot provides no evidence that it is an always-present MBUS peer. A future
tool or peer must attach through the byte callbacks and may respond only to
organic transmitted frames.
