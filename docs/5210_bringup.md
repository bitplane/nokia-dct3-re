# Nokia 5210 NSM-5 bring-up

## Current result

The local v5.40 PPM E Wintesla set normalizes reproducibly and reaches a stable,
firmware-rendered `CONTACT SERVICE` frame. This is a bounded portability
frontier, not a claim that NSM-5 inherits the 3310, 3330, or 3410 contracts.

The initial generic-profile run stopped before RTOS startup while polling
GENSIO status. NSM-5 writes control `0x22`, transmits a CCONT command, and waits
for receive-ready even though control bit 2 is clear. Enabling the controller's
existing receive-ready-after-write behavior lets the firmware complete CCONT
transactions, upload DSP blocks, and drive the LCD. This wiring distinction is
owned by `PRODUCT_5210`; no firmware state or response payload is forced.

The raw LCD result is horizontally reversed. Until the fitted display
controller or board scan wiring is established, this is recorded as an NSM-5
display-profile gap rather than corrected in the capture harness.

## Inputs

The archive and Wintesla member hashes, normalization command, and resulting
image hashes are recorded in `roms/README.md`. `make smoke-5210e` performs the
current bounded run. The shared MAD2 files remain the documented placeholder
inputs and do not establish real NSM-5 internal-ROM identity.

## Resumption question

Which NSM-5-local DSP bootstrap, external-service, and SIM contracts satisfy
the self-test that currently selects `CONTACT SERVICE`? Recover those inputs
from this ROM's consumers and organic traffic; do not enable another product's
complete profile merely because its response reaches idle.
