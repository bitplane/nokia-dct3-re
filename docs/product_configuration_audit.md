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

## Next bounded refactor

The remaining coherent scalar cluster is DSP speech-control policy:

- parameter command;
- request mask; and
- request value.

These should become one typed contract with an explicitly absent predicate.
That representation must preserve NSE-3's evidenced command decoder without
pretending that its speech-start predicate is known. It must not absorb PCM,
radio, service-control or analogue routing, which are separate hardware and
protocol boundaries.

Display dimensions and MAD2 DSP-reset wiring are smaller typed-configuration
candidates after that pass. Broadly wrapping all enable flags in a capability
object is not justified: several apparently related flags deliberately encode
independent negative or dormant evidence.

## Acceptance status discovered by the audit

The 3330 and 3410 runs are reproducible but their checked-in LCD oracles do not
currently match. The stable outputs remain:

- 3330: `7e3ade861af1e0e47c76100c7a7c7f8c7719c1c497e02d1024ab91c1e55c1f8e`
- 3410: `e25ff95856e78489015a5116b2c7f42f19b40cc6e5ac637b3a1d7e066739bb26`

Those hashes are observations, not replacement acceptance oracles. Model
promotion stays conservative until the divergence is explained and the named
frontier/navigation gates pass again.
