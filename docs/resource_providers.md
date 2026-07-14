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

The direct `0x05e1` surface is also finite: ten calls owned by callbacks
`0x4e`, `0x51`, `0x34`, `0x32`, `0x59`, `0x47` (three calls), `0x6d`, and
`0x0f`. None is an autonomous ordinary-boot initializer:

| Owner | Direct call(s) | Required lifecycle |
|---|---|---|
| `0x4e` | `0x248dfa` | downstream input `0x05eb`; republishes an already-complete lifecycle |
| `0x51` | `0x25c88c` | object cleanup after `0x0bd6`, itself constructed from input `0x0bcc`; no direct in-ROM `0x0bcc` publisher recovered |
| `0x34` | `0x2686bc` | input `0x00c8` with an outstanding stored transaction handle |
| `0x32` | `0x27c090` | object-bearing input `0x13f8` with subtype `0x65` or `0x67`; no direct in-ROM `0x13f8` publisher recovered |
| `0x59` | `0x288030` | input `0x0348` after registration/session work and local state `0x11fcc3 == 1`; no direct in-ROM `0x0348` publisher recovered |
| `0x47` | `0x28f514`, `0x28f540`, `0x28f56a` | respectively text/UI transaction completions `0x0578`/`0x1440`, call-message inputs `0x138b`/`0x138c`, or the excluded local/test `0x05e7` lifecycle |
| `0x6d` | `0x299122` | input `0x0546`, local state `0x0d`, and resource `0x25` available |
| `0x0f` | `0x2a0dc8` | UI/session inputs `0x0274` or `0x0348` followed by teardown/resource release |

The fixed sequence catalogue adds one indirect numeric route: catalogue-mode
packed input `0x212b` or `0x612b` (status index `0x012b`) expands to a sequence
headed by `0x05e1`. The census finds no literal load, recovered constant
construction, or direct packed-event call for either invoking form. The table
entry therefore proves a valid later transition, not an ordinary producer.

The only owner selected in the coherent run is callback `0x47`. A corrected
execution of its subtract cascade shows that the shared branch at `0x28f4e4`
accepts `0x0578` and `0x1440`, not `0x0778` and `0x1441`. The adjacent
`0x143f -> 0x28f6a4 -> 0x1440` path is therefore numerically relevant after
all, although `0x1440` is not observed in the coherent boot.

The observed input `0x05dc` instead enters `0x28f588` and starts the generic
text/UI transaction manager through `0x24b174 -> 0x24af70`. That constructor
stores `0x0578` as its completion status at `[0x110590 + 0x16]`, publishes
`0x057c` when the editor is presented, and waits for the UI transaction to
finish. The coherent run receives `0x05dc` at 4.557 s and `0x057c` shortly
afterward, but no `0x0578`, `0x1440`, or `0x05e1`. This classifies the branch
as an interactive UI lifecycle, not an absent radio/DSP reply and not an
unconditional ordinary-boot hardware prerequisite.

The transient and resident service-registration completion paths at `0x2632be` and
`0x263e64` both publish readiness after
`0x26265c` reports no registrations remain. The reviewed deep run executes
transient registration only for service `0x0a`, never service `0x05`, and
executes no resident registration through `0x263d30`. The callback-`0x21` path
at `0x2298d2` consumes event
`0x0c20`, but its only recovered direct producer (`0x27616e`) sits in a
service-command/session block and is not presently an ordinary-hardware
candidate.

The canonical profile produces `0x08ac` twice and enters `0x27f150` twice.
Availability helper `0x287250(0)` has two card-provisioning success paths:

- `0x26f5a4` first requires product-feature query `0x2b47a0(0x1a)` to return
  zero (`EEPROM` record `0x150`, mirrored byte `0x1124e9.bit0 == 0`), then
  requires parsed-valid CPHS `EF_CSP (6F15)` and tests group `0x03` mask
  `0x20` in its 40-byte result;
- fallback `0x26eef4` requires the parsed-valid flag for `EF_SST (6F38)` and
  tests both bits `0xc0` in result byte 1. Parser `0x201150` derives those bits
  from EF_SST byte 2 bits 1 and 2.

The ordinary non-CPHS card correctly satisfies neither. An opt-in valid CPHS
phase-2 profile with CSP allocated/activated and Advice of Charge enabled
organically reads `6F16` and `6F15`, sets `[0x10d128].bit19`, and makes
`0x287250(0)` return one. Enabling service 5 then makes firmware read
`EF_ACM (6F39)`, `EF_ACMmax (6F37)`, and `EF_PUCT (6F41)` normally.

Initializer `0x27f9e8` owns the following guard `0x11fd04`: it sets the byte
only when an ACM maximum is active. GSM 11.11's zero value means the maximum
is not valid and correctly leaves the guard clear. A diagnostic but
standards-valid exhausted account (`ACM == ACMmax == 1`) makes `0x27f150`
construct its activity slot and take `0x27f23e` without firmware mutation.
That call publishes status `0x019a`, not `0x0795`. The former
`0x13fe -> 0x0795` frontier was a false join between controller lifecycles.

There are exactly two literal `0x0795` producers. The display producer at
`0x28c2be` requires service/display state 7 and is excluded from mode 4. The
other, `0x28796a`, requires selectors 0 and 1 both unavailable plus controller
state `0x10fcd5 == 2`. Status `0x08b0` invokes that reconciliation; the fixed
catalogue expands packed callback input `0x213a`/`0x613a` to
`0x089a, 0x08b0`, and adjacent `0x213b`/`0x613b` directly to `0x08b0`.
Status `0x013a` is a callback-`0x24` input in framework mode 11; callback
`0x013b` belongs to a resident descriptor registered at `0x28ba9a` only after
input `0x03ab` establishes the same mode. The coherent boot remains in mode 0, making
this a later conditional route rather than the ordinary code-7 owner.
