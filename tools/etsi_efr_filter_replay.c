/* license:BSD-3-Clause
 * copyright-holders:Gaz
 * Argument-only calculator linked to a separately acquired reference filter.
 * No codec implementation or decoder state is included here.
 */
#include <stdio.h>

extern void Syn_filt(short *, short *, short *, short, short *, short);
typedef char word16_requires_two_bytes[sizeof(short) == 2 ? 1 : -1];

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
    short arguments[61], result[40];
    unsigned count = 0, i;
    int status;
    if (argc != 3) return 2;
    input = fopen(argv[1], "rb");
    if (!input) return 2;
    output = fopen(argv[2], "wb");
    if (!output) { fclose(input); return 2; }
    while ((status = read_word(input, arguments)) == 1) {
        for (i = 1; i < 61; ++i)
            if (read_word(input, arguments + i) != 1) { status = -1; break; }
        if (status == -1) break;
        Syn_filt(arguments, arguments + 11, result, 40, arguments + 51, 1);
        if (!write_words(output, result, 40) || !write_words(output, arguments + 51, 10)) {
            status = -1;
            break;
        }
        ++count;
    }
    if (fclose(input) != 0) status = -1;
    if (fclose(output) != 0) status = -1;
    if (status < 0 || !count) return 1;
    printf("reference filter calls=%u\n", count);
    return 0;
}
