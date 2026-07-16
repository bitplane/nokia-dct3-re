// license:BSD-3-Clause
#include "emu.h"
#include "nokia_ccont.h"

DEFINE_DEVICE_TYPE(NOKIA_CCONT, nokia_ccont_device, "nokia_ccont", "Nokia CCONT power-management ASIC")

namespace {
constexpr uint8_t CMD_READ = 0x04;
constexpr unsigned CMD_ADDR_SHIFT = 3;
constexpr uint8_t ADC_CTRL = 0x00;
constexpr uint8_t ADC_LSB = 0x02;
constexpr uint8_t ADC_MSB = 0x03;
constexpr uint8_t WATCHDOG = 0x05;
constexpr uint8_t IRQ_STATUS = 0x0e;
constexpr uint8_t IRQ_MASK = 0x0f;
constexpr uint8_t IRQ_SOURCE_MASK = 0xf8;
}

nokia_ccont_device::nokia_ccont_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, NOKIA_CCONT, tag, owner, clock),
	m_irq_cb(*this),
	m_power_cb(*this)
{
}

void nokia_ccont_device::device_start()
{
	save_item(NAME(m_cmd));
	save_item(NAME(m_watchdog));
	save_item(NAME(m_regs));
	save_item(NAME(m_adc_source));
	save_item(NAME(m_boot_status));
	save_item(NAME(m_data_cycle));
	save_item(NAME(m_present));
}

void nokia_ccont_device::device_reset()
{
	m_cmd = 0;
	m_watchdog = 0;
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	m_data_cycle = false;
	m_irq_cb(0);
	m_power_cb(1);
}

void nokia_ccont_device::device_post_load()
{
	update_irq();
}

void nokia_ccont_device::set_adc_source(unsigned channel, uint16_t value)
{
	if (channel < std::size(m_adc_source))
		m_adc_source[channel] = value & 0x3ff;
}

void nokia_ccont_device::select_w(int selected)
{
	if (selected)
		m_data_cycle = false;
}

void nokia_ccont_device::set_boot_status(uint8_t status)
{
	m_boot_status = status;
	m_regs[IRQ_STATUS] = (m_regs[IRQ_STATUS] & IRQ_SOURCE_MASK) | status;
	update_irq();
}

void nokia_ccont_device::latch_irq_sources(uint8_t sources)
{
	m_regs[IRQ_STATUS] |= sources & IRQ_SOURCE_MASK;
	update_irq();
}

void nokia_ccont_device::update_irq()
{
	m_irq_cb((m_regs[IRQ_STATUS] & ~m_regs[IRQ_MASK] & IRQ_SOURCE_MASK) != 0);
}

void nokia_ccont_device::serial_w(uint8_t data)
{
	if (!m_data_cycle)
	{
		m_cmd = data;
	}
	else
	{
		const uint8_t address = (m_cmd >> CMD_ADDR_SHIFT) & 0x0f;
		switch (address)
		{
		case ADC_CTRL:
		{
			const uint16_t value = m_adc_source[(data >> 4) & 0x07];
			m_regs[address] = data;
			m_regs[ADC_LSB] = value & 0xff;
			m_regs[ADC_MSB] = (value >> 8) & 0x03;
			break;
		}
		case WATCHDOG:
			if (data == 0x00)
				m_power_cb(0);
			else if (data == 0x20)
				m_regs[address] = data;
			else if (data == 0x31)
				m_watchdog = m_regs[address];
			else if (data == 0x3f)
				m_watchdog = 0;
			break;
		case IRQ_STATUS:
			m_regs[address] &= ~data;
			update_irq();
			break;
		default:
			m_regs[address] = data;
			if (address == IRQ_MASK)
				update_irq();
			break;
		}
	}
	m_data_cycle = !m_data_cycle;
}

uint8_t nokia_ccont_device::serial_r()
{
	const uint8_t address = (m_cmd >> CMD_ADDR_SHIFT) & 0x0f;
	uint8_t data = m_regs[address];
	if ((m_cmd & CMD_READ) != 0)
	{
		system_time systime;
		machine().current_datetime(systime);
		switch (address)
		{
		case ADC_MSB: data = 0xb0 | (data & 0x03); break;
		case 0x07: data = systime.local_time.second; break;
		case 0x08: data = systime.local_time.minute; break;
		case 0x09: data = systime.local_time.hour; break;
		case 0x0a: data = systime.local_time.mday; break;
		case IRQ_STATUS: if (m_present) data |= 0x01; break;
		}
	}
	m_data_cycle = !m_data_cycle;
	return data;
}

bool nokia_ccont_device::watchdog_tick()
{
	if (m_watchdog == 0)
		return false;
	return --m_watchdog == 0;
}
