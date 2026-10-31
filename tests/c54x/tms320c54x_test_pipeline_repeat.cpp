// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::set_repeat_count_source(u16 address, u16 value)
{
	if (address >= 0x60) { auto const disable = machine().disable_side_effects(); m_cpu->space(AS_DATA).write_word(address, value); }
	else if (address < 14)
	{
		unsigned const part = (address - 8) % 3; auto const state = address < 11 ? tms320c54x_device::STATE_A : tms320c54x_device::STATE_B;
		u64 const mask = part == 2 ? 0xff : 0xffff;
		m_cpu->set_state_int(state, (m_cpu->state_int(state) & ~(mask << (16 * part))) | ((u64(value) & mask) << (16 * part)));
	}
	else m_cpu->set_state_int(address == 14 ? tms320c54x_device::STATE_T : address == 15 ? tms320c54x_device::STATE_TRN : address < 24 ? tms320c54x_device::STATE_AR0 + address - 16 : address == 24 ? tms320c54x_device::STATE_SP : tms320c54x_device::STATE_BK, value);
}


} // namespace c54x_test
