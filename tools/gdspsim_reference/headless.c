/* Repository-owned adapter, not an implementation of DSP instructions. */
#include <stdio.h>
#include <stdlib.h>
#include "core_def.h"
#include "c54_core.h"
#include "memory.h"
#include "pipeline.h"

struct _Registers *Registers;
gboolean bound_program_mem_read = TRUE;
gboolean bound_data_mem_read = TRUE;
gboolean add_data_mem_on_write = TRUE;
gboolean add_data_mem_on_read = FALSE;
int gStopRun;
int decode_follow_pref = 1;
struct _fileIO;

void set_mem_changed(WordA a, int t) { (void)a; (void)t; }
void fill_reg_entries(struct _Registers *r) { (void)r; }
void unhighlight_pipeline(void) {}
void highlight_pipeline(WordA a) { (void)a; }
void update_pipeline(WordA a) { (void)a; }
void update_all_memory_windows(int h) { (void)h; }
void fileIO_process(struct _fileIO *io)
{
    (void)io;
    fputs("Unsupported file-I/O execution in reference fixture\n", stderr);
    abort();
}

int main(int argc, char **argv)
{
    int i, changes = 0, failed = 0;
    Word before;
    int status_fixture = argc > 1;
    (void)argv;
    Registers = pipe_new();
    fill_to_mem(0xf495, 0x100, 0x100, PROGRAM_MEM_TYPE);
    Registers->PC = 0x100;
    if (status_fixture) {
        write_program_mem(0x100, 0xf6bd);
        write_program_mem(0x108, 0xf7bd);
    }
    for (i = 0; i < 32; ++i) {
        before = MMR->ST1;
        pipeline(Registers);
        if (MMR->ST1 != before) {
            ++changes;
            printf("status change: cycle=%d PC=%04x ST1=%04x\n",
                   i + 1, Registers->PC, MMR->ST1);
            if (!status_fixture || changes > 2 ||
                MMR->ST1 != (changes == 1 ? 0x0900 : 0x2900))
                failed = 1;
        }
    }
    printf("reference %s: cycles=32 PC=%04x ST1=%04x changes=%d\n",
           status_fixture ? "XF" : "NOP", Registers->PC, MMR->ST1, changes);
    return failed || gStopRun || MMR->ST1 != 0x2900 ||
           Registers->PC != 0x120 || changes != (status_fixture ? 2 : 0);
}
