// license:BSD-3-Clause
#include "emu.h"
#include "nokia_gensio.h"

DEFINE_DEVICE_TYPE(NOKIA_GENSIO, nokia_gensio_device, "nokia_gensio", "Nokia MAD2 GENSIO controller")

namespace {
constexpr offs_t CCONT_WRITE = 0x2c;
constexpr offs_t CONTROL = 0x2d;
constexpr offs_t LCD_DATA = 0x2e;
constexpr offs_t CCONT_READ = 0x6c;
constexpr offs_t STATUS = 0x6d;
constexpr offs_t LCD_COMMAND = 0x6e;
constexpr u8 STATUS_IDLE_TX_READY = 0x03;
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

bool nokia_gensio_device::owns(offs_t offset)
{
	return (offset >= CCONT_WRITE && offset <= LCD_DATA) ||
		(offset >= CCONT_READ && offset <= 0x6f) ||
		(offset >= 0xad && offset <= 0xaf) ||
		(offset >= 0xed && offset <= 0xef);
}

void nokia_gensio_device::device_start()
{
	save_item(NAME(m_regs));
	save_item(NAME(m_status));
}

void nokia_gensio_device::device_reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	m_status = STATUS_IDLE_TX_READY;
	m_ccont_select_cb(0);
	m_lcd_dc_cb(1);
	m_lcd_sdin_cb(0);
	m_lcd_sclk_cb(1);
}

u8 nokia_gensio_device::peek(offs_t offset) const
{
	if (offset == STATUS)
		return m_status;
	return offset < std::size(m_regs) ? m_regs[offset] : 0;
}

u8 nokia_gensio_device::read(offs_t offset)
{
	if (offset == CCONT_READ)
	{
		const u8 data = m_ccont_read_cb();
		m_status &= ~STATUS_RX_READY;
		return data;
	}
	if (offset == STATUS)
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
	switch (offset)
	{
	case CONTROL:
		// Both recovered 3210 ROMs treat selection as a new serial transaction.
		m_status = STATUS_IDLE_TX_READY;
		m_ccont_select_cb(BIT(data, 2));
		break;
	case CCONT_WRITE:
		m_ccont_write_cb(data);
		if (BIT(m_regs[CONTROL], 2))
			m_status |= STATUS_RX_READY;
		break;
	case LCD_DATA:
		write_lcd(data, true);
		break;
	case LCD_COMMAND:
		write_lcd(data, false);
		break;
	}
}
