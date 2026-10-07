// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_TLV320AIC23_H
#define MAME_NOKIA_TLV320AIC23_H

// Partial control-word and master DSP-format interface; converted ADC samples
// must be supplied explicitly. Analog conversion/filtering is not implemented.
class tlv320aic23_device : public device_t
{
public:
	tlv320aic23_device(machine_config const &config, char const *tag, device_t *owner, u32 clock);
	void control_word_w(u16 value);
	void din_w(int state) { m_din = bool(state); }
	u16 reg(unsigned index) const { return index < 10 ? m_regs[index] : 0; }
	auto bclk_cb() { return m_bclk_cb.bind(); }
	auto frame_cb() { return m_frame_cb.bind(); }
	auto dout_cb() { return m_dout_cb.bind(); }
	auto converted_adc_cb() { return m_converted_adc_cb.bind(); }
	auto din_word_cb() { return m_din_word_cb.bind(); }
protected:
	void device_start() override ATTR_COLD;
	void device_reset() override ATTR_COLD;
private:
	TIMER_CALLBACK_MEMBER(clock_tick);
	void update_clock();
	u16 m_regs[10] = {};
	u16 m_cycle = 0;
	bool m_bclk = false, m_frame = false, m_running = false;
	bool m_din = false, m_dout = false;
	u16 m_adc_words[2] = {}, m_din_shift = 0;
	emu_timer *m_timer = nullptr;
	devcb_write_line m_bclk_cb, m_frame_cb;
	devcb_write_line m_dout_cb;
	devcb_read16 m_converted_adc_cb;
	devcb_write16 m_din_word_cb;
};
DECLARE_DEVICE_TYPE(TLV320AIC23, tlv320aic23_device)
#endif
