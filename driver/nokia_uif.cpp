// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#include "emu.h"
#include "nokia_uif.h"

DEFINE_DEVICE_TYPE(NOKIA_UIF, nokia_uif_device, "nokia_uif", "Nokia MAD2 UIF GPIO controller")

nokia_uif_device::nokia_uif_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_UIF, tag, owner, clock)
{
}

bool nokia_uif_device::owns(offs_t offset)
{
	const offs_t bank = offset & 0xc0;
	return (offset & 0x3c) == 0x30 &&
			(bank == 0x00 || bank == 0x40 || bank == 0x80 || bank == 0xc0);
}

void nokia_uif_device::device_start()
{
	save_item(NAME(m_regs));
}

void nokia_uif_device::device_reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
}

u8 nokia_uif_device::read(offs_t offset) const
{
	return owns(offset) ? m_regs[offset] : 0;
}

void nokia_uif_device::write(offs_t offset, u8 data)
{
	if (owns(offset))
		m_regs[offset] = data;
}
