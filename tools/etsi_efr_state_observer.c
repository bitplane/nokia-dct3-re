/* license:BSD-3-Clause
 * copyright-holders:Gaz
 * Read-only link-time observers for the separately acquired ETSI reference.
 * No codec implementation is included here.
 */
#include <stdio.h>

extern void __real_Decoder_12k2(short *, short *, short *, short, short);
extern void __real_D_plsf_5(short *, short *, short *, short, short, short);
extern short __real_d_gain_pitch(short, short, short, short, short);
extern void __real_d_gain_code(short, short *, short, short *, short, short, short, short, short, short);
extern short past_r2_q[10], past_lsf_q[10];
extern short prev_gc;
typedef char word16_requires_two_bytes[sizeof(short) == 2 ? 1 : -1];
static unsigned frame, pitch_subframe, code_subframe;

static void words(const char *kind, const short *values, unsigned count)
{
    unsigned i;
    fprintf(stderr, "\nEFRSTATE frame=%u kind=%s values=", frame, kind);
    for (i = 0; i < count; ++i) fprintf(stderr, "%04x", (unsigned)(unsigned short)values[i]);
    fputc('\n', stderr);
}

void __wrap_Decoder_12k2(short *parameters, short *synth, short *lpc, short taf, short sid)
{
    pitch_subframe = code_subframe = 0;
    __real_Decoder_12k2(parameters, synth, lpc, taf, sid);
    words("RAW", synth, 160);
    words("LPC", lpc, 44);
    ++frame;
}

void __wrap_D_plsf_5(short *indices, short *first, short *second, short bfi, short ctrl, short state)
{
    __real_D_plsf_5(indices, first, second, bfi, ctrl, state);
    words("R2", past_r2_q, 10);
    words("LSF", past_lsf_q, 10);
}

short __wrap_d_gain_pitch(short index, short bfi, short state, short prev_bf, short ctrl)
{
    short value = __real_d_gain_pitch(index, bfi, state, prev_bf, ctrl);
    fprintf(stderr, "\nEFRGAIN frame=%u kind=pitch subframe=%u value=%04x\n", frame,
            pitch_subframe++, (unsigned)(unsigned short)value);
    return value;
}

void __wrap_d_gain_code(short index, short *code, short length, short *gain, short bfi,
                       short state, short prev_bf, short ctrl, short subframe, short dtx_state)
{
    short previous_gain = prev_gc;
    __real_d_gain_code(index, code, length, gain, bfi, state, prev_bf, ctrl, subframe, dtx_state);
    if (!bfi)
        fprintf(stderr, "\nEFRPOLICY frame=%u subframe=%u prev_bf=%04x previous_gain=%04x value=%04x\n", frame,
                code_subframe, (unsigned)(unsigned short)prev_bf,
                (unsigned)(unsigned short)previous_gain, (unsigned)(unsigned short)*gain);
    fprintf(stderr, "\nEFRGAIN frame=%u kind=code subframe=%u value=%04x\n", frame,
            code_subframe++, (unsigned)(unsigned short)*gain);
}
