// license:BSD-3-Clause
// copyright-holders:Gaz
#include "emu.h"
#include "tms320c54x_mcbsp.h"

DEFINE_DEVICE_TYPE(TMS320C54X_MCBSP, tms320c54x_mcbsp_device, "tms54mcbsp", "TMS320C54x McBSP (partial)")

tms320c54x_mcbsp_device::tms320c54x_mcbsp_device(machine_config const &config, char const *tag, device_t *owner, u32 clock)
	: device_t(config, TMS320C54X_MCBSP, tag, owner, clock), m_tx_word_cb(*this), m_tx_bit_cb(*this), m_tx_irq_cb(*this), m_tx_event_cb(*this), m_rx_irq_cb(*this), m_rx_event_cb(*this)
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
	save_item(NAME(m_clock_input)); save_item(NAME(m_frame_input)); save_item(NAME(m_ready_pending));
	save_item(NAME(m_async_bit)); save_item(NAME(m_frame_words)); save_item(NAME(m_async_time));
	save_item(NAME(m_rx_shift)); save_item(NAME(m_rx_completed)); save_item(NAME(m_rx_buffer));
	save_item(NAME(m_rx_word)); save_item(NAME(m_rx_high)); save_item(NAME(m_rx_frame_words));
	save_item(NAME(m_rx_bits)); save_item(NAME(m_rx_delay)); save_item(NAME(m_rx_buffer_width));
	save_item(NAME(m_rx_clock_input)); save_item(NAME(m_rx_frame_input)); save_item(NAME(m_rx_data_input));
	save_item(NAME(m_rx_frame_seen)); save_item(NAME(m_rx_shift_full)); save_item(NAME(m_rx_buffer_full)); save_item(NAME(m_rx_ready));
}

void tms320c54x_mcbsp_device::device_reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	m_index = 0;
	m_buffer = m_shift = 0;
	m_bits = m_delay = 0;
	m_buffer_full = m_shift_active = m_ready = false;
	m_clock_input = m_frame_input = m_ready_pending = m_async_bit = false;
	m_frame_words = 0; m_async_time = attotime::zero;
	m_bit_timer->adjust(attotime::never);
	m_tx_event_cb(CLEAR_LINE);
	m_rx_clock_input = m_rx_frame_input = m_rx_data_input = false;
	m_rx_word = m_rx_high = 0;
	reset_receiver();
}

u16 tms320c54x_mcbsp_device::control_r(offs_t offset)
{
	if (!offset) return m_index;
	if (m_index == 0) return (m_regs[0] & ~u16(6)) | (m_rx_ready ? 2 : 0);
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
			set_ready(false);
			m_buffer_full = m_shift_active = false;
			m_ready_pending = m_async_bit = false; m_frame_words = 0;
			m_bits = m_delay = 0;
			m_bit_timer->adjust(attotime::never);
		}
		else if (!enabled)
			set_ready(true); // SPRU302B 2.3.2.2: empty DXR becomes ready on XRST release.
	}
	else if (m_index == 0)
	{
		bool const enabled = BIT(m_regs[0], 0);
		if (BIT(value, 0) && ((value ^ m_regs[0]) & 0xe030) && (m_rx_frame_words || m_rx_shift_full || m_rx_buffer_full))
			fatalerror("McBSP active receive control reconfiguration is not implemented");
		m_regs[0] = value & 0xf8f9;
		if (!BIT(value, 0)) reset_receiver();
		else if (!enabled)
			m_rx_frame_seen = m_rx_frame_input != BIT(m_regs[14], 2); // Require a new sampled frame transition after release.
	}
	else
	{
		if ((m_index == 2 || m_index == 3) && value != m_regs[m_index] && (m_rx_frame_words || m_rx_shift_full || m_rx_buffer_full))
			fatalerror("McBSP active receive format reconfiguration is not implemented");
		m_regs[m_index] = value;
	}
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
	bool const changed = ready != m_ready;
	m_ready = ready;
	if (changed) m_tx_event_cb(ready ? ASSERT_LINE : CLEAR_LINE);
	if (rising && !(m_regs[1] & 0x0030))
	{
		m_tx_irq_cb(ASSERT_LINE);
		m_tx_irq_cb(CLEAR_LINE);
	}
}

u16 tms320c54x_mcbsp_device::data_r(offs_t offset)
{
	if (offset == 0) return m_rx_high;
	if (offset == 1)
	{
		if (!machine().side_effects_disabled()) set_rx_ready(false);
		return m_rx_word;
	}
	return 0;
}

void tms320c54x_mcbsp_device::reset_receiver()
{
	m_rx_shift = m_rx_completed = m_rx_buffer = 0;
	m_rx_frame_words = m_rx_bits = m_rx_delay = m_rx_buffer_width = 0;
	m_rx_frame_seen = m_rx_shift_full = m_rx_buffer_full = false;
	set_rx_ready(false);
}

void tms320c54x_mcbsp_device::set_rx_ready(bool ready)
{
	bool const rising = ready && !m_rx_ready;
	if (ready != m_rx_ready) { m_rx_ready = ready; m_rx_event_cb(ready); }
	if (rising && !(m_regs[0] & 0x30)) { m_rx_irq_cb(ASSERT_LINE); m_rx_irq_cb(CLEAR_LINE); }
}

void tms320c54x_mcbsp_device::begin_receive()
{
	unsigned const width = (m_regs[2] >> 5) & 7;
	if (width > 2 || BIT(m_regs[3], 15) || (m_regs[3] & 0x18) || (m_regs[3] & 3) == 3 ||
		(m_regs[0] & 0x9070) || ((m_regs[0] >> 13) & 3) == 3)
		fatalerror("McBSP unsupported active receive format rcr=%04x/%04x spcr1=%04x", m_regs[2], m_regs[3], m_regs[0]);
	static constexpr unsigned widths[] = {8, 12, 16};
	m_rx_frame_words = ((m_regs[2] >> 8) & 0x7f) + 1;
	m_rx_bits = widths[width]; m_rx_shift = 0; m_rx_delay = m_regs[3] & 3;
}

void tms320c54x_mcbsp_device::publish_receive()
{
	unsigned const justify = (m_regs[0] >> 13) & 3;
	m_rx_word = m_rx_buffer; m_rx_high = 0;
	if (justify == 1 && BIT(m_rx_buffer, m_rx_buffer_width - 1))
	{
		m_rx_word |= u16(u32(0xffff) << m_rx_buffer_width); m_rx_high = 0xffff;
	}
	else if (justify == 2) m_rx_word <<= 16 - m_rx_buffer_width;
	m_rx_buffer_full = false;
	set_rx_ready(true);
}

void tms320c54x_mcbsp_device::rx_clock_w(int state)
{
	bool const changed = bool(state) != m_rx_clock_input;
	m_rx_clock_input = bool(state);
	if (!changed || !BIT(m_regs[0], 0) || BIT(m_regs[14], 8)) return;
	bool const sample_edge = bool(state) == BIT(m_regs[14], 0);
	if (!sample_edge)
	{
		// SPRU302B 2.3.5.1: RSR -> RBR on the edge opposite sampling.
		if (m_rx_shift_full)
		{
			if (m_rx_buffer_full) fatalerror("McBSP receive overrun recovery is not implemented");
			m_rx_buffer = m_rx_completed;
			m_rx_buffer_width = (m_regs[2] >> 5 & 7) == 0 ? 8 : (m_regs[2] >> 5 & 7) == 1 ? 12 : 16;
			m_rx_buffer_full = true; m_rx_shift_full = false;
		}
		return;
	}
	// RBR -> DRR and RRDY become visible on the following sampling edge.
	if (m_rx_buffer_full && !m_rx_ready) publish_receive();
	bool const active = m_rx_frame_input != BIT(m_regs[14], 2);
	bool const new_frame = active && !m_rx_frame_seen;
	m_rx_frame_seen = active;
	if (new_frame && !BIT(m_regs[14], 10))
	{
		if (!m_rx_frame_words) begin_receive();
		else if (!BIT(m_regs[3], 2)) fatalerror("McBSP receive unexpected-frame recovery is not implemented");
	}
	if (!m_rx_frame_words) return;
	if (m_rx_delay) { --m_rx_delay; return; }
	m_rx_shift = (m_rx_shift << 1) | m_rx_data_input;
	if (!--m_rx_bits)
	{
		m_rx_completed = m_rx_shift; m_rx_shift_full = true;
		--m_rx_frame_words; m_rx_shift = 0;
		unsigned const widths[] = {8, 12, 16};
		unsigned const width = (m_regs[2] >> 5) & 7;
		if (width > 2) fatalerror("McBSP receive width changed during a frame");
		m_rx_bits = widths[width];
	}
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
	bool const external = !BIT(m_regs[14], 9) && !BIT(m_regs[14], 11);
	if (BIT(m_regs[5], 15) || (!external && ((m_regs[4] & 0x7f00) || BIT(m_regs[7], 12) || BIT(m_regs[5], 2))) ||
		((m_regs[4] >> 5) & 7) > 2 || (m_regs[5] & 0x0018) ||
		(m_regs[5] & 3) == 3 || (m_regs[1] & 0x0030) || (m_regs[0] & 0x8040))
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
		load_shift(false, true);
	if (m_shift_active)
	{
		if (m_delay) --m_delay;
		else
		{
			shift_bit();
		}
	}
	if (m_buffer_full || m_shift_active) m_bit_timer->adjust(bit_period());
}

void tms320c54x_mcbsp_device::load_shift(bool external, bool first)
{
	unsigned const widths[] = {8, 12, 16};
	unsigned const width = (m_regs[4] >> 5) & 7;
	if (width > 2) fatalerror("McBSP wider transmit configuration changed while buffered");
	m_bits = widths[width];
	m_shift = m_buffer & ((1U << m_bits) - 1);
	m_delay = first ? m_regs[5] & 3 : 0;
	m_shift_active = true; m_buffer_full = false;
	if (external) m_ready_pending = true; // XRDY follows the opposite internal clock edge.
	else set_ready(true);
}

void tms320c54x_mcbsp_device::shift_bit()
{
	m_tx_bit_cb(BIT(m_shift, m_bits - 1));
	if (!--m_bits)
	{
		m_shift_active = false;
		if (m_frame_words) --m_frame_words;
		m_tx_word_cb(m_shift);
	}
}

void tms320c54x_mcbsp_device::tx_frame_w(int state)
{
	bool const changed = bool(state) != m_frame_input;
	m_frame_input = bool(state);
	if (!changed || bool(state) == BIT(m_regs[14], 3) || !BIT(m_regs[1], 0) || BIT(m_regs[14], 11)) return;
	if (BIT(m_regs[14], 9)) fatalerror("McBSP external frame with internal clock is not implemented");
	if (m_frame_words)
	{
		if (BIT(m_regs[5], 2)) return;
		fatalerror("McBSP unexpected frame recovery is not implemented");
	}
	if (!m_buffer_full) fatalerror("McBSP framed transmit underrun is not implemented");
	m_frame_words = ((m_regs[4] >> 8) & 0x7f) + 1;
	load_shift(true, true);
	// SPRU302B: zero-delay first bit is asynchronous to the bit clock.
	if (!m_delay)
	{
		shift_bit(); m_async_bit = true; m_async_time = machine().time();
	}
}

void tms320c54x_mcbsp_device::tx_clock_w(int state)
{
	bool const changed = bool(state) != m_clock_input;
	m_clock_input = bool(state);
	if (!changed || BIT(m_regs[14], 9) || !BIT(m_regs[1], 0)) return;
	bool const shift_edge = bool(state) != BIT(m_regs[14], 1);
	if (!shift_edge)
	{
		if (m_ready_pending) { m_ready_pending = false; set_ready(true); }
		return;
	}
	if (m_async_bit)
	{
		m_async_bit = false;
		if (m_async_time == machine().time()) return; // Same physical edge must not shift twice.
	}
	if (!m_frame_words) return;
	if (!m_shift_active)
	{
		if (!m_buffer_full) fatalerror("McBSP framed transmit underrun is not implemented");
		load_shift(true, false);
	}
	if (m_delay && --m_delay) return;
	shift_bit();
}
