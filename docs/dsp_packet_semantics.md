# DSP packet-semantics census

Profiles: `v501`, `v600`. Observed packets: **90**.

This report classifies behavior implemented at the current HLE boundary. A packet marked unmodeled is valid firmware traffic that the HLE presently discards, not evidence that real DSP hardware ignores it.
Types `0x0d` and `0x3c` are named for their recovered wire structure only: the ROM-4 DSP consumer and physical purpose remain unidentified.

| direction | type | semantic family | disposition | v5.01 | v6.00 | lengths | payload prefix examples |
| --- | ---: | --- | --- | ---: | ---: | --- | --- |
| RX | `0x74` | service_control_completion | derived from type-0x70/0d00 | 1 | 1 | 2 | `0d00` |
| RX | `0x8e` | external_channel_map | peer-initiated canned channel map | 1 | 1 | 75 | `1e000240004500017000000000000000` |
| RX | `0x8e` | external_discovery_completion | derived from discovery request | 1 | 1 | 10 | `1e0002d000030401c100` |
| RX | `0x8e` | external_discovery_echo | derived from discovery request | 1 | 1 | 10 | `1e0002d000030101e000` |
| RX | `0x8e` | external_registration | peer-initiated canned service frame | 1 | 1 | 12 | `1e0002400006000164010142` |
| RX | `0x8e` | external_transport_ack | derived from MCU external frame | 6 | 6 | 9 | `1e00027f000200005f`, `1e00027f000200055f`, `1e00027f000200065f` |
| TX | `0x05` | external_discovery_close | one-way acknowledgement of peer state-4 completion | 3 | 3 | 10 | `1e0200d0000305014100` |
| TX | `0x05` | external_discovery_control | one-way discovery-side control publication | 1 | 1 | 8 | `1e1400f400010300` |
| TX | `0x05` | external_discovery_request | derived discovery echo/completion | 1 | 1 | 10 | `1eff00d000030101e000` |
| TX | `0x05` | external_poll | derived transport acknowledgement | 4 | 4 | 18 | `1e020000000c01015f0003a7020200d9`, `1e020000000c01015f0003ab020200d9`, `1e020000000c01015f0005b3030200d9` |
| TX | `0x05` | external_service_reply | derived transport acknowledgement | 2 | 2 | 12, 20 | `1e0200400006010170010144`, `1e020040000e0101640301450d010101` |
| TX | `0x05` | external_transport_ack | consumed; no response | 2 | 2 | 9 | `1e02007f0002400200`, `1e02007f0002400364` |
| TX | `0x05` | service_empty_report | one-way packet; completion uses DSP shared control | 1 | 1 | 16 | `1e020000000a01016206008d010001c3` |
| TX | `0x0d` | indexed_64_byte_block_upload | one-way DSP configuration publication | 4 | 4 | 66 | `0000bab6a4d59afba123bab6a3d59afb`, `00013c43552b64075fde3c43552b6306`, `0002b5bcd5a5fc9b25a2b5bcd6a5fc9c` |
| TX | `0x1a` | arfcn_bitmap_publication | one-way GSM channel-set publication | 1 | 1 | 68 | `00819800000000000000000000000000` |
| TX | `0x3c` | selector_lookup_table_upload | one-way DSP configuration publication | 2 | 2 | 156 | `0800ffffffffffffffffffffffffffff`, `1800ffffffffffffffffffffffffffff` |
| TX | `0x51` | segmented_dsp_memory_upload | one-way command-0x22 DSP memory image | 7 | 7 | 16, 28, 80 | `2206545e7e3e611949a8ec1416c2599a`, `222d0000000000007fffffffe610dc10`, `2254001a074e00524f3b200020002000` |
| TX | `0x70` | bootstrap_platform_word | one-way DSP bootstrap publication | 1 | 1 | 6 | `13042386fef6`, `1304debb0f52` |
| TX | `0x70` | bootstrap_table | one-way DSP bootstrap publication | 3 | 3 | 14, 22, 26 | `140cffffffffffffffffffffffff`, `1514ffffffffffffffffffffffffffff`, `1618ffffffffffffffffffffffffffff` |
| TX | `0x70` | service_control_followup | one-way publication after type-0x74 completion | 1 | 1 | 2 | `0a09` |
| TX | `0x70` | service_control_request | request-derived type-0x74 completion | 1 | 1 | 2 | `0d00` |

- `v501`: 34 TX, 11 RX, 10 RX notifications; 0 outbound packets are consumed without modeled semantics.

- `v600`: 34 TX, 11 RX, 10 RX notifications; 0 outbound packets are consumed without modeled semantics.
