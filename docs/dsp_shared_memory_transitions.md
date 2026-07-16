# DSP-owned shared-memory transition census

Profiles: `v501`, `v600`. Peer-written scalar offsets: **9**.

The traces contain **404** peer writes, of which **400** change the stored value. Firmware subsequently observes **382** changed values before the next peer write to that word.

This is a reachable-runtime transaction inventory, not proof that dormant DSP paths use no additional words. A missing consumer means only that the traced firmware did not subsequently read that changed value.

| profile | offset | writes / changes | peer role | evidenced trigger | values written | observing PCs |
| --- | ---: | ---: | --- | --- | --- | --- |
| v501 | `0x000` | 1 / 1 | bootstrap_ready | 64 bootstrap exchanges complete | `0x0001` | `0x002904c4` |
| v501 | `0x002` | 1 / 1 | bootstrap_ready | 64 bootstrap exchanges complete | `0x0001` | `0x002904b6` |
| v501 | `0x004` | 1 / 1 | bootstrap_ready | 64 bootstrap exchanges complete | `0x0001` | none observed |
| v501 | `0x0a6` | 34 / 34 | tx_consumer | peer consumes committed TX packet | `0x0001`, `0x0003`, `0x0006`, `0x000b`, `0x000f`, `0x0011`, `0x0015`, `0x001d`, `0x0023`, `0x0025`, `0x0026`, `0x0029`, `0x002b`, `0x0030`, `0x0031`, `0x0033`, `0x0036`, `0x0037`, `0x0039`, `0x003a`, `0x003c`, `0x003f`, `0x0044`, `0x0045`, `0x004c`, `0x004e` | `0x00290392` |
| v501 | `0x0e0` | 19 / 17 | shared_control_busy | DSPIF command-4 doorbell; reset publication | `0x0000` | `0x002906be` |
| v501 | `0x0e4` | 1 / 1 | service_counter | peer completes pending service work | `0x0000` | `0x00290a46` |
| v501 | `0x0fe` | 66 / 66 | bootstrap_ack | MCU zero-write request | `0x0001` | `0x00290472`, `0x002904b0` |
| v501 | `0x100` | 66 / 66 | bootstrap_ack | MCU zero-write request | `0x0001` | `0x00290466` |
| v501 | `0x1c8` | 11 / 11 | rx_producer | peer enqueues RX packet | `0x0086`, `0x008c`, `0x008e`, `0x0095`, `0x009b`, `0x00c2`, `0x00c8`, `0x00ce`, `0x00d4`, `0x00da`, `0x00e0` | `0x0029026e` |
| v600 | `0x000` | 1 / 1 | bootstrap_ready | 64 bootstrap exchanges complete | `0x0001` | `0x00290b0a` |
| v600 | `0x002` | 1 / 1 | bootstrap_ready | 64 bootstrap exchanges complete | `0x0001` | `0x00290afc` |
| v600 | `0x004` | 1 / 1 | bootstrap_ready | 64 bootstrap exchanges complete | `0x0001` | none observed |
| v600 | `0x0a6` | 34 / 34 | tx_consumer | peer consumes committed TX packet | `0x0001`, `0x0002`, `0x0006`, `0x0009`, `0x000b`, `0x000f`, `0x0011`, `0x0015`, `0x001d`, `0x0023`, `0x0025`, `0x0029`, `0x002b`, `0x002c`, `0x0031`, `0x0033`, `0x0036`, `0x0037`, `0x0039`, `0x003c`, `0x003f`, `0x0040`, `0x0045`, `0x004a`, `0x004c` | `0x0029099e` |
| v600 | `0x0e0` | 23 / 21 | shared_control_busy | DSPIF command-4 doorbell; reset publication | `0x0000` | `0x00290d0a` |
| v600 | `0x0e4` | 1 / 1 | service_counter | peer completes pending service work | `0x0000` | `0x00291096` |
| v600 | `0x0fe` | 66 / 66 | bootstrap_ack | MCU zero-write request | `0x0001` | `0x00290aba`, `0x00290af6` |
| v600 | `0x100` | 66 / 66 | bootstrap_ack | MCU zero-write request | `0x0001` | `0x00290aae` |
| v600 | `0x1c8` | 11 / 11 | rx_producer | peer enqueues RX packet | `0x0086`, `0x008c`, `0x008e`, `0x0095`, `0x009b`, `0x00c2`, `0x00c8`, `0x00ce`, `0x00d4`, `0x00da`, `0x00e0` | `0x0029087a` |
