/* license:BSD-3-Clause
 * copyright-holders:Gaz
 * Read-only link-time observation of the separately acquired reference.
 */
#include <stdio.h>

extern void __real_rx_dtx(short *, short, short, short);
static unsigned frame;
typedef char word16_requires_two_bytes[sizeof(short) == 2 ? 1 : -1];

void __wrap_rx_dtx(short *ctrl, short taf, short bfi, short sid)
{
    __real_rx_dtx(ctrl, taf, bfi, sid);
    fprintf(stderr, "\nEFRDTX frame=%u taf=%u bfi=%u sid=%u ctrl=%04x\n",
            frame++, (unsigned)(unsigned short)taf,
            (unsigned)(unsigned short)bfi, (unsigned)(unsigned short)sid,
            (unsigned)(unsigned short)*ctrl);
}
