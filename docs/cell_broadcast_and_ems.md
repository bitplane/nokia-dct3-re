# Cell Broadcast and EMS

This document records the current standards-level implementations and the
remaining Nokia firmware boundary.  Neither feature is part of the ordinary
text-SMS acceptance contract.

## Enhanced Messaging Service

`gsm::ems` constructs and validates the TS 23.040 user-data-header Text
Formatting information element.  The laboratory fixture sends `hello` as
UCS-2 with TP-UDHI set and a formatting IE covering all five characters.
It uses the existing paging, SAPI-3, CP/RP and `SMS-DELIVER` machinery; EMS
does not define a second radio transport.

The Nokia 3210 v6.00 firmware accepts that standards-shaped TPDU and writes it
unchanged to `EF_SMS`.  The exact unread record, including TP-UDHI, DCS,
UDHL, formatting IE and UCS-2 text, is checked by
`tools/radio_ems_trace_check.py`.  This proves transport and persistence.  It
does not yet claim that this firmware interprets EMS formatting in its MMI;
Nokia Smart Messaging remains the separately proved contemporary application
format.

## Cell Broadcast

`gsm::cell_broadcast` owns the TS 03.41 88-octet page header/content contract
and the GSM 04.12 radio-interface segmentation contract.  It produces four
ordered 23-octet CBCH blocks: one LPD/LB/sequence octet and 22 information
octets per block.  Reassembly rejects a wrong LPD, sequence or last-block bit.
The laboratory network can construct a deterministic page for message
identifier `0x1000` containing `hello`.

The physical Menu 2-6 route reaches the firmware's **Info service** view, and
the On/Off setting is firmware-owned.  Changing that setting emits no dedicated
MCU-to-DSP packet in the observed run; this is consistent with an idle-mode
CBCH receiver consuming unsolicited broadcasts rather than establishing an
SMS transaction.

The MCU consumer is now identified.  Task 22 classes `5`, `7`, and `0x0a`
share primitive handler `0x23cde0`; class `7`, primitive `0x30` reaches
`0x23ceba`.  That branch copies four bytes verbatim into a descriptor, reads a
big-endian 16-bit length, caps it at `0xaa`, copies the following blob, and
posts event `0x1859` through `0x2b2ec8` to task 5.  An 88-octet TS 03.41 page
fits this contract exactly: the four copied bytes are serial number and
message identifier, while the 84-byte blob is DCS, page parameter and 82-byte
content.  `make verify-cell-broadcast-static` protects this interpretation at
the instruction level.

The producer boundary remains deliberately open.  A complete message census
finds no in-ROM construction of events `0x1859`, `0x1959`, or `0x1a59`; these
are peer-originated task-22 primitives.  The modeled MDIRCV/FIQ0 ring terminates
in task 4's closed packet-type switch and cannot carry an arbitrary task-22
class.  The separate type-`0x8e` framed-session translator accepts classes
`3`, `5`, `0x11`, and `0x47`, and its class-5 branch does not accept primitive
`0x30`.  Neither path can publish the CBS object without inventing a new
translation.

Cell Broadcast must not be routed through point-to-point SMS CP/RP, SIM
`EF_SMS`, or a direct scheduler injection.  The next required evidence is a
ROM4 DSP or real-phone capture showing the DSP-side publication mechanism
between four CBCH blocks and task 22.  Until then, the network page and CBCH
implementation is complete but firmware presentation is evidence-blocked.

## Standards references

- [ETSI GSM 03.41 v5.3.0](https://www.etsi.org/deliver/etsi_gts/03/0341/05.03.00_60/gsmts_0341v050300p.pdf)
- [ETSI GSM 04.12 v5.0.0](https://www.etsi.org/deliver/etsi_gts/04/0412/05.00.00_60/gsmts_0412v050000p.pdf)
- [ETSI TS 23.040 v6.5.0](https://www.etsi.org/deliver/etsi_ts/123000_123099/123040/06.05.00_60/ts_123040v060500p.pdf)
