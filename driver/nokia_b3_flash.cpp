// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#include "emu.h"

#include "nokia_b3_flash.h"

#include "machine/intelfsh.h"

DEFINE_DEVICE_TYPE(NOKIA_B3_FLASH, nokia_b3_flash_device, "nokia_b3_flash", "Nokia DCT3 B3 flash adapter")

nokia_b3_flash_device::nokia_b3_flash_device(const machine_config &mconfig,
		const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_B3_FLASH, tag, owner, clock),
	m_flash(*this, "^flash")
{
}

void nokia_b3_flash_device::device_start()
{
	m_erase_timer = timer_alloc(FUNC(nokia_b3_flash_device::erase_complete), this);

	save_item(NAME(m_lock_command));
	save_item(NAME(m_program_data));
	save_item(NAME(m_erase_confirm));
	save_item(NAME(m_erase_active));
	save_item(NAME(m_erase_suspended));
	save_item(NAME(m_status_override));
	save_item(NAME(m_erase_remaining_us));
}

void nokia_b3_flash_device::device_reset()
{
	m_lock_command = false;
	m_program_data = false;
	m_erase_confirm = false;
	m_erase_active = false;
	m_erase_suspended = false;
	m_status_override = false;
	m_erase_remaining_us = 0;
	m_erase_timer->adjust(attotime::never);
}

TIMER_CALLBACK_MEMBER(nokia_b3_flash_device::erase_complete)
{
	m_erase_active = false;
	m_erase_suspended = false;
	m_erase_remaining_us = 0;
}

u16 nokia_b3_flash_device::read(offs_t offset, u16 mem_mask)
{
	if (!m_enabled || !m_status_override)
		return m_flash->read(offset) & mem_mask;

	if (offset != m_status_csr)
	{
		// B3 partitions remain array-readable while another partition is busy.
		const u8 *const array = m_flash->base();
		return ((u16(array[offset * 2]) << 8) | array[offset * 2 + 1]) & mem_mask;
	}

	if (m_erase_suspended)
		return 0x00c0 & mem_mask; // ready + erase suspended
	return (m_erase_active ? 0x0000 : 0x0080) & mem_mask;
}

void nokia_b3_flash_device::write(offs_t offset, u16 data, u16 mem_mask)
{
	if (!m_enabled)
	{
		m_flash->write(offset, data);
		return;
	}

	const u8 command = data & 0xff;
	if (m_program_data)
	{
		// The word following 40 is array data even when its low byte resembles
		// another command opcode.
		m_program_data = false;
	}
	else if (m_lock_command)
	{
		m_lock_command = false;
		if (command == 0x01 || command == 0xd0 || command == 0x2f)
		{
			m_flash->write(offset, 0x70);
			return;
		}
	}
	else if (command == 0x60)
	{
		m_lock_command = true;
		return;
	}
	else if (m_erase_active && m_erase_suspended && command == 0xd0)
	{
		m_erase_suspended = false;
		m_status_override = true;
		m_erase_timer->adjust(attotime::from_usec(std::max<u64>(1, m_erase_remaining_us)));
		return;
	}
	else if (m_erase_active && command == 0xb0)
	{
		m_erase_remaining_us = std::max<u64>(1,
				m_erase_timer->remaining().as_ticks(1'000'000));
		m_erase_timer->adjust(attotime::never);
		m_erase_suspended = true;
		m_status_override = true;
		return;
	}
	else if (command == 0x20)
	{
		m_erase_confirm = true;
	}
	else if (command == 0x40)
	{
		m_program_data = true;
	}
	else if (m_erase_confirm)
	{
		m_erase_confirm = false;
		if (command == 0xd0)
		{
			m_erase_active = true;
			m_erase_suspended = false;
			m_status_override = true;
			// Approximate only: firmware polls ready, but no physical erase-time
			// measurement is available for the M28W320ECT.
			m_erase_remaining_us = 1'000'000;
			m_erase_timer->adjust(attotime::from_seconds(1));
		}
	}

	if (command == 0xff || command == 0xf0)
		m_status_override = false;
	m_flash->write(offset, data);
}
