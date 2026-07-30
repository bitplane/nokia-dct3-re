// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz
#include "emu.h"
#include "emuopts.h"
#include "nokia_dsp_hle.h"

#define LOG_DSP_HLE (1U << 0)
#define VERBOSE (LOG_DSP_HLE)
#include "logmacro.h"

DEFINE_DEVICE_TYPE(NOKIA_DSP_HLE, nokia_dsp_hle_device, "nokia_dsp_hle", "Nokia DCT3 DSP HLE")

nokia_dsp_hle_device::nokia_dsp_hle_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_DSP_HLE, tag, owner, clock),
	m_transport(*this, "^dspif"),
	m_external_peer(*this, "^external_service_peer"),
	m_radio_peer(*this, "^radio_peer"),
	m_mad2_pcm(*this, "^mad2_pcm"),
	m_mcu_control_word_cb(*this)
{
}

void nokia_dsp_hle_device::device_start()
{
	m_trace_enabled = machine().options().verbose();
	m_service_timer = timer_alloc(FUNC(nokia_dsp_hle_device::service_tick), this);
	m_packet_timer = timer_alloc(FUNC(nokia_dsp_hle_device::packet_tick), this);
	m_response_timer = timer_alloc(FUNC(nokia_dsp_hle_device::response_tick), this);
	m_keepalive_timer = timer_alloc(FUNC(nokia_dsp_hle_device::keepalive_tick), this);
	m_speech_timer = timer_alloc(FUNC(nokia_dsp_hle_device::speech_tick), this);
	save_item(NAME(m_service_enabled));
	save_item(NAME(m_external_service_enabled));
	save_item(NAME(m_service_delay_us));
	save_item(NAME(m_peer_poll_ms));
	save_item(NAME(m_service_control_completion_sent));
	save_item(NAME(m_service_code_block_published));
	save_item(NAME(m_bootstrap_exchange_count));
	save_item(NAME(m_mcu_control_word));
	save_item(NAME(m_mcu_control_wire));
	save_item(NAME(m_data_memory));
	save_item(NAME(m_data_memory_loaded));
	save_item(NAME(m_speech_active));
	save_item(NAME(m_pcm_link_fault));
	save_item(NAME(m_speech_uplink_frames));
	save_item(NAME(m_speech_downlink_frames));
	save_item(NAME(m_speech_concealed_frames));
	save_item(NAME(m_speech_muted_frames));
	save_item(NAME(m_speech_nonzero_microphone_blocks));
	save_item(NAME(m_speech_nonzero_earpiece_blocks));
	save_item(STRUCT_MEMBER(m_speech_codec_state.channels, dp0));
	save_item(STRUCT_MEMBER(m_speech_codec_state.channels, e));
	save_item(STRUCT_MEMBER(m_speech_codec_state.channels, z1));
	save_item(STRUCT_MEMBER(m_speech_codec_state.channels, l_z2));
	save_item(STRUCT_MEMBER(m_speech_codec_state.channels, mp));
	save_item(STRUCT_MEMBER(m_speech_codec_state.channels, u));
	save_item(STRUCT_MEMBER(m_speech_codec_state.channels, larpp));
	save_item(STRUCT_MEMBER(m_speech_codec_state.channels, j));
	save_item(STRUCT_MEMBER(m_speech_codec_state.channels, ltp_cut));
	save_item(STRUCT_MEMBER(m_speech_codec_state.channels, nrp));
	save_item(STRUCT_MEMBER(m_speech_codec_state.channels, v));
	save_item(STRUCT_MEMBER(m_speech_codec_state.channels, msr));
	save_item(STRUCT_MEMBER(m_speech_codec_state.channels, verbose));
	save_item(STRUCT_MEMBER(m_speech_codec_state.channels, fast));
	save_item(STRUCT_MEMBER(m_speech_codec_state.channels, wav_fmt));
	save_item(STRUCT_MEMBER(m_speech_codec_state.channels, frame_index));
	save_item(STRUCT_MEMBER(m_speech_codec_state.channels, frame_chain));
	save_item(NAME(m_speech_receiver_state.last_good));
	save_item(NAME(m_speech_receiver_state.have_good));
	save_item(NAME(m_speech_receiver_state.lost_frames));
	machine().save().register_presave(
			save_prepost_delegate(
				FUNC(nokia_dsp_hle_device::prepare_speech_codec_save), this));
	machine().save().register_postload(
			save_prepost_delegate(
				FUNC(nokia_dsp_hle_device::restore_speech_codec_state), this));
}

void nokia_dsp_hle_device::device_reset()
{
	m_service_timer->adjust(attotime::never);
	m_packet_timer->adjust(attotime::never);
	m_response_timer->adjust(attotime::never);
	m_keepalive_timer->adjust(m_service_enabled ? attotime::from_seconds(1) : attotime::never,
			0, attotime::from_seconds(1));
	// The PCM endpoint owns converter rate and transfer-block shape. Derive
	// the DSP service cadence from that physical contract so a product cannot
	// silently combine one serial-bus profile with an unrelated HLE timer.
	const attotime speech_period = m_mad2_pcm->block_period();
	m_speech_timer->adjust(speech_period, 0, speech_period);
	m_service_control_completion_sent = false;
	m_service_code_block_published = false;
	m_bootstrap_exchange_count = 0;
	m_mcu_control_word = 0;
	m_mcu_control_wire = 0;
	std::fill(m_data_memory.begin(), m_data_memory.end(), 0);
	std::fill(m_data_memory_loaded.begin(), m_data_memory_loaded.end(), 0);
	m_speech_codec.reset();
	m_speech_receiver.reset();
	m_speech_active = false;
	m_pcm_link_fault = false;
	m_speech_uplink_frames = 0;
	m_speech_downlink_frames = 0;
	m_speech_concealed_frames = 0;
	m_speech_muted_frames = 0;
	m_speech_nonzero_microphone_blocks = 0;
	m_speech_nonzero_earpiece_blocks = 0;
	publish_bootstrap_state();
}

void nokia_dsp_hle_device::prepare_speech_codec_save()
{
	m_speech_codec_state = m_speech_codec.snapshot();
	m_speech_receiver_state = m_speech_receiver.snapshot();
}

void nokia_dsp_hle_device::restore_speech_codec_state()
{
	if (!m_speech_codec.restore(m_speech_codec_state))
		fatalerror("DSP HLE: invalid GSM-FR codec state in save image");
	if (!m_speech_receiver.restore(m_speech_receiver_state))
		fatalerror("DSP HLE: invalid GSM-FR receiver state in save image");
}

void nokia_dsp_hle_device::publish_bootstrap_state()
{
	// Transition timing is not recovered. Publish the minimum reset-visible DSP
	// state synchronously, through DSPIF-owned shared RAM, before the MCU starts.
	m_transport->peer_shared_w(0x0e0 / 2, 0);
	if (m_bootstrap.exchange ==
			bootstrap_exchange_strategy::zero_acknowledge)
	{
		m_transport->peer_shared_w(0x0fe / 2, 1);
		m_transport->peer_shared_w(0x100 / 2, 1);
	}
}

bool nokia_dsp_hle_device::bootstrap_ping_pong() const
{
	return m_bootstrap.exchange == bootstrap_exchange_strategy::ping_pong;
}

void nokia_dsp_hle_device::publish_bootstrap_completion()
{
	const unsigned count = std::min<unsigned>(
			m_bootstrap.completion_count, m_bootstrap.completion.size());
	for (unsigned index = 0; index < count; ++index)
	{
		const bootstrap_publication &publication =
				m_bootstrap.completion[index];
		m_transport->peer_shared_w(
				publication.offset / 2, publication.value);
		if (m_trace_enabled)
			LOGMASKED(LOG_DSP_HLE,
					"dsp_hle: bootstrap publication offset=%03x value=%04x t=%.6f\n",
					publication.offset, publication.value,
					machine().time().as_double());
	}
	if (m_trace_enabled)
		LOGMASKED(LOG_DSP_HLE,
				"dsp_hle: bootstrap completion exchanges=%u publications=%u t=%.6f\n",
				m_bootstrap_exchange_count, count,
				machine().time().as_double());
}

void nokia_dsp_hle_device::tx_commit_w(int state)
{
	if (state && (m_external_service_enabled || m_radio_peer->enabled() ||
			m_service_control.enabled()))
		m_packet_timer->adjust(attotime::from_usec(100));
}

void nokia_dsp_hle_device::service_pending_w(int state)
{
	if (state && m_service_enabled)
		m_service_timer->adjust(attotime::from_usec(m_service_delay_us));
}

void nokia_dsp_hle_device::doorbell_w(int state)
{
	if (state && m_transport->dspif_r(0) == 0 && m_transport->dspif_r(1) == 4)
	{
		m_mcu_control_wire = m_transport->shared_word(0x0a8 / 2);
		// The wire is multiplexed: bits 15..12 select one of the command
		// table's first sixteen entries and bits 11..0 carry its value.
		// Preserve command 0x08's applied state when later command 0x09
		// transactions reuse the same physical word.
		if (m_speech_control.accepts_parameter_command(m_mcu_control_wire))
		{
			m_mcu_control_word = m_mcu_control_wire & 0x0fff;
			m_mcu_control_word_cb(m_mcu_control_word);
		}
		m_transport->peer_shared_w(0x0e0 / 2, 0);
	}
	if (state && m_trace_enabled)
		LOGMASKED(LOG_DSP_HLE,
				"dsp_hle: doorbell pending=%04x wire=%04x speech_control=%04x t=%.6f\n",
				m_transport->service_pending(), m_mcu_control_wire,
				m_mcu_control_word,
				machine().time().as_double());
}

void nokia_dsp_hle_device::shared_002_write_w(int state)
{
	if (state)
		handle_bootstrap_parked_write(0x002);
}

void nokia_dsp_hle_device::handle_bootstrap_parked_write(
		u16 callback_offset)
{
	if (!m_bootstrap.parked)
		return;
	const bootstrap_parked_contract &parked = *m_bootstrap.parked;
	if (parked.offset == callback_offset &&
			m_transport->shared_word(parked.offset / 2) == parked.sentinel)
		m_transport->peer_shared_w(parked.offset / 2, parked.response);
}

void nokia_dsp_hle_device::shared_006_write_w(int state)
{
	if (state)
		handle_bootstrap_preupload_write(0x006);
}

void nokia_dsp_hle_device::handle_bootstrap_preupload_write(
		u16 callback_offset)
{
	if (!m_bootstrap.preupload)
		return;
	const bootstrap_pair_contract &preupload = *m_bootstrap.preupload;
	if (preupload.second_offset != callback_offset ||
			m_transport->shared_word(preupload.first_offset / 2) !=
					preupload.sentinel ||
			m_transport->shared_word(preupload.second_offset / 2) !=
					preupload.sentinel)
		return;

	m_transport->peer_shared_w(
			preupload.first_offset / 2, preupload.response);
	m_transport->peer_shared_w(
			preupload.second_offset / 2, preupload.response);
	if (m_trace_enabled)
		LOGMASKED(LOG_DSP_HLE,
				"dsp_hle: bootstrap preupload first=%03x second=%03x value=%04x t=%.6f\n",
				preupload.first_offset, preupload.second_offset,
				preupload.response, machine().time().as_double());
}

void nokia_dsp_hle_device::shared_0fe_read_w(int state)
{
	if (state)
		handle_bootstrap_exchange_read(0x0fe);
}

void nokia_dsp_hle_device::shared_0fe_write_w(int state)
{
	if (state)
		handle_bootstrap_exchange_write(0x0fe);
}

void nokia_dsp_hle_device::shared_100_read_w(int state)
{
	if (state)
		handle_bootstrap_exchange_read(0x100);
}

void nokia_dsp_hle_device::shared_100_write_w(int state)
{
	if (state)
		handle_bootstrap_exchange_write(0x100);
}

void nokia_dsp_hle_device::handle_bootstrap_exchange_read(u16 offset)
{
	if (!bootstrap_ping_pong() ||
			m_transport->shared_word(offset / 2) == 0 ||
			(offset == 0x100 &&
				m_transport->shared_word(0x1ca / 2) != 0))
		return;
	m_transport->peer_shared_w(offset / 2, 0);
}

void nokia_dsp_hle_device::handle_bootstrap_exchange_write(u16 offset)
{
	const u16 token = m_transport->shared_word(offset / 2);
	if (bootstrap_ping_pong())
	{
		if (offset == 0x100 &&
				m_transport->shared_word(0x1ca / 2) != 0)
			return;
		const u16 peer_offset = offset == 0x0fe ? 0x100 : 0x0fe;
		m_transport->peer_shared_w(offset / 2, 0);
		m_transport->peer_shared_w(
				peer_offset / 2, token != 0 ? token : 1);
	}
	else if (m_bootstrap.exchange ==
				bootstrap_exchange_strategy::zero_acknowledge &&
			token == 0)
	{
		m_transport->peer_shared_w(offset / 2, 1);
		if (offset == 0x100 &&
				++m_bootstrap_exchange_count ==
						m_bootstrap.exchange_limit)
			publish_bootstrap_completion();
	}
}

TIMER_CALLBACK_MEMBER(nokia_dsp_hle_device::service_tick)
{
	// NHM-2's DSP publishes the initial code-block selector. Firmware consumes
	// bounded chunks and eventually clears 0x0e2 itself before publishing final
	// state 4 at 0x0e4. Reasserting selector 1 on every IRQ completion restarts
	// that finite transfer indefinitely.
	if (!m_service_code_block_published &&
			m_bootstrap.service_code_block_request != 0)
	{
		m_transport->peer_shared_w(
				0x0e2 / 2, m_bootstrap.service_code_block_request);
		m_service_code_block_published = true;
		if (m_trace_enabled)
			LOGMASKED(LOG_DSP_HLE,
					"dsp_hle: service code-block request=%04x event=initial t=%.6f\n",
					m_bootstrap.service_code_block_request,
					machine().time().as_double());
	}
	m_transport->complete_service();
}

TIMER_CALLBACK_MEMBER(nokia_dsp_hle_device::keepalive_tick)
{
	// A running DSP continues to publish an idle group-0x03 indication. The MCU
	// treats any non-fault MDI packet as DSP activity and otherwise enters its
	// reason-0x68 terminal watchdog path after roughly 32 seconds. This packet
	// has no payload or higher-level completion semantics; it only traverses the
	// ordinary MDIRCV ring and FIQ0 boundary.
	if (m_transport->enqueue_rx_packet(0x03, nullptr, 0))
		m_transport->notify_rx();
}

TIMER_CALLBACK_MEMBER(nokia_dsp_hle_device::speech_tick)
{
	// Command 0x08 is a bit-field, not an enum. Across both NSE-8 ROMs the
	// non-speech dedicated-channel state is 0x040a; Answer adds field 0x0201,
	// and release removes that same field before the TCH is deconfigured.
	// Product configuration carries the recovered field so another MAD2/DSP
	// variant need not inherit NSE-8 wiring.
	const bool dsp_speech_route =
			m_speech_control.speech_requested(m_mcu_control_word);
	const bool requested =
			m_radio_peer->speech_channel_active() && dsp_speech_route;
	const bool pcm_link_ready = m_mad2_pcm->link_ready();
	const bool active = requested && pcm_link_ready;
	if (requested && !pcm_link_ready && !m_pcm_link_fault)
		LOGMASKED(LOG_DSP_HLE,
				"dsp_hle: speech blocked by unsupported PCM link "
				"control=%04x enabled=%u clock=%u/%u shape=%u t=%.6f\n",
				m_mcu_control_word, m_mad2_pcm->enabled(),
				m_mad2_pcm->data_clock(),
				m_mad2_pcm->frame_clock(),
				m_mad2_pcm->data_clocks_per_frame(),
				machine().time().as_double());
	m_pcm_link_fault = requested && !pcm_link_ready;
	if (!active)
	{
		if (m_speech_active && m_trace_enabled)
			LOGMASKED(LOG_DSP_HLE,
					"dsp_hle: speech stop control=%04x uplink=%llu downlink=%llu t=%.6f\n",
					m_mcu_control_word, m_speech_uplink_frames,
					m_speech_downlink_frames, machine().time().as_double());
		m_speech_active = false;
		return;
	}
	if (!m_speech_active)
	{
		// GSM-FR predictor state belongs to one continuous traffic-channel
		// activation and must not leak from a previous call.
		m_speech_codec.reset();
		m_speech_receiver.reset();
		m_speech_active = true;
	}

	nokia_mad2_pcm_device::pcm_block earpiece{};
	nokia_radio_peer_device::speech_frame radio_frame{};
	const auto radio_delivery =
			m_radio_peer->take_downlink_speech(radio_frame);
	nokia_gsm_fr_codec::speech_frame downlink{};
	const nokia_gsm_fr_codec::speech_frame *decoder_frame = nullptr;
	if (radio_delivery ==
			nokia_radio_peer_device::speech_delivery::good)
	{
		std::copy(radio_frame.begin(), radio_frame.end(), downlink.begin());
		decoder_frame = &downlink;
	}
	nokia_gsm_fr_codec::pcm_block decoder_output{};
	if (radio_delivery !=
			nokia_radio_peer_device::speech_delivery::none &&
			m_speech_receiver.decode(
			m_speech_codec, decoder_frame, decoder_output))
	{
		std::copy(decoder_output.begin(), decoder_output.end(), earpiece.begin());
		if (decoder_frame)
			++m_speech_downlink_frames;
		else
		{
			++m_speech_concealed_frames;
			if (m_speech_receiver.lost_frames() >=
					nokia_gsm_fr_receiver::mute_after_lost_frames)
				++m_speech_muted_frames;
		}
	}

	nokia_mad2_pcm_device::pcm_block microphone{};
	if (!m_mad2_pcm->transfer_frame_block(earpiece, microphone))
	{
		LOGMASKED(LOG_DSP_HLE,
				"dsp_hle: speech PCM transfer rejected failures=%llu t=%.6f\n",
				m_mad2_pcm->transfer_failures(),
				machine().time().as_double());
		return;
	}
	const auto block_peak = [] (const auto &block)
	{
		u16 peak = 0;
		for (s16 sample : block)
			peak = std::max<u16>(peak,
					u16(sample < 0 ? -s32(sample) : s32(sample)));
		return peak;
	};
	const u16 microphone_peak = block_peak(microphone);
	const u16 earpiece_peak = block_peak(earpiece);
	if (microphone_peak)
		++m_speech_nonzero_microphone_blocks;
	if (earpiece_peak)
		++m_speech_nonzero_earpiece_blocks;
	nokia_gsm_fr_codec::pcm_block encoder_input{};
	std::copy(microphone.begin(), microphone.end(), encoder_input.begin());
	nokia_gsm_fr_codec::speech_frame uplink{};
	if (m_speech_codec.encode(encoder_input, uplink))
	{
		nokia_radio_peer_device::speech_frame radio_frame{};
		std::copy(uplink.begin(), uplink.end(), radio_frame.begin());
		if (m_radio_peer->submit_uplink_speech(radio_frame))
			++m_speech_uplink_frames;
	}

	if (m_trace_enabled &&
			(m_speech_uplink_frames <= 3 || (m_speech_uplink_frames % 50) == 0))
		LOGMASKED(LOG_DSP_HLE,
				"dsp_hle: speech tick uplink=%llu downlink=%llu pcm=%llu "
				"pcm_clock=%u/%u pcm_shape=%u serial_clocks=%llu/%llu "
				"mic_peak=%u ear_peak=%u nonzero=%llu/%llu "
				"concealed=%llu muted=%llu t=%.6f\n",
				m_speech_uplink_frames, m_speech_downlink_frames,
				m_mad2_pcm->blocks_transferred(), m_mad2_pcm->data_clock(),
				m_mad2_pcm->frame_clock(),
				m_mad2_pcm->data_clocks_per_frame(),
				m_mad2_pcm->data_clocks_transferred(),
				m_mad2_pcm->idle_clocks_transferred(),
				microphone_peak, earpiece_peak,
				m_speech_nonzero_microphone_blocks,
				m_speech_nonzero_earpiece_blocks,
				m_speech_concealed_frames, m_speech_muted_frames,
				machine().time().as_double());
}

void nokia_dsp_hle_device::drain_responses()
{
	nokia_external_service_peer_device::response response;
	// The DSP-facing service link is serialized. Delivering the entire peer
	// queue on one timer edge can reorder firmware lifecycle work around its
	// scheduler delays even though the individual frames are valid.
	if (m_external_peer->peek_response(response) &&
			m_transport->enqueue_rx_packet(response.type, response.payload.data(), response.length))
	{
		if (m_trace_enabled)
			LOGMASKED(LOG_DSP_HLE, "dsp_hle: peer RX type=%02x length=%u t=%.6f\n",
					response.type, response.length, machine().time().as_double());
		m_external_peer->consume_response();
		if (response.notify)
			m_transport->notify_rx();
	}
}

void nokia_dsp_hle_device::schedule_response()
{
	nokia_external_service_peer_device::response response;
	if (m_response_timer->remaining().is_never() && m_external_peer->peek_response(response))
	{
		if (m_trace_enabled)
			LOGMASKED(LOG_DSP_HLE, "dsp_hle: peer RX scheduled length=%u delay=%.6f t=%.6f\n",
					response.length, attotime::from_ticks(response.length * 10, 9'600).as_double(),
					machine().time().as_double());
		m_response_timer->adjust(attotime::from_ticks(response.length * 10, 9'600));
	}
}

TIMER_CALLBACK_MEMBER(nokia_dsp_hle_device::response_tick)
{
	drain_responses();
	nokia_external_service_peer_device::response response;
	if (m_external_peer->peek_response(response))
		m_response_timer->adjust(attotime::from_ticks(response.length * 10, 9'600));
}

bool nokia_dsp_hle_device::consume_memory_upload(const nokia_dspif_device::packet &packet)
{
	// Type 0x51 carries a big-endian DSP word address followed by a contiguous
	// sequence of big-endian data words.  The firmware fragments one logical
	// image into transport-sized packets; it neither requests nor receives a
	// per-fragment acknowledgement.
	if (packet.type != 0x51 || packet.length < 4 || BIT(packet.length, 0))
		return false;

	u16 address = (u16(packet.payload[0]) << 8) | packet.payload[1];
	const u16 first = address;
	for (unsigned index = 2; index < packet.length; index += 2, ++address)
	{
		m_data_memory[address] = (u16(packet.payload[index]) << 8) | packet.payload[index + 1];
		m_data_memory_loaded[address] = 1;
	}
	if (m_trace_enabled)
		LOGMASKED(LOG_DSP_HLE,
				"dsp_hle: data memory upload first=%04x words=%u last=%04x t=%.6f\n",
				first, (packet.length - 2) / 2, u16(address - 1),
				machine().time().as_double());
	return true;
}

TIMER_CALLBACK_MEMBER(nokia_dsp_hle_device::packet_tick)
{
	if (m_external_service_enabled || m_radio_peer->enabled() ||
			m_service_control.enabled())
	{
		nokia_dspif_device::packet packet;
		while (m_transport->peek_tx_packet(packet))
		{
			consume_memory_upload(packet);
			if (m_external_service_enabled && packet.type == 0x05 &&
					packet.length >= 9 && packet.length <= 75)
				m_external_peer->receive_frame(packet.payload.data(), packet.length);
			if (m_service_control.enabled() &&
					!m_service_control_completion_sent &&
					packet.type == 0x70 && packet.length == 2 &&
					packet.payload[0] == 0x0d && packet.payload[1] == 0x00)
			{
				if (m_transport->enqueue_rx_packet(
						0x74, m_service_control.completion.data(),
						m_service_control.completion_length))
				{
					m_service_control_completion_sent = true;
					if (m_external_service_enabled)
						m_external_peer->set_service_control_complete();
					m_transport->notify_rx();
				}
			}
			if (m_radio_peer->enabled())
				m_radio_peer->receive_packet(packet);
			if (m_trace_enabled)
			{
				std::string payload_hex;
				const bool cipher_secret =
						packet.type == 0x14 && packet.length >= 10 &&
						packet.payload[0] != 0x00;
				if (cipher_secret)
					payload_hex = "<redacted>";
				else
					for (unsigned index = 0; index < packet.length; ++index)
						payload_hex += util::string_format(
								"%02x", packet.payload[index]);
				LOGMASKED(LOG_DSP_HLE, "dsp_hle: TX packet type=%02x payload=%u words=%u radio_phase=%s data=%s t=%.6f\n",
						packet.type, packet.length, packet.words, m_radio_peer->phase_name(), payload_hex,
						machine().time().as_double());
			}
			m_transport->consume_tx_packet(packet);
		}
		m_radio_peer->tick();
		if (m_external_service_enabled)
		{
			m_external_peer->tick();
			schedule_response();
		}
	}
	m_packet_timer->adjust(m_radio_peer->fast_completion_pending() ? attotime::from_usec(50) :
			attotime::from_msec(m_peer_poll_ms));
}
