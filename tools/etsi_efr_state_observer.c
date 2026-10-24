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
extern void __real_Syn_filt(short *, short *, short *, short, short *, short);
extern void __real_Pred_lt_6(short *, short, short, short);
typedef char word16_requires_two_bytes[sizeof(short) == 2 ? 1 : -1];
static unsigned frame, pitch_subframe, code_subframe, filter_subframe, in_decoder;

static void excitation_words(const char *kind, unsigned subframe, const short *values)
{
    unsigned i;
    fprintf(stderr, "\nEFREXC frame=%u subframe=%u kind=%s values=", frame, subframe, kind);
    for (i = 0; i < 40; ++i) fprintf(stderr, "%04x", (unsigned)(unsigned short)values[i]);
    fputc('\n', stderr);
}

void __wrap_Pred_lt_6(short *excitation, short lag, short fraction, short length)
{
    if (in_decoder) {
        unsigned i;
        fprintf(stderr, "\nEFRPREDINPUT frame=%u subframe=%u lag=%04x fraction=%04x length=%d\n",
                frame, pitch_subframe, (unsigned)(unsigned short)lag,
                (unsigned)(unsigned short)fraction, length);
        fprintf(stderr, "EFRPREDHISTORY frame=%u subframe=%u values=", frame, pitch_subframe);
        for (i = 0; i < 154; ++i)
            fprintf(stderr, "%04x", (unsigned)(unsigned short)excitation[(int)i - 154]);
        fputc('\n', stderr);
    }
    __real_Pred_lt_6(excitation, lag, fraction, length);
    if (in_decoder) {
        fprintf(stderr, "\nEFREXCPRED frame=%u subframe=%u length=%d\n", frame, pitch_subframe, length);
        excitation_words("ADAPT", pitch_subframe, excitation);
    }
}

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
    filter_subframe = 0;
    in_decoder = 1;
    __real_Decoder_12k2(parameters, synth, lpc, taf, sid);
    in_decoder = 0;
    words("RAW", synth, 160);
    words("LPC", lpc, 44);
    ++frame;
}

void __wrap_Syn_filt(short *coefficients, short *input, short *output, short length,
                     short *memory, short update)
{
    if (in_decoder) {
        const char *kinds[] = {"A", "EXC", "MEM"};
        const short *vectors[] = {coefficients, input, memory};
        unsigned widths[] = {11, 40, 10};
        unsigned kind, i;
        fprintf(stderr, "\nEFRFILTERCALL frame=%u subframe=%u length=%d update=%d\n",
                frame, filter_subframe, length, update);
        for (kind = 0; kind < 3; ++kind) {
            fprintf(stderr, "EFRFILTER frame=%u subframe=%u kind=%s values=",
                    frame, filter_subframe, kinds[kind]);
            for (i = 0; i < widths[kind]; ++i)
                fprintf(stderr, "%04x", (unsigned)(unsigned short)vectors[kind][i]);
            fputc('\n', stderr);
        }
        ++filter_subframe;
    }
    __real_Syn_filt(coefficients, input, output, length, memory, update);
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
    fprintf(stderr, "\nEFREXCCALL frame=%u subframe=%u ctrl=%04x length=%d\n", frame,
            code_subframe, (unsigned)(unsigned short)ctrl, length);
    excitation_words("FIXED", code_subframe, code);
    __real_d_gain_code(index, code, length, gain, bfi, state, prev_bf, ctrl, subframe, dtx_state);
    if (!bfi)
        fprintf(stderr, "\nEFRPOLICY frame=%u subframe=%u prev_bf=%04x previous_gain=%04x value=%04x\n", frame,
                code_subframe, (unsigned)(unsigned short)prev_bf,
                (unsigned)(unsigned short)previous_gain, (unsigned)(unsigned short)*gain);
    fprintf(stderr, "\nEFRGAIN frame=%u kind=code subframe=%u value=%04x\n", frame,
            code_subframe++, (unsigned)(unsigned short)*gain);
}
