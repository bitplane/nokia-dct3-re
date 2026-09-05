// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz
#ifndef MAME_NOKIA_NOKIA_KBGPIO_H
#define MAME_NOKIA_NOKIA_KBGPIO_H

class nokia_kbgpio_device : public device_t
{
public:
	nokia_kbgpio_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	struct wiring_contract
	{
		u8 rows = 4;
		u8 power_on_column_mask = 0x04;
		u8 row_signal = 0x28;
		u8 column_input = 0x2a;
		u8 column_irq_mask = 0x6b;
		u8 row_direction = 0xa8;

		constexpr bool valid() const
		{
			return (rows == 4 || rows == 5) &&
					power_on_column_mask != 0 &&
					(power_on_column_mask & ~0x1f) == 0 &&
					(power_on_column_mask &
							(power_on_column_mask - 1)) == 0;
		}
	};

	auto matrix_cb(unsigned column) { return m_matrix_cb[column].bind(); }
	auto power_cb() { return m_power_cb.bind(); }
	auto irq_cb() { return m_irq_cb.bind(); }

	void set_wiring_contract(wiring_contract contract);
	void set_trace(bool enabled) { m_trace = enabled; }

	bool owns(offs_t offset) const;
	u8 read(offs_t offset);
	u8 peek(offs_t offset) const;
	void write(offs_t offset, u8 data);
	void input_changed();
	void irq_acknowledge();
	void clear_power_on_latch();

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:
	TIMER_CALLBACK_MEMBER(sample_tick);
	u8 sample_columns(bool consume_power_on);
	void update_columns();
	void update_irq();

	devcb_read8::array<5> m_matrix_cb;
	devcb_read8 m_power_cb;
	devcb_write_line m_irq_cb;
	emu_timer *m_sample_timer = nullptr;
	u8 m_regs[0x100] = {0};
	u8 m_columns = 0x1f;
	u8 m_power_on = 0xff;
	wiring_contract m_wiring;
	bool m_irq_latched = false;
	bool m_trace = false;
};

DECLARE_DEVICE_TYPE(NOKIA_KBGPIO, nokia_kbgpio_device)

#endif // MAME_NOKIA_NOKIA_KBGPIO_H
