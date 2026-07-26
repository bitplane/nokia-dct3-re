// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_DSP_HLE_H
#define MAME_NOKIA_NOKIA_DSP_HLE_H

#include "nokia_dspif.h"
#include "nokia_external_service.h"
#include "nokia_gsm_fr_codec.h"
#include "nokia_mad2_pcm.h"
#include "nokia_radio_peer.h"

#include <array>

class nokia_dsp_hle_device : public device_t
{
public:
	nokia_dsp_hle_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	enum class bootstrap_completion_profile : u8
	{
		ready_words_one,
		nse3_final_b06_second_unknown,
		nse3_v548_preupload_and_completion_unknown
	};

	enum class service_control_profile : u8
	{
		none,
		compact,
		framed
	};

	void set_service_enabled(bool enabled) { m_service_enabled = enabled; }
	void set_external_service_enabled(bool enabled) { m_external_service_enabled = enabled; }
	void set_service_control_profile(service_control_profile profile)
	{
		m_service_control_profile = profile;
	}
	void set_service_delay_us(unsigned delay) { m_service_delay_us = delay; }
	void set_peer_poll_ms(unsigned period) { m_peer_poll_ms = period; }
	void set_parameter_command(u8 command)
	{
		m_parameter_command = command;
	}
	void set_speech_request_policy(u16 mask, u16 value)
	{
		m_speech_request_mask = mask;
		m_speech_request_value = value;
	}
	void set_bootstrap_exchange_limit(unsigned exchanges) { m_bootstrap_exchange_limit = exchanges; }
	void set_bootstrap_completion(bootstrap_completion_profile profile)
	{
		m_bootstrap_completion = profile;
	}
	void set_bootstrap_ping_pong(bool enabled) { m_bootstrap_ping_pong = enabled; }
	void set_code_block_request(bool enabled) { m_code_block_request = enabled; }
	void set_parked_boot_status(bool enabled, u16 response)
	{
		m_parked_boot_status = enabled;
		m_boot_status_response = response;
	}
	auto mcu_control_word_cb() { return m_mcu_control_word_cb.bind(); }
	u16 mcu_control_word() const { return m_mcu_control_word; }
	u16 mcu_control_wire() const { return m_mcu_control_wire; }
	u16 data_word(u16 address) const { return m_data_memory[address]; }
	bool data_word_loaded(u16 address) const { return m_data_memory_loaded[address] != 0; }
	u64 speech_uplink_frames() const { return m_speech_uplink_frames; }
	u64 speech_downlink_frames() const { return m_speech_downlink_frames; }

	void tx_commit_w(int state);
	void service_pending_w(int state);
	void doorbell_w(int state);
	void shared_002_write_w(int state);
	void shared_0fe_read_w(int state);
	void shared_0fe_write_w(int state);
	void shared_100_read_w(int state);
	void shared_100_write_w(int state);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	TIMER_CALLBACK_MEMBER(service_tick);
	TIMER_CALLBACK_MEMBER(packet_tick);
	TIMER_CALLBACK_MEMBER(response_tick);
	TIMER_CALLBACK_MEMBER(keepalive_tick);
	TIMER_CALLBACK_MEMBER(speech_tick);
	void drain_responses();
	void schedule_response();
	void publish_bootstrap_state();
	void prepare_speech_codec_save();
	void restore_speech_codec_state();
	bool consume_memory_upload(const nokia_dspif_device::packet &packet);
	required_device<nokia_dspif_device> m_transport;
	required_device<nokia_external_service_peer_device> m_external_peer;
	required_device<nokia_radio_peer_device> m_radio_peer;
	required_device<nokia_mad2_pcm_device> m_mad2_pcm;
	devcb_write16 m_mcu_control_word_cb;
	emu_timer *m_service_timer = nullptr;
	emu_timer *m_packet_timer = nullptr;
	emu_timer *m_response_timer = nullptr;
	emu_timer *m_keepalive_timer = nullptr;
	emu_timer *m_speech_timer = nullptr;
	bool m_service_enabled = false;
	bool m_external_service_enabled = false;
	service_control_profile m_service_control_profile = service_control_profile::none;
	bool m_trace_enabled = false;
	unsigned m_service_delay_us = 5'000;
	unsigned m_peer_poll_ms = 5;
	bool m_service_control_completion_sent = false;
	unsigned m_bootstrap_exchange_limit = 64;
	unsigned m_bootstrap_exchange_count = 0;
	bootstrap_completion_profile m_bootstrap_completion =
			bootstrap_completion_profile::ready_words_one;
	bool m_bootstrap_ping_pong = false;
	bool m_code_block_request = false;
	bool m_parked_boot_status = false;
	u16 m_boot_status_response = 0;
	u16 m_mcu_control_word = 0;
	u16 m_mcu_control_wire = 0;
	u8 m_parameter_command = 0xff;
	u16 m_speech_request_mask = 0;
	u16 m_speech_request_value = 0;
	std::array<u16, 0x10000> m_data_memory = { 0 };
	std::array<u8, 0x10000> m_data_memory_loaded = { 0 };
	nokia_gsm_fr_codec m_speech_codec;
	nokia_gsm_fr_codec::state m_speech_codec_state{};
	nokia_gsm_fr_receiver m_speech_receiver;
	nokia_gsm_fr_receiver::state m_speech_receiver_state{};
	bool m_speech_active = false;
	bool m_pcm_link_fault = false;
	u64 m_speech_uplink_frames = 0;
	u64 m_speech_downlink_frames = 0;
	u64 m_speech_concealed_frames = 0;
	u64 m_speech_muted_frames = 0;
	u64 m_speech_nonzero_microphone_blocks = 0;
	u64 m_speech_nonzero_earpiece_blocks = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_DSP_HLE, nokia_dsp_hle_device)

#endif // MAME_NOKIA_NOKIA_DSP_HLE_H
