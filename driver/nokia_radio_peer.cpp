// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz
#include "emu.h"
#include "emuopts.h"
#include "nokia_radio_peer.h"

#define LOG_RADIO (1U << 0)
#define VERBOSE (LOG_RADIO)
#include "logmacro.h"

DEFINE_DEVICE_TYPE(NOKIA_RADIO_PEER, nokia_radio_peer_device,
		"nokia_radio_peer", "Nokia DCT3 radio peer HLE")

bool nokia_radio_peer_device::queue_host_incoming_call(
		const u8 *digits, unsigned length)
{
	if (!m_enabled || !m_registered ||
			m_host_incoming_call_pending || !m_gsm_session->idle() ||
			!m_gsm_session->set_incoming_caller(digits, length))
		return false;
	// The fixture latch is one-shot; a fresh host transaction deliberately
	// rearms paging only after the preceding firmware-owned call is idle.
	m_page_transmitted = false;
	m_host_incoming_call_pending = true;
	return true;
}

nokia_radio_peer_device::nokia_radio_peer_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_RADIO_PEER, tag, owner, clock),
	m_transport(*this, "^dspif"),
	m_gsm_network(*this, "^gsm_network"),
	m_gsm_session(*this, "^gsm_session"),
	m_voice_peer(*this, "^gsm_voice_peer"),
	m_lapdm_link(*this, "^lapdm_link")
{
}

void nokia_radio_peer_device::device_start()
{
	m_trace_enabled = machine().options().verbose();
	m_burst_timer = timer_alloc(FUNC(nokia_radio_peer_device::burst_tick), this);
	// One assigned timeslot per GSM TDMA frame: 60/13 ms exactly.
	m_burst_timer->adjust(
			attotime::from_ticks(60, 13'000), 0,
			attotime::from_ticks(60, 13'000));
	save_item(NAME(m_enabled));
	save_item(NAME(m_reports_sent));
	save_item(NAME(m_reports_remaining));
	save_item(NAME(m_phase));
	save_item(NAME(m_search_round));
	save_item(NAME(m_idle_measurement_sample));
	save_item(NAME(m_wait_ticks));
	save_item(NAME(m_search_mode));
	save_item(NAME(m_search_arfcns));
	save_item(NAME(m_search_arfcn_count));
	save_item(NAME(m_neighbour_arfcns));
	save_item(NAME(m_neighbour_arfcn_count));
	save_item(NAME(m_neighbour_bcch_pending));
	save_item(NAME(m_neighbour_bcch_arfcn));
	save_item(NAME(m_neighbour_instruction_mode));
	save_item(NAME(m_neighbour_instruction_bsic));
	save_item(NAME(m_last_neighbour_instruction_arfcn));
	save_item(NAME(m_last_neighbour_instruction_bsic));
	save_item(NAME(m_neighbour_resume_wait_ticks));
	save_item(NAME(m_access_ra));
	save_item(NAME(m_access_frame));
	save_item(NAME(m_search_has_serving_arfcn));
	save_item(NAME(m_serving_arfcn));
	save_item(NAME(m_receiver_arfcn));
	save_item(NAME(m_receiver_bsic));
	save_item(NAME(m_sch_observation_arfcn));
	save_item(NAME(m_candidate_bcch_valid));
	save_item(NAME(m_reselection_validation_pending));
	save_item(NAME(m_downlink_signalling_count));
	save_item(NAME(m_downlink_signalling_failed));
	save_item(NAME(m_serving_loss_pending));
	save_item(NAME(m_report_deferred));
	save_item(NAME(m_search_requested));
	save_item(NAME(m_selected_reports_remaining));
	save_item(NAME(m_registered));
	save_item(NAME(m_idle_common_control_active));
	save_item(NAME(m_has_reselected));
	save_item(NAME(m_pch_fill_delivered));
	save_item(NAME(m_page_transmitted));
	save_item(NAME(m_host_incoming_call_pending));
	save_item(NAME(m_traffic_channel_active));
	save_item(NAME(m_downlink_offset));
	save_item(NAME(m_followup_downlink_opportunity));
	save_item(NAME(m_downlink_speech));
	save_item(NAME(m_downlink_speech_good));
	save_item(NAME(m_uplink_speech));
	save_item(NAME(m_downlink_speech_head));
	save_item(NAME(m_downlink_speech_count));
	save_item(NAME(m_uplink_speech_head));
	save_item(NAME(m_uplink_speech_count));
	save_item(NAME(m_speech_loopback));
	save_item(NAME(m_lab_voice_source));
	save_item(NAME(m_host_voice_peer));
	save_item(NAME(m_uplink_speech_received));
	save_item(NAME(m_tdma_frame_number));
	save_item(NAME(m_bcch_frame_number));
	save_item(NAME(m_bcch_frame_valid));
	save_item(NAME(m_l1_traffic_active));
	save_item(NAME(m_uplink_facch_blocks));
	save_item(NAME(m_downlink_facch_blocks));
	save_item(NAME(m_uplink_bad_speech_blocks));
	save_item(NAME(m_downlink_bad_speech_blocks));
	save_item(NAME(m_sacch_slots));
	save_item(NAME(m_uplink_tch_burst_error_period));
	save_item(NAME(m_uplink_tch_burst_error_span));
	save_item(NAME(m_uplink_tch_bursts));
	save_item(NAME(m_uplink_tch_bursts_impaired));
	save_item(NAME(m_downlink_tch_burst_error_period));
	save_item(NAME(m_downlink_tch_burst_error_span));
	save_item(NAME(m_downlink_tch_bursts));
	save_item(NAME(m_downlink_tch_bursts_impaired));
	save_item(NAME(m_uplink_ciphered_bursts));
	save_item(NAME(m_downlink_ciphered_bursts));
	save_item(NAME(m_uplink_ciphered_xcch_blocks));
	save_item(NAME(m_downlink_ciphered_xcch_blocks));

	auto save_diagonal_transmitter =
			[this](gsm::tch_f::diagonal_transmitter &endpoint, int index)
	{
		auto &state = endpoint.live_state();
		save_item(STRUCT_MEMBER(state.queue, coded), index);
		save_item(NAME(state.head), index);
		save_item(NAME(state.count), index);
		save_item(NAME(state.phase), index);
		save_item(STRUCT_MEMBER(state.previous, data), index);
		save_item(STRUCT_MEMBER(state.previous, hl), index);
		save_item(STRUCT_MEMBER(state.previous, hu), index);
		save_item(STRUCT_MEMBER(state.current, data), index);
		save_item(STRUCT_MEMBER(state.current, hl), index);
		save_item(STRUCT_MEMBER(state.current, hu), index);
	};
	auto save_diagonal_receiver =
			[this](gsm::tch_f::diagonal_receiver &endpoint, int index)
	{
		auto &state = endpoint.live_state();
		save_item(NAME(state.phase), index);
		save_item(NAME(state.pending_valid), index);
		save_item(STRUCT_MEMBER(state.pending, data), index);
		save_item(STRUCT_MEMBER(state.pending, hl), index);
		save_item(STRUCT_MEMBER(state.pending, hu), index);
		save_item(STRUCT_MEMBER(state.incoming, data), index);
		save_item(STRUCT_MEMBER(state.incoming, hl), index);
		save_item(STRUCT_MEMBER(state.incoming, hu), index);
	};
	auto save_sacch_transmitter =
			[this](gsm::tch_f::sacch_transmitter &endpoint, int index)
	{
		auto &state = endpoint.live_state();
		save_item(NAME(state.pending), index);
		save_item(NAME(state.phase), index);
		save_item(STRUCT_MEMBER(state.bursts, data), index);
		save_item(STRUCT_MEMBER(state.bursts, hl), index);
		save_item(STRUCT_MEMBER(state.bursts, hu), index);
	};
	auto save_sacch_receiver =
			[this](gsm::tch_f::sacch_receiver &endpoint, int index)
	{
		auto &state = endpoint.live_state();
		save_item(NAME(state.phase), index);
		save_item(STRUCT_MEMBER(state.bursts, data), index);
		save_item(STRUCT_MEMBER(state.bursts, hl), index);
		save_item(STRUCT_MEMBER(state.bursts, hu), index);
	};

	// These indexes distinguish identical generic endpoint fields in MAME's
	// save registry. Preserve every half-block rather than inventing an
	// erasure, dropping FACCH, or restarting SACCH after a state load.
	save_diagonal_transmitter(m_uplink_transmitter, 0);
	save_diagonal_transmitter(m_downlink_transmitter, 1);
	save_diagonal_receiver(m_network_receiver, 2);
	save_diagonal_receiver(m_handset_receiver, 3);
	save_sacch_transmitter(m_uplink_sacch_transmitter, 4);
	save_sacch_transmitter(m_downlink_sacch_transmitter, 5);
	save_sacch_receiver(m_network_sacch_receiver, 6);
	save_sacch_receiver(m_handset_sacch_receiver, 7);
	save_item(NAME(m_uplink_l1_block_kinds));
	save_item(NAME(m_downlink_l1_block_kinds));
	machine().save().register_presave(
			save_prepost_delegate(
				FUNC(nokia_radio_peer_device::prepare_l1_save), this));
	machine().save().register_postload(
			save_prepost_delegate(
				FUNC(nokia_radio_peer_device::restore_l1_block_kinds), this));
}

void nokia_radio_peer_device::device_reset()
{
	m_reports_sent = 0;
	m_reports_remaining = 0;
	set_phase(phase::inactive);
	m_search_round = 0;
	m_idle_measurement_sample = 0;
	m_wait_ticks = 0;
	m_search_mode = 0;
	m_search_arfcns.fill(0xffff);
	m_search_arfcn_count = 0;
	m_neighbour_arfcns.fill(0xffff);
	m_neighbour_arfcn_count = 0;
	m_neighbour_bcch_pending = false;
	m_neighbour_bcch_arfcn = 0xffff;
	m_neighbour_instruction_mode = 0;
	m_neighbour_instruction_bsic = 0;
	m_last_neighbour_instruction_arfcn = 0xffff;
	m_last_neighbour_instruction_bsic = 0;
	m_neighbour_resume_wait_ticks = 0;
	m_access_ra = 0;
	m_access_frame = 0;
	m_search_has_serving_arfcn = false;
	m_serving_arfcn = 1;
	m_receiver_arfcn = 1;
	m_receiver_bsic = 0x12;
	m_sch_observation_arfcn = 0xffff;
	m_candidate_bcch_valid = false;
	m_reselection_validation_pending = false;
	restart_downlink_signalling_counter();
	m_serving_loss_pending = false;
	m_report_deferred = false;
	m_search_requested = false;
	m_selected_reports_remaining = 0;
	m_registered = false;
	m_idle_common_control_active = false;
	m_has_reselected = false;
	m_pch_fill_delivered = false;
	m_page_transmitted = false;
	m_host_incoming_call_pending = false;
	m_traffic_channel_active = false;
	m_downlink_offset = 0;
	m_followup_downlink_opportunity = false;
	clear_speech_queues();
	m_uplink_speech_received = 0;
	m_tdma_frame_number = 0;
	m_bcch_frame_number = 0;
	m_bcch_frame_valid = false;
	m_l1_traffic_active = false;
	m_uplink_facch_blocks = 0;
	m_downlink_facch_blocks = 0;
	m_uplink_bad_speech_blocks = 0;
	m_downlink_bad_speech_blocks = 0;
	m_sacch_slots = 0;
	m_uplink_tch_bursts = 0;
	m_uplink_tch_bursts_impaired = 0;
	m_downlink_tch_bursts = 0;
	m_downlink_tch_bursts_impaired = 0;
	m_uplink_ciphered_bursts = 0;
	m_downlink_ciphered_bursts = 0;
	m_uplink_ciphered_xcch_blocks = 0;
	m_downlink_ciphered_xcch_blocks = 0;
	reset_l1_pipeline();
}

void nokia_radio_peer_device::prepare_l1_save()
{
	const auto &uplink = m_uplink_transmitter.live_state();
	const auto &downlink = m_downlink_transmitter.live_state();
	for (unsigned k = 0; k < gsm::tch_f::diagonal_transmitter::queue_depth; ++k)
	{
		m_uplink_l1_block_kinds[k] = u8(uplink.queue[k].kind);
		m_downlink_l1_block_kinds[k] = u8(downlink.queue[k].kind);
	}
}

void nokia_radio_peer_device::restore_l1_block_kinds()
{
	auto &uplink = m_uplink_transmitter.live_state();
	auto &downlink = m_downlink_transmitter.live_state();
	for (unsigned k = 0; k < gsm::tch_f::diagonal_transmitter::queue_depth; ++k)
	{
		uplink.queue[k].kind =
				gsm::tch_f::traffic_block_kind(m_uplink_l1_block_kinds[k]);
		downlink.queue[k].kind =
				gsm::tch_f::traffic_block_kind(m_downlink_l1_block_kinds[k]);
	}
}

nokia_lapdm_link_device::uplink_result
nokia_radio_peer_device::receive_lapdm_uplink(
		const u8 *frame, unsigned length)
{
	if (length < gsm::tch_f::packed_control_block{}.size())
		return nokia_lapdm_link_device::uplink_result::ignored;
	gsm::xcch::block clear;
	std::copy_n(frame, clear.size(), clear.begin());
	const bool ciphered = m_gsm_session->cipher_active() ||
			m_gsm_session->cipher_command_pending();
	const gsm::xcch::cipher_context context{
		ciphered ? m_gsm_session->cipher_algorithm() :
				gsm::a5::algorithm::a5_0,
		m_gsm_session->cipher_key(), gsm::a5::direction::uplink
	};
	const auto frames = gsm::xcch::sdcch8_subchannel0_frames(
			m_tdma_frame_number, gsm::a5::direction::uplink);
	const auto decoded = gsm::xcch::transport(clear, frames, context, context);
	if (!decoded.good)
		return nokia_lapdm_link_device::uplink_result::ignored;
	if (ciphered)
		++m_uplink_ciphered_xcch_blocks;
	if (ciphered && m_trace_enabled && m_uplink_ciphered_xcch_blocks == 1)
		LOGMASKED(LOG_RADIO,
				"radio_l1: kind=xcch direction=uplink algorithm=%u first_fn=%u last_fn=%u\n",
				u8(context.algorithm), frames.front(), frames.back());
	return m_lapdm_link->receive_uplink(
			decoded.data.data(), decoded.data.size());
}

void nokia_radio_peer_device::deliver_lapdm_downlink(
		const std::array<u8, nokia_lapdm_link_device::frame_length> &frame,
		u8 *payload, u32 reference_frame)
{
	gsm::xcch::block clear;
	std::copy_n(frame.begin(), clear.size(), clear.begin());
	const bool ciphered = m_gsm_session->cipher_active();
	const gsm::xcch::cipher_context context{
		ciphered ? m_gsm_session->cipher_algorithm() :
				gsm::a5::algorithm::a5_0,
		m_gsm_session->cipher_key(), gsm::a5::direction::downlink
	};
	const auto frames = gsm::xcch::sdcch8_subchannel0_frames(
			reference_frame, gsm::a5::direction::downlink);
	const auto decoded = gsm::xcch::transport(clear, frames, context, context);
	if (!decoded.good)
		return;
	std::copy(decoded.data.begin(), decoded.data.end(), payload);
	payload[decoded.data.size()] = frame[decoded.data.size()];
	if (ciphered)
		++m_downlink_ciphered_xcch_blocks;
	if (ciphered && m_trace_enabled && m_downlink_ciphered_xcch_blocks == 1)
		LOGMASKED(LOG_RADIO,
				"radio_l1: kind=xcch direction=downlink algorithm=%u first_fn=%u last_fn=%u\n",
				u8(context.algorithm), frames.front(), frames.back());
}

bool nokia_radio_peer_device::apply_active_cipher(
		gsm::tch_f::burst_payload &payload, u32 frame_number,
		gsm::a5::direction direction) const
{
	if (!m_gsm_session->cipher_active())
		return true;
	return gsm::a5::apply(payload, m_gsm_session->cipher_algorithm(),
			m_gsm_session->cipher_key(),
			gsm::a5::count_from_frame_number(frame_number), direction);
}

bool nokia_radio_peer_device::speech_channel_active() const
{
	// This is the physical, speech-mode TCH/F assigned by RR. Call-control
	// state and handset PCM routing are deliberately separate: the TCH exists
	// while the phone rings and briefly while release signalling uses FACCH.
	return m_enabled && m_traffic_channel_active;
}

bool nokia_radio_peer_device::speech_queue_push(
		std::array<speech_frame, speech_queue_depth> &queue,
		u8 &head, u8 &count, const speech_frame &frame)
{
	if (count == speech_queue_depth)
		return false;
	queue[(head + count) % speech_queue_depth] = frame;
	++count;
	return true;
}

bool nokia_radio_peer_device::speech_queue_pop(
		std::array<speech_frame, speech_queue_depth> &queue,
		u8 &head, u8 &count, speech_frame &frame)
{
	if (!count)
		return false;
	frame = queue[head];
	head = (head + 1) % speech_queue_depth;
	--count;
	return true;
}

bool nokia_radio_peer_device::queue_downlink_speech(const speech_frame &frame)
{
	return queue_downlink_delivery(frame, true);
}

bool nokia_radio_peer_device::queue_downlink_delivery(
		const speech_frame &frame, bool good)
{
	if (!speech_channel_active())
		return false;
	if (m_downlink_speech_count == speech_queue_depth)
		return false;
	m_downlink_speech_good[
			(m_downlink_speech_head + m_downlink_speech_count) %
				speech_queue_depth] = good;
	return speech_queue_push(
			m_downlink_speech, m_downlink_speech_head,
			m_downlink_speech_count, frame);
}

nokia_radio_peer_device::speech_delivery
nokia_radio_peer_device::take_downlink_speech(speech_frame &frame)
{
	if (!speech_channel_active())
		return speech_delivery::none;
	if (!m_downlink_speech_count)
		return speech_delivery::none;
	const speech_delivery result =
			m_downlink_speech_good[m_downlink_speech_head]
				? speech_delivery::good
				: speech_delivery::bad;
	if (!speech_queue_pop(
			m_downlink_speech, m_downlink_speech_head,
			m_downlink_speech_count, frame))
		return speech_delivery::none;
	return result;
}

bool nokia_radio_peer_device::submit_uplink_speech(const speech_frame &frame)
{
	if (!speech_channel_active())
		return false;
	return speech_queue_push(
			m_uplink_speech, m_uplink_speech_head,
			m_uplink_speech_count, frame);
}

bool nokia_radio_peer_device::take_uplink_speech(speech_frame &frame)
{
	return speech_queue_pop(
			m_uplink_speech, m_uplink_speech_head,
			m_uplink_speech_count, frame);
}

bool nokia_radio_peer_device::queue_downlink_sacch(
		const gsm::tch_f::packed_control_block &block)
{
	return speech_channel_active() &&
			m_downlink_sacch_transmitter.enqueue(
				gsm::tch_f::unpack_control(block));
}

bool nokia_radio_peer_device::submit_uplink_sacch(
		const gsm::tch_f::packed_control_block &block)
{
	return speech_channel_active() &&
			m_uplink_sacch_transmitter.enqueue(
				gsm::tch_f::unpack_control(block));
}

void nokia_radio_peer_device::clear_speech_queues()
{
	m_downlink_speech_head = 0;
	m_downlink_speech_count = 0;
	m_uplink_speech_head = 0;
	m_uplink_speech_count = 0;
}

void nokia_radio_peer_device::reset_l1_pipeline()
{
	m_uplink_transmitter.reset();
	m_network_receiver.reset();
	m_downlink_transmitter.reset();
	m_handset_receiver.reset();
	m_uplink_sacch_transmitter.reset();
	m_network_sacch_receiver.reset();
	m_downlink_sacch_transmitter.reset();
	m_handset_sacch_receiver.reset();
}

TIMER_CALLBACK_MEMBER(nokia_radio_peer_device::burst_tick)
{
	const u32 frame_number = m_tdma_frame_number++;
	if (!speech_channel_active())
	{
		if (m_l1_traffic_active)
		{
			reset_l1_pipeline();
			m_l1_traffic_active = false;
		}
		return;
	}
	if (!m_l1_traffic_active)
	{
		reset_l1_pipeline();
		m_voice_peer->start_call();
		m_l1_traffic_active = true;
	}

	// The laboratory RR assignment is TCH/F timeslot 1. Its frame 12 is idle
	// and frame 25 is the SACCH/TF position; neither advances the diagonal TCH
	// interleaver.
	const auto slot = gsm::tch_f::full_rate_slot(frame_number, 1);
	if (slot == gsm::tch_f::tdma_slot_kind::idle)
		return;
	if (slot == gsm::tch_f::tdma_slot_kind::sacch)
	{
		const unsigned phase = gsm::tch_f::sacch_burst_index(frame_number, 1);
		++m_sacch_slots;
		if (m_trace_enabled)
			LOGMASKED(LOG_RADIO,
					"radio_l1: kind=sacch slot=%llu phase=%u "
					"uplink_pending=%u downlink_pending=%u fn=%u t=%.6f\n",
					m_sacch_slots, phase,
					m_uplink_sacch_transmitter.snapshot().pending,
					m_downlink_sacch_transmitter.snapshot().pending,
					frame_number, machine().time().as_double());
		const auto training = gsm::tch_f::training_sequence(2);
		if (const auto uplink =
					m_uplink_sacch_transmitter.next_burst(phase))
		{
			auto transmitted = *uplink;
			if (m_gsm_session->cipher_active())
			{
				if (!apply_active_cipher(transmitted, frame_number,
						gsm::a5::direction::uplink))
					return;
				++m_uplink_ciphered_bursts;
			}
			const auto air =
					gsm::tch_f::pack_normal_burst(transmitted, training);
			auto received = gsm::tch_f::unpack_normal_burst(air);
			if (!apply_active_cipher(received, frame_number,
					gsm::a5::direction::uplink))
				return;
			m_network_sacch_receiver.receive(received);
		}
		if (const auto downlink =
					m_downlink_sacch_transmitter.next_burst(phase))
		{
			auto transmitted = *downlink;
			if (m_gsm_session->cipher_active())
			{
				if (!apply_active_cipher(transmitted, frame_number,
						gsm::a5::direction::downlink))
					return;
				++m_downlink_ciphered_bursts;
			}
			const auto air =
					gsm::tch_f::pack_normal_burst(transmitted, training);
			auto received = gsm::tch_f::unpack_normal_burst(air);
			if (!apply_active_cipher(received, frame_number,
					gsm::a5::direction::downlink))
				return;
			m_handset_sacch_receiver.receive(received);
		}
		return;
	}

	// Keep the DSP's 20 ms codec clock independent. At each four-traffic-burst
	// block boundary, consume at most the next frame it has made available.
	speech_frame uplink{};
	if (take_uplink_speech(uplink))
	{
		gsm::tch_f::packed_speech_frame packed{};
		std::copy(uplink.begin(), uplink.end(), packed.begin());
		m_uplink_transmitter.enqueue(
				{gsm::tch_f::encode_speech(packed),
					gsm::tch_f::traffic_block_kind::speech});
	}

	const auto training = gsm::tch_f::training_sequence(2);
	auto uplink_payload = m_uplink_transmitter.next_burst();
	++m_uplink_tch_bursts;
	if (m_uplink_tch_burst_error_period &&
			((m_uplink_tch_bursts - 1) %
				m_uplink_tch_burst_error_period) <
					m_uplink_tch_burst_error_span &&
			!uplink_payload.hl && !uplink_payload.hu)
	{
		gsm::tch_f::invert_data_bits(uplink_payload);
		++m_uplink_tch_bursts_impaired;
		if (m_trace_enabled)
			LOGMASKED(LOG_RADIO,
					"radio_l1: direction=uplink impairment=invert-data "
					"burst=%llu count=%llu fn=%u t=%.6f\n",
					m_uplink_tch_bursts, m_uplink_tch_bursts_impaired,
					frame_number, machine().time().as_double());
	}
	auto uplink_air =
			gsm::tch_f::pack_normal_burst(uplink_payload, training);
	auto network_payload = gsm::tch_f::unpack_normal_burst(uplink_air);
	if (m_gsm_session->cipher_active())
	{
		if (!apply_active_cipher(uplink_payload, frame_number,
				gsm::a5::direction::uplink))
			return;
		uplink_air = gsm::tch_f::pack_normal_burst(uplink_payload, training);
		network_payload = gsm::tch_f::unpack_normal_burst(uplink_air);
		if (!apply_active_cipher(network_payload, frame_number,
				gsm::a5::direction::uplink))
			return;
		++m_uplink_ciphered_bursts;
		if (m_trace_enabled && m_uplink_ciphered_bursts == 1)
			LOGMASKED(LOG_RADIO,
					"radio_l1: kind=cipher direction=uplink algorithm=%u fn=%u count=%u\n",
					u8(m_gsm_session->cipher_algorithm()), frame_number,
					gsm::a5::count_from_frame_number(frame_number));
	}
	const auto network_block = m_network_receiver.receive(network_payload);
	if (network_block &&
			network_block->kind == gsm::tch_f::traffic_block_kind::facch)
	{
		if (network_block->control.good)
			++m_uplink_facch_blocks;
		if (m_trace_enabled)
			LOGMASKED(LOG_RADIO,
					"radio_l1: direction=uplink kind=facch good=%u count=%llu fn=%u t=%.6f\n",
					network_block->control.good, m_uplink_facch_blocks,
					frame_number, machine().time().as_double());
	}
	else if (network_block && !network_block->speech.good)
	{
		++m_uplink_bad_speech_blocks;
		if (m_trace_enabled)
			LOGMASKED(LOG_RADIO,
					"radio_l1: direction=uplink kind=speech good=0 "
					"count=%llu fn=%u t=%.6f\n",
					m_uplink_bad_speech_blocks, frame_number,
					machine().time().as_double());
	}
	if (network_block)
	{
		speech_frame network_uplink{};
		const speech_frame *network_uplink_frame = nullptr;
		if (network_block->kind == gsm::tch_f::traffic_block_kind::speech &&
				network_block->speech.good)
		{
			++m_uplink_speech_received;
			std::copy(
					network_block->speech.frame.begin(),
					network_block->speech.frame.end(), network_uplink.begin());
			network_uplink_frame = &network_uplink;
		}
		speech_frame network_downlink{};
		bool have_downlink = false;
		if (m_lab_voice_source || m_host_voice_peer)
		{
			nokia_gsm_voice_peer_device::speech_frame peer_uplink{};
			const nokia_gsm_voice_peer_device::speech_frame *peer_uplink_frame =
					nullptr;
			nokia_gsm_voice_peer_device::speech_frame peer_downlink{};
			if (network_uplink_frame)
			{
				std::copy(
						network_uplink.begin(), network_uplink.end(),
						peer_uplink.begin());
				peer_uplink_frame = &peer_uplink;
			}
			if (m_voice_peer->exchange(peer_uplink_frame, peer_downlink))
			{
				std::copy(
						peer_downlink.begin(), peer_downlink.end(),
						network_downlink.begin());
				have_downlink = true;
			}
		}
		else if (m_speech_loopback && network_uplink_frame)
		{
			network_downlink = network_uplink;
			have_downlink = true;
		}
		if (have_downlink)
		{
			gsm::tch_f::packed_speech_frame packed{};
			std::copy(
					network_downlink.begin(), network_downlink.end(),
					packed.begin());
			m_downlink_transmitter.enqueue(
					{gsm::tch_f::encode_speech(packed),
						gsm::tch_f::traffic_block_kind::speech});
		}
	}

	auto downlink_payload =
			[&]()
			{
				auto payload = m_downlink_transmitter.next_burst();
				++m_downlink_tch_bursts;
				// A generic deterministic hard-error profile before the A5
				// seam. Preserve FACCH so media degradation cannot force call
				// control success or failure.
				if (m_downlink_tch_burst_error_period &&
						((m_downlink_tch_bursts - 1) %
							m_downlink_tch_burst_error_period) <
								m_downlink_tch_burst_error_span &&
						!payload.hl && !payload.hu)
				{
					gsm::tch_f::invert_data_bits(payload);
					++m_downlink_tch_bursts_impaired;
					if (m_trace_enabled)
						LOGMASKED(LOG_RADIO,
								"radio_l1: direction=downlink impairment=invert-data "
								"burst=%llu count=%llu fn=%u t=%.6f\n",
								m_downlink_tch_bursts,
								m_downlink_tch_bursts_impaired,
								frame_number, machine().time().as_double());
				}
				return payload;
			}();
	if (m_gsm_session->cipher_active())
	{
		if (!apply_active_cipher(downlink_payload, frame_number,
				gsm::a5::direction::downlink))
			return;
		++m_downlink_ciphered_bursts;
		if (m_trace_enabled && m_downlink_ciphered_bursts == 1)
			LOGMASKED(LOG_RADIO,
					"radio_l1: kind=cipher direction=downlink algorithm=%u fn=%u count=%u\n",
					u8(m_gsm_session->cipher_algorithm()), frame_number,
					gsm::a5::count_from_frame_number(frame_number));
	}
	auto downlink_air =
			gsm::tch_f::pack_normal_burst(downlink_payload, training);
	auto handset_payload = gsm::tch_f::unpack_normal_burst(downlink_air);
	if (!apply_active_cipher(handset_payload, frame_number,
			gsm::a5::direction::downlink))
		return;
	const auto handset_block = m_handset_receiver.receive(handset_payload);
	if (handset_block &&
			handset_block->kind == gsm::tch_f::traffic_block_kind::facch)
	{
		// FACCH/F steals one complete speech block.  Preserve that fact as a
		// BFI at the codec-clock boundary instead of an ambiguous empty queue.
		queue_downlink_delivery({}, false);
		if (handset_block->control.good)
			++m_downlink_facch_blocks;
		if (m_trace_enabled)
			LOGMASKED(LOG_RADIO,
					"radio_l1: direction=downlink kind=facch good=%u count=%llu fn=%u t=%.6f\n",
					handset_block->control.good, m_downlink_facch_blocks,
					frame_number, machine().time().as_double());
	}
	else if (handset_block && !handset_block->speech.good)
	{
		queue_downlink_delivery({}, false);
		++m_downlink_bad_speech_blocks;
		if (m_trace_enabled)
			LOGMASKED(LOG_RADIO,
					"radio_l1: direction=downlink kind=speech good=0 count=%llu fn=%u t=%.6f\n",
					m_downlink_bad_speech_blocks, frame_number,
					machine().time().as_double());
	}
	if (handset_block &&
			handset_block->kind == gsm::tch_f::traffic_block_kind::speech &&
			handset_block->speech.good)
	{
		speech_frame downlink{};
		std::copy(
				handset_block->speech.frame.begin(),
				handset_block->speech.frame.end(), downlink.begin());
		queue_downlink_speech(downlink);
	}
}

const char *nokia_radio_peer_device::phase_name(u8 value)
{
	static constexpr const char *NAMES[] = {
		"inactive", "initial_search", "post_deactivate_search",
		"candidate_measurement", "candidate_sync", "candidate_channel_change",
		"candidate_ra_info", "serving_bcch", "candidate_retry",
		"selected_search", "serving_channel_change", "selected_channel_change",
		"selected_bcch", "selected_ra_info", "selected_bcch_channel_change",
		"random_access", "assigned_channel_change", "lapdm_establish",
		"contention_resolution", "location_update_accept",
		"location_update_ack_request", "location_update_acknowledgement",
		"rr_channel_release", "channel_release_uplink_request",
		"channel_release_acknowledgement", "release_deconfigure",
		"release_channel_change", "service_downlink", "service_uplink_request",
		"service_uplink_wait", "service_uplink_acknowledgement",
		"traffic_channel_change", "traffic_lapdm_establish",
		"traffic_contention_resolution", "traffic_release_acknowledgement",
		"candidate_terminal_control", "serving_sch_observation"
	};
	// The table is positional, and the checkers match on the names it emits.
	// Without this, adding or removing a phase renames every later phase in the
	// traces and fails as a puzzling gate mismatch rather than a build error.
	static_assert(std::size(NAMES) == std::size_t(phase::count),
			"phase_name table does not cover every phase");
	const unsigned index = unsigned(value);
	return index < std::size(NAMES) ? NAMES[index] : "invalid";
}

const char *nokia_radio_peer_device::phase_name() const
{
	return phase_name(m_phase);
}

bool nokia_radio_peer_device::uses_candidate_window() const
{
	// Deliberately not an equality test against acquisition_strategy::
	// candidate_window. The autonomous band scan also runs a candidate
	// measurement window: it reports measured topology RSSI and holds the idle
	// common-control receiver open, where bitmap-multistage acquisition does
	// neither. Every call site below asks about that shared window, not about
	// which of the two named strategies produced it.
	return m_protocol.acquisition == acquisition_strategy::candidate_window ||
			m_protocol.acquisition ==
					acquisition_strategy::autonomous_band_scan;
}

u8 nokia_radio_peer_device::next_report_type() const
{
	// Fixed entries are declarative; 0xff marks phases whose report depends on
	// request data or position within a correlated multi-report transaction.
	// Positional and index-aligned to the phase enum: the array size catches a
	// phase added or removed, but nothing catches a reorder, so entries must
	// move with their phase.
	static constexpr std::array<u8, std::size_t(phase::count)> FIXED_REPORT = {
		0x87, 0x87, 0x87, 0xff, 0xff, 0xff, 0x84, 0xff,
		0xff, 0xff, 0x89, 0x89, 0xff, 0x84, 0x89, 0xff,
		0x89, 0x86, 0x80, 0x80, 0x86, 0x87, 0x80, 0x86,
		0x87, 0x87, 0x89, 0x80, 0x86, 0x87, 0x80, 0x89,
		0x86, 0x80, 0x80, 0xff, 0xff
	};

	const u8 fixed = m_phase < FIXED_REPORT.size() ? FIXED_REPORT[m_phase] : 0x87;
	if (fixed != 0xff)
		return fixed;

	switch (current_phase())
	{
	case phase::candidate_measurement:
		if (uses_candidate_window())
			return m_reports_remaining == 2 ? 0x80 : 0x8b;
		return 0x8b;
	case phase::candidate_sync:
		return m_reports_remaining == 2 ? 0x8b : 0x80;
	case phase::candidate_channel_change:
		return m_reports_remaining == 2 ? 0x8f : 0x89;
	case phase::serving_bcch:
		if (m_registered || m_idle_common_control_active)
			return (m_reports_remaining % 3) == 1 ? 0x83 : 0x80;
		return (m_reports_remaining & 1) == 0 ? 0x80 : 0x83;
	case phase::candidate_retry:
		return m_reports_remaining == 2 ? 0x8b : 0x87;
	case phase::selected_search:
		return m_reports_remaining == 2 ? 0x8b :
				(((m_search_mode == 0x00 && m_search_has_serving_arfcn) ||
					m_search_mode == 0x40 || m_search_mode == 0x50) ? 0x80 : 0x87);
	case phase::selected_bcch:
		return m_reports_remaining == 1 && !m_reselection_validation_pending ?
				0x87 :
				((m_reports_remaining & 1) == 0 ? 0x83 : 0x80);
	case phase::random_access:
		return m_reports_remaining == 2 ? 0x84 : 0x80;
	case phase::serving_sch_observation:
		return m_gsm_network->cell_receivable(m_sch_observation_arfcn) ?
				0x80 : (m_reports_remaining == 2 ? 0x8a : 0x8f);
	default:
		return 0x87;
	}
}

unsigned nokia_radio_peer_device::serving_cycle_reports() const
{
	return (m_registered || m_idle_common_control_active) ? 12 : 8;
}

bool nokia_radio_peer_device::serving_pch_report() const
{
	if (current_phase() != phase::serving_bcch)
		return false;
	return (m_registered || m_idle_common_control_active) &&
			m_receiver_arfcn == m_serving_arfcn &&
			(m_reports_remaining % 3) == 2;
}

u32 nokia_radio_peer_device::paging_frame_number(
		u32 minimum_frame_number, paging_schedule schedule) const
{
	static constexpr u32 FRAME_NUMBER_MODULUS = 26 * 51 * 2048;
	const auto group = schedule == paging_schedule::transmitted ?
			m_gsm_network->paging_request_group(
					m_gsm_session->registered_mobile_identity().data(),
					m_gsm_session->registered_mobile_identity_length()) :
			m_gsm_network->subscriber_paging_group(
					m_gsm_session->registered_mobile_identity().data(),
					m_gsm_session->registered_mobile_identity_length());
	u32 multiframe = minimum_frame_number / 51;
	if ((multiframe & 1) != group.multiframe_phase)
		++multiframe;
	u32 frame_number = multiframe * 51 + group.frame_offset;
	if (frame_number < minimum_frame_number)
		frame_number += 102;
	return frame_number % FRAME_NUMBER_MODULUS;
}

void nokia_radio_peer_device::populate_search_from_receivable_cells(u8 mode)
{
	// Both untargeted search forms answer from the standards-level topology
	// rather than from an MCU-supplied candidate list, so the receivability
	// filter and the capacity bound are stated once for both.
	m_search_mode = mode;
	m_search_arfcns.fill(0xffff);
	m_search_arfcn_count = 0;
	for (unsigned index = 0;
			index < m_gsm_network->cell_count() &&
				m_search_arfcn_count < m_search_arfcns.size();
			++index)
	{
		if (const auto *cell = m_gsm_network->cell_at(index);
				cell && m_gsm_network->cell_receivable(cell->arfcn))
			m_search_arfcns[m_search_arfcn_count++] = cell->arfcn;
	}
	m_search_has_serving_arfcn = m_search_arfcn_count != 0;
}

u16 nokia_radio_peer_device::retune_receiver(u16 arfcn)
{
	// Retuning always invalidates both decoded BCCH contexts: the frame
	// belonged to the previous carrier. Returns the previous ARFCN so a caller
	// can tell which carrier the receiver left.
	const u16 previous_receiver = m_receiver_arfcn;
	if (m_trace_enabled)
		LOGMASKED(LOG_RADIO,
				"dsp_hle: receiver tuned old_arfcn=%u new_arfcn=%u "
				"t=%.6f\n",
				previous_receiver, arfcn,
				machine().time().as_double());
	m_receiver_arfcn = arfcn;
	m_bcch_frame_valid = false;
	m_candidate_bcch_valid = false;
	return previous_receiver;
}

void nokia_radio_peer_device::commit_receiver_as_serving()
{
	// Only the part every commit shares. What follows a commit differs by the
	// evidence that produced it — whether validation is still pending, whether
	// the candidate BCCH is consumed, whether the DSC counter restarts — and
	// stays at the call sites so those differences remain visible.
	if (m_trace_enabled)
		LOGMASKED(LOG_RADIO,
				"dsp_hle: serving cell selected old_arfcn=%u "
				"new_arfcn=%u t=%.6f\n",
				m_serving_arfcn, m_receiver_arfcn,
				machine().time().as_double());
	m_serving_arfcn = m_receiver_arfcn;
	m_has_reselected = true;
}

void nokia_radio_peer_device::restart_downlink_signalling_counter()
{
	// Begin a fresh TS 45.008 downlink-signalling observation interval. The
	// counter starts at its ceiling and the failure latch clears together:
	// leaving the latch set against a restarted counter would report a failure
	// the new interval has not yet observed.
	m_downlink_signalling_count =
			gsm::mobility::downlink_signalling_counter().ceiling();
	m_downlink_signalling_failed = false;
}

void nokia_radio_peer_device::trace_layer3_uplink(const char *direction)
{
	// The checkers match on this line, so both the establish and the ordinary
	// uplink case emit it from one place: a divergence in the wording would be
	// a silent gate failure rather than a compile error.
	if (!m_trace_enabled)
		return;
	const auto &information = m_lapdm_link->layer3_information();
	std::string information_hex;
	for (unsigned index = 0; index < m_lapdm_link->layer3_length(); ++index)
		information_hex += util::string_format("%02x", information[index]);
	LOGMASKED(LOG_RADIO,
			"dsp_hle: GSM service %s sapi=%u pd=%02x message=%02x length=%u data=%s t=%.6f\n",
			direction,
			m_lapdm_link->layer3_sapi(),
			information[0] & 0x0f, information[1] & 0x3f,
			m_lapdm_link->layer3_length(),
			information_hex.c_str(),
			machine().time().as_double());
}

void nokia_radio_peer_device::enter_release_deconfigure()
{
	if (m_trace_enabled)
		LOGMASKED(LOG_RADIO,
				"dsp_hle: LAPDm service Channel Release acknowledged nr=%u t=%.6f\n",
				m_lapdm_link->pending_receive_sequence(),
				machine().time().as_double());
	set_phase(phase::release_deconfigure);
	m_reports_remaining = 0;
}

auto nokia_radio_peer_device::decode_search_request(
		const nokia_dspif_device::packet &packet) -> search_request
{
	switch (m_protocol.acquisition)
	{
	case acquisition_strategy::bitmap_multistage:
		if (packet.type != 0x1a || packet.length == 0)
			return search_request::none;
		m_search_mode = packet.payload[0];
		m_search_arfcns.fill(0xffff);
		m_search_arfcn_count = 0;
		// SEARCH_LIST carries a 512-bit ARFCN set after its four-byte control
		// header. In the ROM-4 wire layout ARFCN 1 is bit 0 of byte 65.
		m_serving_arfcn = 1;
		m_search_has_serving_arfcn =
				packet.length > 65 && BIT(packet.payload[65], 0);
		// Topology is RF environment, while the bitmap remains firmware-owned
		// search policy. Bitmap-multistage acquisition has an independently
		// recovered ARFCN-1 laboratory contract. The has-serving flag describes
		// this request bitmap; measurement history still reports that evidenced
		// carrier during the preceding multistage scan. Do not infer the
		// remaining Nokia bit ordering from the network's configured cells.
		m_search_arfcns[m_search_arfcn_count++] = 1;
		return search_request::bitmap_multistage;

	case acquisition_strategy::candidate_window:
		if (decode_candidate_window(packet, false))
			return search_request::candidate_window;
		if (packet.type != 0x55 || packet.length != 4 ||
				packet.payload[0] != 0x03)
			return search_request::none;
		// ROM6 0x55 is the untargeted counterpart to the explicit 0x56
		// candidate window. Populate its DSP measurement result from the
		// receivable standards-level topology; the MCU still ranks and selects.
		populate_search_from_receivable_cells(packet.payload[1]);
		return search_request::autonomous_band_scan;

	case acquisition_strategy::autonomous_band_scan:
		if (decode_candidate_window(packet, true))
			return search_request::candidate_window;
		if (packet.type != 0x55 || packet.length != 2 ||
				packet.payload[0] != 0x03 ||
				(packet.payload[1] != 0x03 &&
					packet.payload[1] != 0x05))
			return search_request::none;
		// NHM-2 asks the DSP to scan the supported bands rather than passing an
		// MCU-built candidate list. The laboratory cell is a valid GSM 900
		// carrier; its exact channel is network topology, not handset state.
		populate_search_from_receivable_cells(packet.payload[1]);
		if (m_search_has_serving_arfcn)
			m_serving_arfcn = m_search_arfcns[0];
		return search_request::autonomous_band_scan;

	case acquisition_strategy::none:
		return search_request::none;
	}
	return search_request::none;
}

bool nokia_radio_peer_device::decode_candidate_window(
		const nokia_dspif_device::packet &packet, bool ignore_zero)
{
	if (packet.type != 0x56 || packet.length != 160)
		return false;
	// Candidate-window requests contain eighty big-endian channels and use
	// 0xffff for unused slots. NHM-2's autonomous-scan publication also pads
	// the list with zeroes before its first real candidate.
	m_search_mode = 0;
	m_search_has_serving_arfcn = false;
	m_search_arfcns.fill(0xffff);
	m_search_arfcn_count = 0;
	for (unsigned offset = 0; offset < packet.length; offset += 2)
	{
		const u16 candidate =
				(packet.payload[offset] << 8) | packet.payload[offset + 1];
		if (candidate != 0xffff && (!ignore_zero || candidate != 0x0000))
		{
			if (m_search_arfcn_count < m_search_arfcns.size())
				m_search_arfcns[m_search_arfcn_count++] = candidate;
			if (!m_search_has_serving_arfcn)
				m_serving_arfcn = candidate;
			m_search_has_serving_arfcn = true;
		}
	}
	return true;
}

bool nokia_radio_peer_device::decode_neighbour_measurement_list(
		const nokia_dspif_device::packet &packet)
{
	if (packet.type != 0x0f ||
			(packet.length != 64 && packet.length != 66 &&
				packet.length != 68))
		return false;

	m_neighbour_arfcns.fill(0xffff);
	m_neighbour_arfcn_count = 0;
	if (packet.length == 66 || packet.length == 68)
	{
		// The array has a two-byte controller prefix and a two-byte trailer;
		// product publications differ only in the number of available slots.
		u16 published_first = 0xffff;
		for (unsigned offset = 2;
				offset + 1 < packet.length - 2; offset += 2)
		{
			const u16 arfcn =
					(packet.payload[offset] << 8) | packet.payload[offset + 1];
			if (arfcn == 0xffff)
				break;
			if (published_first == 0xffff)
				published_first = arfcn;
			// Some 68-byte publications include the configured receiver
			// alongside its measurement candidates. Keep per-cell serving
			// state separate from the neighbour set used for ranking.
			if (packet.length == 68 && arfcn == m_receiver_arfcn)
				continue;
			if (m_neighbour_arfcn_count < m_neighbour_arfcns.size())
				m_neighbour_arfcns[m_neighbour_arfcn_count++] = arfcn;
		}
		if (packet.length == 68 &&
				m_protocol.serving_list_commits_receiver &&
				published_first == m_receiver_arfcn &&
				m_receiver_arfcn != m_serving_arfcn)
		{
			commit_receiver_as_serving();
			m_reselection_validation_pending = false;
			restart_downlink_signalling_counter();
		}
	}
	else
	{
		// NSE-8 independently publishes a reversed 512-bit neighbour bitmap.
		// Its organic two-cell form ends in 0x02, naming ARFCN 2 exactly like
		// the previously recovered bitmap SEARCH_LIST grammar.  Consult only
		// cells present in the standards-owned topology rather than assigning
		// meanings to unevidenced high-band/control bits.
		for (unsigned index = 0; index < m_gsm_network->cell_count(); ++index)
		{
			const auto *cell = m_gsm_network->cell_at(index);
			if (!cell || cell->arfcn == 0 || cell->arfcn > 512)
				continue;
			const unsigned bitmap_index = cell->arfcn - 1;
			const unsigned byte = 63 - (bitmap_index / 8);
			if (BIT(packet.payload[byte], bitmap_index & 7) &&
					m_neighbour_arfcn_count < m_neighbour_arfcns.size())
				m_neighbour_arfcns[m_neighbour_arfcn_count++] = cell->arfcn;
		}
		// NSE-8's post-validation bitmap is relative to the newly selected
		// receiver: it contains the former serving ARFCN as the new neighbour.
		// Combined with a successfully decoded candidate BCCH, that
		// firmware-owned publication commits the receiver as serving.
		bool former_serving_published = false;
		for (unsigned index = 0; index < m_neighbour_arfcn_count; ++index)
			former_serving_published |=
					m_neighbour_arfcns[index] == m_serving_arfcn;
		if (m_protocol.serving_list_commits_receiver &&
				m_candidate_bcch_valid &&
				m_receiver_arfcn != m_serving_arfcn &&
				former_serving_published)
		{
			commit_receiver_as_serving();
			m_candidate_bcch_valid = false;
			m_reselection_validation_pending = false;
			restart_downlink_signalling_counter();
		}
	}

	if (m_trace_enabled)
		LOGMASKED(LOG_RADIO,
				"dsp_hle: neighbour measurement list grammar=%s "
				"count=%u first=%04x t=%.6f\n",
				packet.length >= 66 ? "array" : "bitmap",
				m_neighbour_arfcn_count,
				m_neighbour_arfcn_count ? m_neighbour_arfcns[0] : 0xffff,
				machine().time().as_double());
	if (m_neighbour_arfcn_count && packet.length != 68)
		m_gsm_network->neighbour_list_observed();
	return true;
}

bool nokia_radio_peer_device::decode_neighbour_measurement_instruction(
		const nokia_dspif_device::packet &packet)
{
	if (packet.type != 0x11 || packet.length < 9)
		return false;
	const bool already_pending = m_neighbour_bcch_pending;

	// Two independent public NHM-5 trace samples identify byte 1 as the expected
	// BSIC and byte 7 as the ARFCN.  NHM-2 independently emits zero in byte 1
	// while requesting synchronization of its SI2-derived carrier, so keep the
	// independently evidenced validation rule in the typed product contract.
	// In either grammar the target must be a member of the firmware-published
	// neighbour list as well as a receivable cell in the generic topology.
	const u8 expected_bsic = packet.payload[1];
	u16 arfcn = packet.payload[7];
	if (m_protocol.neighbour_instruction_arfcn ==
			neighbour_arfcn_encoding::topology_low_octet)
	{
		for (unsigned index = 0; index < m_gsm_network->cell_count(); ++index)
		{
			const auto *cell = m_gsm_network->cell_at(index);
			if (cell && cell->arfcn != m_serving_arfcn &&
					u8(cell->arfcn) == packet.payload[7])
			{
				arfcn = cell->arfcn;
				break;
			}
		}
	}
	const auto *candidate = m_gsm_network->cell_by_arfcn(arfcn);
	const bool bsic_matches =
			m_protocol.neighbour_instruction_bsic ==
					neighbour_bsic_encoding::none ||
			(m_protocol.neighbour_instruction_bsic ==
						neighbour_bsic_encoding::direct &&
					candidate && candidate->bsic == expected_bsic) ||
			(m_protocol.neighbour_instruction_bsic ==
						neighbour_bsic_encoding::low_six_bits &&
					candidate && candidate->bsic == (expected_bsic & 0x3f));
	bool listed = false;
	for (unsigned index = 0; index < m_neighbour_arfcn_count; ++index)
		listed |= m_neighbour_arfcns[index] == arfcn;
	if (!listed && m_protocol.neighbour_instruction_establishes_candidate)
		listed = m_gsm_network->configured_neighbour(
				m_serving_arfcn, arfcn);
	if (candidate && listed && bsic_matches)
	{
		if (m_neighbour_arfcn_count == 0)
		{
			m_neighbour_arfcns[0] = arfcn;
			m_neighbour_arfcn_count = 1;
		}
		// The accepted instruction is the first boundary proving that the
		// decoded candidate set has entered a real measurement cycle.
		m_gsm_network->neighbour_list_observed();
		m_last_neighbour_instruction_arfcn = arfcn;
		m_last_neighbour_instruction_bsic = expected_bsic;
	}
	m_neighbour_bcch_pending = candidate &&
			listed &&
			bsic_matches &&
			m_gsm_network->cell_receivable(arfcn);
	m_neighbour_bcch_arfcn =
			m_neighbour_bcch_pending ? arfcn : 0xffff;
	m_neighbour_instruction_mode =
			m_neighbour_bcch_pending ? packet.payload[0] : 0;
	m_neighbour_instruction_bsic =
			m_neighbour_bcch_pending ? expected_bsic : 0;
	if (m_neighbour_bcch_pending && !already_pending &&
			m_neighbour_instruction_mode == 0x02)
	{
		// Mode 2 requests an asynchronous RSSI observation at the supplied
		// GSM-frame offset. The peer service tick is 4 ms while a TDMA frame is
		// exactly 60/13 ms, so ceil(frames * 15/13) preserves that air-time
		// request without coupling it to the 51-frame serving-report cadence.
		const unsigned response_ticks =
				std::max(1U, (unsigned(packet.payload[5]) * 15 + 12) / 13);
		m_neighbour_resume_wait_ticks =
				m_wait_ticks > response_ticks ?
						m_wait_ticks - response_ticks : 0;
		m_wait_ticks = response_ticks;
	}

	if (m_trace_enabled)
		LOGMASKED(LOG_RADIO,
				"dsp_hle: neighbour measurement instruction arfcn=%u "
				"bsic=%u validated=%u accepted=%u t=%.6f\n",
				arfcn, expected_bsic,
				m_protocol.neighbour_instruction_bsic !=
						neighbour_bsic_encoding::none,
				m_neighbour_bcch_pending,
				machine().time().as_double());
	return true;
}

bool nokia_radio_peer_device::decode_serving_sch_request(
		const nokia_dspif_device::packet &packet)
{
	if (!m_serving_loss_pending ||
			m_protocol.serving_sch_request_type == 0 ||
			packet.type != m_protocol.serving_sch_request_type ||
			packet.length != 2 ||
			current_phase() != phase::serving_bcch)
		return false;

	// Nokia packet grammar owns the request, but its DSP-visible consequence is
	// standards-level: observe one SCH opportunity on the configured receiver.
	// The DSP observes the strongest currently receivable synchronization
	// candidate and reports its real ARFCN/BSIC. A missing carrier yields
	// NO_PSW_FOUND rather than a fabricated decoded block. This is an RF
	// observation, not a serving-cell selection; firmware still owns that
	// decision and every subsequent receiver configuration.
	m_sch_observation_arfcn = 0xffff;
	s8 strongest_rssi = -128;
	for (unsigned index = 0; index < m_gsm_network->cell_count(); ++index)
	{
		const auto *cell = m_gsm_network->cell_at(index);
		if (!cell || !m_gsm_network->cell_receivable(cell->arfcn))
			continue;
		const s8 rssi = m_gsm_network->cell_rssi(
				cell->arfcn, m_idle_measurement_sample);
		if (m_sch_observation_arfcn == 0xffff || rssi > strongest_rssi)
		{
			m_sch_observation_arfcn = cell->arfcn;
			strongest_rssi = rssi;
		}
	}
	set_phase(phase::serving_sch_observation);
	m_reports_remaining =
			m_sch_observation_arfcn != 0xffff ? 1 : 2;
	m_wait_ticks = 0;
	m_report_deferred = true;
	return true;
}

bool nokia_radio_peer_device::handle_search_request(search_request request)
{
	if (request == search_request::none)
		return false;

	if (request == search_request::candidate_window)
	{
		// This one is load-bearing, unlike the band-scan branch below: both
		// candidate-window strategies decode this request, and only the
		// autonomous scan may be followed by an explicit window while its own
		// measurement is still the current phase.
		const bool autonomous_scan_complete =
				m_protocol.acquisition ==
						acquisition_strategy::autonomous_band_scan &&
				current_phase() == phase::candidate_measurement;
		if ((!autonomous_scan_complete && current_phase() != phase::inactive) ||
				m_reports_remaining != 0)
			return false;
		// Candidate-window acquisition reports SCH before its alternative
		// measurement terminal and preserves the full SI validation interval.
		set_phase(phase::candidate_measurement);
		m_reports_remaining = 2;
		// One 51-frame GSM control multiframe spans 59 peer polls at the
		// calibrated four-millisecond service cadence used below for BCCH.
		// Keep this eight-multiframe window in the same unit; 8 * 51 confused
		// TDMA frames with peer polls and shortened acquisition by about 256 ms.
		m_wait_ticks = 8 * 59;
		return true;
	}

	if (request == search_request::autonomous_band_scan)
	{
		const bool lost_serving_cell = m_serving_loss_pending;
		const bool after_receiver_deactivation =
				current_phase() == phase::post_deactivate_search;
		// No strategy test here: only the two candidate-window strategies can
		// decode an autonomous band scan in the first place, so reaching this
		// branch already establishes what uses_candidate_window() would ask.
		const bool after_candidate_window =
				current_phase() == phase::candidate_measurement &&
				m_reports_remaining == 0;
		if ((!lost_serving_cell && !after_receiver_deactivation &&
					!after_candidate_window &&
					current_phase() != phase::inactive) ||
				(!lost_serving_cell && m_reports_remaining != 0))
			return false;
		// NHM-2 uses the same autonomous band-scan request after organic
		// DOWNLINK_SIGNALLING_FAIL as it does at initial acquisition.  Preserve
		// the firmware-owned trigger while restarting only the bounded radio
		// search transaction.
		if (lost_serving_cell)
		{
			m_bcch_frame_valid = false;
			m_neighbour_bcch_pending = false;
			m_neighbour_bcch_arfcn = 0xffff;
			m_neighbour_instruction_mode = 0;
			m_neighbour_instruction_bsic = 0;
			m_neighbour_resume_wait_ticks = 0;
			m_serving_loss_pending = false;
		}
		set_phase(phase::candidate_measurement);
		m_reports_remaining = 1;
		return true;
	}

	switch (current_phase())
	{
	case phase::inactive:
		if (m_reports_remaining == 0)
		{
			set_phase(phase::initial_search);
			m_reports_remaining = 2;
			return true;
		}
		break;

	case phase::initial_search:
		if (m_reports_remaining == 0)
		{
			// A cached EF_BCCH request takes the same bounded terminal path as
			// the explicit deactivation transition.
			set_phase(phase::post_deactivate_search);
			m_reports_remaining = 2;
			return true;
		}
		break;

	case phase::post_deactivate_search:
		if (m_reports_remaining == 0)
		{
			set_phase(phase::candidate_measurement);
			m_reports_remaining = 1;
			return true;
		}
		break;

	case phase::candidate_measurement:
		if (m_reports_remaining == 0)
		{
			if (m_search_round >= 3)
			{
				set_phase(phase::candidate_sync);
				m_reports_remaining = 2;
				m_report_deferred = true;
			}
			else
				m_reports_remaining = 1;
			return true;
		}
		break;

	case phase::candidate_sync:
		if (m_reports_remaining == 0)
		{
			set_phase(phase::candidate_retry);
			m_reports_remaining = 2;
			m_report_deferred = true;
			return true;
		}
		break;

	case phase::serving_bcch:
		// An explicit measurement request preempts the periodic serving stream.
		m_search_requested = false;
		set_phase(phase::selected_search);
		m_reports_remaining = 2;
		m_wait_ticks = 0;
		m_report_deferred = true;
		return true;

	case phase::selected_search:
		if (m_reports_remaining == 0)
		{
			m_reports_remaining = 2;
			m_report_deferred = true;
			return true;
		}
		break;

	case phase::candidate_retry:
		if (m_reports_remaining == 0)
		{
			set_phase(phase::candidate_measurement);
			m_reports_remaining = 1;
			m_report_deferred = true;
			return true;
		}
		break;

	default:
		break;
	}
	return false;
}

bool nokia_radio_peer_device::handle_acquisition_packet(
		const nokia_dspif_device::packet &packet)
{
	if (uses_candidate_window() &&
			packet.type == 0x55 && packet.length == 4 &&
			current_phase() == phase::candidate_measurement &&
			m_reports_remaining == 0)
	{
		// Candidate-window firmware publishes a separate terminal-control
		// transaction after consuming SCH and its final RSSI result. Complete
		// that bounded DSP transaction with the ordinary no-more-candidates
		// indication; firmware remains responsible for the next search step.
		set_phase(phase::candidate_terminal_control);
		m_reports_remaining = 1;
		m_report_deferred = true;
		return true;
	}

	if (packet.type == 0x02 &&
			(((current_phase() == phase::candidate_sync ||
					current_phase() == phase::selected_search) &&
				m_reports_remaining == 0) ||
			(uses_candidate_window() &&
				current_phase() == phase::candidate_measurement &&
				m_reports_remaining == 1)))
	{
		const bool active_candidate_window =
				uses_candidate_window() &&
				current_phase() == phase::candidate_measurement;
		const bool selected_plmn_search =
				current_phase() == phase::selected_search && m_search_mode == 0x50;
		const u16 requested_arfcn = packet.length >= 12 ?
				(packet.payload[10] << 8) | packet.payload[11] :
				m_receiver_arfcn;
		if (packet.length >= 2)
			m_receiver_bsic = packet.payload[1];
		if (requested_arfcn != m_receiver_arfcn &&
				m_gsm_network->cell_by_arfcn(requested_arfcn))
			retune_receiver(requested_arfcn);
		if (active_candidate_window &&
				m_receiver_arfcn != m_serving_arfcn)
			commit_receiver_as_serving();
		set_phase(selected_plmn_search ?
				phase::selected_channel_change :
				phase::candidate_channel_change);
		m_reports_remaining = active_candidate_window ?
				2 : (selected_plmn_search ? 1 : 2);
		m_report_deferred = true;
		return true;
	}

	return false;
}

void nokia_radio_peer_device::encode_measurement_report(u8 *payload) const
{
	// ALL_RSSI_RESULTS is a two-byte header plus forty four-byte records.
	// Bitmap acquisition establishes two history entries before using the
	// laboratory signal; candidate-window acquisition measures its requested
	// channel directly.
	payload[0] = 0x00;
	payload[1] = 0x10;
	for (unsigned result = 0; result < 40; ++result)
	{
		const bool measured = result < m_search_arfcn_count;
		const u16 measured_arfcn =
				measured ? m_search_arfcns[result] : 0xffff;
		payload[2 + result * 4] =
				measured ? u8(measured_arfcn >> 8) : 0xff;
		payload[3 + result * 4] =
				measured ? u8(measured_arfcn) : 0xff;
		payload[5 + result * 4] = measured ?
				(uses_candidate_window() ?
					u8(m_gsm_network->cell_rssi(
							measured_arfcn, m_search_round)) :
					(m_search_round < 2 ? u8(0x93) :
						u8(m_gsm_network->cell_rssi(
								measured_arfcn, m_search_round - 2)))) :
				0x81;
	}
}

void nokia_radio_peer_device::encode_channel_confirmation(u8 *payload) const
{
	payload[0] = current_phase() == phase::assigned_channel_change ?
			m_protocol.assigned_channel_confirmation : 0x00;
}

void nokia_radio_peer_device::encode_random_access_info(u8 *payload)
{
	m_access_frame = m_tdma_frame_number % 2'715'648;
	payload[0] = m_access_ra;
	payload[1] = m_access_frame >> 16;
	payload[2] = m_access_frame >> 8;
	payload[3] = m_access_frame;
}

void nokia_radio_peer_device::receive_packet(const nokia_dspif_device::packet &packet)
{
	const search_request search = decode_search_request(packet);
	if (handle_search_request(search))
		return;
	if (handle_acquisition_packet(packet))
		return;
	if (decode_neighbour_measurement_list(packet))
		return;
	if (decode_neighbour_measurement_instruction(packet))
		return;
	if (decode_serving_sch_request(packet))
		return;

	if (packet.type == 0x03 &&
			(current_phase() == phase::initial_search ||
				(current_phase() == phase::candidate_measurement &&
					m_downlink_signalling_failed)) &&
			m_reports_remaining == 0)
	{
		// After a loss-triggered autonomous RSSI pass NHM-2 deactivates the
		// old receiver before continuing candidate selection.  This is the
		// same bounded DSP deactivation transaction used by initial search.
		set_phase(phase::post_deactivate_search);
		m_reports_remaining = 2;
	}
	else if (packet.type == 0x0c && current_phase() == phase::serving_bcch)
	{
		// IDLE_RA form 1 configures the serving-cell receiver. Form 0 carries a
		// CHANNEL REQUEST random-access octet at byte 2 after organic 0x07d1.
		// IDLE_RA has no type-0x8c acknowledgement: recovered protocol
		// vocabulary identifies 0x8c as DOWNLINK_SIGNALLING_FAIL.
		// Byte 1 retains the configured receiver selector and therefore
		// changes after reselection; byte 2 alone distinguishes the
		// random-access form from the idle receiver configuration.
		const bool channel_request =
				packet.length >= 3 && packet.payload[2] != 0;
		if (!channel_request)
			return;
		m_access_ra = channel_request ? packet.payload[2] : 0;
		m_access_frame = 0;
		set_phase(phase::random_access);
		m_reports_remaining = 2;
		m_report_deferred = true;
	}
	else if (packet.type == 0x02 && current_phase() == phase::serving_bcch &&
			packet.length >= 9 && packet.payload[8] == 0x60)
	{
		// After serving-cell selection the ROM configures logical channel 0x12,
		// encoded as DSP receive channel 0x60. This transitions NHM-5 from
		// acquisition BCCH to its idle common-control receiver; subsequent
		// decoded blocks on that receiver are PCH/AGCH, not more SI payloads.
		//
		// NHM-2 places the tuned ARFCN in bytes 10/11.  A mode-0x02 transaction
		// is also used for a temporary neighbour BCCH window, so keep receiver
		// tuning distinct from serving-cell selection.  A later firmware
		// reconfiguration commits the receiver only after at least one valid
		// BCCH frame has established the selected-cell context; its opaque
		// Nokia mode byte differs between products.  MM remains responsible for
		// deciding whether it requires Location Updating.
		const u16 requested_arfcn = packet.length >= 12 ?
				(packet.payload[10] << 8) | packet.payload[11] :
				m_receiver_arfcn;
		// Nokia's channel configuration retains controller bits alongside the
		// six-bit GSM BSIC. RECEIVED_BLOCK must echo that wire value for the
		// configured receiver, while the generic topology continues to own only
		// the standards BSIC used for suitability checks.
		if (packet.length >= 2)
			m_receiver_bsic = packet.payload[1];
		if (requested_arfcn != m_receiver_arfcn &&
				m_gsm_network->cell_by_arfcn(requested_arfcn))
		{
			const u16 previous_receiver = retune_receiver(requested_arfcn);
			// Returning from a temporary neighbour window is the first
			// firmware-owned boundary proving that the complete validation
			// interval has closed.  Scenario-driven serving loss is armed only
			// here, never by receipt of the first neighbour block.
			if (requested_arfcn == m_serving_arfcn &&
					previous_receiver != m_serving_arfcn)
				m_gsm_network->neighbour_bcch_observed(previous_receiver);
		}
		if (m_receiver_arfcn != m_serving_arfcn &&
				m_candidate_bcch_valid)
		{
			commit_receiver_as_serving();
			m_candidate_bcch_valid = false;
			m_reselection_validation_pending = true;
			restart_downlink_signalling_counter();
		}
		const bool completes_candidate_validation =
				!m_idle_common_control_active &&
				uses_candidate_window();
		m_idle_common_control_active =
				uses_candidate_window();
		if (m_downlink_signalling_failed)
		{
			// Firmware explicitly reconfigured the common-control receiver
			// after the failure indication. The DSP begins a fresh TS 45.008
			// downlink-signalling observation interval for that receiver; if
			// the carrier is still absent, another organic failure follows.
			restart_downlink_signalling_counter();
		}
		set_phase(phase::serving_channel_change);
		m_reports_remaining = 1;
		if (completes_candidate_validation && m_bcch_frame_valid)
		{
			// The first common-control activation closes the acquisition batch.
			// Complete the current eight-multiframe SI schedule at its next TC0
			// boundary before confirming that transition. This preserves the
			// ordering between decoded-SI publication and CHANNEL_CHANGED_CNF
			// without assigning a product-specific delay.
			const unsigned tc = (m_bcch_frame_number / 51) & 7;
			m_wait_ticks = (8 - tc) * 59;
			m_report_deferred = false;
		}
		else
			m_report_deferred = true;
	}
	else if (packet.type == 0x02 && current_phase() == phase::selected_bcch &&
			packet.length >= 9 && packet.payload[8] == 0x60)
	{
		// The channel-change acceptance window closes before the selected search's
		// finite terminal. Suspend that search, acknowledge the requested change,
		// then resume its remaining measurement and terminal reports.
		m_selected_reports_remaining = m_reports_remaining;
		set_phase(phase::selected_bcch_channel_change);
		m_reports_remaining = 1;
		m_wait_ticks = 0;
		m_report_deferred = true;
	}
	else if (packet.type == 0x02 && current_phase() == phase::random_access &&
			packet.length >= 9 && packet.payload[8] == 0x80)
	{
		// A matching Immediate Assignment makes RR configure the assigned SDCCH.
		// Complete the same recovered channel-change transaction used for the
		// serving receiver; the firmware owns the subsequent LAPDm establishment.
		set_phase(phase::assigned_channel_change);
		m_reports_remaining = 1;
		m_report_deferred = true;
	}
	else if (packet.type == 0x02 && current_phase() == phase::release_deconfigure &&
			packet.length >= 16 && packet.payload[8] == 0x60 &&
			(packet.payload[15] == 0x0f ||
				(m_traffic_channel_active &&
					packet.payload[15] ==
							m_protocol.traffic_release_parameter)))
	{
		// RR Channel Release makes the ROM issue the same CHANNEL_CONFIGURE
		// transaction used to establish channel 0x60. The recovered SDCCH
		// deconfiguration byte is 0x0f. The independently observed TCH/F
		// transactions carry 0x08 on NSE-8 and 0x14 on NHM-5; keep those exact
		// product-profile contracts typed without assigning an unproved bit
		// meaning. Confirm the transaction here; the firmware owns return to idle.
		set_phase(phase::release_channel_change);
		m_reports_remaining = 1;
		m_report_deferred = true;
	}
	else if (packet.type == 0x02 &&
			(current_phase() == phase::service_uplink_request ||
				current_phase() == phase::service_uplink_wait ||
				current_phase() == phase::service_uplink_acknowledgement) &&
			packet.length >= 9 && packet.payload[8] == 0xc1 &&
			m_gsm_session->begin_traffic_assignment())
	{
		// The organically accepted RR Assignment Command configures the new
		// TCH/F as Nokia DSP channel 0xc1. Confirm that exact transaction before
		// asking the ROM for signalling on the newly established main link.
		m_lapdm_link->begin_mobile_establishment(0);
		set_phase(phase::traffic_channel_change);
		m_reports_remaining = 1;
		m_wait_ticks = 0;
		m_report_deferred = true;
	}
	else if (packet.type == 0x1b &&
			current_phase() == phase::traffic_lapdm_establish &&
			packet.length >= 5 && packet.payload[1] == 0xb0)
	{
		if (receive_lapdm_uplink(packet.payload.data() + 2,
					packet.length - 2) ==
				nokia_lapdm_link_device::uplink_result::establish_indication)
		{
			m_traffic_channel_active = true;
			set_phase(phase::traffic_contention_resolution);
			m_reports_remaining = 1;
			m_report_deferred = true;
		}
	}
	else if (packet.type == 0x03 &&
			(current_phase() == phase::serving_bcch || current_phase() == phase::selected_search))
	{
		// DEACTIVATE retires the old receiver.  If the MCU has already queued a
		// newer SEARCH_LIST after completing selection, begin that request only
		// after teardown. It also cancels a pending selected-search terminal; delivering
		// that report after reset makes task 4 discard it in the new controller state.
		if (m_search_requested)
		{
			m_search_requested = false;
			set_phase(phase::selected_search);
			m_reports_remaining = 2;
			m_wait_ticks = 0;
			m_report_deferred = true;
		}
		else
		{
			m_reports_remaining = 0;
			m_wait_ticks = 0;
			m_report_deferred = false;
		}
	}
	else if (packet.type == 0x1b && current_phase() == phase::lapdm_establish &&
			packet.length >= 5 && packet.payload[1] == 0x80)
	{
		// SEND_BLOCK has a two-byte DSP channel header followed by LAPDm. A
		// SABM with information invokes contention resolution, whose UA echoes
		// that information field exactly.  An empty UI block does not end the
		// assigned-channel transmit schedule: the DSP requests the next SDCCH
		// block at the following 51-frame multiframe opportunity.
		const auto result = receive_lapdm_uplink(
				packet.payload.data() + 2, packet.length - 2);
		if (result == nokia_lapdm_link_device::uplink_result::establish_indication &&
				m_gsm_session->establish_layer3(
					m_lapdm_link->layer3_information().data(),
					m_lapdm_link->layer3_length(), m_serving_arfcn))
		{
			trace_layer3_uplink("establish");
			set_phase(phase::contention_resolution);
			m_reports_remaining = 1;
			m_report_deferred = true;
		}
		else if (m_protocol.repeat_empty_assigned_uplink)
		{
			// NHM-5 keeps ownership of the assigned SDCCH after an empty UI
			// block and requests another transmit opportunity.  NSE-8 does not:
			// injecting this retry into its contract shifts the later idle PCH
			// schedule and makes an otherwise valid page miss its receive slot.
			m_reports_remaining = 1;
			m_wait_ticks = 59;
			m_report_deferred = false;
		}
	}
	else if (packet.type == 0x1b &&
			current_phase() == phase::location_update_acknowledgement &&
			packet.length >= 5 && packet.payload[1] == 0x80)
	{
		if (receive_lapdm_uplink(packet.payload.data() + 2,
					packet.length - 2) ==
				nokia_lapdm_link_device::uplink_result::downlink_acknowledgement)
		{
			const auto acknowledged =
					m_gsm_session->pending_downlink_kind();
			if (m_trace_enabled && acknowledged ==
					nokia_gsm_session_device::downlink_kind::
							location_update_accept)
				LOGMASKED(LOG_RADIO,
						"dsp_hle: LAPDm Location Updating Accept acknowledged nr=%u t=%.6f\n",
						m_lapdm_link->pending_receive_sequence(),
						machine().time().as_double());
			const auto action = m_gsm_session->downlink_acknowledged();
			if (action ==
					nokia_gsm_session_device::downlink_kind::channel_release)
			{
				set_phase(phase::rr_channel_release);
				m_reports_remaining = 1;
				m_report_deferred = true;
			}
			else if (acknowledged ==
					nokia_gsm_session_device::downlink_kind::
							authentication_request)
			{
				set_phase(phase::service_uplink_request);
				m_reports_remaining = 1;
				m_report_deferred = true;
			}
		}
	}
	else if (packet.type == 0x1b &&
			current_phase() == phase::channel_release_acknowledgement &&
			packet.length >= 5 && packet.payload[1] == 0x80)
	{
		const auto result = receive_lapdm_uplink(
				packet.payload.data() + 2, packet.length - 2);
		if (result == nokia_lapdm_link_device::uplink_result::downlink_acknowledgement)
		{
			if (m_trace_enabled)
				LOGMASKED(LOG_RADIO,
						"dsp_hle: LAPDm Channel Release acknowledged nr=%u t=%.6f\n",
						m_lapdm_link->pending_receive_sequence(),
						machine().time().as_double());
			if (m_gsm_session->downlink_acknowledged() ==
					nokia_gsm_session_device::downlink_kind::release_complete)
			{
				m_registered =
						m_gsm_session->registered_mobile_identity_length() == 8;
				// A fill observed before Location Updating does not establish
				// the registered subscriber's post-release paging cadence.
				// Require one correctly phased idle PCH fill before queuing the
				// bounded page requested by the laboratory network.
				m_pch_fill_delivered = false;
				set_phase(phase::release_deconfigure);
				m_reports_remaining = 0;
			}
		}
	}
	else if (packet.type == 0x1b &&
			(current_phase() == phase::service_uplink_wait ||
				(m_traffic_channel_active &&
					current_phase() >= phase::service_downlink &&
					current_phase() <= phase::service_uplink_acknowledgement)) &&
			packet.length >= 5 &&
			(packet.payload[1] == 0x80 ||
				(m_traffic_channel_active &&
					(packet.payload[1] == 0xb0 ||
						packet.payload[1] == 0xf0))))
	{
		if (m_traffic_channel_active && packet.payload[1] == 0xb0 &&
				packet.length >=
					2 + gsm::tch_f::packed_control_block{}.size())
		{
			gsm::tch_f::packed_control_block control{};
			std::copy_n(
					packet.payload.begin() + 2, control.size(),
					control.begin());
			m_uplink_transmitter.substitute_facch(
					gsm::tch_f::encode_control(
						gsm::tch_f::unpack_control(control)));
		}
		const auto result = receive_lapdm_uplink(
				packet.payload.data() + 2, packet.length - 2);
		auto acknowledge_downlink =
				[this]() -> nokia_gsm_session_device::downlink_kind
		{
			m_downlink_offset = 0;
			return m_gsm_session->downlink_acknowledged();
		};
		if (result == nokia_lapdm_link_device::uplink_result::information_indication)
		{
			if (m_lapdm_link->last_downlink_acknowledged())
			{
				if (m_lapdm_link->downlink_segmentation_pending(
						m_lapdm_link->layer3_sapi()))
					m_downlink_offset +=
							nokia_lapdm_link_device::maximum_information_length;
				else
					acknowledge_downlink();
			}
			const auto &information = m_lapdm_link->layer3_information();
			trace_layer3_uplink("uplink");
			m_gsm_session->receive_layer3(
					m_lapdm_link->layer3_sapi(),
					information.data(), m_lapdm_link->layer3_length());
			if (m_trace_enabled &&
					m_lapdm_link->layer3_sapi() == 0 &&
					(information[0] & 0x0f) == 0x03 &&
					(information[1] & 0x3f) == 0x05 &&
					m_gsm_session->outgoing_request_pending())
			{
				std::string digits;
				for (unsigned index = 0;
						index <
								m_gsm_session->outgoing_called_digits_length();
						++index)
					digits += char('0' +
							m_gsm_session->outgoing_called_digits()[index]);
				LOGMASKED(LOG_RADIO,
						"dsp_hle: GSM outgoing request id=%u digits=%s t=%.6f\n",
						m_gsm_session->outgoing_request_id(),
						digits.c_str(), machine().time().as_double());
			}
			// A queued L3 response is itself an I frame and piggybacks N(R) for
			// this uplink. Link establishment is a SABM and cannot piggyback
			// N(R); terminal session results likewise carry no message. Both
			// therefore require a standalone LAPDm RR first.
			const auto pending_kind =
					m_gsm_session->pending_downlink_kind();
			m_wait_ticks = 0;
			if (pending_kind ==
							nokia_gsm_session_device::downlink_kind::
									incoming_call_setup)
				m_wait_ticks = m_protocol.mm_information_settle_ticks;
			const bool queued_information =
					pending_kind !=
							nokia_gsm_session_device::downlink_kind::none &&
					pending_kind !=
							nokia_gsm_session_device::downlink_kind::
									sapi3_establishment;
			set_phase(queued_information ?
					phase::service_downlink :
					phase::service_uplink_acknowledgement);
			m_reports_remaining = 1;
			m_report_deferred = true;
		}
		else if (result ==
				nokia_lapdm_link_device::uplink_result::establish_confirmation)
		{
			m_downlink_offset = 0;
			const auto action = m_gsm_session->downlink_acknowledged();
			set_phase(action != nokia_gsm_session_device::downlink_kind::none ?
					phase::service_downlink : phase::service_uplink_request);
			m_reports_remaining = 1;
			if (action != nokia_gsm_session_device::downlink_kind::none)
				m_wait_ticks = 0;
			m_report_deferred = true;
		}
		else if (result ==
				nokia_lapdm_link_device::uplink_result::downlink_acknowledgement)
		{
			if (m_lapdm_link->downlink_segmentation_pending(
					m_lapdm_link->layer3_sapi()))
			{
				m_downlink_offset +=
						nokia_lapdm_link_device::maximum_information_length;
				set_phase(phase::service_downlink);
				m_reports_remaining = 1;
				m_report_deferred = true;
				return;
			}
			const auto acknowledged_kind =
					m_gsm_session->pending_downlink_kind();
			const auto action = acknowledge_downlink();
			if (action == nokia_gsm_session_device::downlink_kind::release_complete)
			{
				m_registered =
						m_gsm_session->registered_mobile_identity_length() == 8;
				enter_release_deconfigure();
				// Registration release arms the configured incoming page.  A
				// completed one-shot service does not repeat; multipart SMS
				// alone leaves a successor queued and therefore re-arms it.
				m_page_transmitted =
						m_gsm_session->incoming_service_completed() &&
						!m_gsm_session->incoming_service_queued();
				m_pch_fill_delivered = false;
			}
			else if (action != nokia_gsm_session_device::downlink_kind::none)
			{
				set_phase(phase::service_downlink);
				m_reports_remaining = 1;
				m_wait_ticks = 0;
				m_report_deferred = true;
				if (action ==
								nokia_gsm_session_device::downlink_kind::
										incoming_call_setup)
					m_wait_ticks = m_protocol.mm_information_settle_ticks;
			}
			else
			{
				// A terminal network CC message does not suspend the assigned
				// physical channel.  Once its uplink acknowledgement has been
				// confirmed by the DSP, provide the following decoded downlink
				// opportunity so an already queued mobile response can enter
				// LAPDm.
				m_followup_downlink_opportunity =
						acknowledged_kind ==
								nokia_gsm_session_device::downlink_kind::
										incoming_call_setup;
				set_phase(phase::service_uplink_request);
				m_reports_remaining = 1;
				m_report_deferred = true;
			}
		}
		else if (result ==
				nokia_lapdm_link_device::uplink_result::release_indication)
		{
			set_phase(phase::traffic_release_acknowledgement);
			m_reports_remaining = 1;
			m_wait_ticks = 0;
			m_report_deferred = true;
		}
		else
		{
			// The firmware can return an idle UI/fill block while a service waits
			// for user input or an upper-layer response.  The assigned SDCCH
			// continues to have independent downlink opportunities during that
			// wait; alternate a decoded fill opportunity with the next uplink
			// request instead of polling only the uplink direction forever.
			m_followup_downlink_opportunity = true;
			set_phase(phase::service_uplink_request);
			m_reports_remaining = 1;
			m_wait_ticks = 100;
			m_report_deferred = true;
		}
	}
}

void nokia_radio_peer_device::emit_report()
{
	u8 payload[166] = { 0 };
	bool transmitted_page = false;
	bool off_group_page = false;
	u8 report_type = next_report_type();
	const bool neighbour_measurement_report =
			m_neighbour_bcch_pending && current_phase() == phase::serving_bcch &&
			(m_neighbour_instruction_mode == 0x02 ||
					report_type == 0x83);
	const bool neighbour_rssi_report =
			neighbour_measurement_report &&
			m_neighbour_instruction_mode == 0x02;
	const bool asynchronous_neighbour_report = neighbour_rssi_report;
	const bool neighbour_bcch_report =
			neighbour_measurement_report && !neighbour_rssi_report;
	if (neighbour_measurement_report)
		report_type = neighbour_rssi_report ? 0x83 : 0x80;
	if (report_type == 0x80 && serving_pch_report() &&
			!m_downlink_signalling_failed)
	{
		gsm::mobility::downlink_signalling_counter counter;
		counter.restore(m_downlink_signalling_count);
		m_downlink_signalling_failed = counter.observe(
				m_gsm_network->cell_receivable(m_serving_arfcn));
		m_downlink_signalling_count = counter.value();
		if (m_downlink_signalling_failed)
		{
			// MDI 0x8c is the bodyless DSP-to-MCU
			// DOWNLINK_SIGNALLING_FAIL indication. Layer 1 owns the TS 45.008
			// paging-block counter; the MCU remains responsible for deciding
			// whether and how to search or reselect.
			report_type = 0x8c;
			m_serving_loss_pending = true;
			m_gsm_network->downlink_signalling_failed(m_serving_arfcn);
			if (m_trace_enabled)
				LOGMASKED(LOG_RADIO,
						"dsp_hle: DOWNLINK_SIGNALLING_FAIL arfcn=%u "
						"fn=%u t=%.6f\n",
						m_serving_arfcn, m_tdma_frame_number,
						machine().time().as_double());
		}
	}
	if (current_phase() == phase::lapdm_establish ||
			current_phase() == phase::location_update_ack_request ||
			current_phase() == phase::channel_release_uplink_request ||
			current_phase() == phase::service_uplink_request)
		payload[0] = m_traffic_channel_active ? 0xb0 : 0x80;

	if (report_type == 0x8b)
		encode_measurement_report(payload);

	if (report_type == 0x80)
	{
		const bool pch_report =
				!neighbour_bcch_report && serving_pch_report();
		const bool dedicated_downlink =
				(current_phase() >= phase::contention_resolution &&
					current_phase() <= phase::rr_channel_release) ||
				current_phase() == phase::service_downlink ||
				current_phase() == phase::service_uplink_acknowledgement ||
				current_phase() == phase::traffic_contention_resolution ||
				current_phase() == phase::traffic_release_acknowledgement;
		// RECEIVED_BLOCK carries channel, BSIC, error, frame number, ARFCN,
		// shift and then a 24-byte GSM L2 block.  Channel 0x40 is SCH; 0x50 is
		// BCCH; 0x60 is the shared paging/access-grant common control channel;
		// 0x80 is SDCCH and 0xb0 is the active TCH/F FACCH selector.
		payload[0] = current_phase() == phase::serving_sch_observation ? 0x40 :
				neighbour_bcch_report ? 0x50 :
				pch_report ? 0x60 :
				dedicated_downlink ?
					(m_traffic_channel_active ? 0xb0 : 0x80) :
				current_phase() == phase::random_access ? 0x60 :
				((current_phase() < phase::candidate_channel_change ||
					current_phase() == phase::selected_search) ? 0x40 : 0x50);
		const bool candidate_sch =
				payload[0] == 0x40 &&
				(current_phase() == phase::candidate_measurement ||
					current_phase() == phase::candidate_sync) &&
				m_search_arfcn_count != 0;
		const u16 report_arfcn =
				current_phase() == phase::serving_sch_observation ?
					m_sch_observation_arfcn :
				neighbour_bcch_report ?
					m_neighbour_bcch_arfcn :
				candidate_sch ? m_search_arfcns[0] : m_receiver_arfcn;
		// Public DCT3 traces correlate NMEAS_INSTRUCTIONS with the immediately
		// following neighbour block through both its ARFCN and BSIC. Keep that
		// asynchronous observation owned by the named RF carrier; it must not
		// be presented as serving-cell SI merely to enter an unrelated parser.
		const auto *report_cell =
				m_gsm_network->cell_by_arfcn(report_arfcn);
		payload[1] =
				current_phase() == phase::serving_sch_observation &&
						report_arfcn == m_last_neighbour_instruction_arfcn ?
					m_last_neighbour_instruction_bsic :
				neighbour_bcch_report ?
				m_neighbour_instruction_bsic :
				report_arfcn == m_receiver_arfcn ?
					m_receiver_bsic :
					(report_cell ? report_cell->bsic : 0x12);
		// RECEIVED_BLOCK byte 2 is the recovered decoded-block error flag.
		// The independently inspected NSE-3 consumer treats zero as a decoded
		// block and one as a failed block; it does not interpret a magnitude.
		// A vanished carrier still reaches its scheduled receive opportunity,
		// but it cannot yield a valid BCCH/PCH block. This is distinct from
		// NO_BCCH_LEFT, which terminates a finite search transaction.
		payload[2] =
				m_gsm_network->cell_receivable(report_arfcn) ? 0 : 1;
		if (payload[0] == 0x50 &&
				!m_gsm_network->system_information_decodable(report_arfcn))
			payload[2] = 1;
		if (payload[0] == 0x40 &&
				!m_gsm_network->synchronization_stable(report_arfcn))
			payload[2] = 1;
		// Frame numbering belongs to the radio Layer-1 clock, not the host
		// scheduler instant at which the DSP happens to collect a report.
		// m_tdma_frame_number is advanced by the GSM burst timer and is saved
		// with the rest of the L1 state, so SCH/BCCH metadata remains coherent
		// across save/load instead of moving by a frame after a slightly
		// different firmware poll latency.
		u32 frame_number = current_phase() == phase::random_access ? m_access_frame :
				m_tdma_frame_number % 2'715'648;
		if (pch_report)
			frame_number = paging_frame_number(
					frame_number, paging_schedule::monitored);
		else if (payload[0] == 0x50 && !neighbour_bcch_report)
		{
			// Decoded BCCH blocks occupy successive 51-frame control
			// multiframes. Firmware processing can defer a peer poll slightly;
			// resampling the wall clock here used to repeat or skip TC values,
			// corrupting the eight-multiframe SI schedule presented on-air.
			// Anchor the first block to real GSM time, then advance the
			// transport boundary by exactly one multiframe per decoded block.
			if (!m_bcch_frame_valid)
			{
				m_bcch_frame_number = (frame_number / 51) * 51;
				m_bcch_frame_valid = true;
			}
			else
				m_bcch_frame_number =
						(m_bcch_frame_number + 51) % 2'715'648;
			frame_number = m_bcch_frame_number;
		}
		payload[3] = frame_number >> 16;
		payload[4] = frame_number >> 8;
		payload[5] = frame_number;
		payload[6] = report_arfcn >> 8;
		payload[7] = report_arfcn;

		if (current_phase() == phase::contention_resolution)
		{
			const auto frame = m_lapdm_link->build_ua();
			deliver_lapdm_downlink(frame, payload + 10, frame_number);
		}
		else if (current_phase() == phase::traffic_contention_resolution)
		{
			const auto frame = m_lapdm_link->build_ua();
			deliver_lapdm_downlink(frame, payload + 10, frame_number);
		}
		else if (current_phase() == phase::traffic_release_acknowledgement)
		{
			const auto frame = m_lapdm_link->build_release_ua();
			deliver_lapdm_downlink(frame, payload + 10, frame_number);
		}
		else if (current_phase() == phase::location_update_accept ||
				current_phase() == phase::rr_channel_release)
		{
			// Both carry the session's queued message unsegmented on SAPI 0;
			// they differ only in which phase follows the acknowledgement.
			const auto &message = m_gsm_session->pending_downlink();
			const auto frame = m_lapdm_link->build_information_frame(
					0, message.data.data(), message.length);
			deliver_lapdm_downlink(frame, payload + 10, frame_number);
		}
		else if (current_phase() == phase::service_downlink)
		{
			const auto &message = m_gsm_session->pending_downlink();
			if (m_trace_enabled && m_downlink_offset == 0 &&
					message.length >= 2)
				LOGMASKED(LOG_RADIO,
						"dsp_hle: GSM service downlink kind=%u sapi=%u pd=%02x message=%02x length=%u t=%.6f\n",
						message.kind, message.sapi,
						message.data[0] & 0x0f, message.data[1] & 0x3f,
						message.length, machine().time().as_double());
			std::array<u8, nokia_lapdm_link_device::frame_length> frame;
			if (message.kind == u8(
					nokia_gsm_session_device::downlink_kind::sapi3_establishment))
				frame = m_lapdm_link->build_sabm_command(message.sapi);
			else
			{
				const unsigned count = std::min<unsigned>(
						nokia_lapdm_link_device::maximum_information_length,
						message.length - m_downlink_offset);
				const bool more_data =
						m_downlink_offset + count < message.length;
				frame = m_lapdm_link->build_information_frame(
						message.sapi, message.data.data() + m_downlink_offset,
						count, more_data);
			}
			deliver_lapdm_downlink(frame, payload + 10, frame_number);
		}
		else if (current_phase() == phase::service_uplink_acknowledgement)
		{
			const auto frame = m_lapdm_link->build_receive_ready(
					m_lapdm_link->layer3_sapi());
			deliver_lapdm_downlink(frame, payload + 10, frame_number);
		}
		else if (pch_report)
		{
			const auto service = m_incoming_smart_message_after_registration ?
					nokia_gsm_session_device::incoming_service::smart_message :
					m_incoming_sms_after_registration ?
					nokia_gsm_session_device::incoming_service::sms :
					(m_incoming_call_after_registration ||
					 m_host_incoming_call_pending) ?
					nokia_gsm_session_device::incoming_service::call :
					nokia_gsm_session_device::incoming_service::none;
			const bool page_requested =
					m_page_after_registration || m_host_incoming_call_pending;
			const bool service_admissible =
					m_gsm_session->incoming_service_admissible(service);
			if (m_registered && page_requested &&
					m_pch_fill_delivered && !m_page_transmitted &&
					!service_admissible)
			{
				// Reject malformed network ingress before it can become a
				// radio page or mutate handset/SIM state. The one-shot page
				// latch also prevents an invalid queued item from being
				// reconsidered on every monitored PCH block.
				m_page_transmitted = true;
				LOGMASKED(LOG_RADIO,
						"dsp_hle: GSM incoming service rejected before "
						"paging service=%u t=%.6f\n",
						unsigned(service), machine().time().as_double());
			}
			const bool transmit_page =
					m_registered && page_requested && m_pch_fill_delivered &&
					(!m_page_requires_reselection || m_has_reselected) &&
					!m_page_transmitted &&
					service_admissible &&
					m_gsm_session->queue_incoming_page(service);
			const auto &identity =
					m_gsm_session->registered_mobile_identity();
			const unsigned identity_length =
					m_gsm_session->registered_mobile_identity_length();
			const bool monitored_page =
					transmit_page &&
					m_gsm_network->paging_request_monitored(
							identity.data(), identity_length);
			const auto block = monitored_page ?
					m_gsm_session->paging_request() :
					m_gsm_network->paging_fill();
			std::copy(block.begin(), block.end(), std::begin(payload) + 10);
			transmitted_page = monitored_page;
			if (monitored_page)
				m_host_incoming_call_pending = false;
			off_group_page = transmit_page && !monitored_page;
		}
		else if (payload[0] == 0x60)
		{
			const auto assignment = m_gsm_network->immediate_assignment(
					m_access_ra, frame_number, m_serving_arfcn);
			std::copy(assignment.begin(), assignment.end(), std::begin(payload) + 10);
		}
		else if (payload[0] == 0x50)
		{
			// TS 45.002 defines an eight-multiframe BCCH schedule. Broadcast SI1 at
			// TC 0 even though it is optional for this non-hopping cell: the ROM can
			// infer its completion during initial acquisition, but requires the real
			// block when revalidating an already active serving channel.
			static constexpr std::array<unsigned, 8> SI_BY_TC = { 0, 1, 2, 3, 1, 1, 2, 3 };
			const unsigned tc = (frame_number / 51) & 7;
			const auto system_information =
					m_gsm_network->system_information(
							SI_BY_TC[tc], report_arfcn);
			std::copy(system_information.begin(), system_information.end(), std::begin(payload) + 10);
		}
		else if (payload[0] == 0x40)
		{
			const auto *cell = m_gsm_network->cell_by_arfcn(report_arfcn);
			const auto information =
					gsm::mobility::synchronization_channel_information(
							cell ?
								m_gsm_network->synchronization_bsic(
										report_arfcn, frame_number) :
								0,
							frame_number);
			std::copy(information.begin(), information.end(),
					std::begin(payload) + 10);
		}
	}

	if (report_type == 0x83)
	{
		// RSSI_RESULTS carries the serving scalar and, once the MCU has
		// published NEIGHBOUR_LIST, the current background neighbour sample.
		// The scalar controller reads byte 2.  The independently captured DCT3
		// trace and ROM consumer agree that bytes 4..5 carry the measured
		// neighbour ARFCN; the adjacent signed values are radio measurements,
		// not candidate-selection outcomes.
		const s8 serving_rssi =
				m_gsm_network->cell_rssi(
						m_serving_arfcn, m_idle_measurement_sample);
		payload[2] = u8(serving_rssi);
		payload[3] = u8(serving_rssi);
		if (m_neighbour_arfcn_count)
		{
			const u16 neighbour = m_neighbour_arfcns[
					m_idle_measurement_sample % m_neighbour_arfcn_count];
			const s8 neighbour_rssi =
					m_gsm_network->cell_rssi(
							neighbour, m_idle_measurement_sample);
			payload[4] = neighbour >> 8;
			payload[5] = neighbour;
			payload[6] = u8(neighbour_rssi);
			payload[7] = u8(neighbour_rssi);
		}
	}

	if (report_type == 0x89)
		encode_channel_confirmation(payload);

	if (report_type == 0x84 && current_phase() == phase::random_access)
		encode_random_access_info(payload);

	// Camped neighbour RSSI_RESULTS uses the 24-byte form observed on DCT3;
	// the short eight-byte form remains sufficient before a BA list exists.
	// Unused measurement slots are explicitly the signed no-signal value.
	if (report_type == 0x83 && m_neighbour_arfcn_count)
		std::fill(std::begin(payload) + 8, std::begin(payload) + 24, u8(0x80));
	const unsigned payload_length = report_type == 0x8b ? 166 :
			report_type == 0x80 && payload[0] == 0x40 ? 14 :
			report_type == 0x80 ? 34 :
			(report_type == 0x83 && m_neighbour_arfcn_count) ? 24 : 8;
	if (report_type == 0x80 && payload[0] == 0xb0)
	{
		gsm::tch_f::packed_control_block control{};
		std::copy_n(
				std::begin(payload) + 10, control.size(), control.begin());
		m_downlink_transmitter.substitute_facch(
				gsm::tch_f::encode_control(
					gsm::tch_f::unpack_control(control)));
	}
	if (!m_transport->enqueue_rx_packet(report_type, payload, payload_length))
		return;
	if (report_type == 0x80 && payload[0] == 0x50 && payload[2] == 0 &&
			m_receiver_arfcn != m_serving_arfcn)
		m_candidate_bcch_valid = true;
	if (neighbour_bcch_report && payload[2] == 0)
	{
		m_candidate_bcch_valid = true;
		m_gsm_network->neighbour_bcch_observed(m_neighbour_bcch_arfcn);
	}
	if (report_type == 0x83)
		++m_idle_measurement_sample;
	if (neighbour_measurement_report)
	{
		m_neighbour_bcch_pending = false;
		m_neighbour_bcch_arfcn = 0xffff;
		m_neighbour_instruction_mode = 0;
		m_neighbour_instruction_bsic = 0;
		if (asynchronous_neighbour_report)
			m_wait_ticks = m_neighbour_resume_wait_ticks;
		m_neighbour_resume_wait_ticks = 0;
	}

	if (report_type == 0x80 && payload[0] == 0x60)
	{
		const u32 frame_number =
				(payload[3] << 16) | (payload[4] << 8) | payload[5];
		if (transmitted_page || off_group_page)
		{
			m_page_transmitted = true;
			if (m_trace_enabled)
			{
				if (off_group_page)
					LOGMASKED(LOG_RADIO,
							"dsp_hle: PCH off-group IMSI page not monitored "
							"channel=60 air_fn=%u monitor_fn=%u t=%.6f\n",
							paging_frame_number(
									frame_number, paging_schedule::transmitted),
							frame_number,
							machine().time().as_double());
				else
					LOGMASKED(LOG_RADIO,
							"dsp_hle: PCH IMSI page transmitted channel=60 "
							"fn=%u t=%.6f\n",
							frame_number, machine().time().as_double());
			}
		}
		else if (!m_pch_fill_delivered)
		{
			m_pch_fill_delivered = true;
			if (m_registered)
				m_gsm_network->stable_camp_observed();
			if (m_trace_enabled)
				LOGMASKED(LOG_RADIO,
						"dsp_hle: PCH no-identity fill channel=60 fn=%u t=%.6f\n",
						frame_number, machine().time().as_double());
		}
	}

	if (report_type == 0x8b && current_phase() >= phase::candidate_measurement)
		++m_search_round;
	++m_reports_sent;
	if (!asynchronous_neighbour_report)
	{
		--m_reports_remaining;
		advance_after_report(report_type);
	}
	m_transport->notify_rx();
	if (m_trace_enabled)
		LOGMASKED(LOG_RADIO, "dsp_hle: radio peer RX type=%02x sequence=%u t=%.6f\n",
				report_type, m_reports_sent, machine().time().as_double());
}

void nokia_radio_peer_device::advance_after_report(u8 report_type)
{
	if (current_phase() == phase::serving_sch_observation &&
			(report_type == 0x80 || report_type == 0x8a ||
					report_type == 0x8f))
	{
		if (report_type == 0x80)
		{
			m_serving_loss_pending = false;
			set_phase(phase::serving_bcch);
			m_reports_remaining = serving_cycle_reports();
			m_wait_ticks = 59;
		}
		else if (report_type == 0x8a)
		{
			// An unsuccessful power-synchronization observation precedes the
			// finite-search terminal at the next SCH opportunity.
			m_wait_ticks = 12;
		}
		else
		{
			// A finite synchronization search with no carrier cannot resume
			// decoded blocks on the lost serving channel.  Wait for a new
			// firmware-owned search or receiver configuration.
			m_serving_loss_pending = false;
			set_phase(phase::inactive);
			m_reports_remaining = 0;
			m_wait_ticks = 0;
		}
	}
	else if (current_phase() == phase::candidate_channel_change && report_type == 0x89)
	{
		set_phase(phase::candidate_ra_info);
		m_wait_ticks = 100;
	}
	else if (current_phase() == phase::candidate_ra_info && report_type == 0x84)
	{
		set_phase(phase::serving_bcch);
		m_reports_remaining = serving_cycle_reports();
		m_wait_ticks = 59;
	}
	else if (current_phase() == phase::serving_bcch && report_type == 0x80)
	{
		// Space decoded BCCH blocks at a 51-frame-multiframe cadence.  A PCH
		// indication is already delivered in advance for its subscriber paging
		// frame and must not consume another whole BCCH interval: doing so makes
		// the interleaved report cycle advance two multiframes at a time and
		// aliases the eight-multiframe SI schedule down to SI2/SI4.
		const bool pch_report =
				(m_registered || m_idle_common_control_active) &&
				((m_reports_remaining + 1) % 3) == 2;
		if (!pch_report)
			m_wait_ticks = 59;
	}
	else if (current_phase() == phase::serving_bcch && report_type == 0x83 &&
			m_reports_remaining == 0)
	{
		// A camped GSM cell broadcasts System Information continuously.
		m_reports_remaining = serving_cycle_reports();
	}
	else if (current_phase() == phase::serving_bcch && report_type == 0x8c)
	{
		// The indication replaces one scheduled PCH receive opportunity and
		// therefore retains its ordinary control-multiframe pacing.
		m_wait_ticks = 59;
	}
	else if (current_phase() == phase::selected_search && (report_type == 0x87 || report_type == 0x8f))
	{
		set_phase(phase::serving_bcch);
		m_reports_remaining = serving_cycle_reports();
		m_wait_ticks = 59;
	}
	else if (current_phase() == phase::serving_channel_change && report_type == 0x89)
	{
		if (m_reselection_validation_pending)
		{
			// A newly committed serving cell must publish its own complete SI
			// state before inherited registration can resume PCH. RR/MM, not
			// the radio peer, decides whether the decoded LAI requires LU.
			set_phase(phase::selected_bcch);
			m_reports_remaining = 16;
		}
		else
		{
			set_phase(phase::serving_bcch);
			m_reports_remaining = serving_cycle_reports();
		}
		m_wait_ticks = 59;
	}
	else if (current_phase() == phase::selected_channel_change && report_type == 0x89)
	{
		set_phase(phase::selected_ra_info);
		m_wait_ticks = 100;
	}
	else if (current_phase() == phase::selected_ra_info && report_type == 0x84)
	{
		set_phase(phase::selected_bcch);
		// Validate the selected cell across one complete eight-multiframe BCCH
		// schedule. Each block is followed by its serving-channel RSSI result.
		// A usable cell does not also produce NO_BCCH_LEFT: that contradictory
		// terminal can be consumed after the firmware starts its next search and
		// incorrectly fail the newer transaction.
		m_reports_remaining = 16;
		m_wait_ticks = 59;
	}
	else if (current_phase() == phase::selected_bcch && report_type == 0x80)
	{
		if (m_reselection_validation_pending && m_reports_remaining == 0)
		{
			m_reselection_validation_pending = false;
			set_phase(phase::serving_bcch);
			m_reports_remaining = serving_cycle_reports();
		}
		m_wait_ticks = 59;
	}
	else if (current_phase() == phase::selected_bcch && report_type == 0x83)
	{
		// The measurement belongs to the preceding received block; the next BCCH
		// block remains paced by the multiframe delay set on that block.
		if (m_reports_remaining == 0)
		{
			m_reselection_validation_pending = false;
			set_phase(phase::serving_bcch);
			m_reports_remaining = serving_cycle_reports();
			m_wait_ticks = 59;
		}
	}
	else if (current_phase() == phase::selected_bcch && report_type == 0x87)
	{
		set_phase(phase::serving_bcch);
		m_reports_remaining = serving_cycle_reports();
		m_wait_ticks = 59;
	}
	else if (current_phase() == phase::selected_bcch_channel_change && report_type == 0x89)
	{
		// The accepted logical-channel change retires the selected-cell scan.
		// Firmware immediately issues its next SEARCH_LIST after consuming the RR
		// completion; replaying the pre-change terminal makes that newer request
		// lose ownership and restarts selection indefinitely.
		set_phase(phase::serving_bcch);
		m_reports_remaining = serving_cycle_reports();
		m_selected_reports_remaining = 0;
		m_wait_ticks = 59;
	}
	else if (current_phase() == phase::random_access && report_type == 0x80)
	{
		// Further progress is firmware-owned: a matching assignment configures
		// the dedicated channel and causes the MCU to transmit LAPDm SABM.
		m_reports_remaining = 0;
	}
	else if (current_phase() == phase::assigned_channel_change && report_type == 0x89)
	{
		set_phase(phase::lapdm_establish);
		m_reports_remaining = 1;
		m_report_deferred = true;
	}
	else if (current_phase() == phase::lapdm_establish && report_type == 0x86)
	{
		m_reports_remaining = 0;
	}
	else if (current_phase() == phase::contention_resolution && report_type == 0x80)
	{
		const auto action = m_gsm_session->contention_resolution_delivered();
		if (action ==
				nokia_gsm_session_device::downlink_kind::location_update_accept ||
				action ==
				nokia_gsm_session_device::downlink_kind::authentication_request)
		{
			set_phase(phase::location_update_accept);
			m_reports_remaining = 1;
			m_report_deferred = true;
		}
		else if (action == nokia_gsm_session_device::downlink_kind::channel_release)
		{
			set_phase(phase::rr_channel_release);
			m_reports_remaining = 1;
			m_report_deferred = true;
		}
		else if (action ==
				nokia_gsm_session_device::downlink_kind::cm_service_accept ||
				action ==
				nokia_gsm_session_device::downlink_kind::cm_service_reject ||
				action ==
				nokia_gsm_session_device::downlink_kind::authentication_request ||
				action ==
				nokia_gsm_session_device::downlink_kind::authentication_reject ||
				action ==
				nokia_gsm_session_device::downlink_kind::cipher_mode_command ||
				action == nokia_gsm_session_device::downlink_kind::mm_information)
		{
			set_phase(phase::service_downlink);
			m_reports_remaining = 1;
			m_report_deferred = true;
		}
	}
	else if (current_phase() == phase::location_update_accept && report_type == 0x80)
	{
		set_phase(phase::location_update_ack_request);
		m_reports_remaining = 1;
		m_report_deferred = true;
	}
	else if (current_phase() == phase::location_update_ack_request && report_type == 0x86)
	{
		set_phase(phase::location_update_acknowledgement);
		m_reports_remaining = 0;
	}
	else if (current_phase() == phase::rr_channel_release && report_type == 0x80)
	{
		set_phase(phase::channel_release_uplink_request);
		m_reports_remaining = 1;
		m_report_deferred = true;
	}
	else if (current_phase() == phase::channel_release_uplink_request && report_type == 0x86)
	{
		set_phase(phase::channel_release_acknowledgement);
		m_reports_remaining = 0;
	}
	else if (current_phase() == phase::release_channel_change && report_type == 0x89)
	{
		// CHANNEL_CONFIGURE completion is the final physical acknowledgement
		// when CC Release Complete raced ahead of the LAPDm downlink ACK.
		m_gsm_session->downlink_acknowledged();
		m_traffic_channel_active = false;
		clear_speech_queues();
		set_phase(phase::serving_bcch);
		m_reports_remaining = serving_cycle_reports();
		m_wait_ticks = 59;
	}
	else if (current_phase() == phase::service_downlink && report_type == 0x80)
	{
		// LAPDm uses a one-frame transmit window here. Even when M=1, wait for
		// the handset's N(R) before emitting the next Layer-3 segment.
		set_phase(phase::service_uplink_request);
		m_reports_remaining = 1;
		m_report_deferred = true;
	}
	else if (current_phase() == phase::service_uplink_request && report_type == 0x86)
	{
		if (m_followup_downlink_opportunity)
		{
			m_followup_downlink_opportunity = false;
			set_phase(phase::service_uplink_acknowledgement);
			m_reports_remaining = 1;
			m_report_deferred = true;
		}
		else
		{
			set_phase(phase::service_uplink_wait);
			m_reports_remaining = 0;
		}
	}
	else if (current_phase() == phase::service_uplink_acknowledgement && report_type == 0x80)
	{
		if (m_gsm_session->idle())
		{
			enter_release_deconfigure();
		}
		else
		{
			// An uplink I frame can cross a network-initiated SAPI 3 SABM.
			// Acknowledge that I frame without retransmitting the SABM while
			// its UA is already in flight: a second SABM resets the handset's
			// SMS link immediately after it has accepted CP-DATA.
			const bool awaiting_sapi3 =
					m_gsm_session->pending_downlink_kind() ==
							nokia_gsm_session_device::downlink_kind::
									sapi3_establishment &&
					m_lapdm_link->awaiting_establishment(3);
			set_phase(m_gsm_session->pending_downlink_kind() !=
							nokia_gsm_session_device::downlink_kind::none &&
							!awaiting_sapi3 ?
					phase::service_downlink : phase::service_uplink_request);
			m_reports_remaining = 1;
			m_report_deferred = true;
		}
	}
	else if (current_phase() == phase::traffic_channel_change && report_type == 0x89)
	{
		// The mobile initiates the new main link without a BLOCK_REQUEST.
		set_phase(phase::traffic_lapdm_establish);
		m_reports_remaining = 0;
	}
	else if (current_phase() == phase::traffic_contention_resolution &&
			report_type == 0x80)
	{
		set_phase(phase::service_uplink_request);
		m_reports_remaining = 1;
		m_report_deferred = true;
	}
	else if (current_phase() == phase::traffic_release_acknowledgement &&
			report_type == 0x80)
	{
		set_phase(phase::release_deconfigure);
		m_reports_remaining = 0;
	}
}


bool nokia_radio_peer_device::phase_waits() const
{
	switch (current_phase())
	{
	case phase::candidate_measurement:
	case phase::candidate_sync:
	case phase::serving_bcch:
	case phase::serving_sch_observation:
	case phase::selected_bcch:
	case phase::serving_channel_change:
	case phase::service_downlink:
	case phase::service_uplink_request:
		return m_wait_ticks != 0;
	case phase::lapdm_establish:
		return m_protocol.repeat_empty_assigned_uplink && m_wait_ticks != 0;
	default:
		return false;
	}
}

void nokia_radio_peer_device::tick()
{
	if (!m_enabled)
		return;

	// A host or other network policy may enqueue Layer 3 while the assigned
	// link is waiting for the mobile's next uplink opportunity. Promote that
	// saved session message into the ordinary decoded downlink/FACCH path;
	// the producer never owns radio scheduling.
	if (m_traffic_channel_active &&
			current_phase() == phase::service_uplink_request &&
			m_gsm_session->pending_downlink_kind() !=
					nokia_gsm_session_device::downlink_kind::none)
	{
		set_phase(phase::service_downlink);
		m_reports_remaining = 1;
		m_wait_ticks = 0;
		m_report_deferred = true;
	}

	if (m_reports_remaining != 0 && phase_waits())
		--m_wait_ticks;
	if ((current_phase() == phase::candidate_ra_info || current_phase() == phase::selected_ra_info) &&
			m_reports_remaining == 0 && m_wait_ticks != 0 && --m_wait_ticks == 0)
		m_reports_remaining = 1;
	if (m_report_deferred)
		m_report_deferred = false;
	else if (m_reports_remaining != 0 && !phase_waits())
		emit_report();
}

bool nokia_radio_peer_device::fast_completion_pending() const
{
	return m_enabled && current_phase() == phase::candidate_sync && m_reports_remaining == 1;
}
