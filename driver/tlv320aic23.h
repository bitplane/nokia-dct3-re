// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_TLV320AIC23_H
#define MAME_NOKIA_TLV320AIC23_H

// Partial control-word and master DSP-format clock interface; no analog conversion.
class tlv320aic23_device : public device_t
{
public:
	tlv320aic23_device(machine_config const &config, char const *tag, device_t *owner, u32 clock);
	void control_word_w(u16 value);
	u16 reg(unsigned index) const { return index < 10 ? m_regs[index] : 0; }
	auto bclk_cb() { return m_bclk_cb.bind(); }
	auto frame_cb() { return m_frame_cb.bind(); }
protected:
	void device_start() override ATTR_COLD;
	void device_reset() override ATTR_COLD;
private:
	TIMER_CALLBACK_MEMBER(clock_tick);
	void update_clock();
	u16 m_regs[10] = {};
	u16 m_cycle = 0;
	bool m_bclk = false, m_frame = false, m_running = false;
	emu_timer *m_timer = nullptr;
	devcb_write_line m_bclk_cb, m_frame_cb;
};
DECLARE_DEVICE_TYPE(TLV320AIC23, tlv320aic23_device)
#endif
