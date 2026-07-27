# Product configuration debt audit

This audit follows the radio, DSP service-control and DSP bootstrap contract
refactors. Its purpose is to identify invalid product-config combinations and
handset-family branching without turning independent evidence into a single
guessed profile.

## Findings

- Radio configuration is atomic. One `protocol_contract` owns wire values,
  acquisition strategy and lifecycle policy; disabled products cannot combine
  fragments from enabled products.
- DSP bootstrap configuration is atomic. BIOS variants replace the complete
  contract, so a pre-upload token cannot be combined accidentally with another
  exchange or completion policy.
- DSP service-control framing is atomic, but whether a product composes the
  service backend remains independent. This separation is intentional: NSE-3
  proves a receive-side frame contract while its unresolved bootstrap keeps the
  backend disabled.
- External-service transport enablement and its application contract must also
  remain independent for now. NHM-6 proves the lower transport without proving
  NHM-5 application data, while NSE-3 has only bounded dormant receive-side
  evidence.
- SIMI controller presence and the synthetic laboratory card are distinct
  hardware choices. They currently travel together in executable profiles, but
  combining them would make controller-without-card and removal/error testing
  unrepresentable.
- MAD2/COBBA PCM timing and word format already form one `bus_profile`.
  Analogue HLE routing is separately typed because physical connectivity and
  firmware-controlled mux/gain evidence have different confidence.

No product-name or driver-name dispatch remains in reusable protocol layers.
The only runtime firmware-variant selection is a generic, product-owned BIOS
override that replaces a complete bootstrap contract.

## Trace quarantine

Every retained firmware-address diagnostic in `nokia_dct3_trace.inc` has a
named focused consumer. The quarantine remains observational, bounded by its
structural test, and contains no firmware result writes or injected messages.
There is therefore no dead trace family to retire in this pass.

## Completed bounded refactor

DSP speech-control policy now forms one typed contract containing:

- parameter command;
- request mask; and
- request value.

The predicate is explicitly optional. NSE-8 and NHM-5 select independently
named contracts backed by their separate organic call lifecycles. NSE-3 selects
its evidenced command decoder with no speech predicate, so decoding cannot
silently enable media. PCM, radio, service-control and analogue routing remain
separate hardware and protocol boundaries.

## Completed display-geometry refactor

Display controller and viewport dimensions now form one typed geometry
contract. The LCD controller and MAME screen consume the same contract, and
zero-sized or controller-exceeding viewports are rejected centrally. The 3410
retains its evidenced 102-by-72 controller RAM and 96-by-65 viewport. The
unsupported 7110 and 6210 no longer resize only the screen while silently
leaving the controller at 84-by-48; they retain conservative defaults until
their different controller is modeled.

## Completed MAD2 DSP-reset wiring refactor

MAD2 DSP running-status readback and its release mask now form one typed wiring
contract. The default contract preserves ordinary latch behavior; the 3410
selects its evidenced `0x53`/`0x04` pair atomically, and half-configured pairs
are rejected.

## Next bounded refactor

The next coherent board-wiring pair is KBGPIO row topology and the power-key
column mask. They should form one typed keypad wiring contract so a five-row
matrix cannot accidentally retain an unrelated product's power-key wiring.
Broadly wrapping all remaining enable flags in a capability object is still not
justified: several apparently related flags deliberately encode independent
negative or dormant evidence.

## Acceptance status discovered by the audit

The 3330 and 3410 runs are reproducible but their checked-in LCD oracles do not
currently match. The stable outputs remain:

- 3330: `7e3ade861af1e0e47c76100c7a7c7f8c7719c1c497e02d1024ab91c1e55c1f8e`
- 3410: `e25ff95856e78489015a5116b2c7f42f19b40cc6e5ac637b3a1d7e066739bb26`

Those hashes are observations, not replacement acceptance oracles. Model
promotion stays conservative until the divergence is explained and the named
frontier/navigation gates pass again.
