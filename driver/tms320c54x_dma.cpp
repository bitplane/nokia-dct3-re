// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_dma.h"

DEFINE_DEVICE_TYPE(TMS320C54X_DMA, tms320c54x_dma_device, "tms320c54x_dma", "TMS320C54x enhanced DMA subset")

tms320c54x_dma_device::tms320c54x_dma_device(machine_config const &config, char const *tag, device_t *owner, u32 clock)
	: device_t(config, TMS320C54X_DMA, tag, owner, clock), m_cpu(*this, finder_base::DUMMY_TAG), m_completion_cb(*this)
{
}

void tms320c54x_dma_device::device_start()
{
	if (!clock()) fatalerror("C54x DMA requires a clock");
	for (auto &timer : m_timers) timer = timer_alloc(FUNC(tms320c54x_dma_device::transfer), this);
	save_item(NAME(m_control));
	save_item(NAME(m_index));
	save_item(NAME(m_regs));
	save_item(NAME(m_frame_elements));
	save_item(NAME(m_sync_levels));
}

void tms320c54x_dma_device::device_reset()
{
	m_control = m_index = 0;
	m_sync_levels = 0;
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	std::fill(std::begin(m_frame_elements), std::end(m_frame_elements), 0);
	for (auto *timer : m_timers) timer->adjust(attotime::never);
}

u16 tms320c54x_dma_device::read(offs_t offset)
{
	if (offset == 0) return m_control;
	if (offset == 1) return m_index;
	if (offset != 2 && offset != 3) return 0xffff;
	u16 const value = valid_index(m_index) ? m_regs[m_index] : 0;
	if (offset == 2 && !machine().side_effects_disabled()) ++m_index;
	return value;
}

void tms320c54x_dma_device::write(offs_t offset, u16 value)
{
	if (offset == 0)
	{
		u16 const previous = m_control;
		m_control = value;
		for (unsigned channel = 0; channel < 6; ++channel)
		{
			if (!BIT(value, channel)) m_timers[channel]->adjust(attotime::never);
			else if (!BIT(previous, channel)) enable(channel);
		}
	}
	else if (offset == 1) m_index = value;
	else if (offset == 2 || offset == 3)
	{
		if (valid_index(m_index))
		{
			if (m_index == 0x1e || m_index == 0x1f) value &= 0x7f;
			else if (m_index < 0x1e && m_index % 5 == 4) value &= 0xf7df;
			else if (m_index < 0x1e && m_index % 5 == 3) value &= 0xf8ff;
			else if (m_index == 0x27 || (m_index >= 0x2a && (m_index - 0x2a) % 4 == 3)) value &= 0xff;
			m_regs[m_index] = value;
			if (m_index < 0x1e && m_index % 5 == 2) m_frame_elements[m_index / 5] = value;
		}
		else if (m_per_channel_reload && m_index == 0x3e && value)
			fatalerror("C54x DMA channel-enable extension is not implemented");
		if (offset == 2) ++m_index;
	}
}

void tms320c54x_dma_device::enable(unsigned channel)
{
	validate(channel);
	m_frame_elements[channel] = m_regs[channel * 5 + 2];
	// Synchronized channels remain enabled but transfer nothing without their event.
	unsigned const event = m_regs[channel * 5 + 3] >> 12;
	if (!event || BIT(m_sync_levels, event))
		m_timers[channel]->adjust(attotime::from_ticks(2, clock()), channel);
}

void tms320c54x_dma_device::validate(unsigned channel) const
{
	unsigned const base = channel * 5;
	u16 const mode = m_regs[base + 4];
	// ABU and double-word elements require different count/address mechanics.
	if (BIT(m_regs[base + 3], 11) || BIT(mode, 12) || ((mode >> 8) & 7) == 7 ||
		((mode >> 2) & 7) == 7 || ((mode >> 6) & 3) == 3 || (mode & 3) == 3)
		fatalerror("C54x DMA unsupported active mode channel=%u mode=%04x sync=%04x source=%04x destination=%04x count=%04x control=%04x globals=%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x",
			channel, mode, m_regs[base + 3], m_regs[base], m_regs[base + 1], m_regs[base + 2], m_control,
			m_regs[0x1e], m_regs[0x1f], m_regs[0x20], m_regs[0x21], m_regs[0x22],
			m_regs[0x23], m_regs[0x24], m_regs[0x25], m_regs[0x26], m_regs[0x27]);
}

void tms320c54x_dma_device::sync_event(unsigned event)
{
	if (!event || event > 15) fatalerror("C54x DMA invalid synchronization event %u", event);
	for (unsigned channel = 0; channel < 6; ++channel)
		if (BIT(m_control, channel) && (m_regs[channel * 5 + 3] >> 12) == event)
		{
			if (m_timers[channel]->enabled() && m_timers[channel]->remaining() != attotime::never)
				fatalerror("C54x DMA overlapping synchronization/arbitration is not implemented");
			// Two-clock bus cadence is an assumption, not measured DA150 arbitration.
			m_timers[channel]->adjust(attotime::from_ticks(2, clock()), channel);
		}
}

void tms320c54x_dma_device::sync_w(unsigned event, int state)
{
	if (!event || event > 15) fatalerror("C54x DMA invalid synchronization line %u", event);
	bool const previous = BIT(m_sync_levels, event);
	if (state) m_sync_levels |= u16(1 << event);
	else m_sync_levels &= ~u16(1 << event);
	if (state && !previous) sync_event(event);
}

s16 tms320c54x_dma_device::address_step(unsigned index, bool frame_end) const
{
	if (!index) return 0;
	if (index == 1) return 1;
	if (index == 2) return -1;
	unsigned const pair = (index - 3) & 1;
	// Sorting mode substitutes the frame index on the final element.
	return s16(m_regs[(index >= 5 && frame_end ? 0x22 : 0x20) + pair]);
}

TIMER_CALLBACK_MEMBER(tms320c54x_dma_device::transfer)
{
	if (!BIT(m_control, param)) return;
	validate(param);
	unsigned const base = param * 5;
	u16 const mode = m_regs[base + 4];
	unsigned const source_space = (mode >> 6) & 3, destination_space = mode & 3;
	int const spaces[] = {AS_PROGRAM, AS_DATA, AS_IO};
	u32 const source = m_regs[base] | (source_space == 0 ? u32(m_regs[0x1e]) << 16 : 0);
	u32 const destination = m_regs[base + 1] | (destination_space == 0 ? u32(m_regs[0x1f]) << 16 : 0);
	u16 const value = m_cpu->space(spaces[source_space]).read_word(source);
	m_cpu->space(spaces[destination_space]).write_word(destination, value);
	unsigned const source_index = (mode >> 8) & 7, destination_index = (mode >> 2) & 7;
	bool const frame_end = !m_regs[base + 2];
	bool const block_end = frame_end && !(m_regs[base + 3] & 0xff);
	m_regs[base] += address_step(source_index, frame_end);
	m_regs[base + 1] += address_step(destination_index, frame_end);
	// Page registers are common and fixed: a low-address carry wraps within the page.
	if (block_end)
	{
		if (BIT(mode, 15))
		{
			unsigned const reload = m_per_channel_reload && param ? 0x26 + param * 4 : 0x24;
			m_regs[base] = m_regs[reload]; m_regs[base + 1] = m_regs[reload + 1];
			m_regs[base + 2] = m_frame_elements[param] = m_regs[reload + 2];
			m_regs[base + 3] = (m_regs[base + 3] & 0xff00) | m_regs[reload + 3];
		}
		else m_control &= ~u16(1 << param);
	}
	else if (frame_end)
	{
		--m_regs[base + 3];
		m_regs[base + 2] = m_frame_elements[param];
	}
	else
		--m_regs[base + 2];
	if (BIT(mode, 14) && (block_end || (frame_end && BIT(mode, 13)))) m_completion_cb(u8(param));
	if (BIT(m_control, param) && !(m_regs[base + 3] >> 12))
		m_timers[param]->adjust(attotime::from_ticks(2, clock()), param);
}
