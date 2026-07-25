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
	save_item(NAME(m_wait_ticks));
	save_item(NAME(m_search_mode));
	save_item(NAME(m_access_ra));
	save_item(NAME(m_access_frame));
	save_item(NAME(m_search_has_arfcn1));
	save_item(NAME(m_report_deferred));
	save_item(NAME(m_search_requested));
	save_item(NAME(m_selected_reports_remaining));
	save_item(NAME(m_registered));
	save_item(NAME(m_pch_fill_delivered));
	save_item(NAME(m_page_transmitted));
	save_item(NAME(m_traffic_channel_active));
	save_item(NAME(m_downlink_offset));
	save_item(NAME(m_downlink_speech));
	save_item(NAME(m_downlink_speech_good));
	save_item(NAME(m_uplink_speech));
	save_item(NAME(m_downlink_speech_head));
	save_item(NAME(m_downlink_speech_count));
	save_item(NAME(m_uplink_speech_head));
	save_item(NAME(m_uplink_speech_count));
	save_item(NAME(m_speech_loopback));
	save_item(NAME(m_lab_voice_source));
	save_item(NAME(m_uplink_speech_received));
	save_item(NAME(m_tdma_frame_number));
	save_item(NAME(m_l1_traffic_active));
	save_item(NAME(m_uplink_facch_blocks));
	save_item(NAME(m_downlink_facch_blocks));
	save_item(NAME(m_uplink_bad_speech_blocks));
	save_item(NAME(m_downlink_bad_speech_blocks));
	save_item(NAME(m_uplink_tch_burst_error_period));
	save_item(NAME(m_uplink_tch_burst_error_span));
	save_item(NAME(m_uplink_tch_bursts));
	save_item(NAME(m_uplink_tch_bursts_impaired));
	save_item(NAME(m_downlink_tch_burst_error_period));
	save_item(NAME(m_downlink_tch_burst_error_span));
	save_item(NAME(m_downlink_tch_bursts));
	save_item(NAME(m_downlink_tch_bursts_impaired));

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
	m_phase = phase::inactive;
	m_search_round = 0;
	m_wait_ticks = 0;
	m_search_mode = 0;
	m_access_ra = 0;
	m_access_frame = 0;
	m_search_has_arfcn1 = false;
	m_report_deferred = false;
	m_search_requested = false;
	m_selected_reports_remaining = 0;
	m_registered = false;
	m_pch_fill_delivered = false;
	m_page_transmitted = false;
	m_traffic_channel_active = false;
	m_downlink_offset = 0;
	clear_speech_queues();
	m_uplink_speech_received = 0;
	m_tdma_frame_number = 0;
	m_l1_traffic_active = false;
	m_uplink_facch_blocks = 0;
	m_downlink_facch_blocks = 0;
	m_uplink_bad_speech_blocks = 0;
	m_downlink_bad_speech_blocks = 0;
	m_uplink_tch_bursts = 0;
	m_uplink_tch_bursts_impaired = 0;
	m_downlink_tch_bursts = 0;
	m_downlink_tch_bursts_impaired = 0;
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
		const auto training = gsm::tch_f::training_sequence(2);
		if (const auto uplink =
					m_uplink_sacch_transmitter.next_burst(phase))
		{
			const auto air = gsm::tch_f::pack_normal_burst(*uplink, training);
			m_network_sacch_receiver.receive(
					gsm::tch_f::unpack_normal_burst(air));
		}
		if (const auto downlink =
					m_downlink_sacch_transmitter.next_burst(phase))
		{
			const auto air = gsm::tch_f::pack_normal_burst(*downlink, training);
			m_handset_sacch_receiver.receive(
					gsm::tch_f::unpack_normal_burst(air));
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
	const auto network_block = m_network_receiver.receive(
			gsm::tch_f::unpack_normal_burst(uplink_air));
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
		if (m_lab_voice_source)
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

	auto downlink_air = gsm::tch_f::pack_normal_burst(
			[&]()
			{
				auto payload = m_downlink_transmitter.next_burst();
				++m_downlink_tch_bursts;
				// A generic deterministic hard-error profile at the future A5
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
			}(), training);
	const auto handset_block = m_handset_receiver.receive(
			gsm::tch_f::unpack_normal_burst(downlink_air));
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
		"candidate_ra_info", "serving_bcch", "serving_idle_ra", "candidate_retry",
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
		"traffic_contention_resolution", "traffic_release_acknowledgement"
	};
	const unsigned index = unsigned(value);
	return index < std::size(NAMES) ? NAMES[index] : "invalid";
}

const char *nokia_radio_peer_device::phase_name() const
{
	return phase_name(m_phase);
}

u8 nokia_radio_peer_device::next_report_type() const
{
	// Fixed entries are declarative; 0xff marks phases whose report depends on
	// request data or position within a correlated multi-report transaction.
	static constexpr std::array<u8, phase::count> FIXED_REPORT = {
		0x87, 0x87, 0x87, 0x8b, 0xff, 0xff, 0x84, 0xff,
		0x8c, 0xff, 0xff, 0x89, 0x89, 0xff, 0x84, 0x89,
		0xff, 0x89, 0x86, 0x80, 0x80, 0x86, 0x87, 0x80,
		0x86, 0x87, 0x87, 0x89, 0x80, 0x86, 0x87, 0x80,
		0x89, 0x86, 0x80, 0x80
	};

	const u8 fixed = m_phase < FIXED_REPORT.size() ? FIXED_REPORT[m_phase] : 0x87;
	if (fixed != 0xff)
		return fixed;

	switch (m_phase)
	{
	case phase::candidate_sync:
		return m_reports_remaining == 2 ? 0x8b : 0x80;
	case phase::candidate_channel_change:
		return m_reports_remaining == 2 ? 0x8f : 0x89;
	case phase::serving_bcch:
		if (m_registered)
			return (m_reports_remaining % 3) == 1 ? 0x83 : 0x80;
		return (m_reports_remaining & 1) == 0 ? 0x80 : 0x83;
	case phase::candidate_retry:
		return m_reports_remaining == 2 ? 0x8b : 0x87;
	case phase::selected_search:
		return m_reports_remaining == 2 ? 0x8b :
				(((m_search_mode == 0x00 && m_search_has_arfcn1) ||
					m_search_mode == 0x40 || m_search_mode == 0x50) ? 0x80 : 0x87);
	case phase::selected_bcch:
		return m_reports_remaining == 1 ? 0x87 :
				((m_reports_remaining & 1) == 0 ? 0x83 : 0x80);
	case phase::random_access:
		return m_reports_remaining == 3 ? 0x8c :
				(m_reports_remaining == 2 ? 0x84 : 0x80);
	default:
		return 0x87;
	}
}

unsigned nokia_radio_peer_device::serving_cycle_reports() const
{
	return m_registered ? 12 : 8;
}

bool nokia_radio_peer_device::serving_pch_report() const
{
	return m_phase == phase::serving_bcch && m_registered &&
			(m_reports_remaining % 3) == 2;
}

u32 nokia_radio_peer_device::paging_frame_number(u32 minimum_frame_number) const
{
	static constexpr u32 FRAME_NUMBER_MODULUS = 26 * 51 * 2048;
	const auto group = m_gsm_network->subscriber_paging_group(
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

void nokia_radio_peer_device::receive_packet(const nokia_dspif_device::packet &packet)
{
	if (packet.type == 0x1a && packet.length != 0)
	{
		m_search_mode = packet.payload[0];
		// SEARCH_LIST carries a 512-bit ARFCN set after its four-byte control
		// header. In the ROM-4 wire layout ARFCN 1 is bit 0 of byte 65.
		m_search_has_arfcn1 = packet.length > 65 && BIT(packet.payload[65], 0);
	}

	if (packet.type == 0x1a && m_phase == phase::inactive && m_reports_remaining == 0)
	{
		// Initial SEARCH_LIST attempts end empty.  The firmware responds by
		// narrowing its own channel bitmap and publishing the next request.
		m_phase = phase::initial_search;
		m_reports_remaining = 2;
	}
	else if (packet.type == 0x03 && m_phase == phase::initial_search && m_reports_remaining == 0)
	{
		m_phase = phase::post_deactivate_search;
		m_reports_remaining = 2;
	}
	else if (packet.type == 0x1a && m_phase == phase::initial_search && m_reports_remaining == 0)
	{
		// A SIM with cached EF_BCCH advances directly to the next bounded search
		// mode instead of deactivating the empty initial scan.  It has the same
		// recovered two-terminal completion contract as the preceding request.
		m_phase = phase::post_deactivate_search;
		m_reports_remaining = 2;
	}
	else if (packet.type == 0x1a && m_phase == phase::post_deactivate_search && m_reports_remaining == 0)
	{
		m_phase = phase::candidate_measurement;
		m_reports_remaining = 1;
	}
	else if (packet.type == 0x1a && m_phase == phase::candidate_measurement && m_reports_remaining == 0)
	{
		if (m_search_round >= 3)
		{
			m_phase = phase::candidate_sync;
			m_reports_remaining = 2;
			m_report_deferred = true;
		}
		else
			m_reports_remaining = 1;
	}
	else if (packet.type == 0x02 &&
			(m_phase == phase::candidate_sync || m_phase == phase::selected_search) &&
			m_reports_remaining == 0)
	{
		// SCH reception makes the ROM issue CHANNEL_CONFIGURE during both initial
		// acquisition and the later mode-0x40 selection pass. Complete the same
		// recovered channel-change transaction while its acceptance window is open.
		const bool selected_plmn_search =
				m_phase == phase::selected_search && m_search_mode == 0x50;
		m_phase = selected_plmn_search ? phase::selected_channel_change : phase::candidate_channel_change;
		m_reports_remaining = selected_plmn_search ? 1 : 2;
		m_report_deferred = true;
	}
	else if (packet.type == 0x1a && m_phase == phase::candidate_sync && m_reports_remaining == 0)
	{
		// If the MCU rejects the measured candidate it requests the next search
		// batch instead of issuing CHANNEL_CONFIGURE.  Close that finite scan
		// with the recovered empty-list terminal so MM can select its fallback.
		m_phase = phase::candidate_retry;
		m_reports_remaining = 2;
		m_report_deferred = true;
	}
	else if (packet.type == 0x0c && m_phase == phase::serving_bcch)
	{
		// IDLE_RA form 1 configures the serving-cell receiver. Form 0 carries a
		// CHANNEL REQUEST random-access octet at byte 2 after organic 0x07d1.
		// Complete the former normally; only the latter starts the network access
		// exchange.
		const bool channel_request = packet.length >= 3 &&
				packet.payload[1] == 0 && packet.payload[2] != 0;
		m_access_ra = channel_request ? packet.payload[2] : 0;
		m_access_frame = 0;
		m_phase = channel_request ? phase::random_access : phase::serving_idle_ra;
		m_reports_remaining = channel_request ? 3 : 1;
		m_report_deferred = true;
	}
	else if (packet.type == 0x02 && m_phase == phase::serving_bcch &&
			packet.length >= 9 && packet.payload[8] == 0x60)
	{
		// After serving-cell selection the ROM configures logical channel 0x12,
		// encoded as DSP receive channel 0x60.
		m_phase = phase::serving_channel_change;
		m_reports_remaining = 1;
		m_report_deferred = true;
	}
	else if (packet.type == 0x02 && m_phase == phase::selected_bcch &&
			packet.length >= 9 && packet.payload[8] == 0x60)
	{
		// The channel-change acceptance window closes before the selected search's
		// finite terminal. Suspend that search, acknowledge the requested change,
		// then resume its remaining measurement and terminal reports.
		m_selected_reports_remaining = m_reports_remaining;
		m_phase = phase::selected_bcch_channel_change;
		m_reports_remaining = 1;
		m_wait_ticks = 0;
		m_report_deferred = true;
	}
	else if (packet.type == 0x02 && m_phase == phase::random_access &&
			packet.length >= 9 && packet.payload[8] == 0x80)
	{
		// A matching Immediate Assignment makes RR configure the assigned SDCCH.
		// Complete the same recovered channel-change transaction used for the
		// serving receiver; the firmware owns the subsequent LAPDm establishment.
		m_phase = phase::assigned_channel_change;
		m_reports_remaining = 1;
		m_report_deferred = true;
	}
	else if (packet.type == 0x02 && m_phase == phase::release_deconfigure &&
			packet.length >= 16 && packet.payload[8] == 0x60 &&
			(packet.payload[15] == 0x0f ||
				(m_traffic_channel_active && packet.payload[15] == 0x08)))
	{
		// RR Channel Release makes the ROM issue the same CHANNEL_CONFIGURE
		// transaction used to establish channel 0x60. The recovered SDCCH
		// deconfiguration flag is 0x0f; the organic TCH/F path uses 0x08.
		// Confirm it at the DSP boundary; the firmware owns the return to idle.
		m_phase = phase::release_channel_change;
		m_reports_remaining = 1;
		m_report_deferred = true;
	}
	else if (packet.type == 0x02 &&
			(m_phase == phase::service_uplink_request ||
				m_phase == phase::service_uplink_wait ||
				m_phase == phase::service_uplink_acknowledgement) &&
			packet.length >= 9 && packet.payload[8] == 0xc1 &&
			m_gsm_session->begin_traffic_assignment())
	{
		// The organically accepted RR Assignment Command configures the new
		// TCH/F as Nokia DSP channel 0xc1. Confirm that exact transaction before
		// asking the ROM for signalling on the newly established main link.
		m_lapdm_link->begin_mobile_establishment(0);
		m_phase = phase::traffic_channel_change;
		m_reports_remaining = 1;
		m_wait_ticks = 0;
		m_report_deferred = true;
	}
	else if (packet.type == 0x1b &&
			m_phase == phase::traffic_lapdm_establish &&
			packet.length >= 5 && packet.payload[1] == 0xb0)
	{
		if (m_lapdm_link->receive_uplink(packet.payload.data() + 2,
					packet.length - 2) ==
				nokia_lapdm_link_device::uplink_result::establish_indication)
		{
			m_traffic_channel_active = true;
			m_phase = phase::traffic_contention_resolution;
			m_reports_remaining = 1;
			m_report_deferred = true;
		}
	}
	else if (packet.type == 0x03 &&
			(m_phase == phase::serving_bcch || m_phase == phase::selected_search))
	{
		// DEACTIVATE retires the old receiver.  If the MCU has already queued a
		// newer SEARCH_LIST after completing selection, begin that request only
		// after teardown. It also cancels a pending selected-search terminal; delivering
		// that report after reset makes task 4 discard it in the new controller state.
		if (m_search_requested)
		{
			m_search_requested = false;
			m_phase = phase::selected_search;
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
	else if (packet.type == 0x1a && m_phase == phase::serving_bcch)
	{
		// An explicit measurement request preempts the periodic serving-cell
		// stream. Delaying it until an eight-block BCCH batch drains leaves its
		// terminal queued behind the firmware's next search, which then consumes
		// the stale result as if it belonged to the newer transaction.
		m_search_requested = false;
		m_phase = phase::selected_search;
		m_reports_remaining = 2;
		m_wait_ticks = 0;
		m_report_deferred = true;
	}
	else if (packet.type == 0x1a && m_phase == phase::selected_search && m_reports_remaining == 0)
	{
		m_reports_remaining = 2;
		m_report_deferred = true;
	}
	else if (packet.type == 0x1a && m_phase == phase::candidate_retry && m_reports_remaining == 0)
	{
		m_phase = phase::candidate_measurement;
		m_reports_remaining = 1;
		m_report_deferred = true;
	}
	else if (packet.type == 0x1b && m_phase == phase::lapdm_establish &&
			packet.length >= 5 && packet.payload[1] == 0x80)
	{
		// SEND_BLOCK has a two-byte DSP channel header followed by LAPDm. A
		// SABM with information invokes contention resolution, whose UA echoes
		// that information field exactly.
		if (m_lapdm_link->receive_uplink(packet.payload.data() + 2,
					packet.length - 2) ==
				nokia_lapdm_link_device::uplink_result::establish_indication &&
				m_gsm_session->establish_layer3(
					m_lapdm_link->layer3_information().data(),
					m_lapdm_link->layer3_length()))
		{
			m_phase = phase::contention_resolution;
			m_reports_remaining = 1;
			m_report_deferred = true;
		}
	}
	else if (packet.type == 0x1b &&
			m_phase == phase::location_update_acknowledgement &&
			packet.length >= 5 && packet.payload[1] == 0x80)
	{
		if (m_lapdm_link->receive_uplink(packet.payload.data() + 2,
					packet.length - 2) ==
				nokia_lapdm_link_device::uplink_result::downlink_acknowledgement)
		{
			if (m_trace_enabled)
				LOGMASKED(LOG_RADIO,
						"dsp_hle: LAPDm Location Updating Accept acknowledged nr=%u t=%.6f\n",
						m_lapdm_link->pending_receive_sequence(),
						machine().time().as_double());
			if (m_gsm_session->downlink_acknowledged() ==
					nokia_gsm_session_device::downlink_kind::channel_release)
			{
				m_phase = phase::rr_channel_release;
				m_reports_remaining = 1;
				m_report_deferred = true;
			}
		}
	}
	else if (packet.type == 0x1b &&
			m_phase == phase::channel_release_acknowledgement &&
			packet.length >= 5 && packet.payload[1] == 0x80)
	{
		const auto result = m_lapdm_link->receive_uplink(
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
				m_phase = phase::release_deconfigure;
				m_reports_remaining = 0;
			}
		}
	}
	else if (packet.type == 0x1b &&
			(m_phase == phase::service_uplink_wait ||
				(m_traffic_channel_active &&
					m_phase >= phase::service_downlink &&
					m_phase <= phase::service_uplink_acknowledgement)) &&
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
		const auto result = m_lapdm_link->receive_uplink(
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
			if (m_trace_enabled)
				LOGMASKED(LOG_RADIO,
						"dsp_hle: GSM service uplink sapi=%u pd=%02x message=%02x length=%u t=%.6f\n",
						m_lapdm_link->layer3_sapi(),
						information[0] & 0x0f, information[1] & 0x3f,
						m_lapdm_link->layer3_length(),
						machine().time().as_double());
			m_gsm_session->receive_layer3(
					m_lapdm_link->layer3_sapi(),
					information.data(), m_lapdm_link->layer3_length());
			// A queued L3 response is itself an I frame and piggybacks N(R) for
			// this uplink. Link establishment is a SABM and cannot piggyback
			// N(R); terminal session results likewise carry no message. Both
			// therefore require a standalone LAPDm RR first.
			const auto pending_kind =
					m_gsm_session->pending_downlink_kind();
			const bool queued_information =
					pending_kind !=
							nokia_gsm_session_device::downlink_kind::none &&
					pending_kind !=
							nokia_gsm_session_device::downlink_kind::
									sapi3_establishment;
			m_phase = queued_information ?
					phase::service_downlink :
					phase::service_uplink_acknowledgement;
			m_reports_remaining = 1;
			m_report_deferred = true;
		}
		else if (result ==
				nokia_lapdm_link_device::uplink_result::establish_confirmation)
		{
			m_downlink_offset = 0;
			const auto action = m_gsm_session->downlink_acknowledged();
			m_phase = action != nokia_gsm_session_device::downlink_kind::none ?
					phase::service_downlink : phase::service_uplink_request;
			m_reports_remaining = 1;
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
				m_phase = phase::service_downlink;
				m_reports_remaining = 1;
				m_report_deferred = true;
				return;
			}
			const auto action = acknowledge_downlink();
			if (action == nokia_gsm_session_device::downlink_kind::release_complete)
			{
				if (m_trace_enabled)
					LOGMASKED(LOG_RADIO,
							"dsp_hle: LAPDm service Channel Release acknowledged nr=%u t=%.6f\n",
							m_lapdm_link->pending_receive_sequence(),
							machine().time().as_double());
				m_phase = phase::release_deconfigure;
				m_reports_remaining = 0;
				m_page_transmitted = false;
			}
			else if (action != nokia_gsm_session_device::downlink_kind::none)
			{
				m_phase = phase::service_downlink;
				m_reports_remaining = 1;
				m_report_deferred = true;
			}
			else
			{
				m_phase = phase::service_uplink_request;
				m_reports_remaining = 1;
				m_report_deferred = true;
			}
		}
		else if (result ==
				nokia_lapdm_link_device::uplink_result::release_indication)
		{
			m_phase = phase::traffic_release_acknowledgement;
			m_reports_remaining = 1;
			m_wait_ticks = 0;
			m_report_deferred = true;
		}
		else
		{
			// The firmware can return an idle UI/fill block while a service waits
			// for user input or an upper-layer response.
			m_phase = phase::service_uplink_request;
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
	const u8 report_type = next_report_type();
	if (m_phase == phase::lapdm_establish ||
			m_phase == phase::location_update_ack_request ||
			m_phase == phase::channel_release_uplink_request ||
			m_phase == phase::service_uplink_request)
		payload[0] = m_traffic_channel_active ? 0xb0 : 0x80;

	if (report_type == 0x8b)
	{
		// ALL_RSSI_RESULTS begins with a two-byte list header followed by forty
		// four-byte records: big-endian ARFCN, flags and signed RSSI. Only ARFCN
		// 1 exists. Two -109 dBm baselines establish the initial acquisition
		// history; subsequent values come from the deterministic laboratory-cell
		// signal model so background measurements do not remain bit-identical.
		payload[0] = 0x00;
		payload[1] = 0x10;
		for (unsigned result = 0; result < 40; ++result)
		{
			const bool serving_result = result == 0;
			payload[2 + result * 4] = serving_result ? 0x00 : 0xff;
			payload[3 + result * 4] = serving_result ? 0x01 : 0xff;
			payload[5 + result * 4] = serving_result ?
					(m_search_round < 2 ? u8(0x93) :
						u8(m_gsm_network->serving_rssi(m_search_round - 2))) : 0x81;
		}
	}

	if (report_type == 0x80)
	{
		const bool pch_report = serving_pch_report();
		const bool dedicated_downlink =
				(m_phase >= phase::contention_resolution &&
					m_phase <= phase::rr_channel_release) ||
				m_phase == phase::service_downlink ||
				m_phase == phase::service_uplink_acknowledgement ||
				m_phase == phase::traffic_contention_resolution ||
				m_phase == phase::traffic_release_acknowledgement;
		// RECEIVED_BLOCK carries channel, BSIC, error, frame number, ARFCN,
		// shift and then a 24-byte GSM L2 block.  Channel 0x40 is SCH; 0x50 is
		// BCCH; 0x60 is the shared paging/access-grant common control channel;
		// 0x80 is SDCCH and 0xb0 is the active TCH/F FACCH selector.
		payload[0] = pch_report ? 0x60 :
				dedicated_downlink ?
					(m_traffic_channel_active ? 0xb0 : 0x80) :
				m_phase == phase::random_access ? 0x60 :
				((m_phase < phase::candidate_channel_change ||
					m_phase == phase::selected_search) ? 0x40 : 0x50);
		payload[1] = 0x12; // BSIC of the laboratory cell, for SCH and BCCH.
		u32 frame_number = m_phase == phase::random_access ? m_access_frame :
				(machine().time().as_ticks(13'000) / 60) % 2'715'648;
		if (pch_report)
			frame_number = paging_frame_number(frame_number);
		payload[3] = frame_number >> 16;
		payload[4] = frame_number >> 8;
		payload[5] = frame_number;
		payload[6] = 0x00;
		payload[7] = 0x01; // ARFCN 1, matching the selected RSSI candidate.

		if (m_phase == phase::contention_resolution)
		{
			const auto frame = m_lapdm_link->build_ua();
			std::copy(frame.begin(), frame.end(), std::begin(payload) + 10);
		}
		else if (m_phase == phase::traffic_contention_resolution)
		{
			const auto frame = m_lapdm_link->build_ua();
			std::copy(frame.begin(), frame.end(), std::begin(payload) + 10);
		}
		else if (m_phase == phase::traffic_release_acknowledgement)
		{
			const auto frame = m_lapdm_link->build_release_ua();
			std::copy(frame.begin(), frame.end(), std::begin(payload) + 10);
		}
		else if (m_phase == phase::location_update_accept)
		{
			const auto &message = m_gsm_session->pending_downlink();
			const auto frame = m_lapdm_link->build_information_frame(
					0, message.data.data(), message.length);
			std::copy(frame.begin(), frame.end(), std::begin(payload) + 10);
		}
		else if (m_phase == phase::rr_channel_release)
		{
			const auto &message = m_gsm_session->pending_downlink();
			const auto frame = m_lapdm_link->build_information_frame(
					0, message.data.data(), message.length);
			std::copy(frame.begin(), frame.end(), std::begin(payload) + 10);
		}
		else if (m_phase == phase::service_downlink)
		{
			const auto &message = m_gsm_session->pending_downlink();
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
			std::copy(frame.begin(), frame.end(), std::begin(payload) + 10);
		}
		else if (m_phase == phase::service_uplink_acknowledgement)
		{
			const auto frame = m_lapdm_link->build_receive_ready(
					m_lapdm_link->layer3_sapi());
			std::copy(frame.begin(), frame.end(), std::begin(payload) + 10);
		}
		else if (pch_report)
		{
			const auto service = m_incoming_smart_message_after_registration ?
					nokia_gsm_session_device::incoming_service::smart_message :
					m_incoming_sms_after_registration ?
					nokia_gsm_session_device::incoming_service::sms :
					m_incoming_call_after_registration ?
					nokia_gsm_session_device::incoming_service::call :
					nokia_gsm_session_device::incoming_service::none;
			const bool transmit_page =
					m_page_after_registration && m_pch_fill_delivered &&
					!m_page_transmitted &&
					m_gsm_session->queue_incoming_page(service);
			const auto block = transmit_page ?
					m_gsm_session->paging_request() :
					m_gsm_network->paging_fill();
			std::copy(block.begin(), block.end(), std::begin(payload) + 10);
			transmitted_page = transmit_page;
		}
		else if (payload[0] == 0x60)
		{
			const auto assignment = m_gsm_network->immediate_assignment(
					m_access_ra, frame_number);
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
			const auto &system_information = m_gsm_network->system_information(SI_BY_TC[tc]);
			std::copy(system_information.begin(), system_information.end(), std::begin(payload) + 10);
		}
		else
			std::copy(m_gsm_network->system_information(2).begin(),
					m_gsm_network->system_information(2).end(), std::begin(payload) + 10);
	}

	if (report_type == 0x83)
	{
		// RSSI_RESULTS is the serving-channel scalar report, distinct from the
		// SEARCH_LIST result array in ALL_RSSI_RESULTS.  The ROM reads its signed
		// measurement from payload byte 2 while controller state 3 is active.
		payload[2] = u8(m_gsm_network->serving_rssi(m_search_round));
	}

	if (report_type == 0x84 && m_phase == phase::random_access)
	{
		// RA_INFO is the DSP's report of the transmitted random-access burst.
		// Task 10 converts the absolute transmit frame into the GSM request
		// reference tuple which task 16 later matches against Immediate Assignment.
		m_access_frame = (machine().time().as_ticks(13'000) / 60) % 2'715'648;
		payload[0] = m_access_ra;
		payload[1] = m_access_frame >> 16;
		payload[2] = m_access_frame >> 8;
		payload[3] = m_access_frame;
	}

	const unsigned payload_length = report_type == 0x8b ? 166 : report_type == 0x80 ? 34 : 8;
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

	if (report_type == 0x80 && serving_pch_report())
	{
		const u32 frame_number =
				(payload[3] << 16) | (payload[4] << 8) | payload[5];
		if (transmitted_page)
		{
			m_page_transmitted = true;
			if (m_trace_enabled)
				LOGMASKED(LOG_RADIO,
						"dsp_hle: PCH IMSI page transmitted channel=60 fn=%u t=%.6f\n",
						frame_number, machine().time().as_double());
		}
		else if (!m_pch_fill_delivered)
		{
			m_pch_fill_delivered = true;
			if (m_trace_enabled)
				LOGMASKED(LOG_RADIO,
						"dsp_hle: PCH no-identity fill channel=60 fn=%u t=%.6f\n",
						frame_number, machine().time().as_double());
		}
	}

	if (report_type == 0x8b && m_phase >= phase::candidate_measurement)
		++m_search_round;
	++m_reports_sent;
	--m_reports_remaining;
	advance_after_report(report_type);
	m_transport->notify_rx();
	if (m_trace_enabled)
		LOGMASKED(LOG_RADIO, "dsp_hle: radio peer RX type=%02x sequence=%u t=%.6f\n",
				report_type, m_reports_sent, machine().time().as_double());
}

void nokia_radio_peer_device::advance_after_report(u8 report_type)
{
	if (m_phase == phase::candidate_channel_change && report_type == 0x89)
	{
		m_phase = phase::candidate_ra_info;
		m_wait_ticks = 100;
	}
	else if (m_phase == phase::candidate_ra_info && report_type == 0x84)
	{
		m_phase = phase::serving_bcch;
		m_reports_remaining = serving_cycle_reports();
		m_wait_ticks = 59;
	}
	else if (m_phase == phase::serving_bcch && report_type == 0x80)
	{
		// Space serving-cell samples at a 51-frame-multiframe cadence. The
		// corresponding RSSI result follows before the next BCCH block.
		m_wait_ticks = 59;
	}
	else if (m_phase == phase::serving_bcch && report_type == 0x83 &&
			m_reports_remaining == 0)
	{
		// A camped GSM cell broadcasts System Information continuously.
		m_reports_remaining = serving_cycle_reports();
	}
	else if (m_phase == phase::selected_search && (report_type == 0x87 || report_type == 0x8f))
	{
		m_phase = phase::serving_bcch;
		m_reports_remaining = serving_cycle_reports();
		m_wait_ticks = 59;
	}
	else if (m_phase == phase::serving_idle_ra && report_type == 0x8c)
	{
		m_phase = phase::serving_bcch;
		m_reports_remaining = serving_cycle_reports();
		m_wait_ticks = 59;
	}
	else if (m_phase == phase::serving_channel_change && report_type == 0x89)
	{
		m_phase = phase::serving_bcch;
		m_reports_remaining = serving_cycle_reports();
		m_wait_ticks = 59;
	}
	else if (m_phase == phase::selected_channel_change && report_type == 0x89)
	{
		m_phase = phase::selected_ra_info;
		m_wait_ticks = 100;
	}
	else if (m_phase == phase::selected_ra_info && report_type == 0x84)
	{
		m_phase = phase::selected_bcch;
		// Validate the selected cell across one complete eight-multiframe BCCH
		// schedule. Each block is followed by its serving-channel RSSI result.
		// A usable cell does not also produce NO_BCCH_LEFT: that contradictory
		// terminal can be consumed after the firmware starts its next search and
		// incorrectly fail the newer transaction.
		m_reports_remaining = 16;
		m_wait_ticks = 59;
	}
	else if (m_phase == phase::selected_bcch && report_type == 0x80)
	{
		m_wait_ticks = 59;
	}
	else if (m_phase == phase::selected_bcch && report_type == 0x83)
	{
		// The measurement belongs to the preceding received block; the next BCCH
		// block remains paced by the multiframe delay set on that block.
		if (m_reports_remaining == 0)
		{
			m_phase = phase::serving_bcch;
			m_reports_remaining = serving_cycle_reports();
			m_wait_ticks = 59;
		}
	}
	else if (m_phase == phase::selected_bcch && report_type == 0x87)
	{
		m_phase = phase::serving_bcch;
		m_reports_remaining = serving_cycle_reports();
		m_wait_ticks = 59;
	}
	else if (m_phase == phase::selected_bcch_channel_change && report_type == 0x89)
	{
		// The accepted logical-channel change retires the selected-cell scan.
		// Firmware immediately issues its next SEARCH_LIST after consuming the RR
		// completion; replaying the pre-change terminal makes that newer request
		// lose ownership and restarts selection indefinitely.
		m_phase = phase::serving_bcch;
		m_reports_remaining = serving_cycle_reports();
		m_selected_reports_remaining = 0;
		m_wait_ticks = 59;
	}
	else if (m_phase == phase::random_access && report_type == 0x80)
	{
		// Further progress is firmware-owned: a matching assignment configures
		// the dedicated channel and causes the MCU to transmit LAPDm SABM.
		m_reports_remaining = 0;
	}
	else if (m_phase == phase::assigned_channel_change && report_type == 0x89)
	{
		m_phase = phase::lapdm_establish;
		m_reports_remaining = 1;
		m_report_deferred = true;
	}
	else if (m_phase == phase::lapdm_establish && report_type == 0x86)
	{
		m_reports_remaining = 0;
	}
	else if (m_phase == phase::contention_resolution && report_type == 0x80)
	{
		const auto action = m_gsm_session->contention_resolution_delivered();
		if (action == nokia_gsm_session_device::downlink_kind::location_update_accept)
		{
			m_phase = phase::location_update_accept;
			m_reports_remaining = 1;
			m_report_deferred = true;
		}
		else if (action == nokia_gsm_session_device::downlink_kind::channel_release)
		{
			m_phase = phase::rr_channel_release;
			m_reports_remaining = 1;
			m_report_deferred = true;
		}
		else if (action ==
				nokia_gsm_session_device::downlink_kind::cipher_mode_command ||
				action == nokia_gsm_session_device::downlink_kind::mm_information)
		{
			m_phase = phase::service_downlink;
			m_reports_remaining = 1;
			m_report_deferred = true;
		}
	}
	else if (m_phase == phase::location_update_accept && report_type == 0x80)
	{
		m_phase = phase::location_update_ack_request;
		m_reports_remaining = 1;
		m_report_deferred = true;
	}
	else if (m_phase == phase::location_update_ack_request && report_type == 0x86)
	{
		m_phase = phase::location_update_acknowledgement;
		m_reports_remaining = 0;
	}
	else if (m_phase == phase::rr_channel_release && report_type == 0x80)
	{
		m_phase = phase::channel_release_uplink_request;
		m_reports_remaining = 1;
		m_report_deferred = true;
	}
	else if (m_phase == phase::channel_release_uplink_request && report_type == 0x86)
	{
		m_phase = phase::channel_release_acknowledgement;
		m_reports_remaining = 0;
	}
	else if (m_phase == phase::release_channel_change && report_type == 0x89)
	{
		m_traffic_channel_active = false;
		clear_speech_queues();
		m_phase = phase::serving_bcch;
		m_reports_remaining = serving_cycle_reports();
		m_wait_ticks = 59;
	}
	else if (m_phase == phase::service_downlink && report_type == 0x80)
	{
		// LAPDm uses a one-frame transmit window here. Even when M=1, wait for
		// the handset's N(R) before emitting the next Layer-3 segment.
		m_phase = phase::service_uplink_request;
		m_reports_remaining = 1;
		m_report_deferred = true;
	}
	else if (m_phase == phase::service_uplink_request && report_type == 0x86)
	{
		m_phase = phase::service_uplink_wait;
		m_reports_remaining = 0;
	}
	else if (m_phase == phase::service_uplink_acknowledgement && report_type == 0x80)
	{
		if (m_gsm_session->idle())
		{
			if (m_trace_enabled)
				LOGMASKED(LOG_RADIO,
						"dsp_hle: LAPDm service Channel Release acknowledged nr=%u t=%.6f\n",
						m_lapdm_link->pending_receive_sequence(),
						machine().time().as_double());
			m_phase = phase::release_deconfigure;
			m_reports_remaining = 0;
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
			m_phase = m_gsm_session->pending_downlink_kind() !=
							nokia_gsm_session_device::downlink_kind::none &&
							!awaiting_sapi3 ?
					phase::service_downlink : phase::service_uplink_request;
			m_reports_remaining = 1;
			m_report_deferred = true;
		}
	}
	else if (m_phase == phase::traffic_channel_change && report_type == 0x89)
	{
		// The mobile initiates the new main link without a BLOCK_REQUEST.
		m_phase = phase::traffic_lapdm_establish;
		m_reports_remaining = 0;
	}
	else if (m_phase == phase::traffic_contention_resolution &&
			report_type == 0x80)
	{
		m_phase = phase::service_uplink_request;
		m_reports_remaining = 1;
		m_report_deferred = true;
	}
	else if (m_phase == phase::traffic_release_acknowledgement &&
			report_type == 0x80)
	{
		m_phase = phase::release_deconfigure;
		m_reports_remaining = 0;
	}
}


void nokia_radio_peer_device::tick()
{
	if (!m_enabled)
		return;

	if (m_phase == phase::candidate_sync && m_reports_remaining != 0 && m_wait_ticks != 0)
		--m_wait_ticks;
	if (m_phase == phase::serving_bcch && m_reports_remaining != 0 && m_wait_ticks != 0)
		--m_wait_ticks;
	if (m_phase == phase::selected_bcch && m_reports_remaining != 0 && m_wait_ticks != 0)
		--m_wait_ticks;
	if (m_phase == phase::service_uplink_request &&
			m_reports_remaining != 0 && m_wait_ticks != 0)
		--m_wait_ticks;
	if ((m_phase == phase::candidate_ra_info || m_phase == phase::selected_ra_info) &&
			m_reports_remaining == 0 && m_wait_ticks != 0 && --m_wait_ticks == 0)
		m_reports_remaining = 1;
	if (m_report_deferred)
		m_report_deferred = false;
	else if (m_reports_remaining != 0 &&
			!(m_phase == phase::candidate_sync && m_wait_ticks != 0) &&
			!(m_phase == phase::serving_bcch && m_wait_ticks != 0) &&
			!(m_phase == phase::selected_bcch && m_wait_ticks != 0) &&
			!(m_phase == phase::service_uplink_request && m_wait_ticks != 0))
		emit_report();
}

bool nokia_radio_peer_device::fast_completion_pending() const
{
	return m_enabled && m_phase == phase::candidate_sync && m_reports_remaining == 1;
}
