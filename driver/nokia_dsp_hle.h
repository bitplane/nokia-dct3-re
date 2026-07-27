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
#include <optional>

class nokia_dsp_hle_device : public device_t
{
public:
	nokia_dsp_hle_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	enum class bootstrap_exchange_strategy : u8
	{
		disabled,
		zero_acknowledge,
		ping_pong
	};

	struct bootstrap_publication
	{
		u16 offset = 0;
		u16 value = 0;
	};

	struct bootstrap_pair_contract
	{
		u16 first_offset = 0;
		u16 second_offset = 0;
		u16 sentinel = 0;
		u16 response = 0;
	};

	struct bootstrap_parked_contract
	{
		u16 offset = 0;
		u16 sentinel = 0;
		u16 response = 0;
	};

	struct bootstrap_contract
	{
		bootstrap_exchange_strategy exchange =
				bootstrap_exchange_strategy::disabled;
		unsigned exchange_limit = 0;
		std::array<bootstrap_publication, 3> completion = {};
		u8 completion_count = 0;
		std::optional<bootstrap_pair_contract> preupload;
		std::optional<bootstrap_parked_contract> parked;
		u16 service_code_block_request = 0;
	};

	struct service_control_contract
	{
		std::array<u8, 6> completion = { 0 };
		u8 completion_length = 0;

		constexpr bool enabled() const
		{
			return completion_length != 0 &&
					completion_length <= completion.size();
		}
	};

	struct speech_request_predicate
	{
		u16 mask = 0;
		u16 value = 0;
	};

	struct speech_control_contract
	{
		std::optional<u8> parameter_command;
		std::optional<speech_request_predicate> request;

		constexpr bool accepts_parameter_command(u16 wire) const
		{
			return parameter_command &&
					(wire >> 12) == *parameter_command;
		}

		constexpr bool speech_requested(u16 control) const
		{
			return request && request->mask != 0 &&
					(control & request->mask) == request->value;
		}
	};

	void set_service_enabled(bool enabled) { m_service_enabled = enabled; }
	void set_external_service_enabled(bool enabled) { m_external_service_enabled = enabled; }
	void set_service_control_contract(service_control_contract contract)
	{
		m_service_control = contract;
	}
	void set_service_delay_us(unsigned delay) { m_service_delay_us = delay; }
	void set_peer_poll_ms(unsigned period) { m_peer_poll_ms = period; }
	void set_speech_control_contract(speech_control_contract contract)
	{
		m_speech_control = contract;
	}
	void set_bootstrap_contract(bootstrap_contract contract)
	{
		m_bootstrap = contract;
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
	void shared_006_write_w(int state);
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
	void publish_bootstrap_completion();
	bool bootstrap_ping_pong() const;
	void handle_bootstrap_exchange_read(u16 offset);
	void handle_bootstrap_exchange_write(u16 offset);
	void handle_bootstrap_parked_write(u16 callback_offset);
	void handle_bootstrap_preupload_write(u16 callback_offset);
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
	service_control_contract m_service_control;
	bool m_trace_enabled = false;
	unsigned m_service_delay_us = 5'000;
	unsigned m_peer_poll_ms = 5;
	bool m_service_control_completion_sent = false;
	unsigned m_bootstrap_exchange_count = 0;
	bootstrap_contract m_bootstrap;
	u16 m_mcu_control_word = 0;
	u16 m_mcu_control_wire = 0;
	speech_control_contract m_speech_control;
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
