// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz
#ifndef MAME_NOKIA_NOKIA_PUP_H
#define MAME_NOKIA_NOKIA_PUP_H

class nokia_pup_device : public device_t
{
public:
	nokia_pup_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	auto eeprom_sda_read_cb() { return m_eeprom_sda_read_cb.bind(); }
	auto eeprom_sda_write_cb() { return m_eeprom_sda_write_cb.bind(); }
	auto eeprom_scl_write_cb() { return m_eeprom_scl_write_cb.bind(); }
	auto buzzer_clock_cb() { return m_buzzer_clock_cb.bind(); }
	auto buzzer_enable_cb() { return m_buzzer_enable_cb.bind(); }
	auto vibrator_enable_cb() { return m_vibrator_enable_cb.bind(); }

	void set_trace(bool enabled) { m_trace = enabled; }

	static bool owns(offs_t offset);
	u8 read(offs_t offset);
	u8 peek(offs_t offset) const;
	void write(offs_t offset, u8 data);

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:
	void update_genio();
	void update_buzzer();
	void update_vibrator();
	void post_load();

	devcb_read_line m_eeprom_sda_read_cb;
	devcb_write_line m_eeprom_sda_write_cb;
	devcb_write_line m_eeprom_scl_write_cb;
	devcb_write32 m_buzzer_clock_cb;
	devcb_write_line m_buzzer_enable_cb;
	devcb_write_line m_vibrator_enable_cb;
	u8 m_regs[0x100] = {0};
	bool m_trace = false;
};

DECLARE_DEVICE_TYPE(NOKIA_PUP, nokia_pup_device)

#endif // MAME_NOKIA_NOKIA_PUP_H
