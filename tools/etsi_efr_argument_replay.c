/* license:BSD-3-Clause
 * copyright-holders:Gaz
 * Argument-only calculator linked to separately acquired reference routines.
 * No decoder state or reference implementation is included here; the mixing
 * mode composes the original fixed-point primitives on supplied arguments.
 */
#include <stdio.h>
#include <string.h>

extern void Syn_filt(short *, short *, short *, short, short *, short);
extern void Pred_lt_6(short *, short, short, short);
extern void agc2(short *, short *, short);
extern int L_mult(short, short);
extern int L_mac(int, short, short);
extern int L_shl(int, short);
extern short rounded_word(int) __asm__("round");
typedef char word16_requires_two_bytes[sizeof(short) == 2 ? 1 : -1];
typedef char word32_requires_four_bytes[sizeof(int) == 4 ? 1 : -1];

static int read_word(FILE *input, short *value)
{
    int low = fgetc(input), high;
    if (low == EOF) return feof(input) ? 0 : -1;
    high = fgetc(input);
    if (high == EOF) return -1;
    *value = (short)(low | (high << 8));
    return 1;
}

static int write_words(FILE *output, const short *values, unsigned count)
{
    unsigned i;
    for (i = 0; i < count; ++i) {
        unsigned value = (unsigned short)values[i];
        if (fputc(value & 255, output) == EOF || fputc(value >> 8, output) == EOF) return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    FILE *input, *output;
    short arguments[196], result[40];
    unsigned count = 0, i, width;
    int status, mode = 0;
    if (argc == 4) {
        if (strcmp(argv[1], "--predictor") == 0) mode = 1;
        else if (strcmp(argv[1], "--mix") == 0) mode = 2;
        else if (strcmp(argv[1], "--agc") == 0) mode = 3;
        else return 2;
    } else if (argc != 3) return 2;
    width = mode == 1 ? 156 : mode == 2 ? 82 : mode == 3 ? 80 : 61;
    input = fopen(argv[1 + (mode != 0)], "rb");
    if (!input) return 2;
    output = fopen(argv[2 + (mode != 0)], "wb");
    if (!output) { fclose(input); return 2; }
    while ((status = read_word(input, arguments)) == 1) {
        for (i = 1; i < width; ++i)
            if (read_word(input, arguments + i) != 1) { status = -1; break; }
        if (status == -1) break;
        if (mode == 1) {
            if (arguments[0] < 17 || arguments[0] > 143 || arguments[1] < -2 || arguments[1] > 3) {
                status = -1;
                break;
            }
            for (i = 156; i < 196; ++i) arguments[i] = 0;
            Pred_lt_6(arguments + 156, arguments[0], arguments[1], 40);
            if (!write_words(output, arguments + 156, 40)) { status = -1; break; }
        } else if (mode == 2) {
            for (i = 0; i < 40; ++i) {
                int value = L_mult(arguments[2 + i], arguments[0]);
                value = L_mac(value, arguments[42 + i], arguments[1]);
                result[i] = rounded_word(L_shl(value, 3));
            }
            if (!write_words(output, result, 40)) { status = -1; break; }
        } else if (mode == 3) {
            agc2(arguments, arguments + 40, 40);
            if (!write_words(output, arguments + 40, 40)) { status = -1; break; }
        } else {
            Syn_filt(arguments, arguments + 11, result, 40, arguments + 51, 1);
            if (!write_words(output, result, 40) || !write_words(output, arguments + 51, 10)) {
                status = -1;
                break;
            }
        }
        ++count;
    }
    if (fclose(input) != 0) status = -1;
    if (fclose(output) != 0) status = -1;
    if (status < 0 || !count) return 1;
    printf("reference argument calls=%u mode=%s\n", count,
            mode == 1 ? "predictor" : mode == 2 ? "mix" : mode == 3 ? "agc" : "filter");
    return 0;
}
