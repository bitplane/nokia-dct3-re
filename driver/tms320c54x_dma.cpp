// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_dma.h"

DEFINE_DEVICE_TYPE(TMS320C54X_DMA, tms320c54x_dma_device, "tms320c54x_dma", "TMS320C54x enhanced DMA subset")

tms320c54x_dma_device::tms320c54x_dma_device(machine_config const &config, char const *tag, device_t *owner, u32 clock)
	: device_t(config, TMS320C54X_DMA, tag, owner, clock), m_cpu(*this, finder_base::DUMMY_TAG)
{
}

void tms320c54x_dma_device::device_start()
{
	if (!clock()) fatalerror("C54x DMA requires a clock");
	for (auto &timer : m_timers) timer = timer_alloc(FUNC(tms320c54x_dma_device::transfer), this);
	save_item(NAME(m_control));
	save_item(NAME(m_index));
	save_item(NAME(m_regs));
}

void tms320c54x_dma_device::device_reset()
{
	m_control = m_index = 0;
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	for (auto *timer : m_timers) timer->adjust(attotime::never);
}

u16 tms320c54x_dma_device::read(offs_t offset)
{
	if (offset == 0) return m_control;
	if (offset == 1) return m_index;
	if (offset != 2 && offset != 3) return 0xffff;
	u16 const value = m_index < std::size(m_regs) ? m_regs[m_index] : 0;
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
		if (m_index < std::size(m_regs))
		{
			if (m_index == 0x1e || m_index == 0x1f) value &= 0x7f;
			else if (m_index < 0x1e && m_index % 5 == 4) value &= 0xf7df;
			m_regs[m_index] = value;
		}
		if (offset == 2) ++m_index;
	}
}

void tms320c54x_dma_device::enable(unsigned channel)
{
	unsigned const base = channel * 5;
	u16 const mode = m_regs[base + 4];
	// Do not silently complete unimplemented serial sync, ABU, reload or interrupt modes.
	if (m_regs[base + 3] || (mode & ~u16(0x03df)) || ((mode >> 8) & 7) > 2 ||
		((mode >> 2) & 7) > 2 || ((mode >> 6) & 3) == 3 || (mode & 3) == 3)
		fatalerror("C54x DMA unsupported active mode channel=%u mode=%04x sync=%04x source=%04x destination=%04x count=%04x control=%04x globals=%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x,%04x",
			channel, mode, m_regs[base + 3], m_regs[base], m_regs[base + 1], m_regs[base + 2], m_control,
			m_regs[0x1e], m_regs[0x1f], m_regs[0x20], m_regs[0x21], m_regs[0x22],
			m_regs[0x23], m_regs[0x24], m_regs[0x25], m_regs[0x26], m_regs[0x27]);
	// Two-clock transfer cadence is a model assumption, not recovered DA150 arbitration timing.
	m_timers[channel]->adjust(attotime::from_ticks(2, clock()), channel);
}

TIMER_CALLBACK_MEMBER(tms320c54x_dma_device::transfer)
{
	unsigned const base = param * 5;
	u16 const mode = m_regs[base + 4];
	unsigned const source_space = (mode >> 6) & 3, destination_space = mode & 3;
	int const spaces[] = {AS_PROGRAM, AS_DATA, AS_IO};
	u32 const source = m_regs[base] | (source_space == 0 ? u32(m_regs[0x1e]) << 16 : 0);
	u32 const destination = m_regs[base + 1] | (destination_space == 0 ? u32(m_regs[0x1f]) << 16 : 0);
	u16 const value = m_cpu->space(spaces[source_space]).read_word(source);
	m_cpu->space(spaces[destination_space]).write_word(destination, value);
	unsigned const source_index = (mode >> 8) & 7, destination_index = (mode >> 2) & 7;
	m_regs[base] += source_index == 1 ? 1 : source_index == 2 ? -1 : 0;
	m_regs[base + 1] += destination_index == 1 ? 1 : destination_index == 2 ? -1 : 0;
	// Page registers are common and fixed: a low-address carry wraps within the page.
	if (!m_regs[base + 2]) m_control &= ~u16(1 << param);
	else
	{
		--m_regs[base + 2];
		m_timers[param]->adjust(attotime::from_ticks(2, clock()), param);
	}
}
