# DSP shared-memory firmware-read census

Profiles: `v501`, `v600`. Unique byte offsets: **125**.

This is a reachable-runtime inventory of first MCU reads, not a static proof that other readers do not exist.

Both profiles share **125** offsets; per-profile counts are `v501`=125, `v600`=125.

Observed classifications: `bootstrap_ack`=4, `bootstrap_ack_or_rx_ring_base`=4, `bootstrap_ready`=6, `rx_consumer`=6, `rx_producer`=4, `rx_ring_payload`=190, `service_counter`=6, `shared_control_busy`=2, `shared_control_request`=2, `shared_ram_self_test`=38, `tx_consumer`=4, `tx_producer`=6.

No firmware read is answered outside DSPIF-owned backing RAM. Peer-owned scalar state is limited to bootstrap flags, shared-control counters/requests, ring indices, and RX packet contents.

The companion [transition census](dsp_shared_memory_transitions.md) correlates the active peer-owned scalar writes with their MCU triggers and observing firmware PCs.

| profile | offset | value | first PC | time (s) | classification | owner |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| v501 | `0x000` | `0x00ea` | `0x00293742` | 0.010205 | shared_ram_self_test | MCU echo |
| v501 | `0x000` | `0x0001` | `0x002904c4` | 0.165180 | bootstrap_ready | DSP peer |
| v600 | `0x000` | `0x00ea` | `0x00295fd6` | 0.011510 | shared_ram_self_test | MCU echo |
| v600 | `0x000` | `0x0001` | `0x00290b0a` | 0.151250 | bootstrap_ready | DSP peer |
| v501 | `0x002` | `0x00ff` | `0x00293742` | 0.010209 | shared_ram_self_test | MCU echo |
| v501 | `0x002` | `0x0001` | `0x002904b6` | 0.165178 | bootstrap_ready | DSP peer |
| v501 | `0x002` | `0x0001` | `0x002904c0` | 0.165180 | bootstrap_ready | DSP peer |
| v600 | `0x002` | `0x00ff` | `0x00295fd6` | 0.011513 | shared_ram_self_test | MCU echo |
| v600 | `0x002` | `0x0001` | `0x00290afc` | 0.151248 | bootstrap_ready | DSP peer |
| v600 | `0x002` | `0x0001` | `0x00290b04` | 0.151249 | bootstrap_ready | DSP peer |
| v501 | `0x004` | `0x00ff` | `0x00293742` | 0.010212 | shared_ram_self_test | MCU echo |
| v600 | `0x004` | `0x00ff` | `0x00295fd6` | 0.011517 | shared_ram_self_test | MCU echo |
| v501 | `0x006` | `0x00ea` | `0x00293742` | 0.010215 | shared_ram_self_test | MCU echo |
| v600 | `0x006` | `0x00ea` | `0x00295fd6` | 0.011520 | shared_ram_self_test | MCU echo |
| v501 | `0x008` | `0x00ea` | `0x00293742` | 0.010219 | shared_ram_self_test | MCU echo |
| v600 | `0x008` | `0x00ea` | `0x00295fd6` | 0.011523 | shared_ram_self_test | MCU echo |
| v501 | `0x00a` | `0x00ea` | `0x00293742` | 0.010222 | shared_ram_self_test | MCU echo |
| v600 | `0x00a` | `0x00ea` | `0x00295fd6` | 0.011527 | shared_ram_self_test | MCU echo |
| v501 | `0x00c` | `0x0000` | `0x00293742` | 0.010226 | shared_ram_self_test | MCU echo |
| v600 | `0x00c` | `0x0000` | `0x00295fd6` | 0.011530 | shared_ram_self_test | MCU echo |
| v501 | `0x00e` | `0x0000` | `0x00293742` | 0.010229 | shared_ram_self_test | MCU echo |
| v600 | `0x00e` | `0x0000` | `0x00295fd6` | 0.011534 | shared_ram_self_test | MCU echo |
| v501 | `0x010` | `0x0000` | `0x00293742` | 0.010233 | shared_ram_self_test | MCU echo |
| v600 | `0x010` | `0x0000` | `0x00295fd6` | 0.011537 | shared_ram_self_test | MCU echo |
| v501 | `0x012` | `0x0000` | `0x00293742` | 0.010236 | shared_ram_self_test | MCU echo |
| v600 | `0x012` | `0x0000` | `0x00295fd6` | 0.011541 | shared_ram_self_test | MCU echo |
| v501 | `0x014` | `0x0000` | `0x00293742` | 0.010240 | shared_ram_self_test | MCU echo |
| v600 | `0x014` | `0x0000` | `0x00295fd6` | 0.011544 | shared_ram_self_test | MCU echo |
| v501 | `0x016` | `0x0000` | `0x00293742` | 0.010243 | shared_ram_self_test | MCU echo |
| v600 | `0x016` | `0x0000` | `0x00295fd6` | 0.011548 | shared_ram_self_test | MCU echo |
| v501 | `0x018` | `0x0000` | `0x00293742` | 0.010247 | shared_ram_self_test | MCU echo |
| v600 | `0x018` | `0x0000` | `0x00295fd6` | 0.011551 | shared_ram_self_test | MCU echo |
| v501 | `0x01a` | `0x0000` | `0x00293742` | 0.010250 | shared_ram_self_test | MCU echo |
| v600 | `0x01a` | `0x0000` | `0x00295fd6` | 0.011555 | shared_ram_self_test | MCU echo |
| v501 | `0x01c` | `0x0000` | `0x00293742` | 0.010254 | shared_ram_self_test | MCU echo |
| v600 | `0x01c` | `0x0000` | `0x00295fd6` | 0.011558 | shared_ram_self_test | MCU echo |
| v501 | `0x01e` | `0x0000` | `0x00293742` | 0.010257 | shared_ram_self_test | MCU echo |
| v600 | `0x01e` | `0x0000` | `0x00295fd6` | 0.011562 | shared_ram_self_test | MCU echo |
| v501 | `0x020` | `0x0000` | `0x00293742` | 0.010260 | shared_ram_self_test | MCU echo |
| v600 | `0x020` | `0x0000` | `0x00295fd6` | 0.011565 | shared_ram_self_test | MCU echo |
| v501 | `0x022` | `0x0000` | `0x00293742` | 0.010264 | shared_ram_self_test | MCU echo |
| v600 | `0x022` | `0x0000` | `0x00295fd6` | 0.011568 | shared_ram_self_test | MCU echo |
| v501 | `0x024` | `0x0000` | `0x00293762` | 0.010268 | shared_ram_self_test | MCU echo |
| v600 | `0x024` | `0x0000` | `0x00295ff6` | 0.011572 | shared_ram_self_test | MCU echo |
| v501 | `0x0a4` | `0x0000` | `0x00290390` | 0.281678 | tx_producer | MCU |
| v501 | `0x0a4` | `0x0000` | `0x002903a4` | 0.281679 | tx_producer | MCU |
| v501 | `0x0a4` | `0x0000` | `0x002901c6` | 0.281690 | tx_producer | MCU |
| v600 | `0x0a4` | `0x0000` | `0x0029099c` | 0.267754 | tx_producer | MCU |
| v600 | `0x0a4` | `0x0000` | `0x002909b0` | 0.267755 | tx_producer | MCU |
| v600 | `0x0a4` | `0x0000` | `0x002907d2` | 0.267766 | tx_producer | MCU |
| v501 | `0x0a6` | `0x0000` | `0x00290392` | 0.281678 | tx_consumer | DSP peer |
| v501 | `0x0a6` | `0x0000` | `0x002903a2` | 0.281679 | tx_consumer | DSP peer |
| v600 | `0x0a6` | `0x0000` | `0x0029099e` | 0.267754 | tx_consumer | DSP peer |
| v600 | `0x0a6` | `0x0000` | `0x002909ae` | 0.267755 | tx_consumer | DSP peer |
| v501 | `0x0da` | `0x0000` | `0x00290a5c` | 0.203131 | service_counter | DSP peer |
| v600 | `0x0da` | `0x0000` | `0x002910ac` | 0.189196 | service_counter | DSP peer |
| v501 | `0x0dc` | `0x0000` | `0x0029015c` | 4.584044 | shared_control_request | MCU request / DSP completion |
| v600 | `0x0dc` | `0x0000` | `0x00290768` | 4.588314 | shared_control_request | MCU request / DSP completion |
| v501 | `0x0e0` | `0x0000` | `0x002906be` | 0.263222 | shared_control_busy | DSP peer |
| v600 | `0x0e0` | `0x0000` | `0x00290d0a` | 0.249290 | shared_control_busy | DSP peer |
| v501 | `0x0e2` | `0x0000` | `0x00290a8a` | 0.203132 | service_counter | DSP peer |
| v600 | `0x0e2` | `0x0000` | `0x002910da` | 0.189197 | service_counter | DSP peer |
| v501 | `0x0e4` | `0x0000` | `0x00290a46` | 0.203129 | service_counter | DSP peer |
| v600 | `0x0e4` | `0x0000` | `0x00291096` | 0.189193 | service_counter | DSP peer |
| v501 | `0x0fe` | `0x0001` | `0x00290472` | 0.015959 | bootstrap_ack | DSP peer |
| v501 | `0x0fe` | `0x0001` | `0x002904b0` | 0.165178 | bootstrap_ack | DSP peer |
| v600 | `0x0fe` | `0x0001` | `0x00290aba` | 0.017035 | bootstrap_ack | DSP peer |
| v600 | `0x0fe` | `0x0001` | `0x00290af6` | 0.151247 | bootstrap_ack | DSP peer |
| v501 | `0x100` | `0x0001` | `0x00290466` | 0.014774 | bootstrap_ack_or_rx_ring_base | DSP peer |
| v501 | `0x100` | `0x0a8e` | `0x002902ac` | 0.302775 | bootstrap_ack_or_rx_ring_base | DSP peer |
| v600 | `0x100` | `0x0001` | `0x00290aae` | 0.015967 | bootstrap_ack_or_rx_ring_base | DSP peer |
| v600 | `0x100` | `0x0a8e` | `0x002908b8` | 0.288851 | bootstrap_ack_or_rx_ring_base | DSP peer |
| v501 | `0x102` | `0x1e00` | `0x00290350` | 0.302827 | rx_ring_payload | DSP peer |
| v600 | `0x102` | `0x1e00` | `0x0029095c` | 0.288904 | rx_ring_payload | DSP peer |
| v501 | `0x104` | `0x02d0` | `0x00290350` | 0.302831 | rx_ring_payload | DSP peer |
| v600 | `0x104` | `0x02d0` | `0x0029095c` | 0.288907 | rx_ring_payload | DSP peer |
| v501 | `0x106` | `0x0003` | `0x00290350` | 0.302834 | rx_ring_payload | DSP peer |
| v600 | `0x106` | `0x0003` | `0x0029095c` | 0.288910 | rx_ring_payload | DSP peer |
| v501 | `0x108` | `0x0101` | `0x00290350` | 0.302838 | rx_ring_payload | DSP peer |
| v600 | `0x108` | `0x0101` | `0x0029095c` | 0.288914 | rx_ring_payload | DSP peer |
| v501 | `0x10a` | `0xe000` | `0x00290350` | 0.302841 | rx_ring_payload | DSP peer |
| v600 | `0x10a` | `0xe000` | `0x0029095c` | 0.288917 | rx_ring_payload | DSP peer |
| v501 | `0x10c` | `0x0a8e` | `0x002902ac` | 0.302924 | rx_ring_payload | DSP peer |
| v600 | `0x10c` | `0x0a8e` | `0x002908b8` | 0.289000 | rx_ring_payload | DSP peer |
| v501 | `0x10e` | `0x1e00` | `0x00290350` | 0.302976 | rx_ring_payload | DSP peer |
| v600 | `0x10e` | `0x1e00` | `0x0029095c` | 0.289052 | rx_ring_payload | DSP peer |
| v501 | `0x110` | `0x02d0` | `0x00290350` | 0.302980 | rx_ring_payload | DSP peer |
| v600 | `0x110` | `0x02d0` | `0x0029095c` | 0.289056 | rx_ring_payload | DSP peer |
| v501 | `0x112` | `0x0003` | `0x00290350` | 0.302983 | rx_ring_payload | DSP peer |
| v600 | `0x112` | `0x0003` | `0x0029095c` | 0.289059 | rx_ring_payload | DSP peer |
| v501 | `0x114` | `0x0401` | `0x00290350` | 0.302987 | rx_ring_payload | DSP peer |
| v600 | `0x114` | `0x0401` | `0x0029095c` | 0.289063 | rx_ring_payload | DSP peer |
| v501 | `0x116` | `0xc100` | `0x00290350` | 0.302990 | rx_ring_payload | DSP peer |
| v600 | `0x116` | `0xc100` | `0x0029095c` | 0.289066 | rx_ring_payload | DSP peer |
| v501 | `0x118` | `0x0274` | `0x002902ac` | 0.398726 | rx_ring_payload | DSP peer |
| v600 | `0x118` | `0x0274` | `0x002908b8` | 0.384828 | rx_ring_payload | DSP peer |
| v501 | `0x11a` | `0x0d00` | `0x00290350` | 0.398778 | rx_ring_payload | DSP peer |
| v600 | `0x11a` | `0x0d00` | `0x0029095c` | 0.384880 | rx_ring_payload | DSP peer |
| v501 | `0x11c` | `0x0c8e` | `0x002902ac` | 0.547294 | rx_ring_payload | DSP peer |
| v600 | `0x11c` | `0x0c8e` | `0x002908b8` | 0.533405 | rx_ring_payload | DSP peer |
| v501 | `0x11e` | `0x1e00` | `0x00290350` | 0.547347 | rx_ring_payload | DSP peer |
| v600 | `0x11e` | `0x1e00` | `0x0029095c` | 0.533459 | rx_ring_payload | DSP peer |
| v501 | `0x120` | `0x0240` | `0x00290350` | 0.547350 | rx_ring_payload | DSP peer |
| v600 | `0x120` | `0x0240` | `0x0029095c` | 0.533463 | rx_ring_payload | DSP peer |
| v501 | `0x122` | `0x0006` | `0x00290350` | 0.547353 | rx_ring_payload | DSP peer |
| v600 | `0x122` | `0x0006` | `0x0029095c` | 0.533466 | rx_ring_payload | DSP peer |
| v501 | `0x124` | `0x0001` | `0x00290350` | 0.547357 | rx_ring_payload | DSP peer |
| v600 | `0x124` | `0x0001` | `0x0029095c` | 0.533469 | rx_ring_payload | DSP peer |
| v501 | `0x126` | `0x6401` | `0x00290350` | 0.547360 | rx_ring_payload | DSP peer |
| v600 | `0x126` | `0x6401` | `0x0029095c` | 0.533473 | rx_ring_payload | DSP peer |
| v501 | `0x128` | `0x0142` | `0x00290350` | 0.547364 | rx_ring_payload | DSP peer |
| v600 | `0x128` | `0x0142` | `0x0029095c` | 0.533476 | rx_ring_payload | DSP peer |
| v501 | `0x12a` | `0x098e` | `0x002902ac` | 1.794240 | rx_ring_payload | DSP peer |
| v600 | `0x12a` | `0x098e` | `0x002908b8` | 1.795420 | rx_ring_payload | DSP peer |
| v501 | `0x12c` | `0x1e00` | `0x00290350` | 1.794292 | rx_ring_payload | DSP peer |
| v600 | `0x12c` | `0x1e00` | `0x0029095c` | 1.795472 | rx_ring_payload | DSP peer |
| v501 | `0x12e` | `0x027f` | `0x00290350` | 1.794295 | rx_ring_payload | DSP peer |
| v600 | `0x12e` | `0x027f` | `0x0029095c` | 1.795475 | rx_ring_payload | DSP peer |
| v501 | `0x130` | `0x0002` | `0x00290350` | 1.794299 | rx_ring_payload | DSP peer |
| v600 | `0x130` | `0x0002` | `0x0029095c` | 1.795479 | rx_ring_payload | DSP peer |
| v501 | `0x132` | `0x4002` | `0x00290350` | 1.794302 | rx_ring_payload | DSP peer |
| v600 | `0x132` | `0x4002` | `0x0029095c` | 1.795482 | rx_ring_payload | DSP peer |
| v501 | `0x134` | `0x6400` | `0x00290350` | 1.794306 | rx_ring_payload | DSP peer |
| v600 | `0x134` | `0x6400` | `0x0029095c` | 1.795486 | rx_ring_payload | DSP peer |
| v501 | `0x136` | `0x4b8e` | `0x002902ac` | 1.872365 | rx_ring_payload | DSP peer |
| v600 | `0x136` | `0x4b8e` | `0x002908b8` | 1.873545 | rx_ring_payload | DSP peer |
| v501 | `0x138` | `0x1e00` | `0x00290350` | 1.872424 | rx_ring_payload | DSP peer |
| v600 | `0x138` | `0x1e00` | `0x0029095c` | 1.873604 | rx_ring_payload | DSP peer |
| v501 | `0x13a` | `0x0240` | `0x00290350` | 1.872427 | rx_ring_payload | DSP peer |
| v600 | `0x13a` | `0x0240` | `0x0029095c` | 1.873607 | rx_ring_payload | DSP peer |
| v501 | `0x13c` | `0x0045` | `0x00290350` | 1.872431 | rx_ring_payload | DSP peer |
| v600 | `0x13c` | `0x0045` | `0x0029095c` | 1.873611 | rx_ring_payload | DSP peer |
| v501 | `0x13e` | `0x0001` | `0x00290350` | 1.872434 | rx_ring_payload | DSP peer |
| v600 | `0x13e` | `0x0001` | `0x0029095c` | 1.873614 | rx_ring_payload | DSP peer |
| v501 | `0x140` | `0x7000` | `0x00290350` | 1.872438 | rx_ring_payload | DSP peer |
| v600 | `0x140` | `0x7000` | `0x0029095c` | 1.873618 | rx_ring_payload | DSP peer |
| v501 | `0x142` | `0x0000` | `0x00290350` | 1.872441 | rx_ring_payload | DSP peer |
| v600 | `0x142` | `0x0000` | `0x0029095c` | 1.873621 | rx_ring_payload | DSP peer |
| v501 | `0x144` | `0x0000` | `0x00290350` | 1.872445 | rx_ring_payload | DSP peer |
| v600 | `0x144` | `0x0000` | `0x0029095c` | 1.873624 | rx_ring_payload | DSP peer |
| v501 | `0x146` | `0x0000` | `0x00290350` | 1.872448 | rx_ring_payload | DSP peer |
| v600 | `0x146` | `0x0000` | `0x0029095c` | 1.873628 | rx_ring_payload | DSP peer |
| v501 | `0x148` | `0x0000` | `0x00290350` | 1.872452 | rx_ring_payload | DSP peer |
| v600 | `0x148` | `0x0000` | `0x0029095c` | 1.873631 | rx_ring_payload | DSP peer |
| v501 | `0x14a` | `0x0000` | `0x00290350` | 1.872455 | rx_ring_payload | DSP peer |
| v600 | `0x14a` | `0x0000` | `0x0029095c` | 1.873635 | rx_ring_payload | DSP peer |
| v501 | `0x14c` | `0x0120` | `0x00290350` | 1.872458 | rx_ring_payload | DSP peer |
| v600 | `0x14c` | `0x0120` | `0x0029095c` | 1.873638 | rx_ring_payload | DSP peer |
| v501 | `0x14e` | `0x0000` | `0x00290350` | 1.872462 | rx_ring_payload | DSP peer |
| v600 | `0x14e` | `0x0000` | `0x0029095c` | 1.873642 | rx_ring_payload | DSP peer |
| v501 | `0x150` | `0x0000` | `0x00290350` | 1.872465 | rx_ring_payload | DSP peer |
| v600 | `0x150` | `0x0000` | `0x0029095c` | 1.873645 | rx_ring_payload | DSP peer |
| v501 | `0x152` | `0x0000` | `0x00290350` | 1.872469 | rx_ring_payload | DSP peer |
| v600 | `0x152` | `0x0000` | `0x0029095c` | 1.873649 | rx_ring_payload | DSP peer |
| v501 | `0x154` | `0x0000` | `0x00290350` | 1.872472 | rx_ring_payload | DSP peer |
| v600 | `0x154` | `0x0000` | `0x0029095c` | 1.873652 | rx_ring_payload | DSP peer |
| v501 | `0x156` | `0x0000` | `0x00290350` | 1.872476 | rx_ring_payload | DSP peer |
| v600 | `0x156` | `0x0000` | `0x0029095c` | 1.873656 | rx_ring_payload | DSP peer |
| v501 | `0x158` | `0x0000` | `0x00290350` | 1.872479 | rx_ring_payload | DSP peer |
| v600 | `0x158` | `0x0000` | `0x0029095c` | 1.873659 | rx_ring_payload | DSP peer |
| v501 | `0x15a` | `0x0000` | `0x00290350` | 1.872483 | rx_ring_payload | DSP peer |
| v600 | `0x15a` | `0x0000` | `0x0029095c` | 1.873663 | rx_ring_payload | DSP peer |
| v501 | `0x15c` | `0x0000` | `0x00290350` | 1.872486 | rx_ring_payload | DSP peer |
| v600 | `0x15c` | `0x0000` | `0x0029095c` | 1.873666 | rx_ring_payload | DSP peer |
| v501 | `0x15e` | `0x0000` | `0x00290350` | 1.872490 | rx_ring_payload | DSP peer |
| v600 | `0x15e` | `0x0000` | `0x0029095c` | 1.873669 | rx_ring_payload | DSP peer |
| v501 | `0x160` | `0x0000` | `0x00290350` | 1.872493 | rx_ring_payload | DSP peer |
| v600 | `0x160` | `0x0000` | `0x0029095c` | 1.873673 | rx_ring_payload | DSP peer |
| v501 | `0x162` | `0x0000` | `0x00290350` | 1.872497 | rx_ring_payload | DSP peer |
| v600 | `0x162` | `0x0000` | `0x0029095c` | 1.873676 | rx_ring_payload | DSP peer |
| v501 | `0x164` | `0x0000` | `0x00290350` | 1.872500 | rx_ring_payload | DSP peer |
| v600 | `0x164` | `0x0000` | `0x0029095c` | 1.873680 | rx_ring_payload | DSP peer |
| v501 | `0x166` | `0x0000` | `0x00290350` | 1.872503 | rx_ring_payload | DSP peer |
| v600 | `0x166` | `0x0000` | `0x0029095c` | 1.873683 | rx_ring_payload | DSP peer |
| v501 | `0x168` | `0x0000` | `0x00290350` | 1.872507 | rx_ring_payload | DSP peer |
| v600 | `0x168` | `0x0000` | `0x0029095c` | 1.873687 | rx_ring_payload | DSP peer |
| v501 | `0x16a` | `0x0000` | `0x00290350` | 1.872510 | rx_ring_payload | DSP peer |
| v600 | `0x16a` | `0x0000` | `0x0029095c` | 1.873690 | rx_ring_payload | DSP peer |
| v501 | `0x16c` | `0x0000` | `0x00290350` | 1.872514 | rx_ring_payload | DSP peer |
| v600 | `0x16c` | `0x0000` | `0x0029095c` | 1.873694 | rx_ring_payload | DSP peer |
| v501 | `0x16e` | `0x0000` | `0x00290350` | 1.872517 | rx_ring_payload | DSP peer |
| v600 | `0x16e` | `0x0000` | `0x0029095c` | 1.873697 | rx_ring_payload | DSP peer |
| v501 | `0x170` | `0x0000` | `0x00290350` | 1.872521 | rx_ring_payload | DSP peer |
| v600 | `0x170` | `0x0000` | `0x0029095c` | 1.873701 | rx_ring_payload | DSP peer |
| v501 | `0x172` | `0x0000` | `0x00290350` | 1.872524 | rx_ring_payload | DSP peer |
| v600 | `0x172` | `0x0000` | `0x0029095c` | 1.873704 | rx_ring_payload | DSP peer |
| v501 | `0x174` | `0x0000` | `0x00290350` | 1.872528 | rx_ring_payload | DSP peer |
| v600 | `0x174` | `0x0000` | `0x0029095c` | 1.873708 | rx_ring_payload | DSP peer |
| v501 | `0x176` | `0x0000` | `0x00290350` | 1.872531 | rx_ring_payload | DSP peer |
| v600 | `0x176` | `0x0000` | `0x0029095c` | 1.873711 | rx_ring_payload | DSP peer |
| v501 | `0x178` | `0x0000` | `0x00290350` | 1.872535 | rx_ring_payload | DSP peer |
| v600 | `0x178` | `0x0000` | `0x0029095c` | 1.873714 | rx_ring_payload | DSP peer |
| v501 | `0x17a` | `0x0000` | `0x00290350` | 1.872538 | rx_ring_payload | DSP peer |
| v600 | `0x17a` | `0x0000` | `0x0029095c` | 1.873718 | rx_ring_payload | DSP peer |
| v501 | `0x17c` | `0x0000` | `0x00290350` | 1.872542 | rx_ring_payload | DSP peer |
| v600 | `0x17c` | `0x0000` | `0x0029095c` | 1.873721 | rx_ring_payload | DSP peer |
| v501 | `0x17e` | `0x0000` | `0x00290350` | 1.872545 | rx_ring_payload | DSP peer |
| v600 | `0x17e` | `0x0000` | `0x0029095c` | 1.873725 | rx_ring_payload | DSP peer |
| v501 | `0x180` | `0x0001` | `0x00290350` | 1.872548 | rx_ring_payload | DSP peer |
| v600 | `0x180` | `0x0001` | `0x0029095c` | 1.873728 | rx_ring_payload | DSP peer |
| v501 | `0x182` | `0x4300` | `0x00290350` | 1.872552 | rx_ring_payload | DSP peer |
| v600 | `0x182` | `0x4300` | `0x0029095c` | 1.873732 | rx_ring_payload | DSP peer |
| v501 | `0x184` | `0x098e` | `0x002902ac` | 1.885547 | rx_ring_payload | DSP peer |
| v600 | `0x184` | `0x098e` | `0x002908b8` | 1.886685 | rx_ring_payload | DSP peer |
| v501 | `0x186` | `0x1e00` | `0x00290350` | 1.885599 | rx_ring_payload | DSP peer |
| v600 | `0x186` | `0x1e00` | `0x0029095c` | 1.886737 | rx_ring_payload | DSP peer |
| v501 | `0x188` | `0x027f` | `0x00290350` | 1.885602 | rx_ring_payload | DSP peer |
| v600 | `0x188` | `0x027f` | `0x0029095c` | 1.886741 | rx_ring_payload | DSP peer |
| v501 | `0x18a` | `0x0002` | `0x00290350` | 1.885606 | rx_ring_payload | DSP peer |
| v600 | `0x18a` | `0x0002` | `0x0029095c` | 1.886744 | rx_ring_payload | DSP peer |
| v501 | `0x18c` | `0x4004` | `0x00290350` | 1.885609 | rx_ring_payload | DSP peer |
| v600 | `0x18c` | `0x4004` | `0x0029095c` | 1.886748 | rx_ring_payload | DSP peer |
| v501 | `0x18e` | `0x7000` | `0x00290350` | 1.885613 | rx_ring_payload | DSP peer |
| v600 | `0x18e` | `0x7000` | `0x0029095c` | 1.886751 | rx_ring_payload | DSP peer |
| v501 | `0x190` | `0x098e` | `0x002902ac` | 7.941225 | rx_ring_payload | DSP peer |
| v600 | `0x190` | `0x098e` | `0x002908b8` | 7.941225 | rx_ring_payload | DSP peer |
| v501 | `0x192` | `0x1e00` | `0x00290350` | 7.941277 | rx_ring_payload | DSP peer |
| v600 | `0x192` | `0x1e00` | `0x0029095c` | 7.941277 | rx_ring_payload | DSP peer |
| v501 | `0x194` | `0x027f` | `0x00290350` | 7.941280 | rx_ring_payload | DSP peer |
| v600 | `0x194` | `0x027f` | `0x0029095c` | 7.941280 | rx_ring_payload | DSP peer |
| v501 | `0x196` | `0x0002` | `0x00290350` | 7.941284 | rx_ring_payload | DSP peer |
| v600 | `0x196` | `0x0002` | `0x0029095c` | 7.941284 | rx_ring_payload | DSP peer |
| v501 | `0x198` | `0x0005` | `0x00290350` | 7.941287 | rx_ring_payload | DSP peer |
| v600 | `0x198` | `0x0005` | `0x0029095c` | 7.941287 | rx_ring_payload | DSP peer |
| v501 | `0x19a` | `0x5f00` | `0x00290350` | 7.941291 | rx_ring_payload | DSP peer |
| v600 | `0x19a` | `0x5f00` | `0x0029095c` | 7.941291 | rx_ring_payload | DSP peer |
| v501 | `0x19c` | `0x098e` | `0x002902ac` | 11.906451 | rx_ring_payload | DSP peer |
| v600 | `0x19c` | `0x098e` | `0x002908b8` | 11.906451 | rx_ring_payload | DSP peer |
| v501 | `0x19e` | `0x1e00` | `0x00290350` | 11.906503 | rx_ring_payload | DSP peer |
| v600 | `0x19e` | `0x1e00` | `0x0029095c` | 11.906503 | rx_ring_payload | DSP peer |
| v501 | `0x1a0` | `0x027f` | `0x00290350` | 11.906507 | rx_ring_payload | DSP peer |
| v600 | `0x1a0` | `0x027f` | `0x0029095c` | 11.906507 | rx_ring_payload | DSP peer |
| v501 | `0x1a2` | `0x0002` | `0x00290350` | 11.906510 | rx_ring_payload | DSP peer |
| v600 | `0x1a2` | `0x0002` | `0x0029095c` | 11.906510 | rx_ring_payload | DSP peer |
| v501 | `0x1a4` | `0x0006` | `0x00290350` | 11.906514 | rx_ring_payload | DSP peer |
| v600 | `0x1a4` | `0x0006` | `0x0029095c` | 11.906514 | rx_ring_payload | DSP peer |
| v501 | `0x1a6` | `0x5f00` | `0x00290350` | 11.906517 | rx_ring_payload | DSP peer |
| v600 | `0x1a6` | `0x5f00` | `0x0029095c` | 11.906517 | rx_ring_payload | DSP peer |
| v501 | `0x1a8` | `0x098e` | `0x002902ac` | 15.871779 | rx_ring_payload | DSP peer |
| v600 | `0x1a8` | `0x098e` | `0x002908b8` | 15.871824 | rx_ring_payload | DSP peer |
| v501 | `0x1aa` | `0x1e00` | `0x00290350` | 15.871831 | rx_ring_payload | DSP peer |
| v600 | `0x1aa` | `0x1e00` | `0x0029095c` | 15.871876 | rx_ring_payload | DSP peer |
| v501 | `0x1ac` | `0x027f` | `0x00290350` | 15.871835 | rx_ring_payload | DSP peer |
| v600 | `0x1ac` | `0x027f` | `0x0029095c` | 15.871879 | rx_ring_payload | DSP peer |
| v501 | `0x1ae` | `0x0002` | `0x00290350` | 15.871838 | rx_ring_payload | DSP peer |
| v600 | `0x1ae` | `0x0002` | `0x0029095c` | 15.871883 | rx_ring_payload | DSP peer |
| v501 | `0x1b0` | `0x0007` | `0x00290350` | 15.871842 | rx_ring_payload | DSP peer |
| v600 | `0x1b0` | `0x0007` | `0x0029095c` | 15.871886 | rx_ring_payload | DSP peer |
| v501 | `0x1b2` | `0x5f00` | `0x00290350` | 15.871845 | rx_ring_payload | DSP peer |
| v600 | `0x1b2` | `0x5f00` | `0x0029095c` | 15.871890 | rx_ring_payload | DSP peer |
| v501 | `0x1b4` | `0x098e` | `0x002902ac` | 19.837058 | rx_ring_payload | DSP peer |
| v600 | `0x1b4` | `0x098e` | `0x002908b8` | 19.837016 | rx_ring_payload | DSP peer |
| v501 | `0x1b6` | `0x1e00` | `0x00290350` | 19.837111 | rx_ring_payload | DSP peer |
| v600 | `0x1b6` | `0x1e00` | `0x0029095c` | 19.837068 | rx_ring_payload | DSP peer |
| v501 | `0x1b8` | `0x027f` | `0x00290350` | 19.837114 | rx_ring_payload | DSP peer |
| v600 | `0x1b8` | `0x027f` | `0x0029095c` | 19.837071 | rx_ring_payload | DSP peer |
| v501 | `0x1ba` | `0x0002` | `0x00290350` | 19.837117 | rx_ring_payload | DSP peer |
| v600 | `0x1ba` | `0x0002` | `0x0029095c` | 19.837075 | rx_ring_payload | DSP peer |
| v501 | `0x1bc` | `0x0000` | `0x00290350` | 19.837121 | rx_ring_payload | DSP peer |
| v600 | `0x1bc` | `0x0000` | `0x0029095c` | 19.837078 | rx_ring_payload | DSP peer |
| v501 | `0x1be` | `0x5f00` | `0x00290350` | 19.837124 | rx_ring_payload | DSP peer |
| v600 | `0x1be` | `0x5f00` | `0x0029095c` | 19.837082 | rx_ring_payload | DSP peer |
| v501 | `0x1c8` | `0x008c` | `0x0029026e` | 0.302768 | rx_producer | DSP peer |
| v501 | `0x1c8` | `0x008c` | `0x0029029c` | 0.302774 | rx_producer | DSP peer |
| v600 | `0x1c8` | `0x008c` | `0x0029087a` | 0.288844 | rx_producer | DSP peer |
| v600 | `0x1c8` | `0x008c` | `0x002908a8` | 0.288850 | rx_producer | DSP peer |
| v501 | `0x1ca` | `0x0080` | `0x0029026a` | 0.302767 | rx_consumer | MCU |
| v501 | `0x1ca` | `0x0080` | `0x00290290` | 0.302773 | rx_consumer | MCU |
| v501 | `0x1ca` | `0x0081` | `0x0029032a` | 0.302823 | rx_consumer | MCU |
| v600 | `0x1ca` | `0x0080` | `0x00290876` | 0.288844 | rx_consumer | MCU |
| v600 | `0x1ca` | `0x0080` | `0x0029089c` | 0.288849 | rx_consumer | MCU |
| v600 | `0x1ca` | `0x0081` | `0x00290936` | 0.288899 | rx_consumer | MCU |
