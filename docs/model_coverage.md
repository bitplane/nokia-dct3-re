# DCT3 model coverage

This matrix owns product promotion state. Each row names the tested firmware
and composition; research-HLE or mask-compatibility results do not promote the
normal machine. `Yes` describes laboratory acceptance, not public-network or
electrical fidelity. Product contracts own detailed fixtures, gates and limits.

## Promotion rules

A cell requires a named reproducible gate or reviewed hardware/firmware
evidence. A declaration or acquired image is not execution evidence. PIN
acceptance and registration must precede an authenticated save boundary;
post-load events cannot satisfy those prerequisites. Exact state restoration
does not imply restoration of external SIP dialogs.

The levels are cumulative:

- **Booting**: the handset completes its evidenced flash, MAD2, CCONT, SIM and
  DSP bootstrap path without firmware hooks.
- **Interactive**: physical matrix inputs reproducibly drive normal firmware UI.
- **Registered**: the product's own radio packet grammar camps and completes
  Location Updating against the independent network peer.
- **Call control**: paging, ringing, physical Answer/End and clean teardown pass.
- **Internal media**: bidirectional GSM-FR crosses the firmware/DSP boundary,
  including FACCH substitution, degraded frames, SACCH coexistence and
  save-state replay.
- **Physical duplex**: isolated host microphone and playback paths pass without
  loopback, feedback, or UI-level injection.
- **Hardware-faithful**: all material product flash, DSP, PCM, COBBA and analogue
  topology claims are backed by hardware documentation, firmware analysis or
  reproducible traces. A working calibrated compatibility response is
  insufficient.

`Partial` in the Booting column means a deterministic execution boundary is
reached but the complete product boot acceptance gate does not currently pass.

## Product matrix

| Product / tested firmware | Booting | Interactive | Registered | Call control | Internal media | Physical duplex | Hardware-faithful | Contract and limiting boundary |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Nokia 5110i NSE-2 v5.53 PPM A | Yes (research, short-run) | Physical menus, SIM security and cold storage | No | No | No | No | No | Own uploads on unproved NSE-1 mask compatibility; bounded security/storage and menus, receive-supervision reset near 32 s. No registered radio or speech. [Contract](5110i_bringup.md). |
| Nokia 8290 NSB-7 v5.22 PPM A | Failure frame only | No | No | No | No | No | No | Normal profile is fail-closed; explicit nsb7hle separately qualifies laboratory phone services and SIP/physical HLE media. Native resident code and speech are missing. [Contract](8290_bringup.md). |
| Nokia 8290 NSB-7 v5.22 research-HLE `nsb7hle` | Yes (research) | Physical menus/input/storage | Yes (lab) | Incoming/outgoing SIP | Yes (HLE) | Yes (HLE) | No | Normal profile is fail-closed; explicit nsb7hle separately qualifies laboratory phone services and SIP/physical HLE media. Native resident code and speech are missing. [Contract](8290_bringup.md). |
| Nokia 6130 NSK-3 v5.61 PPM A | Bounded research menus | Physical code/menu/storage | No | No | No | No | No | Unproved NSE-1 mask compatibility, own NV fixtures and nominal ADC input; cold SIM/security/storage and bounded native restore. Reason-68 restart near 94 s; no registered radio. [Contract](6130_bringup.md). |
| Nokia 3210 NSE-8 v6.00 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Paired HLE phone/UI, security, storage and call/media gates; see protocol authorities below. Native speech and COBBA mux/gain remain separate. [Contract](mmi_layer.md). |
| Nokia 3210 NSE-8 v5.01 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Paired HLE phone/UI, security, storage and call/media gates; see protocol authorities below. Native speech and COBBA mux/gain remain separate. [Contract](mmi_layer.md). |
| Nokia 3310 NHM-5 v6.39 | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Own 58-exchange bootstrap, radio/PCM and physical-input acceptance; HLE calls/media and persistent services. Saved-tone pitch and analogue gain remain unproved. [Contract](cross_rom_confidence.md). |
| Nokia 3330 NHM-6 v4.50E | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Own fresh/preserved PMM, physical security/clock setup and HLE phone/media acceptance; derived phone-code repair is a separate fixture, not canonical provisioning. [Contract](cross_rom_confidence.md). |
| Nokia 3410 NHM-2 v5.46E | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Native ST flash/PMM and own HLE radio/PCM acceptance; encrypted degraded media and neutral-gain physical duplex. Native speech remains unproved. [Contract](cross_rom_confidence.md). |
| Nokia 5110 NSE-1 v5.30 | Yes | Yes | No | No | No | No | Partial | Recovered ROM4 runs on native C54x with physical menu/save/power acceptance. Native scan observations do not establish RF acquisition, calls or speech. [Contract](rom4_dsp_loader.md). |
| Nokia 2100 NAM-2 v5.84 PPM E | Partial | Yes (bounded donor fixture) | No | No | No | No | No | verify-2100-frontier/MBUS qualify board and terminal contracts. v5.84 donor-tail Wrong code and v5.21 own-checksum/C3 Code accepted are distinct instruments; neither proves usable idle. [Contract](2100_bringup.md). |
| Nokia 2100 NAM-2 v5.21 Sharp-labelled full image | Partial | Yes (security editor) | No | No | No | No | No | verify-2100-frontier/MBUS qualify board and terminal contracts. v5.84 donor-tail Wrong code and v5.21 own-checksum/C3 Code accepted are distinct instruments; neither proves usable idle. [Contract](2100_bringup.md). |
| Nokia 3610 NAM-1 v5.11 PPM E | Partial | Yes (service application + terminal) | No | No | No | No | No | Own MCU, display and M2BUS service application; missing matching NAM-1 PMM excludes the normal application task batch. [Contract](3610_bringup.md). |
| Nokia 5210 NSM-5 v5.40 PPM E | Yes | Yes | Yes | Yes | Yes | Yes | Partial | Own radio, display, keypad and MIC2/EAR contracts; HLE calls/media, SIM/Phone storage, security and supplementary services. Capacity/native speech/gain remain scoped separately. [Contract](5210_bringup.md). |
| Nokia 6110 NSE-3 v4.06 PPM B (ROM3 candidate) | No | No | No | No | No | No | No | ROM3/ROM4 verifier families are independently mapped. Matching internal ROM, storage and final DSP publication remain missing; no executable promotion. [Contract](6110_bringup.md). |
| Nokia 6110 NSE-3 v5.48 PPM B (ROM3) | No | No | No | No | No | No | No | ROM3/ROM4 verifier families are independently mapped. Matching internal ROM, storage and final DSP publication remain missing; no executable promotion. [Contract](6110_bringup.md). |
| Nokia 6110 NSE-3 v05.48 PPM B (ROM4) | No | No | No | No | No | No | No | ROM3/ROM4 verifier families are independently mapped. Matching internal ROM, storage and final DSP publication remain missing; no executable promotion. [Contract](6110_bringup.md). |
| Nokia 8210 NSM-3 v5.31 PPM C (ROM6) | Research HLE | Yes | Yes | Yes | Yes (HLE) | Yes (HLE) | No | Explicit HLE and acquired base-record comparison, not normal native support. Own GSM900/DCS, cold software services, SIP and physical media gates; factory data/native speech remain unproved. [Contract](8210_bringup.md). |
| Nokia 6210 NPE-3 v5.56 PPM C | Partial | Yes (research HLE) | Yes (laboratory) | Yes (signaling) | No | No | No | Explicit npe3hle on unchanged own PMM qualifies coherent phone signaling, storage/security and restoration. Normal native verification and speech remain fail-closed. [Contract](6210_bringup.md). |
| Nokia 7110 NSE-5 v5.01 PPM C | Partial | No | No | No | No | No | No | Own full backups, separate PMM and one-byte consistency fixture have distinct checksum/native serial-poll boundaries; no usable handset or fitted-mask acceptance. [Contract](7110_bringup.md). |
| Nokia 8250 v5.02 PPM K | Partial | No | No | No | No | No | No | Own uploads stop at missing resident 2c75; declared PMM comparison fails structural validation. No donor provisioning or success verdict. [Contract](8xxx_bringup.md). |
| Nokia 8850 v5.31 C | Partial | Yes (research) | Yes (research) | Yes (research) | Yes (HLE) | Yes (HLE) | No | Explicit nsm2hle qualifies own physical software services, SIP/media and restoration; normal DSP-result wait and native speech remain unresolved. [Contract](8xxx_bringup.md). |
| Nokia 8890 v12.20 C | Research HLE | Yes | Yes | Yes | Yes (HLE) | Yes (HLE) | Partial | Explicit nsb6hle qualifies own GSM900/PCS1900 software/SIP/media and restore gates, including documented 512 kHz/64-clock PCM. Native speech/gain/factory data remain unproved. [Contract](8xxx_bringup.md). |
| Nokia 6250 v5.03 PPM C | Partial | Yes (research PMM comparison) | Yes (research) | Yes (research) | No | No | No | Explicit HLE/initial-record PMM comparison. Coherent carrier-19 phone signaling and cold software services have separate gates; older mismatched-carrier controls do not qualify them. No native speech. [Contract](6250_bringup.md). |
| Nokia 5510 NPM-5 v3.53 PPM C | Partial | Diagnostic only | No | No | No | No | No | MA4 handset stops at missing 2c75 or a separate HLE diagnostic frame. Independent MU4 recording/metadata/playback scopes do not establish handset UI or phone services. [Contract](5510_bringup.md). |
| Nokia 8810 NSE-6 v6.02 PPM A | Partial | Diagnostic only | No | No | No | No | No | Own MCU with unproved NSE-1 mask and synthetic integrity/retry inputs has bounded UI/storage experiments; serial-control port36 completion, stable radio and speech remain unresolved. [Contract](8810_bringup.md). |
| Nokia 3350 NHM-9 v5.30 PPM A | Partial | No | No | No | No | No | No | Own MCU and 247-word upload; separate staged CALL861f missing-mask boundary. Key conformance is not organic UI; no phone boot acceptance. [Contract](3350_bringup.md). |
| Nokia 5130 NSK-1 v5.61 PPM A | Partial (native compatibility; late reset) | Physical code/menu/contacts and offline SMS | No | No | No | No | No | Own MCU with unproved native-mask comparison; physical menus, cold SIM/security/FDN/offline SMS and bounded restore. Reason-68 reset near 94 s; no registered radio. [Contract](5130_bringup.md). |
| Other declared DCT3 products | No | No | No | No | No | No | No | Driver declarations and acquired service packages are not executable evidence. Extraction, matching storage inputs and product-specific contracts must be established before promotion. |
| Nokia 5190 NSB-1 v6.71 PPM A/B | Partial (graphical comparison) | Physical code/menu/storage | No | No | No | No | No | Own PPM/input/NV fixtures and unproved native-mask comparison; cold SIM/security/offline SMS, bounded Toolkit and restore. Native radio and sustained operation remain unproved. [Contract](5190_bringup.md). |
| Nokia 6150 NSM-1 v5.23 PPM C | Partial (graphical diagnostic) | Physical menus/SIM security/storage/local FDN | No | No | No | No | No | Unproved native-mask comparison with explicit ADC/retry/template policy; physical SIM/Phone storage, security/FDN/Toolkit and bounded restore, not native RF/calls. [Contract](6150_bringup.md). |
| Nokia 6190 NSB-3 v6.13 | Partial (native compatibility) | Physical menus/SIM storage/security | No | No | No | No | No | Own NV/checksum inputs and unproved NSE-1 mask; bounded Toolkit/restore and accessory controls. Reason-68 reset near 94 s; no registered radio or native speech. [Contract](6190_bringup.md). |
| Nokia 8855 NSM-4 v5.18 | No | No | No | No | No | No | No | Own erased-storage/control runs stop at DSP ownership acknowledgement; no donor PMM or graphical acceptance. [Contract](8855_bringup.md). |

## Capability authorities

Use the product contract above for independent acceptance; shared protocol
support is not automatically supported on every handset.

| Capability | Shared contract and acceptance index |
| --- | --- |
| Boot, physical input, idle/menu and power | [Structural profiles](structural_regression.md), [MMI](mmi_layer.md), [cross-ROM differences](cross_rom_confidence.md). |
| SIM controller, files, security, FDN and Toolkit | [SIM hardware/card contract](sim_subsystem.md), [card scope](sim_emulator_scope.md), plus product-local physical/cold gates. |
| SIM/Phone contacts and persistence | Product bring-up contract; phone-bank capacity, multiple numbers, copy/move and local FDN rejection are separate qualifications. |
| Laboratory registration, mobility and call control | [Network contract](network_scouting.md), [supplementary call control](supplementary_call_control.md). |
| SMS, Smart Messaging and Cell Broadcast | [Ordinary SMS](ordinary_sms.md), [CBS/EMS](cell_broadcast_and_ems.md), and each product's storage/application gates. |
| USSD and forwarding | [Supplementary services](gsm_supplementary_services.md); interrogation, activation, cold persistence and host multi-turn behavior are distinct. |
| Host calls, SIP, media and external-epoch restoration | [Host bridge](external_call_bridge.md); signaling-only and physical-waveform gates remain separate. |
| Native DSP, RF and codec execution | [ROM4 loader](rom4_dsp_loader.md), [COBBA boundary](cobba_control_boundary.md), [CPU qualifications](c54x_execution_contracts.md). |
| Independent MU4 music recording/playback | [5510 composition boundary](5510_bringup.md), [AAC validation](aac_validation.md); not whole-handset support. |

## Portability cautions

- Packet length or similar numbering is not semantic evidence. Recover each
  product's command/report lifecycle and carrier agreement independently.
- A local FDN rejection, offline SMS draft or Toolkit refusal is not network
  registration, connected speech or successful proactive-command execution.
- Synthetic checksum/retry/diagnostic inputs retain their declared scope; a
  valid checksum does not make them factory provisioning.
- HLE audio with documented MIC/EAR wiring and neutral gains does not recover
  DSP-controlled analogue mux/gain behavior or establish native speech.
- The 6110 stays fail-closed until matching inputs or an independently accepted
  [bootstrap capture](6110_bootstrap_capture.md) supplies its missing boundary.
