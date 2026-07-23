// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz
#ifndef MAME_NOKIA_NOKIA_CCONT_H
#define MAME_NOKIA_NOKIA_CCONT_H

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
	void set_charger_input(unsigned channel, bool connected, uint16_t vchar = 0x03ff);
	void set_ready(bool ready) { m_ready = ready; }
	void set_wddisx_grounded(bool grounded) { m_wddisx_grounded = grounded; }
	bool watchdog_tick();

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_post_load() override;

private:
	TIMER_CALLBACK_MEMBER(rtc_tick);
	TIMER_CALLBACK_MEMBER(rtc_alarm_latch_complete);
	void update_irq();
	void advance_rtc();
	void latch_irq_sources(uint8_t sources);

	devcb_write_line m_irq_cb;
	devcb_write_line m_power_cb;
	uint8_t m_cmd = 0;
	uint8_t m_watchdog = 0;
	uint8_t m_regs[0x10] = {0};
	uint16_t m_adc_source[8] = {0};
	bool m_data_cycle = false;
	bool m_ready = true;
	bool m_powered = true;
	bool m_charger_connected = false;
	bool m_wddisx_grounded = false;
	bool m_rtc_alarm_armed = false;
	bool m_adc_trace = false;
	bool m_rtc_trace = false;
	emu_timer *m_rtc_timer = nullptr;
	emu_timer *m_rtc_alarm_latch_timer = nullptr;
};

DECLARE_DEVICE_TYPE(NOKIA_CCONT, nokia_ccont_device)

#endif // MAME_NOKIA_NOKIA_CCONT_H
