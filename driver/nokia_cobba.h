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
	struct hle_voice_profile
	{
		u8 microphone = disconnected;
		u8 output = disconnected;
		float microphone_gain_db = 0.0F;
		float output_gain_db = 0.0F;
	};

	nokia_cobba_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);
	auto rf_receive_cb() { return m_rf_receive_cb.bind(); }
	u16 rf_receive_sample();
	u64 rf_receive_samples() const { return m_rf_receive_samples; }

	void set_pcm_sample_bits(u8 bits) { m_pcm_sample_bits = bits; }
	// Temporary HLE selection used only until a DSP backend drives the opaque
	// COBBA control transport. Physical board connections are machine-config
	// sound routes; this profile is neither topology nor decoded register
	// semantics, and must not be changed from MCU call state.
	void set_hle_voice_profile(hle_voice_profile const &profile)
	{
		m_hle_microphone = profile.microphone;
		m_hle_output = profile.output;
		m_hle_microphone_gain_db = profile.microphone_gain_db;
		m_hle_output_gain_db = profile.output_gain_db;
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
	u64 control_transactions() const { return m_control_transactions; }
	u64 control_reads() const { return m_control_reads; }
	u64 control_writes() const { return m_control_writes; }
	// Parallel MFI control frame: high nibble selects one of 16 opaque
	// registers and the remaining 12 bits carry its value.
	void parallel_control_w(u16 frame);
	u16 parallel_register(u8 address) const
	{
		return m_parallel_registers[address & 0x0f];
	}
	u64 parallel_writes() const { return m_parallel_writes; }
	// Completed words at the DSP-facing codec serial pins. C54x BDXR/BDRR,
	// clocks and ready flags remain properties of the DSP's BSP peripheral.
	void codec_serial_transmit(u16 data);
	u16 codec_serial_receive() const { return m_codec_serial_receive_latch; }
	bool codec_serial_receive_ready() const { return m_codec_serial_receive_ready; }
	void codec_serial_receive_ack() { m_codec_serial_receive_ready = false; }
	bool codec_serial_loopback() const;
	u64 codec_serial_loopbacks() const { return m_codec_serial_loopbacks; }
	u8 run_control_conformance_checks();
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
	devcb_read16 m_rf_receive_cb;
	u64 m_rf_receive_samples = 0;
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
	float m_hle_microphone_gain_db = 0.0F;
	float m_hle_output_gain_db = 0.0F;
	float m_hle_microphone_gain = 1.0F;
	float m_hle_output_gain = 1.0F;
	u8 m_pcm_sample_bits = 0;
	u8 m_hle_microphone = disconnected;
	u8 m_hle_output = disconnected;
	std::array<u16, 16> m_control_registers{};
	u16 m_control_data_latch = 0;
	u8 m_control_address = 0;
	bool m_control_read = false;
	bool m_trace_enabled = false;
	u64 m_control_transactions = 0;
	u64 m_control_reads = 0;
	u64 m_control_writes = 0;
	std::array<u16, 16> m_parallel_registers{};
	u64 m_parallel_writes = 0;
	u16 m_codec_serial_receive_latch = 0;
	bool m_codec_serial_receive_ready = false;
	u64 m_codec_serial_loopbacks = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_COBBA, nokia_cobba_device)

#endif // MAME_NOKIA_NOKIA_COBBA_H
