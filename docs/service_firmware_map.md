# Service-startup firmware map

This is the concise address map for the Nokia 3210 v6.00 lower-service and
contact-startup path. It replaces the chronological static-branch investigation;
full disassembly transcripts and deleted experiments remain in Git history.

## Boundary summary

Startup spans three distinct peers:

1. the MCU queues lower-service work in DSP shared RAM;
2. the DSP-side service peer drains the pending counter and raises MAD2 IRQ 4;
3. task 7 adapts the external service/test transport into class-`0x40` task-2
   messages.

The current coherent profile models these boundaries without writing firmware
state. `contact_service_topology.md` owns command direction and session ordering;
this document owns the principal firmware branch and address map.

## Lower-service readiness

`service_context_ready_2b03d8` checks whether the lower transmit context is
idle. Firmware queueing makes the lower queue/busy fields nonzero; real hardware
completion must drain them before startup can proceed.

The DSP service path is separately observable through shared pending counter
`0x100e4`. Firmware setter `0x291068` writes readiness byte `0x110c2c = 1` only
when the counter is zero and the IRQ-4 handler runs. Draining the counter without
IRQ 4, or raising IRQ 4 without draining it, is insufficient.

| Address | Symbolic role |
| --- | --- |
| `0x2b03d8` | `service_context_ready` |
| `0x2ad1c8` | `service_lower_idle_check` |
| `0x2ad3e4` | `service_lower_enqueue_external_frame` |
| `0x2aaca8` | `service_lower_tx_busy_set` |
| `0x2aae76` | `service_lower_mbus_rx_state_machine` |
| `0x2b052e` | `service_transport_complete` |
| `0x290c98` | DSP pending-counter queue point |
| `0x2af3ca` | MAD2 IRQ-4 completion handler |
| `0x291068` | service-ready setter |

## Contact watchdog

Task 2's D9 poll at `0x237b28` increments `0x11fed6`. If startup does not reach
a healthy contact result, the terminal path calls `0x2b4dda` and presents
CONTACT SERVICE. The watchdog is a symptom of the incomplete lower-service
session, not an independent hardware device.

Service-present bit 6 in `0x11fed0` determines whether the extended task startup
path arms the watchdog. Contact initialization sets the bit, then may clear it
when DSP readiness, EEPROM checksums, CCONT presence, channel status, or the
lower service transaction fails.

| Address | Symbolic role |
| --- | --- |
| `0x237b28` | `contact_service_d9_watchdog_poll` |
| `0x237ade` | `contact_service_da_status_update` |
| `0x2379e8` | `contact_service_e2_event_source_update` |
| `0x236dc4` | contact result/timeout handler |
| `0x2b4dda` | terminal startup outcome/CONTACT SERVICE path |

## External service session

Task 7 is the lower-transport adapter. Task 2 receives class-`0x40` frames and
learns their address/route header before command dispatch. A coherent peer must
therefore initiate the session through the lower transport; posting a task-2
message bypasses address learning and completion ownership.

`contact_response_code_70_71_handler_23670c` applies incoming channel maps.
`contact_service_response_dispatch_237400` dispatches the command byte at
`message[+8]`. The current peer supplies result-1 discovery, a `0x70` channel
map, result-5 healthy completion, and correlated transport acknowledgements.

| Address | Symbolic role |
| --- | --- |
| `0x234634` | contact frame allocation/constructor |
| `0x234684` | contact frame send/release path |
| `0x23670c` | command-`0x70`/`0x71` channel-map handler |
| `0x237400` | contact command dispatcher |
| `0x237c70` | class-`0x40` receive/address-learning path |
| `0x2b0482` | lower service queue entry |
| `0x2b140a` | service channel-open writer |

## DSP service-session completion

Contact initialization posts task-3 object `0x2db250`, serialized as DSP TX
type `0x70`, payload `0d 00`. RX type `0x74`, payload `0d 00`, reaches decoder
`0x29bc00`, which builds the class-`0x74` task-2 message consumed at `0x234954`.
Command `0x0d` clears contact busy bit 2 at `0x2349dc` while retaining present
bit 6. This is distinct from class-`0x40` service command `0x74`.

## Extended-task resume

After service-session completion, supervisor `0x2a8ff2` requests channel-empty report
`0x622a` through `0x2b13d4` and waits at `0x29bb06` for the transaction to
complete. Its second resume group activates the application tasks that fill the
checklist at `0x112280`. Checklist completion posts event `0x15`; startup mode
`0x000d` then advances.

| Address | Symbolic role |
| --- | --- |
| `0x2a8ff2` | startup power/service supervisor |
| `0x29bafc` | service-empty readiness request |
| `0x29bb06` | service-busy bounded wait |
| `0x2b13d4` | shared service report entry |
| `0x2521cc` | application checklist completion test |
| `0x2af208` | event-`0x15` producer thunk |
| `0x270e22` | mode-`0x000d` event accumulator |

## EEPROM and CCONT prerequisites

Two generated EEPROM checksum regions and CCONT presence participate in contact
startup. The external EEPROM contract is documented in `eeprom_analysis.md`;
CCONT register and IRQ behavior is documented in `ccont_subsystem.md`. These are
ordinary device inputs, not service messages.

## Acceptance

The coherent profile must demonstrate, in one boot:

- DSP pending work drains with IRQ-4 completion;
- service-session status reaches `0x49` without a firmware write;
- class-`0x40` header learning and channel-map traffic pass through task 7;
- the final `0x622a` transaction completes;
- startup advances `0x000d -> 0x0004`; and
- ordinary SIM traffic begins.

These predicates are protected by `make verify-frontier`, the contact runtime
manifest, and `evidence/state_predicates.json`.
