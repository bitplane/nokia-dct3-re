# Display resource and content boundary

This document records the current display-resource conclusion. Historical
resource-enable injections and availability sweeps are summarized in
`removed_forcing_knobs.md`; they are not part of the current driver strategy.

## Resource handles, not content providers

`resource_get_2b257e(id, request)` first checks availability through
`0x2b12b4`, validates ROM metadata, and calls `resource_acquire_2b12dc`.
Acquire allocates a RAM descriptor containing the ID, size, lock and payload
storage. There is no class-indexed provider-object registry or dormant provider
initialization function.

The ROM definition table at `0x2e0a50` describes the statically backed classes
`0x4c`, `0x4f`, `0x50`, `0x52`, and `0x56`. The availability mask table at
`0x2e2f5c` must be read with MCU byte lanes: the firmware sees
`{80,40,20,10,08,04,02,01}`, not the byte-paired order visible in a raw swap16
dump.

## Contact-service configuration

The sole writer of the service availability flag and class bitmap is
`service_channel_config_2b140a`. Enable is reached by receiving contact-service
command `0x70` with a checksum-valid 0x40-byte map; `0x71` disables it.

The command census proves that the MCU constructors at `0x236742` and
`0x236736` are acknowledgements. The initiating `0x70`/`0x71` producer is the
external service/test peer behind task 7, not an organic MCU boot producer. A
standalone phone need not enable this service channel, so absence of command
`0x70` is not evidence that ordinary boot-to-idle lacks product data.

Forcing the bitmap or synthesizing command `0x70` bypasses the external service
contract and is not a route to faithful idle boot.

## Idle content

`display_idle_2a255c` acquires idle-window resource `0x4c22` and posts render
event `0x0547`. Runtime tracing established that the handler does not fetch a
second tree of content resources. The window is a container; independent live
subsystems later post child events for text, clock, operator, signal and status
elements.

This explains the old forcing result: enabling unsupported classes can allocate
handles without creating the live content and may blank or destabilize the
screen. It does not reveal a missing provider registry.

## Current implication

The active boot frontier is upstream in generic-service/lower-radio object
ownership, documented in `network_scouting.md` and `sim_registration.md`.
Resource availability should be revisited only if an organic interactive run
requests a handle and fails availability. Until that observation exists:

- do not model an automatic command-`0x70` session;
- do not set `[0x11fee4]` or bitmap RAM from the driver;
- do not treat idle content as a set of missing resource objects; and
- trace the subsystem child-event producers instead of forcing a draw.
