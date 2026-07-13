# The display resource-provider layer — a scoping pass

The one wall every thread of this project terminates at: the boot renders
**"Insert SIM card"** but never repaints to a richer screen, because the idle draw
needs display **resources** whose backing isn't present on our reconstructed boot.
Earlier notes called this "the resource-provider graph a real boot constructs and ours
doesn't," and (from the retired `EXPERIMENT_IDLE_DRAW`) established empirically that
*forcing the missing classes' availability bits on blanks/crashes the display*. This
pass mechanizes **what a "resource provider" actually is** here, so the wall is
described in code, not intuition — and, critically, answers whether there is a dormant
provider-registration init we could simply switch on. **There is not.**

All addresses are in the swap16 image (`tools/disrom.py`). Oracle unchanged
(`d8a9a7a58e587be8`); this pass is disassembly-only, no driver edits.

## The idle draw is three lines

`display_idle 0x2a255c`:

```
resource-get(id=0x4c22, 0)     ; 0x2b257e
render-post(0x0547)            ; 0x2af6ea  -> message 0x0547 to task 5 (MMI VM)
return
```

So `display_idle` itself only acquires the **idle window** resource `0x4c22` and posts a
render message.

**Correction (2026-07): the `0x0547` handler issues NO content resource-gets.** With the
mask-table fix so `0x4c22` actually acquires, a runtime trace (`TRACE_MMIVM`) shows the
*only* resource-get in the entire idle sequence is `0x4c22`; after task 5 dequeues `0x0547`
it does no further `resource-get`/availability calls and goes quiet. So the ~18-class
"content composition inside the `0x0547` handler, each child issuing its own resource-get"
model above is wrong. The idle window is a **container** that opens empty; its content
(fonts/icons/clock/operator/signal) is drawn by **separate child render events that live
subsystems post** after the window opens — not by resource-gets in the render. On our boot
none arrive (no network/operator producers), so the window renders blank forever. See
`docs/interactive_handoff.md` "Content-backing wall dig".

## What `resource-get` / acquire actually build (a descriptor, not pixels)

`resource-get 0x2b257e (id, req_struct)`:

1. **availability** `0x2b12b4(id)` → if not available, **return fail** (no descriptor).
2. compute `class = id>>8`; consult three ROM tables that hold only **metadata**:
   - **min/max id per class** (`0x2e0a94` / `0x2e0a96`, `ldrh [tbl + class*4]`) — an
     id-range sanity bracket; out-of-range → fail.
   - **def-table** `0x2e0a50` via `0x2b2560` — see below; returns a small integer
     (an element/line **count**), or a hard-coded cascade for a few special ids.
3. **always finish by calling acquire** `0x2b12dc(id, req_struct)`.

`acquire 0x2b12dc`:

- gate on master enable `[0x11fee4]` (0 → fail);
- take a semaphore (`0x2698e4`), bump a counter `[0x11f824]`;
- **allocate** `size+0x10` bytes (`0x26afe0`) and fill a **16-byte header**: id at
  `[+8/9]`, size at `[+4/5]`, semaphore handle at `[+a/b]`, the enable/config bytes
  copied from `[0x11fee4/5]` and `[0x1128ff]`, flags. The `req_struct` also carries a
  **name string** at `[+4]` (`0x2b6680` is `strlen`, not a provider lookup — earlier
  mislabel corrected here).

So a "resource" at this layer is a **RAM management descriptor / handle** — an id, a
size, a payload buffer, a lock. **It is not pixels and not a content object.** The
descriptor's payload is filled, and its content drawn, later by the task-5 render using
ROM data + live state. **Nowhere in the acquire/get path is there a function-pointer
table indexed by class** — i.e. no "provider object" is dispatched through. The word
"provider" in older notes overstates it: there is availability + metadata + a handle,
then draw-time content.

## Availability and the def-table are the only two backing structures

**Availability** `0x2b12b4`:

```
available(id) = [0x11fee4] != 0
              AND  ( masktable[class & 7] )  bit-set-in  [0x11ff08 + (class>>3)]
  where class = id>>8, masktable @ 0x2e2f5c
```

**Correction (2026-07): the mask table is a swap16 trap.** The swap16-image bytes at
`0x2e2f5c` read `{40,80,10,20,04,08,01,02}` and earlier notes recorded that as a
"permuted" table. But the firmware indexes it with `ldrb`, which reads the *real* rom
byte `image[addr ^ 1]`, so the table the firmware actually sees is a clean descending
`masktable = {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01} = 0x80 >> (class & 7)`. This was
confirmed at runtime: with `[0x11fee4]=1` and the class-`0x4c` bitmap byte `[0x11ff11]`
holding bit `0x04`, `available(0x4c22)` returned **0** — because the real mask for class
`0x4c` (`class&7=4`) is `0x08`, not `0x04`. Correcting the bit to `0x08` makes
`available(0x4c22)=1` and the idle window acquires. Any blob built from the permuted
table enables the *wrong* classes (e.g. the old sparse blob byte9=`0x06`/byteA=`0x51`
enabled `{0x4d,0x4e,0x51,0x53,0x57}`, never the intended `{0x4c,0x4f,0x50,0x52,0x56}`).

The bitmap `[0x11ff08..0x11ff10]` + `[0x11fee8..]` is written **only** by the registrar:

**Registrar** `0x2b140a(r0, enable, r2, config_ptr)` — the *sole* writer of the enable
flag and the bitmaps:

- `config_ptr==0` → `[0x11fee4]=0` (disable), return.
- else set `[0x11fee4]=enable`, `[0x1128ff]=r2`, `[0x11fee5]=r0`, and if `enable≠0`
  **memcpy a 0x40-byte config blob** into `[0x11ff08]`(0x20) + `[0x11fee8]`(0x20), then
  OR bit 0x80 into `[0x11ff08]`.

So *which classes are available is caller-supplied DATA* — the 0x40-byte blob — not
hard-coded. That blob arrives as the **contact-service cmd `0x70`** payload
(`msg+9`; `docs/sim_emulator_scope.md`, `firmware_code_maps.md`). It is **provisioned
product data** we don't have; `MODEL_RES_ENABLE` fakes a *sparse* blob enabling only the
ROM-backed classes.

**Def-table** `0x2e0a50` (referenced from exactly one site, `0x2b2560` via `0x2b270c`) —
16 live entries, `{id:hword, count:hword}`, 0-terminated:

```
0x4c5c→2   0x4f41→2  0x4f42→2  0x4f43→3  0x4f44→4
0x5031→4   0x5034→6  0x5223→1  0x5633→2  0x5634→8
0x5635→3   0x5636→7  0x5637→0xe 0x5638→0x10 0x5639→2  0x563a→0x2a
```

Classes present: **only 0x4c, 0x4f, 0x50, 0x52, 0x56.** These are the ROM-backed
resources; the sparse-5 registration renders the real screen (`blank→o058→o074`). **None
of the ~13 idle-content classes** the task-5 render queries
(`0x22 0x25 0x26 0x27 0x2a 0x2b 0x30 0x31 0x3a 0x3c 0x3d 0x44 0x4a`) appears in this
table, nor in the special-id cascade.

## The scoping verdict: there is no dormant provider init to switch on

Putting it together, a resource decomposes into exactly two mechanisms, and **both are
data/state, not a runtime object graph a subsystem forgot to build**:

1. **Availability = provisioned data.** The 0x40-byte cmd-`0x70` blob (per-product,
   which classes exist) → registrar → bitmap. We don't have the real blob. Forcing all
   bits on (`RES_ENABLE_FILL=0xff`) enables classes with no backing → the render acquires
   descriptors for content that doesn't exist → **blank/crash** (the retired experiment).

2. **Backing = ROM-static or draw-time-composed, never registered.**
   - The 5 ROM classes have fixed def-table entries + ROM content → they render.
   - The ~13 other classes have **no def-table entry at all**. Their content is not a
     registrable object; it is **composed at draw time by the UI element code from live
     subsystem state** — fonts/icons/layout sub-windows, plus clock (RTC), operator name
     + signal bars (SIM + network/DSP). There is nothing to "create" and register; the
     content is *computed* from subsystems that must be coherently up.

So the earlier framing — "a provider-object graph a real boot constructs and ours
doesn't" — resolves to: **(a) an availability bitmap set by a received enable command
+ (b) live subsystem content**, both already known walls. There is **no
provider-registration function that runs on a real boot and not on ours** that we could
call to populate a registry — the acquire/get path has no such registry, only a bitmap
(data) and a handle allocator. This closes the "just register the providers" hypothesis:
it is proven, in code, to have no seam. Consistent with the empirical result that forcing
availability blanks the screen.

## Follow-up pass: the enable is an unobserved lower-service transaction

A second pass established that the blob is message-supplied, but not where its initiating
producer resides. Evidence:

- **The whole availability state has exactly one writer.** `findptr` on the enable flag
  `0x11fee4` and both bitmaps `0x11ff08` / `0x11fee8` returns a **single reference each**,
  all pool literals inside the resource module (`0x2b1488/90/92/a4/a6`), i.e. only
  `resource_registrar 0x2b140a` writes them. Availability `0x2b12b4` and the report path
  `0x2b13d4` only *read* them.
- **The registrar is reachable only via contact-service enable/disable.** Its four
  callers (`0x2366d4`, `0x23672c`, `0x236e6c`, `0x236f10`) are all branch targets *inside*
  the one channel-map handler `0x23670c`, which dispatches on `[msg+8] ∈ {0x70, 0x71}`
  (enable / disable). The ENABLE handler `0x2366d4` takes the blob straight from the
  message (`config_ptr = msg+9`), checksums it (`0x2a41d0`, a plain 0x40-byte byte-sum),
  and calls the registrar — **no ROM default blob is referenced**, so the 0x40 bytes are
  the sender's.
- **The sender is the external service/test peer.** The constructor census
  (`contact_service_topology.md`) finds only the MCU acknowledgement at `0x236742`, not an
  MCU-side initiating `0x70`; outgoing contact frames address the learned external service
  node through task 7. A standalone normal boot need not enable this service channel, so
  cmd-`0x70` availability cannot be assumed to be a prerequisite for the ordinary idle UI.
- **The base "Insert SIM card" screen doesn't need it.** That screen renders through a
  pre-resource path (no bitmap), which is why our boot shows it despite the enable never
  firing. The enable is needed by a **later, interactive/idle** boot phase — the one gated
  behind the SIM/network coherent-boot walls we never clear. On our boot the
  contact-service processes exactly one command (`0x64`, injected by `MODEL_SVC_RESPONDER`);
  cmd `0x70` never arrives in the current profiles.

**Refined verdict:** the display-resource wall is not a dormant registry we could populate.
It is an unobserved external-service transaction: no current coherent profile receives `0x70`,
and the MCU ROM contains no independent initiating constructor for it. The 0x40-byte blob is
peer-supplied service configuration; forcing the registrar would skip that contract. It should
not be treated as the ordinary boot-to-idle critical path without new runtime evidence.

Additional structure mapped this pass (for future work): the low icon/indicator classes
(`0x22–0x4a`) are **optional** — availability `0x2b12b4` is consulted from ~74 sites
(mostly the `0x281xxx` resource-enumeration/report cluster), and each **skips silently**
when its class is unavailable (`beq` past the report call `0x2b13d4`), rather than
faulting. So the "forcing availability blanks the screen" failure is specifically the
*all-0xff* case enabling ids whose content fetch has no backing; the optional icons are
gated on availability **and** live subsystem state (signal/SIM/etc.), not on a registrable
provider object.

## Net

Boot-to-"Insert SIM card" is the complete digital milestone. The richer idle screen is
gated on two things this pass has now localized precisely — the real provisioned cmd-0x70
resource-config blob (data we lack), and the live SIM+network+display subsystem state
(the coherent-bringup / RF-hardware walls) — **not** on a switchable provider-init. No
bit-flip or registry-population shortcut exists. Symbols/addresses touched:
`display_idle 0x2a255c`, `resource_get 0x2b257e`, `resource_acquire 0x2b12dc`,
`resource_available 0x2b12b4`, `resource_registrar 0x2b140a`, def-table `0x2e0a50`,
mask-table `0x2e2f5c`, id-range tables `0x2e0a94/0x2e0a96`.
