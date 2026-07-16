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
constexpr uint8_t RTC_SECOND = 0x07;
constexpr uint8_t RTC_MINUTE = 0x08;
constexpr uint8_t RTC_HOUR = 0x09;
constexpr uint8_t RTC_DAY = 0x0a;
constexpr uint8_t RTC_ALARM_MINUTE = 0x0b;
constexpr uint8_t RTC_ALARM_HOUR = 0x0c;
constexpr uint8_t IRQ_STATUS = 0x0e;
constexpr uint8_t IRQ_MASK = 0x0f;
constexpr uint8_t RESET_READY = 0x01;
constexpr uint8_t RESET_PWRONX = 0x02;
constexpr uint8_t IRQ_SOURCE_MASK = 0xf8;
constexpr uint8_t IRQ_RTC_SECOND = 0x10;
constexpr uint8_t IRQ_RTC_MINUTE = 0x20;
constexpr uint8_t IRQ_RTC_ALARM = 0x80;
}

nokia_ccont_device::nokia_ccont_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, NOKIA_CCONT, tag, owner, clock),
	m_irq_cb(*this),
	m_power_cb(*this)
{
}

void nokia_ccont_device::device_start()
{
	m_rtc_timer = timer_alloc(FUNC(nokia_ccont_device::rtc_tick), this);
	save_item(NAME(m_cmd));
	save_item(NAME(m_watchdog));
	save_item(NAME(m_regs));
	save_item(NAME(m_adc_source));
	save_item(NAME(m_data_cycle));
	save_item(NAME(m_wddisx_grounded));
	save_item(NAME(m_rtc_alarm_armed));
}

void nokia_ccont_device::device_reset()
{
	m_cmd = 0;
	m_watchdog = 0;
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	// Ordinary cold start exposes the persistent CCONT-ready bit together with
	// the PWRONX reset cause. Both 3210 ROMs require this 0x03 reset value.
	m_regs[IRQ_STATUS] = (m_ready ? RESET_READY : 0) | RESET_PWRONX;
	// Use a fixed epoch rather than the host clock.  The firmware consumes these
	// registers as binary counters, and deterministic reset state keeps frames
	// and save states reproducible across hosts.
	m_regs[RTC_SECOND] = 0;
	m_regs[RTC_MINUTE] = 0;
	m_regs[RTC_HOUR] = 12;
	m_regs[RTC_DAY] = 1;
	m_rtc_alarm_armed = false;
	m_data_cycle = false;
	m_rtc_timer->adjust(attotime::from_seconds(1), 0, attotime::from_seconds(1));
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

void nokia_ccont_device::latch_irq_sources(uint8_t sources)
{
	m_regs[IRQ_STATUS] |= sources & IRQ_SOURCE_MASK;
	update_irq();
}

void nokia_ccont_device::update_irq()
{
	m_irq_cb((m_regs[IRQ_STATUS] & ~m_regs[IRQ_MASK] & IRQ_SOURCE_MASK) != 0);
}

void nokia_ccont_device::advance_rtc()
{
	latch_irq_sources(IRQ_RTC_SECOND);
	if (++m_regs[RTC_SECOND] < 60)
		return;

	m_regs[RTC_SECOND] = 0;
	latch_irq_sources(IRQ_RTC_MINUTE);
	if (++m_regs[RTC_MINUTE] >= 60)
	{
		m_regs[RTC_MINUTE] = 0;
		if (++m_regs[RTC_HOUR] >= 24)
		{
			m_regs[RTC_HOUR] = 0;
			// The recovered interface exposes only a day counter here.  Month
			// rollover remains outside the established CCONT contract.
			if (++m_regs[RTC_DAY] == 0 || m_regs[RTC_DAY] > 31)
				m_regs[RTC_DAY] = 1;
		}
	}

	// ROM IRQ dispatch identifies bit 7 as the alarm source, and the recovered
	// alarm helper programs the minute/hour pair without a separate enable bit.
	if (m_rtc_alarm_armed &&
		m_regs[RTC_MINUTE] == m_regs[RTC_ALARM_MINUTE] &&
		m_regs[RTC_HOUR] == m_regs[RTC_ALARM_HOUR])
	{
		latch_irq_sources(IRQ_RTC_ALARM);
		m_rtc_alarm_armed = false;
	}
}

TIMER_CALLBACK_MEMBER(nokia_ccont_device::rtc_tick)
{
	advance_rtc();
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
			else
			{
				m_regs[address] = data;
				// CCONT has an eight-bit, one-second down-counter. Every
				// non-zero write reloads it directly; watchdog disable is the
				// separate board-level WDDISX input, not a register command.
				m_watchdog = data;
			}
			break;
		case RTC_ALARM_MINUTE:
		case RTC_ALARM_HOUR:
			m_regs[address] = data;
			m_rtc_alarm_armed = true;
			break;
		case IRQ_STATUS:
			// Ready bit 0 is persistent device status. PWRONX bit 1 is a
			// clearable reset cause, and the upper bits are IRQ-source latches.
			m_regs[address] = (m_regs[address] & RESET_READY) |
				((m_regs[address] & ~RESET_READY) & ~data);
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
		switch (address)
		{
		case ADC_MSB: data = 0xb0 | (data & 0x03); break;
		}
	}
	m_data_cycle = !m_data_cycle;
	return data;
}

bool nokia_ccont_device::watchdog_tick()
{
	if (m_wddisx_grounded || m_watchdog == 0)
		return false;
	return --m_watchdog == 0;
}
