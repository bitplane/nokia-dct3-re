# Nokia 6110 NSE-3 bring-up

This is the evidence boundary for the first 6110-family profile. It deliberately
separates hardware facts established by Nokia documentation from contracts that
can only be recovered from an identified firmware image. Until both sides are
available, the coverage matrix remains `Unsupported`.

## Primary hardware evidence

Nokia's *NSE-3 Series Transceivers, Chapter 3 System Module*, Original
11/97, and *UI Module UE4*, Original 11/97, establish:

| Boundary | NSE-3 contract | Emulator consequence |
| --- | --- | --- |
| MAD | MAD2 with ARM and TI Lead DSP; the parts list identifies ROM3 variant `F711604` | Do not assume a later product's boot ROM or bootstrap exchange count. ROM3 and ROM4 firmware pairs must be labelled separately. |
| Program flash | Intel `TE28F800`, 512K x 16, 120 ns; 8 Mbit total | Add an explicit 1 MiB TE28F800 component. A 2 MiB TE28F160 compatibility device is not faithful. |
| Work RAM | 64K x 8 (512 Kbit) SRAM | NSE-3 has a 64 KiB physical work-memory extent, not the later generic 512 KiB allocation. Address decode/mirroring must be verified against firmware. |
| EEPROM | 8 KiB serial EEPROM | Compose a 24C64-class device and require a separately sourced/provisioned image. Do not truncate a 3210 16 KiB profile. |
| Display | UE4 GD40 COG module with 84 x 48 one-bit display RAM and a serial interface | The existing 84 x 48 serial LCD component is structurally applicable; command compatibility still needs a boot trace. |
| Keypad | Five rows by five columns; documented Send, End/Mode, softkeys, Up/Down, digits, `*`, `#`, side keys and flip input | Add a dedicated NSE-3 input map from the UE4 matrix. Do not inherit the 3210 Navi map or the 3310 firmware-derived map. |
| Internal audio | Internal microphone on differential MIC2; internal dynamic receiver on differential EAR | Product topology is MIC2/EAR. Accessory MIC1/MIC3 and HF remain separate. |
| PCM serial bus | COBBA-GJ generates 1.000 MHz `PCMDClk` from 13 MHz / 13 and 8.0 kHz `PCMSClk` by /125; 16-bit word contains a sign-extended 13-bit sample | Reuse the generic typed 1 MHz/125-clock, 13-in-16 PCM bus shape, but do not inherit NHM-5 speech-control values or analogue gains. |
| Alert path | Dynamic buzzer driven by MAD `BuzzerPWM`; vibra is in a special battery | Keep alert audio outside the speech path and do not add an internal vibrator route. |

Primary source:
[Nokia NSE-3 service-manual archive and extracted text](https://files.elektroda.pl/55977%2Cnokia%2B6110%2Bservice%2Bmanual.html).
The standalone system-module copy is
[NSE-3 Chapter 3](https://electronicsandbooks.com/edt/manual/Hardware/N/Nokia/Phone/6110/03SYS%20%5B73%5D.pdf).

## Firmware baseline

Public historical indexes consistently label the final GSM NSE-3 release as
v5.48 and describe an MCU + PPM B archive of about 1.07 MiB. They are discovery
leads, not identity evidence. The currently indexed firmware.center NSE-3
directory is empty, and the surviving links found during the audit terminate at
obsolete file hosts. No local image exists yet.

An accepted baseline must:

1. be identified as Nokia 6110 NSE-3, not the later 6110 Navigator;
2. retain the original archive and member names outside the repository;
3. record byte size and SHA-256 for every source member and normalized image;
4. distinguish ROM3 and ROM4 compatibility;
5. account for the documented 1 MiB program-flash extent;
6. keep EEPROM content separate from MCU/PPM code;
7. pass static vector/reset inspection before it is declared as a MAME BIOS.

The firmware and any extracted proprietary data remain git-ignored.

## Staged executable acceptance

Once a baseline is present, promotion proceeds in this order:

1. **Declared hardware:** TE28F800, 64 KiB SRAM, 8 KiB EEPROM, UE4 display and
   keypad, MIC2/EAR topology, and the documented PCM shape.
2. **Boot/DSP boundary:** organic reset vector, ROM3/ROM4 identity, DSP upload
   and handshake census. Any HLE service remains disabled until its packet
   grammar is observed.
3. **Interactive:** firmware-rendered idle UI plus physical softkey,
   Send/End, navigation and digit acceptance.
4. **SIM/radio:** recover the NSE-3 SIM transaction and radio packet profiles;
   do not select either the NSE-8 bitmap search or NHM-5 candidate-list grammar
   by resemblance.
5. **Call control:** registration, paging, ringing, physical Send/End and clean
   teardown, with firmware-owned RR/MM/CC state.
6. **Media:** recover the NSE-3 speech-control lifecycle, then enable its
   independently documented PCM profile and prove bidirectional GSM-FR,
   FACCH/BFI/SACCH coexistence and save-state replay.
7. **Physical duplex:** connect only MIC2 and EAR, with neutral host scale until
   product gain programming is recovered. Hardware-faithful status remains
   `Partial` while analogue gain/mux control is opaque.

This order permits shared DCT3 devices and GSM protocol layers to be reused,
while ensuring that no 3210 or 3310 firmware contract is silently inherited.
