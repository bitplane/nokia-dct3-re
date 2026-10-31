// license:BSD-3-Clause
// copyright-holders:Gaz
// Copied firmware context programs; generic ISA tests use sections/runner.cpp.
#include "emu.h"
#include "tms320c54x_test_state.h"
namespace {
using c54x_test::tms320c54x_test_state;
ROM_START(tms54test)
	ROM_SYSTEM_BIOS(0,"rom4-context","Acquired ROM4 context integration")
	ROM_REGION(1,"unused",ROMREGION_ERASE00)
ROM_END
}
SYST(2026,tms54test,0,0,test,0,tms320c54x_test_state,empty_init,
	"Test","TMS320C54x acquired firmware context tests",MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING)
