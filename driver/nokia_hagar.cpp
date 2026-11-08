// license:BSD-3-Clause
// copyright-holders:Gaz
#include "emu.h"
#include "emuopts.h"
#include "nokia_hagar.h"

DEFINE_DEVICE_TYPE(NOKIA_HAGAR, nokia_hagar_device, "nokia_hagar", "Nokia HAGAR partial command model")

nokia_hagar_device::nokia_hagar_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock)
	: device_t(mconfig, NOKIA_HAGAR, tag, owner, clock)
{
}

void nokia_hagar_device::device_start()
{
	save_item(NAME(m_state.route));
	save_item(NAME(m_state.pll));
	save_item(NAME(m_state.configuration));
	save_item(NAME(m_state.seen));
	save_item(NAME(m_state.last_word));
	save_item(NAME(m_transactions));
}

void nokia_hagar_device::device_reset()
{
	m_state.reset();
	m_transactions = 0;
}

void nokia_hagar_device::compare_output_w(u64 tuple)
{
	u16 const control = tuple >> 48;
	u32 const word = (u32(u16(tuple >> 16)) << 16) | u16(tuple >> 32);
	m_state.write(control, word);
	++m_transactions;
	if ((m_trace || machine().options().verbose()) && m_transactions <= 65536)
	{
		u16 key = 0;
		u64 frequency = 0;
		bool const candidate = rx_candidate(key, frequency);
		machine().logerror("hagar_command: sequence=%llu control=%04x word=%08x seen=%u route=%04x pll=%04x configuration=%04x candidate=%u key=%u frequency=%llu rf_ready=0 t=%.9f\n",
			m_transactions, control, word, m_state.seen, m_state.route, m_state.pll,
			m_state.configuration, candidate, key, frequency, machine().time().as_double());
	}
	else if ((m_trace || machine().options().verbose()) && m_transactions == 65537)
		machine().logerror("hagar_command: overflow=1\n");
}
