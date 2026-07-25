// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_NOKIA_COBBA_H
#define MAME_NOKIA_NOKIA_COBBA_H

#include "sound.h"

#include <array>

class nokia_cobba_device : public device_t, public device_sound_interface
{
public:
	static constexpr unsigned pcm_rate = 8'000;
	static constexpr unsigned pcm_block_samples = 160;
	static constexpr u8 microphone_inputs = 3;
	static constexpr u8 audio_outputs = 2;
	static constexpr u8 disconnected = 0xff;
	enum microphone_input : u8 { mic1 = 0, mic2 = 1, mic3 = 2 };
	enum audio_output : u8 { ear = 0, hf = 1 };
	using pcm_block = std::array<s16, pcm_block_samples>;

	nokia_cobba_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);

	void set_internal_voice_gains(float microphone_db, float earpiece_db)
	{
		m_microphone_gain_db = microphone_db;
		m_earpiece_gain_db = earpiece_db;
	}
	void set_pcm_sample_bits(u8 bits) { m_pcm_sample_bits = bits; }
	// Temporary HLE route used only until a DSP backend drives the opaque
	// COBBA control transport. This is product topology, not decoded register
	// semantics, and must not be changed from MCU call state.
	void set_hle_internal_voice_route(u8 microphone, u8 output)
	{
		m_selected_microphone = microphone;
		m_selected_output = output;
	}
	// DSP serial-control plane. The register payload is 12 bits; meanings are
	// deliberately opaque until recovered independently for the product.
	void control_data_w(u16 data);
	void control_select_w(u16 select);
	u16 control_data_r() const;
	u16 control_register(u8 address) const
	{
		return m_control_registers[address & 0x0f];
	}
	bool write_earpiece_pcm(const pcm_block &block);
	void read_microphone_pcm(pcm_block &block);
	u64 earpiece_blocks() const { return m_earpiece_blocks; }
	u64 microphone_blocks() const { return m_microphone_blocks; }
	u64 earpiece_overruns() const { return m_earpiece_overruns; }
	u64 microphone_overruns() const { return m_microphone_overruns; }
	u64 microphone_underruns() const { return m_microphone_underruns; }

protected:
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void sound_stream_update(sound_stream &stream) override;

private:
	static constexpr unsigned queue_blocks = 10;
	static constexpr unsigned queue_samples = queue_blocks * pcm_block_samples;

	sound_stream *m_stream = nullptr;
	std::array<s16, queue_samples> m_earpiece_queue{};
	std::array<s16, queue_samples> m_microphone_queue{};
	u16 m_earpiece_head = 0;
	u16 m_earpiece_count = 0;
	u16 m_microphone_head = 0;
	u16 m_microphone_count = 0;
	u64 m_earpiece_blocks = 0;
	u64 m_microphone_blocks = 0;
	u64 m_earpiece_overruns = 0;
	u64 m_microphone_overruns = 0;
	u64 m_microphone_underruns = 0;
	float m_microphone_gain_db = 0.0F;
	float m_earpiece_gain_db = 0.0F;
	float m_microphone_gain = 1.0F;
	float m_earpiece_gain = 1.0F;
	u8 m_pcm_sample_bits = 0;
	u8 m_selected_microphone = disconnected;
	u8 m_selected_output = disconnected;
	std::array<u16, 16> m_control_registers{};
	u16 m_control_data_latch = 0;
	u8 m_control_address = 0;
	bool m_control_read = false;
};

DECLARE_DEVICE_TYPE(NOKIA_COBBA, nokia_cobba_device)

#endif // MAME_NOKIA_NOKIA_COBBA_H
