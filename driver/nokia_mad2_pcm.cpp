// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "nokia_mad2_pcm.h"

DEFINE_DEVICE_TYPE(NOKIA_MAD2_PCM, nokia_mad2_pcm_device,
		"nokia_mad2_pcm", "Nokia MAD2 DSP/COBBA PCM port")

nokia_mad2_pcm_device::nokia_mad2_pcm_device(
		const machine_config &mconfig, const char *tag,
		device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_MAD2_PCM, tag, owner, clock),
	m_cobba(*this, "^cobba")
{
}

void nokia_mad2_pcm_device::device_start()
{
	save_item(NAME(m_data_clock));
	save_item(NAME(m_frame_clock));
	save_item(NAME(m_sample_bits));
	save_item(NAME(m_sync_clocks));
	save_item(NAME(m_word_clocks));
	save_item(NAME(m_msb_first));
	save_item(NAME(m_data_edge));
	save_item(NAME(m_blocks_transferred));
	save_item(NAME(m_frames_transferred));
	save_item(NAME(m_data_clocks_transferred));
	save_item(NAME(m_idle_clocks_transferred));
}

void nokia_mad2_pcm_device::device_reset()
{
	m_blocks_transferred = 0;
	m_frames_transferred = 0;
	m_data_clocks_transferred = 0;
	m_idle_clocks_transferred = 0;
}

bool nokia_mad2_pcm_device::transfer_frame_block(
		const pcm_block &dsp_to_cobba, pcm_block &cobba_to_dsp)
{
	// NSE-8 configuration supplies PCMDClk=520 kHz and PCMSClk=8 kHz:
	// 65 serial data-clock periods per full-duplex PCM frame. Keep the
	// validation derived from product clocks; another MAD2/COBBA pair may
	// configure a different integral frame shape.
	const u32 clocks_per_frame = data_clocks_per_frame();
	if (m_frame_clock != nokia_cobba_device::pcm_rate ||
			!m_data_clock ||
			(m_data_clock % m_frame_clock) != 0 ||
			m_sample_bits < 2 || m_sample_bits > 16 ||
			m_sync_clocks != 1 ||
			m_word_clocks != 16 ||
			!m_msb_first ||
			m_data_edge != u8(clock_edge::falling) ||
			clocks_per_frame < u32(m_sync_clocks + m_word_clocks) ||
			dsp_to_cobba.size() !=
					(m_frame_clock / 50))
		return false;

	// The MAD2/COBBA serial word is 16 bits wide, but the converter sample is
	// sign-extended from the product-configured linear resolution. The DSP
	// speech codec consumes the corresponding left-aligned 16-bit domain. Run
	// each value through the actual MSB-first wire word: frame sync occupies
	// one clock, these 16 bits cross on falling edges, and the remainder of
	// the 65-clock NSE-8 frame is idle.
	const unsigned shift = 16 - m_sample_bits;
	const s32 divisor = s32(1) << shift;
	const auto cross_serial_word = [shift](s16 converter_sample)
	{
		const u16 transmitted = u16(converter_sample);
		u16 received = 0;
		for (unsigned bit = 0; bit != 16; ++bit)
			received = u16((received << 1) |
					BIT(transmitted, 15 - bit));
		return s16(received << shift) >> shift;
	};
	pcm_block wire_earpiece{};
	for (unsigned index = 0; index != wire_earpiece.size(); ++index)
	{
		const s32 sample = dsp_to_cobba[index];
		const s16 converter_sample = sample >= 0
				? s16(sample / divisor)
				: s16(-((-sample + divisor - 1) / divisor));
		wire_earpiece[index] = cross_serial_word(converter_sample);
	}

	pcm_block wire_microphone{};
	m_cobba->read_microphone_pcm(wire_microphone);
	for (unsigned index = 0; index != cobba_to_dsp.size(); ++index)
		cobba_to_dsp[index] = s16(
				s32(cross_serial_word(wire_microphone[index])) * divisor);

	const bool accepted = m_cobba->write_earpiece_pcm(wire_earpiece);
	if (accepted)
	{
		++m_blocks_transferred;
		m_frames_transferred += dsp_to_cobba.size();
		m_data_clocks_transferred +=
				u64(dsp_to_cobba.size()) * clocks_per_frame;
		m_idle_clocks_transferred += u64(dsp_to_cobba.size()) *
				(clocks_per_frame - m_sync_clocks - m_word_clocks);
	}
	return accepted;
}
