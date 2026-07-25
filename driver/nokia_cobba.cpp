// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "nokia_cobba.h"

DEFINE_DEVICE_TYPE(NOKIA_COBBA, nokia_cobba_device,
		"nokia_cobba", "Nokia COBBA audio codec")

nokia_cobba_device::nokia_cobba_device(
		const machine_config &mconfig, const char *tag,
		device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_COBBA, tag, owner, clock),
	device_sound_interface(mconfig, *this)
{
}

void nokia_cobba_device::device_start()
{
	// The three differential microphone inputs and two audio outputs are
	// separate physical COBBA pins. PCM conversion and route selection remain
	// inside COBBA.
	m_stream = stream_alloc(microphone_inputs, audio_outputs, pcm_rate);
	m_hle_microphone_gain =
			std::pow(10.0F, m_hle_microphone_gain_db / 20.0F);
	m_hle_output_gain = std::pow(10.0F, m_hle_output_gain_db / 20.0F);
	save_item(NAME(m_earpiece_queue));
	save_item(NAME(m_microphone_queue));
	save_item(NAME(m_earpiece_head));
	save_item(NAME(m_earpiece_count));
	save_item(NAME(m_microphone_head));
	save_item(NAME(m_microphone_count));
	save_item(NAME(m_earpiece_blocks));
	save_item(NAME(m_microphone_blocks));
	save_item(NAME(m_earpiece_overruns));
	save_item(NAME(m_microphone_overruns));
	save_item(NAME(m_microphone_underruns));
	save_item(NAME(m_pcm_sample_bits));
	save_item(NAME(m_hle_microphone));
	save_item(NAME(m_hle_output));
	save_item(NAME(m_control_registers));
	save_item(NAME(m_control_data_latch));
	save_item(NAME(m_control_address));
	save_item(NAME(m_control_read));
}

void nokia_cobba_device::device_reset()
{
	m_stream->update();
	m_earpiece_head = 0;
	m_earpiece_count = 0;
	m_microphone_head = 0;
	m_microphone_count = 0;
	m_earpiece_blocks = 0;
	m_microphone_blocks = 0;
	m_earpiece_overruns = 0;
	m_microphone_overruns = 0;
	m_microphone_underruns = 0;
	std::fill(m_control_registers.begin(), m_control_registers.end(), 0);
	// Recovered ROM4 multi-register transactions wait for status register D
	// bits 1:0 clear and bits 3:2 set between transfers.
	m_control_registers[0x0d] = 0x000c;
	m_control_data_latch = 0;
	m_control_address = 0;
	m_control_read = false;
}

void nokia_cobba_device::control_data_w(u16 data)
{
	m_control_data_latch = data & 0x0fff;
}

void nokia_cobba_device::control_select_w(u16 select)
{
	m_control_address = select & 0x0f;
	m_control_read = BIT(select, 4);
	if (!m_control_read)
		m_control_registers[m_control_address] = m_control_data_latch;
}

u16 nokia_cobba_device::control_data_r() const
{
	// Bit 12 is the observed busy flag. This untimed model completes the
	// transfer immediately, so it remains clear on readback.
	return m_control_registers[m_control_address] & 0x0fff;
}

bool nokia_cobba_device::write_earpiece_pcm(const pcm_block &block)
{
	m_stream->update();
	if (m_earpiece_count > queue_samples - pcm_block_samples)
	{
		++m_earpiece_overruns;
		return false;
	}

	for (s16 sample : block)
	{
		m_earpiece_queue[(m_earpiece_head + m_earpiece_count) % queue_samples] = sample;
		++m_earpiece_count;
	}
	++m_earpiece_blocks;
	return true;
}

void nokia_cobba_device::read_microphone_pcm(pcm_block &block)
{
	m_stream->update();
	for (s16 &sample : block)
	{
		if (m_microphone_count)
		{
			sample = m_microphone_queue[m_microphone_head];
			m_microphone_head = (m_microphone_head + 1) % queue_samples;
			--m_microphone_count;
		}
		else
		{
			sample = 0;
			++m_microphone_underruns;
		}
	}
	++m_microphone_blocks;
}

void nokia_cobba_device::sound_stream_update(sound_stream &stream)
{
	const s32 converter_scale =
			(m_pcm_sample_bits >= 2 && m_pcm_sample_bits <= 16)
					? s32(1) << (m_pcm_sample_bits - 1)
					: 32768;
	for (int index = 0; index < stream.samples(); ++index)
	{
		if (m_microphone_count == queue_samples)
		{
			m_microphone_head = (m_microphone_head + 1) % queue_samples;
			--m_microphone_count;
			++m_microphone_overruns;
		}
		const float analogue_input =
				m_hle_microphone < microphone_inputs
						? float(stream.get(m_hle_microphone, index))
						: 0.0F;
		const float microphone = std::clamp(
				analogue_input * m_hle_microphone_gain,
				-1.0F, float(converter_scale - 1) / converter_scale);
		m_microphone_queue[
				(m_microphone_head + m_microphone_count) % queue_samples] =
				s16(microphone * converter_scale);
		++m_microphone_count;

		s16 sample = 0;
		if (m_earpiece_count)
		{
			sample = m_earpiece_queue[m_earpiece_head];
			m_earpiece_head = (m_earpiece_head + 1) % queue_samples;
			--m_earpiece_count;
		}
		for (u8 output = 0; output != audio_outputs; ++output)
			stream.put(output, index, 0.0F);
		if (m_hle_output < audio_outputs)
			stream.put(m_hle_output, index,
					float(sample) / converter_scale * m_hle_output_gain);
	}
}
