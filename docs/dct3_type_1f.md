# DCT3 type-0x1f cross-model boundary

`make verify-dct3-type-1f-static` compares identified Nokia 3210 v6.00,
3310 v6.39 and 6110 v4.06 images without transferring behavior between
them. Each image independently constructs a status-2 queue object, declares
four payload bytes, and posts it to task 3. The common transmitted shape is:

```text
1f 00 flags value
```

Object byte 7 can carry local metadata, but it lies beyond the declared
four-byte payload beginning at object +3. It is not part of this wire
profile.

## Exact-image comparison

| Firmware | Direct constructors | Independently constructed policy |
|---|---:|---|
| Nokia 3210 NSE-8 v6.00 | 4 | Specialized forms with flags `04`, `01`, `0c/0d` and `01/06/07`; values are zero, table-derived or controller-derived. |
| Nokia 3310 NHM-5 v6.39 | 1 | One parameterized builder composes flag bits 0..3 from four runtime inputs and optionally supplies the value byte. |
| Nokia 6110 NSE-3 v4.06 | 5 | Specialized forms with flags `04`, `01`, `0d`, `0c/0d` and `01/06/07`; values are zero, table-derived, state-derived or controller-derived. |

The 3210 and 6110 constructions are related in shape but are not assumed to
be identical implementations. The 3310's single parameterized constructor
is materially different policy and prevents the shared component from
hard-coding either older handset's call graph or flag selection.

## Architectural consequence

The reusable boundary is a typed four-byte task-3 envelope:

- fixed type byte `0x1f`;
- fixed reserved byte zero;
- opaque flags byte;
- opaque product-policy value byte.

The product radio/controller layer owns flag and value construction. Local
queue metadata remains outside the wire object. No generic peer should infer
radio state from a particular flag until its DSP-side consumer or an
independent protocol source establishes that meaning.

This comparison does not prove acknowledgement behavior, timing units, a
DSP-side state transition, or that every product uses the message at the
same lifecycle point. It therefore adds a reusable structural contract but
does not enable the 6110 radio peer or promote its coverage status.

The checker pins all claims to SHA-256 identified images and writes
`run_census/dct3_type_1f_static_boundary.json`.
