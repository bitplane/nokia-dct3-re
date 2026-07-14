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

Resource availability should be revisited only if an organic interactive run
requests a handle and fails availability. Until that observation exists:

- do not model an automatic command-`0x70` session;
- do not set `[0x11fee4]` or bitmap RAM from the driver;
- do not treat idle content as a set of missing resource objects; and
- trace the subsystem child-event producers instead of forcing a draw.

## Task-1 code-7 service/test display lifecycle

Backward recovery from task 1's missing report code `0x07` identifies a valid
firmware-internal display lifecycle. This is not the contact-service class
bitmap described above, but its producers belong to local/test transactions:

```text
display status 0x0280 / 0x0281 / 0x0282
  -> display dispatcher 0x28bddc
  -> 0x256f68 emits task-5 event 0x05e7
  -> callback 0x57 at 0x2882dc emits event 0x0389
  -> callback 0x31 at 0x27bc68 emits event 0x157e
  -> catalogue maps 0x157e to internal status 0x0396
  -> task-5 handler 0x2638e4 emits event 0x05eb
  -> callback 0x5d at 0x27b370
  -> reporter 0x2af190 posts code 0x07 to task 1
```

The coherent run reaches callback `0x5d` organically during the status
initialization sequence. Its state byte `[0x11fcdd]` is `0x0b`, not clear, but
its input is only initialization status `0x05e2`. Callback `0x57`
likewise sees only `0x05e2`, and neither `0x0389` nor `0x157e` appears.

The callback contract is broader than the originally mapped display chain.
Inputs `0x05e1`, `0x05e7`, and `0x05dc` schedule task-local timer class `0x52`;
the task-5 recode table returns that timer as `0x06c5`, which reports code
`0x07`. Inputs `0x05eb` and `0x06c5` report immediately. The earlier
`0x06dc` reading was an arithmetic error in the subtract cascade. Callback
flags `0x01a00000` deliberately choose status `0x032d`, rather than automatic
`0x05dc`, during framework selection, so callback activation alone does not
start the timer.

The transaction engine around `0x264504` and `0x264c98` owns statuses
`0x0280`-`0x0283`. Contact-service commands `0x81` and `0x82`, plus independent
ASCII local/test command frontends around `0x27c270`, reach these functions.
There are no catalogue entries or literal producers establishing an ordinary
boot source. A coherent trace of controller transform `0x253e20` also produces
none of `0x0280`-`0x0282`. This route is therefore mapped but excluded as the
ordinary code-7 owner.

The static producer inventory is now closed at the reporter boundary. Reporter
`0x2af190` has four of four callers classified. The two power callers are
excluded from ordinary boot; `0x27b3b6` is the callback-`0x5d` route; and
`0x255c3c` is the independent controller route. Five of five direct global
`0x05eb` publishers are classified. The registration family owns three of
them (`0x2632be`, `0x263bd4`, and `0x263e64`); `0x2298d2` is service/session
owned and `0x263ba0` handles display-owned `0x0396`. The sole effective
`0x0395` literal is the intermediate output at `0x263b8e`; its predecessor
`0x0397` has no recovered direct publisher. Four of four effective `0x0795`
literal sites are classified as one comparator and three producer paths.

The transient and resident service-registration completion paths at `0x2632be` and
`0x263e64` both publish readiness after
`0x26265c` reports no registrations remain. The reviewed deep run executes
transient registration only for service `0x0a`, never service `0x05`, and
executes no resident registration through `0x263d30`. The callback-`0x21` path
at `0x2298d2` consumes event
`0x0c20`, but its only recovered direct producer (`0x27616e`) sits in a
service-command/session block and is not presently an ordinary-hardware
candidate.

The controller route is now the active frontier. The canonical eight-second
profile produces `0x08ac` twice and enters `0x27f150` twice. Selector getter
`0x287216` returns `0`; availability helper `0x287250(0)` returns zero; and the
handler aborts at `0x27f16e` before building the activity slots that can
publish `0x0795`. No registration completion tail, global `0x0395`, `0x05eb`,
`0x06c5`, `0x0795`, or report code 7 is observed. The next investigation is
therefore the bounded contract behind `0x287250` selector 0, not another
search for an unexecuted reporter.
