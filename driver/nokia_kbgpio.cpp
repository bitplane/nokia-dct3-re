// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz
#include "emu.h"
#include "nokia_kbgpio.h"

DEFINE_DEVICE_TYPE(NOKIA_KBGPIO, nokia_kbgpio_device, "nokia_kbgpio", "Nokia MAD2 keyboard GPIO controller")

namespace {
constexpr offs_t ROW_SIGNAL = 0x28;
constexpr offs_t COLUMN_INPUT = 0x2a;
constexpr offs_t COLUMN_IRQ_MASK = 0x6b;
constexpr offs_t ROW_DIRECTION = 0xa8;
}

nokia_kbgpio_device::nokia_kbgpio_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_KBGPIO, tag, owner, clock),
	m_matrix_cb(*this, 0xff),
	m_power_cb(*this, 0xff),
	m_irq_cb(*this)
{
}

void nokia_kbgpio_device::set_wiring_contract(wiring_contract contract)
{
	if (!contract.valid())
		fatalerror("KBGPIO: invalid wiring rows=%u power-column-mask=%02x",
				contract.rows, contract.power_on_column_mask);
	m_wiring = contract;
}

bool nokia_kbgpio_device::owns(offs_t offset)
{
	return (offset >= 0x28 && offset <= 0x2b) ||
		(offset >= 0x68 && offset <= 0x6b) ||
		(offset >= 0xa8 && offset <= 0xab);
}

void nokia_kbgpio_device::device_start()
{
	save_item(NAME(m_regs));
	save_item(NAME(m_columns));
	save_item(NAME(m_power_on));
	save_item(NAME(m_irq_latched));
	machine().save().register_postload(save_prepost_delegate(FUNC(nokia_kbgpio_device::update_irq), this));
}

void nokia_kbgpio_device::device_reset()
{
	std::fill(std::begin(m_regs), std::end(m_regs), 0);
	m_columns = 0x1f;
	m_power_on = ~m_wiring.power_on_column_mask;
	m_irq_latched = false;
	update_irq();
}

u8 nokia_kbgpio_device::sample_columns(bool consume_power_on)
{
	u8 data = 0x1f;
	const bool five_rows = m_wiring.rows == 5;
	const u8 row_mask = five_rows ? 0x1f : 0x0f;
	const u8 rows_low = m_regs[ROW_DIRECTION] & ~m_regs[ROW_SIGNAL] & row_mask;
	const unsigned row_count = m_wiring.rows;

	for (unsigned column = 0; column < 5; column++)
	{
		const u8 keys = m_matrix_cb[column]();
		for (unsigned row = 0; row < row_count; row++)
		{
			const unsigned key_bit = five_rows ? row : row + 1;
			if (BIT(rows_low, row) && !BIT(keys, key_bit))
				data &= ~(u8(1) << column);
		}
	}

	if (!BIT(m_power_cb(), 0))
		data &= 0xfe;
	if (m_power_on != 0xff)
	{
		data &= m_power_on;
		if (consume_power_on)
			m_power_on = 0xff;
	}
	return data | 0xe0;
}

void nokia_kbgpio_device::update_irq()
{
	m_irq_cb(m_irq_latched);
}

void nokia_kbgpio_device::update_columns()
{
	const u8 columns = sample_columns(false) & 0x1f;
	// Firmware masks all five columns while changing row drive, then restores
	// the idle mask. Physical press and release both reach IRQ0, so this is a
	// masked change detector rather than a falling-edge-only input. Host input
	// callbacks must not bypass the hardware mask.
	const u8 changed = (m_columns ^ columns) & ~m_regs[COLUMN_IRQ_MASK] & 0x1f;
	m_columns = columns;
	if (changed)
	{
		m_irq_latched = true;
		update_irq();
	}
}

u8 nokia_kbgpio_device::peek(offs_t offset) const
{
	return owns(offset) ? m_regs[offset] : 0;
}

u8 nokia_kbgpio_device::read(offs_t offset)
{
	if (offset == COLUMN_INPUT)
		return sample_columns(true);
	return peek(offset);
}

void nokia_kbgpio_device::write(offs_t offset, u8 data)
{
	if (!owns(offset))
		return;
	m_regs[offset] = data;
	if (offset == ROW_SIGNAL || offset == COLUMN_IRQ_MASK || offset == ROW_DIRECTION)
		update_columns();
}

void nokia_kbgpio_device::input_changed()
{
	update_columns();
}

void nokia_kbgpio_device::irq_acknowledge()
{
	m_irq_latched = false;
	update_irq();
}
