# Ordinary SMS

This document is the authoritative boundary for ordinary text `SMS-DELIVER`
and `SMS-SUBMIT`. Smart Messaging remains a separate application contract.

## Ownership and ordering

`nokia_gsm_network_device` constructs laboratory TS 23.040 TPDUs and
TS 24.011 CP/RP envelopes. The product-independent `gsm::sms` transport
parser validates them before paging: it derives the PID, DCS, timestamp and
UDL positions from the originating-address length and rejects inconsistent
CP/RP lengths, unsupported or compressed alphabets and truncated user data.
`nokia_gsm_session_device` owns the saved CP transaction,
RP reference, acknowledgement phase and queued-message index.
`nokia_lapdm_link_device` independently owns SAPI-3 establishment, I-frame
sequence, uplink/downlink segmentation and release. The radio peer schedules ordinary PCH, SDCCH and RR
transport; it does not parse or store messages.

For an admitted message the required order is:

1. one correctly phased page and assigned SDCCH;
2. SAPI-3 establishment and segmented CP-DATA/RP-DATA/SMS-DELIVER;
3. firmware `UPDATE RECORD` when the application accepts ordinary text;
4. handset CP-ACK and correlated RP-ACK or RP-ERROR;
5. network CP-ACK, SAPI-3 closure and RR Channel Release;
6. return to PCH before another queued message is admitted.

The SIM card owns ten 176-byte `EF_SMS` records. The firmware writes status
`03` for unread, changes only the status to `01` when read, and changes it to
`00` when erased while leaving the old payload bytes in the freed record.
There is no host inbox, card-side SMS parser or synthetic storage
acknowledgement.

`SMSCFG` selects a laboratory-network incoming delivery profile and an
independent outgoing outcome: acceptance, permanent RP rejection, no CP
response or CP acknowledgement followed by no RP response. It does not
describe handset capability, storage policy or Nokia firmware behavior.

## Nokia 3210 mobile-originated lifecycle

`verify-radio-outgoing-sms` drives only physical keypad switches through
Menu -> Messages -> Write messages, composes `HI`, selects Send and enters
recipient `5551234`. The unmodified v6.00 firmware then requests MM short-
message service, establishes SAPI 3, and emits a two-frame CP-DATA/RP-DATA/
SMS-SUBMIT. The generic parser derives rather than assumes:

- CP transaction `0x29` and RP reference `0x01`;
- service centre `+1234567890`;
- destination `5551234`;
- GSM default-alphabet user-data length two, whose packed bytes are `c8 24`.

The laboratory network returns CP-ACK followed by RP-ACK in CP-DATA using
those observed transaction identifiers. Firmware sends the final CP-ACK,
renders `Message sent`, and the ordinary SAPI-0 RR release returns the radio
to idle scheduling. The gate requires both 20-byte uplink segments to become
one 28-byte Layer-3 object; accepting only the first LAPDm frame cannot pass.

`verify-radio-outgoing-sms-reject` returns the standards-defined network-to-
mobile RP-ERROR with the submitted RP reference and cause 21 (short message
transfer rejected). Firmware acknowledges its CP-DATA, releases RR, renders
`Message not sent this time`, and returns to the composer without a success
RP-ACK. Withholding CP or RP responses reaches the firmware timeout, but the
current HLE then repeats the segmented submit at assigned-channel cadence;
that retry storm is a known transport defect and is not an accepted oracle.

This is not yet a complete originated-SMS product contract. CP/RP timeout and
standards-timed retry, delivery reports, sent-message/SIM status policy,
service-centre editing, save-state
continuation and sibling-product corroboration remain open.

## Nokia 3210 application lifecycle

`verify-radio-sms-inbox` proves the complete NSE-8 v6.00 lifecycle for
`hello` from `5551234`: exact SMSC, originating address, timestamp, GSM
7-bit text and unread record; firmware `1 message received`; physical sender
and text display; unread-to-read update; physical Options → Erase,
confirmation, empty inbox and durable free status. Its preserved-NVRAM cold
run reaches the same sender/text through ordinary Menu 2-1-1 without
redelivery. `verify-radio-sms-inbox-negatives` proves that cancelling erase
keeps the read record.

`verify-radio-sms-sequential` delivers `hello` from `5551234` and `world`
from `5559876` through independent pages, SAPI-3 links, RP references `40`
and `41`, CP acknowledgements and RR releases. Firmware reports two messages
and lists them in deterministic arrival order. Reading the first changes
statuses from `(03,03)` to `(01,03)`; selectively erasing it produces
`(00,03)` and leaves the exact second payload visible and unread.

## Negative and capacity policy

Malformed originating-address geometry, reserved DCS, truncated user data
and inconsistent UDL are rejected by the generic network ingress validator.
They produce no page and no `EF_SMS` mutation. A repeated queued RP reference
after terminal acknowledgement is consumed idempotently without a second
page or record.

The storage-full gate fills the card organically rather than pre-populating
it. Ten separately correlated messages occupy all ten records through normal
firmware APDUs. The eleventh is paged and delivered, and firmware returns
RP-ERROR reference `4a`, cause `16` (memory capacity exceeded). The network
still sends CP-ACK and releases RR; the ten accepted records remain intact.
After physical security unlock, firmware renders `No space for new messages`;
thus the capacity condition is proved at both RP and application/UI boundaries.

## Save-state contract

The machine image saves the active service, message index, CP transaction,
RP reference, CP/RP acknowledgement flags, pending downlink, LAPDm state and
radio/RR phase. It does not save a host-side message or inbox.

`verify-radio-sms-inbox-state` restores during the firmware storage
transaction, before and after RP completion, at the unread notification,
while viewing the message and at the erase-confirmation boundary. Physical
continuation keys are issued from the restored branch, because Lua input
coroutines are external to the machine image. The final firmware/SIM state
contains one record only and never resurrects a deleted record.
`verify-radio-sms-sequential` additionally restores after message one has
closed while message two remains queued; message two retains its independent
reference and record.

## Product evidence

| Product | Proven ordinary-text boundary |
| --- | --- |
| Nokia 3210 NSE-8 v6.00 | Complete receipt, notification, preserved cold-boot listing, physical read, read status, erase/cancel, capacity, duplicate/malformed and two-message isolation. |
| Nokia 3310 NHM-5 v6.39 | Independent localized notification, exact `hello`, SIM read status, erase confirmation and durable deletion. Preserved-NVRAM boot reaches the message through its firmware time/date setup and Menu 2-2 path. |
| Nokia 3410 NHM-2 v5.46E | Independent preserved-PMM receipt, notification, ordinary Messages → Inbox listing, exact `hello`, read status, erase confirmation and durable deletion. Virgin-PMM setup is completed in a separate physical preparation run. |
| Nokia 3330 NHM-6 v4.50E | Paging, exact SIM storage, CP/RP closure and RR release pass after physical fresh-PMM provisioning. A preserved cold boot re-enters the firmware security/time editor, so physical inbox/read/delete promotion remains pending and is not bypassed. |

The cross-product gates share only standards-level transport and record
checks through `tools/radio_sms_acceptance_common.py`. Menu grammar, localized
frames, PMM preparation and product storage paths remain separately evidenced.
`run-captured` and `run-prebuilt-captured` preserve each run's log before the
next invocation; they do not change the machine or its acceptance oracle.

Exact top-level commands are:

```text
JOBS=4 make verify-radio-sms-inbox
JOBS=4 make verify-radio-outgoing-sms
JOBS=4 make verify-radio-outgoing-sms-reject
JOBS=4 make verify-radio-sms-inbox-state
JOBS=4 make verify-radio-sms-inbox-negatives
JOBS=4 make verify-radio-sms-sequential
JOBS=4 make verify-3310-radio-sms-inbox
JOBS=4 make verify-3330-radio-sms-transport
JOBS=4 make verify-3410-radio-sms-inbox
```

Mobile-originated failure/retry policy, service-centre editing, delivery
reports, cell broadcast and EMS remain future independently evidenced work.
