// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_TLV320AIC23_H
#define MAME_NOKIA_TLV320AIC23_H

#include "sound.h"

// Partial control-word and master DSP-format interface; converted ADC samples
// must be supplied explicitly. Audio outputs are ideal DAC-to-headphone
// contributions; analog bypass/sidetone, converter filters and rails are absent.
class tlv320aic23_device : public device_t, public device_sound_interface
{
public:
	tlv320aic23_device(machine_config const &config, char const *tag, device_t *owner, u32 clock);
	void control_word_w(u16 value);
	void din_w(int state) { m_din = bool(state); }
	u16 reg(unsigned index) const { return index < 10 ? m_regs[index] : 0; }
	s16 dac_sample(unsigned channel) const { return m_dac_words[channel & 1]; }
	u8 headphone_volume(unsigned channel) const { return m_headphone_volume[channel & 1]; }
	double headphone_dac_level(unsigned channel) const;
	auto bclk_cb() { return m_bclk_cb.bind(); }
	auto frame_cb() { return m_frame_cb.bind(); }
	auto dout_cb() { return m_dout_cb.bind(); }
	auto converted_adc_cb() { return m_converted_adc_cb.bind(); }
	auto din_word_cb() { return m_din_word_cb.bind(); }
protected:
	void device_start() override ATTR_COLD;
	void device_reset() override ATTR_COLD;
	void sound_stream_update(sound_stream &stream) override;
private:
	TIMER_CALLBACK_MEMBER(clock_tick);
	void update_clock();
	bool dac_active() const;
	void request_headphone_volume(unsigned channel);
	void latch_dac_frame();
	u16 m_regs[10] = {};
	u16 m_cycle = 0;
	bool m_bclk = false, m_frame = false, m_running = false;
	bool m_din = false, m_dout = false;
	u16 m_adc_words[2] = {}, m_din_shift = 0;
	u16 m_din_words[2] = {};
	s16 m_dac_words[2] = {};
	u8 m_headphone_volume[2] = {0x79,0x79};
	bool m_headphone_pending[2] = {};
	sound_stream *m_stream = nullptr;
	emu_timer *m_timer = nullptr;
	devcb_write_line m_bclk_cb, m_frame_cb;
	devcb_write_line m_dout_cb;
	devcb_read16 m_converted_adc_cb;
	devcb_write16 m_din_word_cb;
};
DECLARE_DEVICE_TYPE(TLV320AIC23, tlv320aic23_device)
#endif // MAME_NOKIA_TLV320AIC23_H
