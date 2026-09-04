// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz
#include "emu.h"
#include "nokia_gensio.h"

DEFINE_DEVICE_TYPE(NOKIA_GENSIO, nokia_gensio_device, "nokia_gensio", "Nokia MAD2 GENSIO controller")

namespace {
constexpr u8 STATUS_RX_READY = 0x04;
}

nokia_gensio_device::nokia_gensio_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_GENSIO, tag, owner, clock),
	m_ccont_read_cb(*this, 0),
	m_ccont_write_cb(*this),
	m_ccont_select_cb(*this),
	m_lcd_dc_cb(*this),
	m_lcd_sdin_cb(*this),
	m_lcd_sclk_cb(*this)
{
}

bool nokia_gensio_device::owns(offs_t offset) const
{
	return offset == m_wiring.ccont_write || offset == m_wiring.control ||
			offset == m_wiring.lcd_data || offset == m_wiring.ccont_read ||
			offset == m_wiring.status || offset == m_wiring.lcd_command;
}

void nokia_gensio_device::device_start()
{
	save_item(NAME(m_regs));
	save_item(NAME(m_status));
}

void nokia_gensio_device::device_reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	m_status = m_wiring.idle_status;
	m_ccont_select_cb(0);
	m_lcd_dc_cb(1);
	m_lcd_sdin_cb(0);
	m_lcd_sclk_cb(1);
}

u8 nokia_gensio_device::peek(offs_t offset) const
{
	if (offset == m_wiring.status)
		return m_status;
	return offset < std::size(m_regs) ? m_regs[offset] : 0;
}

u8 nokia_gensio_device::read(offs_t offset)
{
	if (offset == m_wiring.ccont_read)
	{
		const u8 data = m_ccont_read_cb();
		m_status &= ~STATUS_RX_READY;
		return data;
	}
	if (offset == m_wiring.status)
		return m_status;
	return peek(offset);
}

void nokia_gensio_device::write_lcd(u8 data, bool is_data)
{
	m_lcd_dc_cb(is_data);
	for (int bit = 7; bit >= 0; bit--)
	{
		m_lcd_sclk_cb(0);
		m_lcd_sdin_cb(BIT(data, bit));
		m_lcd_sclk_cb(1);
	}
	m_lcd_dc_cb(1);
}

void nokia_gensio_device::write(offs_t offset, u8 data)
{
	if (!owns(offset))
		return;

	m_regs[offset] = data;
	if (offset == m_wiring.control)
	{
		// Both recovered 3210 ROMs treat selection as a new serial transaction.
		m_status = m_wiring.idle_status;
		m_ccont_select_cb(BIT(data, 2));
	}
	else if (offset == m_wiring.ccont_write)
	{
		m_ccont_write_cb(data);
		if (m_wiring.ccont_rx_ready_on_write || BIT(m_regs[m_wiring.control], 2))
			m_status |= STATUS_RX_READY;
	}
	else if (offset == m_wiring.lcd_data)
		write_lcd(data, true);
	else if (offset == m_wiring.lcd_command)
		write_lcd(data, false);
}
