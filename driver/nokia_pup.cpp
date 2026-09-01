// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz
#include "emu.h"
#include "nokia_pup.h"

DEFINE_DEVICE_TYPE(NOKIA_PUP, nokia_pup_device, "nokia_pup", "Nokia MAD2 PUP output controller")

namespace {
constexpr offs_t CONTROL = 0x15;
constexpr offs_t VIBRATOR_CONTROL = 0x1b;
constexpr offs_t BUZZER_DIVIDER_MSB = 0x1c;
constexpr offs_t BUZZER_DIVIDER_LSB = 0x1d;
constexpr offs_t BUZZER_VOLUME = 0x1e;
constexpr offs_t GENIO_SIGNAL = 0x20;
constexpr offs_t GENIO_LATCH = 0x22;
constexpr offs_t GENIO_DIRECTION = 0x24;
constexpr u8 VIBRATOR_MODE_MASK = 0x60;
constexpr u8 VIBRATOR_PARAMETER_MASK = 0x1f;
}

nokia_pup_device::nokia_pup_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_PUP, tag, owner, clock),
	m_eeprom_sda_read_cb(*this, 1),
	m_eeprom_sda_write_cb(*this),
	m_eeprom_scl_write_cb(*this),
	m_buzzer_clock_cb(*this),
	m_buzzer_enable_cb(*this),
	m_vibrator_enable_cb(*this)
{
}

bool nokia_pup_device::owns(offs_t offset)
{
	return offset == CONTROL || (offset >= VIBRATOR_CONTROL && offset <= BUZZER_VOLUME) ||
		offset == GENIO_SIGNAL || offset == GENIO_LATCH || offset == GENIO_DIRECTION;
}

void nokia_pup_device::device_start()
{
	save_item(NAME(m_regs));
	machine().save().register_postload(save_prepost_delegate(FUNC(nokia_pup_device::post_load), this));
}

void nokia_pup_device::device_reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	post_load();
}

void nokia_pup_device::post_load()
{
	update_genio();
	update_buzzer();
	update_vibrator();
}

void nokia_pup_device::update_genio()
{
	const u8 signal = m_regs[GENIO_SIGNAL];
	const u8 direction = m_regs[GENIO_DIRECTION];
	m_eeprom_sda_write_cb(BIT(direction, 0) ? BIT(signal, 0) : 1);
	m_eeprom_scl_write_cb(BIT(signal, m_eeprom_scl_bit));
}

void nokia_pup_device::update_buzzer()
{
	const u16 divider = (u16(m_regs[BUZZER_DIVIDER_MSB]) << 8) | m_regs[BUZZER_DIVIDER_LSB];
	const u32 frequency = divider ? 13'000'000 / divider : 0;
	const bool enabled = BIT(m_regs[CONTROL], 5) && divider != 0;
	if (frequency)
		m_buzzer_clock_cb(frequency);
	m_buzzer_enable_cb(enabled);
	if (m_trace)
		logerror("buzzer: enabled=%u divider=%u frequency=%u volume=%u t=%.6f\n",
				enabled, divider, frequency, m_regs[BUZZER_VOLUME], machine().time().as_double());
}

void nokia_pup_device::update_vibrator()
{
	const bool enabled = BIT(m_regs[CONTROL], 4);
	m_vibrator_enable_cb(enabled);
	if (m_trace)
		logerror("vibrator: enabled=%u control=%02x mode=%02x parameter=%02x t=%.6f\n",
				enabled, m_regs[VIBRATOR_CONTROL],
				m_regs[VIBRATOR_CONTROL] & VIBRATOR_MODE_MASK,
				m_regs[VIBRATOR_CONTROL] & VIBRATOR_PARAMETER_MASK,
				machine().time().as_double());
}

u8 nokia_pup_device::peek(offs_t offset) const
{
	return owns(offset) ? m_regs[offset] : 0;
}

u8 nokia_pup_device::read(offs_t offset)
{
	u8 data = peek(offset);
	if (offset == GENIO_SIGNAL)
	{
		const int sda = m_eeprom_sda_read_cb();
		data = (data & 0xfe) | (BIT(m_regs[GENIO_DIRECTION], 0) ? (BIT(data, 0) & sda) : sda);
	}
	return data;
}

void nokia_pup_device::write(offs_t offset, u8 data)
{
	if (!owns(offset))
		return;
	m_regs[offset] = data;
	if (offset == GENIO_SIGNAL || offset == GENIO_DIRECTION)
		update_genio();
	if (offset == CONTROL || (offset >= BUZZER_DIVIDER_MSB && offset <= BUZZER_VOLUME))
		update_buzzer();
	if (offset == CONTROL || offset == VIBRATOR_CONTROL)
		update_vibrator();
}
