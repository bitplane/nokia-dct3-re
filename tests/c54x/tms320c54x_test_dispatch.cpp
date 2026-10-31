// license:BSD-3-Clause
// copyright-holders:Gaz
#include "emu.h"
#include "tms320c54x_test_state.h"
namespace c54x_test {
TIMER_CALLBACK_MEMBER(tms320c54x_test_state::check_results)
{
	if(m_result_owner){expect((this->*m_result_owner)(),"Selected firmware fixture owns its continuation");return;}
	if(check_rom4_fr_decoder_results() || check_rom4_codec_is_results() || check_rom4_crc_input_results())return;
	throw emu_fatalerror("Unowned firmware fixture phase %u",m_phase);
}
}
