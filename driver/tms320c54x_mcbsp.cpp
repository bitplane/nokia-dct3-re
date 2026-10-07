// license:BSD-3-Clause
// copyright-holders:Gaz
#include "emu.h"
#include "tms320c54x_mcbsp.h"

DEFINE_DEVICE_TYPE(TMS320C54X_MCBSP, tms320c54x_mcbsp_device, "tms54mcbsp", "TMS320C54x McBSP (partial)")

tms320c54x_mcbsp_device::tms320c54x_mcbsp_device(machine_config const &config, char const *tag, device_t *owner, u32 clock)
	: device_t(config, TMS320C54X_MCBSP, tag, owner, clock), m_tx_word_cb(*this), m_tx_bit_cb(*this), m_tx_irq_cb(*this)
{
}

void tms320c54x_mcbsp_device::device_start()
{
	if (!clock()) fatalerror("McBSP requires a CPU source clock");
	m_bit_timer = timer_alloc(FUNC(tms320c54x_mcbsp_device::bit_tick), this);
	save_item(NAME(m_regs)); save_item(NAME(m_index));
	save_item(NAME(m_buffer)); save_item(NAME(m_shift));
	save_item(NAME(m_bits)); save_item(NAME(m_delay));
	save_item(NAME(m_buffer_full)); save_item(NAME(m_shift_active)); save_item(NAME(m_ready));
}

void tms320c54x_mcbsp_device::device_reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	m_index = 0;
	m_buffer = m_shift = 0;
	m_bits = m_delay = 0;
	m_buffer_full = m_shift_active = m_ready = false;
	m_bit_timer->adjust(attotime::never);
}

u16 tms320c54x_mcbsp_device::control_r(offs_t offset)
{
	if (!offset) return m_index;
	if (m_index == 0) return m_regs[0] & ~u16(6); // No receive word or overrun is synthesized.
	if (m_index == 1)
		return (m_regs[1] & ~u16(6)) | (m_ready ? 2 : 0) | (m_shift_active ? 4 : 0);
	return m_regs[m_index];
}

void tms320c54x_mcbsp_device::control_w(offs_t offset, u16 value)
{
	if (!offset) { m_index = value & 31; return; }
	if (m_index == 1)
	{
		bool const enabled = BIT(m_regs[1], 0);
		m_regs[1] = value & 0x03f9; // XRDY/XEMPTY are hardware-owned; reserved bits read zero.
		if (!BIT(value, 0))
		{
			m_ready = m_buffer_full = m_shift_active = false;
			m_bits = m_delay = 0;
			m_bit_timer->adjust(attotime::never);
		}
		else if (!enabled)
			set_ready(true); // SPRU302B 2.3.2.2: empty DXR becomes ready on XRST release.
	}
	else if (m_index == 0)
		m_regs[0] = value & 0xf8b9;
	else
		m_regs[m_index] = value;
	arm_clock();
}

bool tms320c54x_mcbsp_device::internal_clock() const
{
	return BIT(m_regs[1], 6) && BIT(m_regs[7], 13) && BIT(m_regs[14], 9);
}

attotime tms320c54x_mcbsp_device::bit_period() const
{
	// CLKSM=1 selects CPU/2; CLKGDV divides it by the programmed value plus one.
	return attotime::from_ticks(2 * ((m_regs[6] & 0xff) + 1), clock());
}

void tms320c54x_mcbsp_device::arm_clock()
{
	if (!internal_clock() || !BIT(m_regs[1], 0) || (!m_buffer_full && !m_shift_active))
		m_bit_timer->adjust(attotime::never);
	else if (!m_bit_timer->enabled() || m_bit_timer->remaining() == attotime::never)
		m_bit_timer->adjust(bit_period());
}

void tms320c54x_mcbsp_device::set_ready(bool ready)
{
	bool const rising = ready && !m_ready;
	m_ready = ready;
	if (rising && !(m_regs[1] & 0x0030))
	{
		m_tx_irq_cb(ASSERT_LINE);
		m_tx_irq_cb(CLEAR_LINE);
	}
}

u16 tms320c54x_mcbsp_device::data_r(offs_t offset)
{
	// Receiver and wider-word data registers are outside this transmitter subset.
	return 0;
}

void tms320c54x_mcbsp_device::data_w(offs_t offset, u16 value)
{
	if (offset == 2)
	{
		if (BIT(m_regs[1], 0) && ((m_regs[4] >> 5) & 7) > 2)
			fatalerror("McBSP wider-than-16-bit transmit is not implemented");
		return;
	}
	if (offset != 3) return;
	if (!BIT(m_regs[1], 0)) return; // Writes while transmitter is held in reset do not start it.
	if (BIT(m_regs[5], 15) || (m_regs[4] & 0x7f00) || BIT(m_regs[7], 12) ||
		((m_regs[4] >> 5) & 7) > 2 || (m_regs[5] & 0x001c) ||
		(m_regs[5] & 3) == 3 || (m_regs[1] & 0x0030) || BIT(m_regs[0], 15))
		fatalerror("McBSP unsupported active transmit format xcr=%04x/%04x srgr2=%04x", m_regs[4], m_regs[5], m_regs[7]);
	if (m_buffer_full) fatalerror("McBSP DXR overwritten before ready");
	m_buffer = value;
	m_buffer_full = true;
	set_ready(false);
	arm_clock();
}

TIMER_CALLBACK_MEMBER(tms320c54x_mcbsp_device::bit_tick)
{
	if (!internal_clock() || !BIT(m_regs[1], 0)) return;
	if (!m_shift_active && m_buffer_full)
	{
		unsigned const widths[] = {8, 12, 16};
		m_bits = widths[(m_regs[4] >> 5) & 7];
		m_shift = m_buffer & ((1U << m_bits) - 1);
		m_delay = m_regs[5] & 3;
		m_shift_active = true;
		m_buffer_full = false;
		set_ready(true);
	}
	if (m_shift_active)
	{
		if (m_delay) --m_delay;
		else
		{
			m_tx_bit_cb(BIT(m_shift, m_bits - 1));
			if (!--m_bits)
			{
				m_shift_active = false;
				m_tx_word_cb(m_shift);
			}
		}
	}
	if (m_buffer_full || m_shift_active) m_bit_timer->adjust(bit_period());
}
