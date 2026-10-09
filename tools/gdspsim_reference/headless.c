/* Repository-owned adapter, not an implementation of DSP instructions. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    int status_fixture = argc > 1 && !strcmp(argv[1], "xf");
    int interrupt_fixture = argc > 1 && !strcmp(argv[1], "intr");
    int mac_fixture = argc > 1 && (!strcmp(argv[1], "mac") || !strcmp(argv[1], "mac-sat"));
    int saturated = mac_fixture && !strcmp(argv[1], "mac-sat");
    Registers = pipe_new();
    fill_to_mem(0xf495, 0x100, 0x100, PROGRAM_MEM_TYPE);
    Registers->PC = 0x100;
    if (status_fixture) {
        write_program_mem(0x100, 0xf6bd);
        write_program_mem(0x108, 0xf7bd);
    }
    if (interrupt_fixture) {
        fill_to_mem(0xf495, 0x2000, 0x100, PROGRAM_MEM_TYPE);
        fill_to_mem(0, 0x0f00, 0x100, DATA_MEM_TYPE);
        MMR->SP = 0x1000;
        MMR->PMST = 0x2000;
        MMR->ST1 = 0x2100;
        MMR->IMR = 0;
        MMR->IFR = 0xffff;
        write_program_mem(0x100, 0xf7d0);
    }
    if (mac_fixture) {
        fill_to_mem(0, 0x200, 1, DATA_MEM_TYPE);
        write_data_mem(0x200, 1);
        MMR->ar3 = 0x200;
        MMR->T = 1;
        MMR->A = (GP_Reg){0xff, 0xff, 0xff, 0x7f, 0};
        MMR->ST0 = 0x0800;
        MMR->ST1 = saturated ? 0x0200 : 0;
        write_program_mem(0x100, 0x2883);
    }
    for (i = 0; i < 32; ++i) {
        before = MMR->ST1;
        pipeline(Registers);
        if (MMR->ST1 != before) {
            ++changes;
            printf("status change: cycle=%d PC=%04x ST1=%04x\n",
                   i + 1, Registers->PC, MMR->ST1);
            if (!interrupt_fixture && !mac_fixture && (!status_fixture || changes > 2 ||
                MMR->ST1 != (changes == 1 ? 0x0900 : 0x2900))
                )
                failed = 1;
        }
    }
    if (mac_fixture) {
        guint64 result = GP_REG_2_UINT64(MMR->A) & 0xffffffffffULL;
        printf("reference MAC: OVM=%d A=%010llx ST0=%04x ST1=%04x T=%04x AR3=%04x\n",
               saturated, (unsigned long long)result, MMR->ST0, MMR->ST1, MMR->T, MMR->ar3);
        if (saturated)
            puts("reference limitation: OVM MAC A=0080000000 (expected 007fffffff)");
        return failed || gStopRun || result != 0x80000000ULL ||
               MMR->ST0 != 0x0c00 || MMR->ST1 != (saturated ? 0x0200 : 0) ||
               MMR->T != 1 || MMR->ar3 != 0x200;
    }
    if (interrupt_fixture) {
        int wait = 0;
        Word saved = read_data_mem(0x0fff, &wait);
        printf("reference INTR16: cycles=32 PC=%04x ST1=%04x SP=%04x saved=%04x IFR=%04x IMR=%04x\n",
               Registers->PC, MMR->ST1, MMR->SP, saved, MMR->IFR, MMR->IMR);
        /* Reproduce reference defects, not hardware-correct INTR behavior. */
        puts("reference limitation: saved=0102 (expected 0101), IFR=ffff (expected fffe)");
        return failed || gStopRun || MMR->ST1 != 0x2900 ||
               MMR->SP != 0x0fff || saved != 0x0102 || MMR->IFR != 0xffff ||
               Registers->PC < 0x2040 || Registers->PC >= 0x2100;
    }
    printf("reference %s: cycles=32 PC=%04x ST1=%04x changes=%d\n",
           status_fixture ? "XF" : "NOP", Registers->PC, MMR->ST1, changes);
    return failed || gStopRun || MMR->ST1 != 0x2900 ||
           Registers->PC != 0x120 || changes != (status_fixture ? 2 : 0);
}
