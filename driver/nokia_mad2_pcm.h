// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_NOKIA_MAD2_PCM_H
#define MAME_NOKIA_NOKIA_MAD2_PCM_H

#include "nokia_cobba.h"

// MAD2 DSP serial-port boundary to COBBA's four-wire PCM bus.  The available
// board documentation proves clock ownership, rates, directions and one
// sample per frame. DCT3 MAD2/COBBA-GJ documentation further proves a 16-bit
// serial word carrying sign-extended linear converter samples.
class nokia_mad2_pcm_device : public device_t
{
public:
	using pcm_block = nokia_cobba_device::pcm_block;

	nokia_mad2_pcm_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);

	void set_clock_rates(u32 data_clock, u32 frame_clock)
	{
		m_data_clock = data_clock;
		m_frame_clock = frame_clock;
	}
	void set_sample_bits(u8 bits) { m_sample_bits = bits; }
	enum class clock_edge : u8 { rising, falling };
	void set_frame_format(u8 sync_clocks, u8 word_clocks,
			bool msb_first, clock_edge data_edge)
	{
		m_sync_clocks = sync_clocks;
		m_word_clocks = word_clocks;
		m_msb_first = msb_first;
		m_data_edge = u8(data_edge);
	}
	bool transfer_frame_block(
			const pcm_block &dsp_to_cobba, pcm_block &cobba_to_dsp);

	u32 data_clock() const { return m_data_clock; }
	u32 frame_clock() const { return m_frame_clock; }
	u32 data_clocks_per_frame() const
	{
		return m_frame_clock && (m_data_clock % m_frame_clock) == 0
				? m_data_clock / m_frame_clock : 0;
	}
	u64 blocks_transferred() const { return m_blocks_transferred; }
	u64 frames_transferred() const { return m_frames_transferred; }
	u64 data_clocks_transferred() const { return m_data_clocks_transferred; }
	u64 idle_clocks_transferred() const { return m_idle_clocks_transferred; }

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	required_device<nokia_cobba_device> m_cobba;
	u32 m_data_clock = 0;
	u32 m_frame_clock = 0;
	u8 m_sample_bits = 0;
	u8 m_sync_clocks = 0;
	u8 m_word_clocks = 0;
	bool m_msb_first = true;
	u8 m_data_edge = u8(clock_edge::falling);
	u64 m_blocks_transferred = 0;
	u64 m_frames_transferred = 0;
	u64 m_data_clocks_transferred = 0;
	u64 m_idle_clocks_transferred = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_MAD2_PCM, nokia_mad2_pcm_device)

#endif // MAME_NOKIA_NOKIA_MAD2_PCM_H
