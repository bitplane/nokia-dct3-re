/* Load-only control: no guessed GTI ABI and no target execution claim. */
#include <stdio.h>
#include <windows.h>

int main(int argc, char **argv)
{
    static const char *exports[] = {
        "GTI_INIT", "GTI_INIT_EX", "GTI_GET_SIM_MODE", "GTI_SET_SIM_MODE",
        "GTI_GET_PIPELINE_INFO", "GTI_READREG", "GTI_STEP", "GTI_QUIT"
    };
    HMODULE driver;
    unsigned int i;
    int missing = 0;
    if (argc != 2) {
        fprintf(stderr, "usage: driver_load_probe DRIVER_PATH\n");
        return 2;
    }
    driver = LoadLibraryA(argv[1]);
    if (!driver) {
        fprintf(stderr, "driver load failed: win32_error=%lu\n", GetLastError());
        return 1;
    }
    for (i = 0; i < sizeof(exports) / sizeof(exports[0]); ++i) {
        int found = GetProcAddress(driver, exports[i]) != NULL;
        printf("export=%s present=%d\n", exports[i], found);
        missing |= !found;
    }
    FreeLibrary(driver);
    printf("driver_load=%s target_initialized=0 instructions_executed=0\n",
        missing ? "FAIL" : "PASS");
    return missing ? 1 : 0;
}
