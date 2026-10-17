/* license:BSD-3-Clause
 * copyright-holders:Gaz
 * Call the separately acquired reference's SID classifier; no codec code here.
 */
#include <stdio.h>
extern short sid_frame_detection(short *);
typedef char word16_requires_two_bytes[sizeof(short) == 2 ? 1 : -1];

int main(int argc, char **argv)
{
    FILE *input;
    short frame[247];
    unsigned index = 0;
    size_t count;
    if (argc != 2 || !(input = fopen(argv[1], "rb"))) return 1;
    while ((count = fread(frame, sizeof(short), 247, input)) != 0)
    {
        if (count != 247) { fclose(input); return 1; }
        printf("EFRSID frame=%u class=%d\n", index++, (int)sid_frame_detection(frame + 1));
    }
    if (ferror(input)) { fclose(input); return 1; }
    fclose(input);
    return 0;
}
