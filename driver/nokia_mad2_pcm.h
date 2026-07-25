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
	enum class clock_edge : u8 { rising, falling };
	struct bus_profile
	{
		u32 data_clock = 0;
		u32 frame_clock = 0;
		u8 sample_bits = 0;
		u8 sync_clocks = 0;
		u8 word_clocks = 0;
		bool msb_first = true;
		clock_edge data_edge = clock_edge::falling;
	};

	nokia_mad2_pcm_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);

	void set_bus_profile(bus_profile const &profile)
	{
		m_data_clock = profile.data_clock;
		m_frame_clock = profile.frame_clock;
		m_sample_bits = profile.sample_bits;
		m_sync_clocks = profile.sync_clocks;
		m_word_clocks = profile.word_clocks;
		m_msb_first = profile.msb_first;
		m_data_edge = u8(profile.data_edge);
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
