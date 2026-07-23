// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz
#ifndef MAME_NOKIA_NOKIA_KBGPIO_H
#define MAME_NOKIA_NOKIA_KBGPIO_H

class nokia_kbgpio_device : public device_t
{
public:
	nokia_kbgpio_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	auto matrix_cb(unsigned column) { return m_matrix_cb[column].bind(); }
	auto power_cb() { return m_power_cb.bind(); }
	auto irq_cb() { return m_irq_cb.bind(); }

	void set_five_rows(bool enabled) { m_five_rows = enabled; }
	void set_power_on_column_mask(u8 mask) { m_power_on_column_mask = mask; }

	static bool owns(offs_t offset);
	u8 read(offs_t offset);
	u8 peek(offs_t offset) const;
	void write(offs_t offset, u8 data);
	void input_changed();
	void irq_acknowledge();
	void clear_power_on_latch() { m_power_on = 0xff; }

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:
	u8 sample_columns(bool consume_power_on);
	void update_columns(bool physical_edge);
	void update_irq();

	devcb_read8::array<5> m_matrix_cb;
	devcb_read8 m_power_cb;
	devcb_write_line m_irq_cb;
	u8 m_regs[0x100] = {0};
	u8 m_columns = 0x1f;
	u8 m_power_on = 0xff;
	u8 m_power_on_column_mask = 0x04;
	bool m_irq_latched = false;
	bool m_five_rows = false;
};

DECLARE_DEVICE_TYPE(NOKIA_KBGPIO, nokia_kbgpio_device)

#endif // MAME_NOKIA_NOKIA_KBGPIO_H
