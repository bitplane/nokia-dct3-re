# Nokia 6110 ROM3 bootstrap capture contract

The remaining executable NSE-3 boundary is one physical-DSP publication, not
an MCU packet guess. A useful capture must run the pinned Nokia 6110 v5.48
ROM3 PPM-B image (SHA-1
`5768841c9eb39c744f4fa04f0485e4f9ad4553b3`) on a matching real handset and
retain the ordered shared-memory traffic around the bootstrap.

The capture must establish all of the following:

1. the MCU parks shared offsets `0x002`, `0x004` and `0x006` at `0xffff`;
2. the physical DSP publishes the independently evidenced ROM3 pair `3/3` at
   offsets `0x004/0x006`;
3. the physical DSP acknowledges all 64 alternating sparse-flash transfers;
4. after the final acknowledgement, the DSP publishes `0x0b06` at offset
   `0x000` and a non-`0xffff` verification verdict at offset `0x002`; and
5. no HLE completion, firmware patch or RAM patch participates.

Retain the unedited raw trace and record its SHA-256. Convert its relevant
events to the versioned JSON shape exercised by
`tools/test_nse3_bootstrap_capture_check.py`, then validate both provenance
and semantics:

```sh
python3 tools/nse3_bootstrap_capture_check.py \
  capture/nse3_v548_rom3_bootstrap.json \
  --raw-trace capture/nse3_v548_rom3_bootstrap.log
```

This deliberately does not accept a ROM4 package label, the collaborator
emulator's hard-coded value, the measured 8210/ROM6 verdict, or a non-sentinel
chosen merely because the external MCU does not inspect its numeric value.
Once a physical capture passes, its exact verdict can enter the typed NSE-3
bootstrap-completion profile with the raw trace and provenance retained
outside the repository.
