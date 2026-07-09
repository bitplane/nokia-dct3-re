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
render message. The ~18-class content composition (fonts, icons, layout sub-windows,
clock, operator/signal) happens **inside task 5's handler for message `0x0547`**, where
each child element issues its *own* `resource-get`. `display_idle` is not where content
lives; it is the trigger.

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
  where class = id>>8, masktable @ 0x2e2f5c = {40,80,10,20,04,08,01,02} (permuted)
```

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
doesn't" — resolves to: **(a) provisioned availability data + (b) live subsystem
content**, both already known walls. There is **no provider-registration function that
runs on a real boot and not on ours** that we could call to populate a registry — the
acquire/get path has no such registry, only a bitmap (data) and a handle allocator. This
closes the "just register the providers" hypothesis: it is proven, in code, to have no
seam. Consistent with the empirical result that forcing availability blanks the screen.

## Net

Boot-to-"Insert SIM card" is the complete digital milestone. The richer idle screen is
gated on two things this pass has now localized precisely — the real provisioned cmd-0x70
resource-config blob (data we lack), and the live SIM+network+display subsystem state
(the coherent-bringup / RF-hardware walls) — **not** on a switchable provider-init. No
bit-flip or registry-population shortcut exists. Symbols/addresses touched:
`display_idle 0x2a255c`, `resource_get 0x2b257e`, `resource_acquire 0x2b12dc`,
`resource_available 0x2b12b4`, `resource_registrar 0x2b140a`, def-table `0x2e0a50`,
mask-table `0x2e2f5c`, id-range tables `0x2e0a94/0x2e0a96`.
