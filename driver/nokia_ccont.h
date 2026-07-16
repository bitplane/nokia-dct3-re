// license:BSD-3-Clause
#ifndef MAME_NOKIA_NOKIA_CCONT_H
#define MAME_NOKIA_NOKIA_CCONT_H

#pragma once

class nokia_ccont_device : public device_t
{
public:
	nokia_ccont_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	auto irq_cb() { return m_irq_cb.bind(); }
	auto power_cb() { return m_power_cb.bind(); }
	void serial_w(uint8_t data);
	uint8_t serial_r();
	void select_w(int selected);
	void set_adc_source(unsigned channel, uint16_t value);
	void set_boot_status(uint8_t status);
	void latch_irq_sources(uint8_t sources);
	void set_present(bool present) { m_present = present; }
	bool watchdog_tick();

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_post_load() override;

private:
	void update_irq();

	devcb_write_line m_irq_cb;
	devcb_write_line m_power_cb;
	uint8_t m_cmd = 0;
	uint8_t m_watchdog = 0;
	uint8_t m_regs[0x10] = {0};
	uint16_t m_adc_source[8] = {0};
	uint8_t m_boot_status = 0x02;
	bool m_data_cycle = false;
	bool m_present = false;
};

DECLARE_DEVICE_TYPE(NOKIA_CCONT, nokia_ccont_device)

#endif
