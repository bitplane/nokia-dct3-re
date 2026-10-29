// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"
#include "tms320c54x_test_disassembly.h"

namespace c54x_test {

void tms320c54x_test_state::check_control_disassembler()
{
	check_disassembly(m_cpu->get_disassembler(),
		[this](bool valid, char const *contract) { expect(valid, contract); },
		[](char const *, char const *format, auto... values) { osd_printf_info(format, values...); });
}

} // namespace c54x_test
