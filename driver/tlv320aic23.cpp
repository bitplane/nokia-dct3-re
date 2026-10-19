// license:BSD-3-Clause
// copyright-holders:Gaz
#include "emu.h"
#include "tlv320aic23.h"

// Register/clock subset: TI SLWS106H 3.1.3, 3.3.1.4 and 3.3.2.1.
// Original AIC23 SLEU003 corroborates master DSP framing; electrical revision
// equivalence is not claimed (see docs/5510_bringup.md).
DEFINE_DEVICE_TYPE(TLV320AIC23, tlv320aic23_device, "tlv320aic23", "TI TLV320AIC23 (partial)")

tlv320aic23_device::tlv320aic23_device(machine_config const &config, char const *tag, device_t *owner, u32 clock)
	: device_t(config, TLV320AIC23, tag, owner, clock), device_sound_interface(config, *this), m_bclk_cb(*this), m_frame_cb(*this),
	  m_dout_cb(*this), m_converted_adc_cb(*this, 0), m_din_word_cb(*this)
{
}

void tlv320aic23_device::device_start()
{
	// Oversampled zero-order hold, as in MAME DAC devices. This rendering rate
	// is not the codec's delta-sigma clock or a modeled interpolation filter.
	m_stream = stream_alloc(0, 2, 48'000 * 32);
	m_timer = timer_alloc(FUNC(tlv320aic23_device::clock_tick), this);
	save_item(NAME(m_regs)); save_item(NAME(m_cycle)); save_item(NAME(m_bclk));
	save_item(NAME(m_frame)); save_item(NAME(m_running));
	save_item(NAME(m_din)); save_item(NAME(m_dout)); save_item(NAME(m_adc_words)); save_item(NAME(m_din_shift));
	save_item(NAME(m_din_words)); save_item(NAME(m_dac_words));
	save_item(NAME(m_headphone_volume)); save_item(NAME(m_headphone_pending));
}

void tlv320aic23_device::device_reset()
{
	m_stream->update();
	static constexpr u16 defaults[10] = { 0x97, 0x97, 0x79, 0x79, 0x0a, 0x08, 0x07, 0x01, 0x20, 0 };
	std::copy(std::begin(defaults), std::end(defaults), std::begin(m_regs));
	m_cycle = 0; m_bclk = false; m_frame = false; m_running = false;
	m_timer->adjust(attotime::never);
	m_bclk_cb(0); m_frame_cb(0);
	m_din = m_dout = false; m_adc_words[0] = m_adc_words[1] = m_din_shift = 0;
	m_dout_cb(0);
	std::fill(std::begin(m_din_words), std::end(m_din_words), 0);
	std::fill(std::begin(m_dac_words), std::end(m_dac_words), 0);
	std::fill(std::begin(m_headphone_volume), std::end(m_headphone_volume), 0x79);
	std::fill(std::begin(m_headphone_pending), std::end(m_headphone_pending), false);
}

void tlv320aic23_device::control_word_w(u16 value)
{
	unsigned const address = value >> 9;
	u16 const data = value & 0x1ff;
	if (address == 15 && !data) { device_reset(); return; }
	if (address >= 10) return;
	m_stream->update();
	m_regs[address] = data;
	// SLWS106H 3.1.3: simultaneous headphone volume/mute update. The
	// opposite channel's independent zero-cross/control fields are preserved.
	if ((address == 2 || address == 3) && BIT(data, 8))
		m_regs[address ^ 1] = (m_regs[address ^ 1] & ~u16(0x7f)) | (data & 0x7f);
	if (address == 2 || address == 3)
	{
		request_headphone_volume(address - 2);
		if (BIT(data, 8)) request_headphone_volume((address - 2) ^ 1);
	}
	for (unsigned channel=0;channel<2;++channel)
		if (m_headphone_pending[channel] && (!dac_active() || !m_dac_words[channel]))
		{
			m_headphone_volume[channel] = m_regs[channel+2] & 0x7f;
			m_headphone_pending[channel] = false;
		}
	if (address >= 6) update_clock();
}

bool tlv320aic23_device::dac_active() const
{
	return BIT(m_regs[9],0) && !(m_regs[6] & 0x88) && BIT(m_regs[4],4) && !BIT(m_regs[5],3);
}

double tlv320aic23_device::headphone_dac_level(unsigned channel) const
{
	channel &= 1;
	if (!dac_active() || BIT(m_regs[6],4) || m_headphone_volume[channel] < 0x30) return 0.0;
	return double(m_dac_words[channel]) / 32768.0 * std::pow(10.0, (int(m_headphone_volume[channel])-121) / 20.0);
}

void tlv320aic23_device::sound_stream_update(sound_stream &stream)
{
	for (unsigned channel=0;channel<2;++channel) stream.fill(channel, headphone_dac_level(channel));
}

void tlv320aic23_device::request_headphone_volume(unsigned channel)
{
	// Ideal digital zero/sign crossing, not a measured analog detector threshold.
	if (!BIT(m_regs[channel+2],7) || !dac_active() || !m_dac_words[channel])
	{
		m_headphone_volume[channel] = m_regs[channel+2] & 0x7f;
		m_headphone_pending[channel] = false;
	}
	else m_headphone_pending[channel] = true;
}

void tlv320aic23_device::latch_dac_frame()
{
	m_stream->update();
	for (unsigned channel=0;channel<2;++channel)
	{
		s16 const next = m_din_words[channel ^ unsigned(BIT(m_regs[7],5))];
		bool const crossing = !next || !m_dac_words[channel] || ((next < 0) != (m_dac_words[channel] < 0));
		if (m_headphone_pending[channel] && (!dac_active() || crossing))
		{
			m_headphone_volume[channel] = m_regs[channel+2] & 0x7f;
			m_headphone_pending[channel] = false;
		}
		m_dac_words[channel] = next;
	}
}

void tlv320aic23_device::update_clock()
{
	// Output-amplifier power-down does not stop the digital interface clock.
	bool const enabled = BIT(m_regs[9], 0) && BIT(m_regs[7], 6) && !(m_regs[6] & 0xc0);
	if (enabled)
	{
		// Only the recovered USB/272, 16-bit DSP mode is implemented. The CLKOUT
		// divider does not divide BCLK; CLKIN divides the entire codec instead.
		if ((m_regs[7] & 0x5f) != 0x53 || (m_regs[8] & 0x3f) != 0x23 || !clock())
			fatalerror("AIC23 unsupported active master format/rate %03x/%03x", m_regs[7], m_regs[8]);
		if (!BIT(m_regs[6], 2) && m_converted_adc_cb.isunset())
			fatalerror("AIC23 requires an explicit converted-ADC source; analog conversion is not implemented");
		if (!m_running)
		{
			m_running = true; m_cycle = 0; m_bclk = false; m_frame = false;
			m_timer->adjust(attotime::from_hz(clock() * (BIT(m_regs[8], 6) ? 1 : 2)));
		}
	}
	else
	{
		m_running = false; m_timer->adjust(attotime::never);
		m_bclk = false; m_frame = false; m_bclk_cb(0); m_frame_cb(0);
	}
}

TIMER_CALLBACK_MEMBER(tlv320aic23_device::clock_tick)
{
	m_bclk = !m_bclk;
	if (m_bclk && m_cycle >= 2 && m_cycle <= 33)
	{
		// LRP=1 samples DIN on the second rising edge after frame assertion.
		m_din_shift = (m_din_shift << 1) | m_din;
		if (m_cycle == 17 || m_cycle == 33)
		{
			m_din_words[m_cycle == 17 ? 0 : 1] = m_din_shift;
			// Ideal stereo DAC boundary after both complete words. Physical
			// interpolation/group delay is deliberately not synthesized here.
			if (m_cycle == 33) latch_dac_frame();
			m_din_word_cb(m_cycle == 17 ? 0 : 1, m_din_shift);
			m_din_shift = 0;
		}
	}
	// SLWS106H Figure 2-2: FS and DOUT change AFTER falling BCLK. RX must
	// sample the old pin levels at that edge; TX reacts asynchronously to FS.
	m_bclk_cb(m_bclk);
	if (!m_bclk)
	{
		bool const frame = m_cycle == 0;
		if (frame != m_frame) { m_frame = frame; m_frame_cb(frame); }
		if (m_cycle == 0)
		{
			m_din_shift = 0;
			if (!BIT(m_regs[6], 2))
				for (unsigned channel = 0; channel < 2; ++channel) m_adc_words[channel] = m_converted_adc_cb(channel);
		}
		else if (m_cycle <= 32 && !BIT(m_regs[6], 2))
		{
			unsigned const bit = m_cycle - 1;
			m_dout = BIT(m_adc_words[bit / 16], 15 - (bit & 15));
			m_dout_cb(m_dout);
		}
		m_cycle = (m_cycle + 1) % 272;
	}
	m_timer->adjust(attotime::from_hz(clock() * (BIT(m_regs[8], 6) ? 1 : 2)));
}
