# Nokia 5210 NSM-5 profile

## Validated result

The local v5.40 PPM E Wintesla set reaches a stable, interactive standby
screen through the normal firmware lifecycle. `make verify-5210-frontier`
checks the unattended firmware-rendered standby frame against an exact
stable-pixel oracle. The separate menu, Messages and navigation gates use only
physical matrix keys to open the main menu, enter the Messages application,
and return through both Back levels to standby. No firmware RAM, message,
rendered pixel, or completion flag is injected.

The archive members, normalization command, and image hashes are recorded in
`roms/README.md`. The shared MAD2 inputs remain documented placeholders; this
profile does not establish NSM-5-specific internal-ROM identity.

## Product contracts

NSM-5 differs from the other validated products at explicit device boundaries:

- GENSIO control `0x22` selects CCONT without the NSE-8 control-bit
  convention. Receive-ready follows a completed command byte.
- Firmware organically issues DSP bootstrap command 4 with pending value 2
  and later type `0x70` payload `0d00`. The product enables the existing
  request-derived service completion and compact type-`0x74` completion only.
  A read-only run observed upload latch `0x13b528 == 1`, expected and actual
  self-test values both `0x9c49`, and the verdict retaining healthy bit 6.
- The external-service transport answers organic D0/01 discovery. NSM-5
  accepts class-`0x40` registration sequence `0x42` and channel-map sequence
  `0x43`, then organically exercises channel `0x5f`. Channel `0x62` remains in
  the accepted map but has not been exercised independently by this gate.
- The SIMI block and removable synthetic GSM card use the standard controller
  boundary. Card contents remain external input.
- The CCONT inputs represent a BLB-2 pack. Nominal BSI `0x150` and BTEMP
  `0x140` lie inside recovered windows `0x13e..0x172` and `0x133..0x157`;
  VBATT is `0x2c0`. The generic tuple was outside those windows and selected
  the fault screen even after the DSP self-test had passed.
- The 84-by-48 display uses reversed segment order. This is a product display
  property, not a capture-only mirror.
- The five-row NSM Family-A keypad uses power mask `0x02` and a dedicated MAME
  matrix containing softkeys, scroll, call, end, volume, digits, star and hash.

## Remaining scope

The gates validate v5.40 PPM E, board bring-up, standby rendering, the main
menu, application entry, and clean return navigation. They do not yet validate
network registration, calls, audio, persistent application writes, or another
5210 firmware revision. The radio peer therefore remains disabled. The
unexercised external-service channel and placeholder MAD2 inputs must not be
promoted to cross-product facts.
