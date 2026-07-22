// license:BSD-3-Clause
#include "emu.h"
#include "nokia_dsp_hle.h"

DEFINE_DEVICE_TYPE(NOKIA_DSP_HLE, nokia_dsp_hle_device, "nokia_dsp_hle", "Nokia DCT3 DSP HLE")

nokia_dsp_hle_device::nokia_dsp_hle_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_DSP_HLE, tag, owner, clock),
	m_transport(*this, "^dspif"),
	m_external_peer(*this, "^external_service_peer"),
	m_gsm_network(*this, "^gsm_network")
{
}

void nokia_dsp_hle_device::device_start()
{
	m_service_timer = timer_alloc(FUNC(nokia_dsp_hle_device::service_tick), this);
	m_packet_timer = timer_alloc(FUNC(nokia_dsp_hle_device::packet_tick), this);
	m_response_timer = timer_alloc(FUNC(nokia_dsp_hle_device::response_tick), this);
	save_item(NAME(m_service_enabled));
	save_item(NAME(m_external_service_enabled));
	save_item(NAME(m_service_delay_ms));
	save_item(NAME(m_peer_poll_ms));
	save_item(NAME(m_radio_peer_enabled));
	save_item(NAME(m_service_control_completion_sent));
	save_item(NAME(m_radio_reports_sent));
	save_item(NAME(m_radio_reports_remaining));
	save_item(NAME(m_radio_phase));
	save_item(NAME(m_radio_search_round));
	save_item(NAME(m_radio_wait_ticks));
	save_item(NAME(m_radio_search_mode));
	save_item(NAME(m_radio_access_ra));
	save_item(NAME(m_radio_access_frame));
	save_item(NAME(m_radio_contention_l3));
	save_item(NAME(m_radio_contention_length));
	save_item(NAME(m_radio_search_has_arfcn1));
	save_item(NAME(m_radio_report_deferred));
	save_item(NAME(m_radio_search_requested));
	save_item(NAME(m_radio_selected_reports_remaining));
	save_item(NAME(m_bootstrap_exchange_count));
}

void nokia_dsp_hle_device::device_reset()
{
	m_service_timer->adjust(attotime::never);
	m_packet_timer->adjust(attotime::never);
	m_response_timer->adjust(attotime::never);
	m_service_control_completion_sent = false;
	m_radio_reports_sent = 0;
	m_radio_reports_remaining = 0;
	m_radio_phase = radio_phase::inactive;
	m_radio_search_round = 0;
	m_radio_wait_ticks = 0;
	m_radio_search_mode = 0;
	m_radio_access_ra = 0;
	m_radio_access_frame = 0;
	m_radio_contention_l3.fill(0);
	m_radio_contention_length = 0;
	m_radio_search_has_arfcn1 = false;
	m_radio_report_deferred = false;
	m_radio_search_requested = false;
	m_radio_selected_reports_remaining = 0;
	m_bootstrap_exchange_count = 0;
	publish_bootstrap_state();
}

void nokia_dsp_hle_device::publish_bootstrap_state()
{
	// Transition timing is not recovered. Publish the minimum reset-visible DSP
	// state synchronously, through DSPIF-owned shared RAM, before the MCU starts.
	m_transport->peer_shared_w(0x0e0 / 2, 0);
	m_transport->peer_shared_w(0x0fe / 2, 1);
	m_transport->peer_shared_w(0x100 / 2, 1);
}

void nokia_dsp_hle_device::tx_commit_w(int state)
{
	if (state && (m_external_service_enabled || m_radio_peer_enabled))
		m_packet_timer->adjust(attotime::from_usec(100));
}

void nokia_dsp_hle_device::service_pending_w(int state)
{
	if (state && m_service_enabled)
		m_service_timer->adjust(attotime::from_msec(m_service_delay_ms));
}

void nokia_dsp_hle_device::doorbell_w(int state)
{
	if (state && m_transport->dspif_r(0) == 0 && m_transport->dspif_r(1) == 4)
		m_transport->peer_shared_w(0x0e0 / 2, 0);
	if (state && m_trace_enabled)
		logerror("dsp_hle: doorbell pending=%04x t=%.6f\n",
				m_transport->service_pending(), machine().time().as_double());
}

void nokia_dsp_hle_device::bootstrap_fe_w(int state)
{
	if (state)
		m_transport->peer_shared_w(0x0fe / 2, 1);
}

void nokia_dsp_hle_device::bootstrap_100_w(int state)
{
	if (state)
	{
		m_transport->peer_shared_w(0x100 / 2, 1);
		if (++m_bootstrap_exchange_count == 64)
		{
			for (offs_t offset = 0; offset <= (0x004 / 2); offset++)
				m_transport->peer_shared_w(offset, 1);
		}
	}
}

TIMER_CALLBACK_MEMBER(nokia_dsp_hle_device::service_tick)
{
	m_transport->complete_service();
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
			logerror("dsp_hle: peer RX type=%02x length=%u t=%.6f\n",
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
			logerror("dsp_hle: peer RX scheduled length=%u delay=%.6f t=%.6f\n",
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

const char *nokia_dsp_hle_device::radio_phase_name(u8 phase)
{
	static constexpr const char *NAMES[] = {
		"inactive",
		"initial_search",
		"post_deactivate_search",
		"candidate_measurement",
		"candidate_sync",
		"candidate_channel_change",
		"candidate_ra_info",
		"serving_bcch",
		"serving_idle_ra",
		"candidate_retry",
		"selected_search",
		"serving_channel_change",
		"selected_channel_change",
		"selected_bcch",
		"selected_ra_info",
		"selected_bcch_channel_change",
		"random_access",
		"assigned_channel_change",
		"lapdm_establish",
		"contention_resolution",
		"location_update_accept",
		"rr_channel_release",
		"release_deconfigure",
		"release_channel_change"
	};
	return phase < std::size(NAMES) ? NAMES[phase] : "invalid";
}

void nokia_dsp_hle_device::observe_radio_request(const nokia_dspif_device::packet &packet)
{
	if (packet.type == 0x1a && packet.length != 0)
	{
		m_radio_search_mode = packet.payload[0];
		// SEARCH_LIST carries a 512-bit ARFCN set after its four-byte control
		// header. In the ROM-4 wire layout ARFCN 1 is bit 0 of byte 65.
		m_radio_search_has_arfcn1 = packet.length > 65 && BIT(packet.payload[65], 0);
	}

	if (packet.type == 0x1a && m_radio_phase == radio_phase::inactive && m_radio_reports_remaining == 0)
	{
		// Initial SEARCH_LIST attempts end empty.  The firmware responds by
		// narrowing its own channel bitmap and publishing the next request.
		m_radio_phase = radio_phase::initial_search;
		m_radio_reports_remaining = 2;
	}
	else if (packet.type == 0x03 && m_radio_phase == radio_phase::initial_search && m_radio_reports_remaining == 0)
	{
		m_radio_phase = radio_phase::post_deactivate_search;
		m_radio_reports_remaining = 2;
	}
	else if (packet.type == 0x1a && m_radio_phase == radio_phase::initial_search && m_radio_reports_remaining == 0)
	{
		// A SIM with cached EF_BCCH advances directly to the next bounded search
		// mode instead of deactivating the empty initial scan.  It has the same
		// recovered two-terminal completion contract as the preceding request.
		m_radio_phase = radio_phase::post_deactivate_search;
		m_radio_reports_remaining = 2;
	}
	else if (packet.type == 0x1a && m_radio_phase == radio_phase::post_deactivate_search && m_radio_reports_remaining == 0)
	{
		m_radio_phase = radio_phase::candidate_measurement;
		m_radio_reports_remaining = 1;
	}
	else if (packet.type == 0x1a && m_radio_phase == radio_phase::candidate_measurement && m_radio_reports_remaining == 0)
	{
		if (m_radio_search_round >= 3)
		{
			m_radio_phase = radio_phase::candidate_sync;
			m_radio_reports_remaining = 2;
			m_radio_report_deferred = true;
		}
		else
			m_radio_reports_remaining = 1;
	}
	else if (packet.type == 0x02 &&
			(m_radio_phase == radio_phase::candidate_sync || m_radio_phase == radio_phase::selected_search) &&
			m_radio_reports_remaining == 0)
	{
		// SCH reception makes the ROM issue CHANNEL_CONFIGURE during both initial
		// acquisition and the later mode-0x40 selection pass. Complete the same
		// recovered channel-change transaction while its acceptance window is open.
		const bool selected_plmn_search =
				m_radio_phase == radio_phase::selected_search && m_radio_search_mode == 0x50;
		m_radio_phase = selected_plmn_search ? radio_phase::selected_channel_change : radio_phase::candidate_channel_change;
		m_radio_reports_remaining = selected_plmn_search ? 1 : 2;
		m_radio_report_deferred = true;
	}
	else if (packet.type == 0x1a && m_radio_phase == radio_phase::candidate_sync && m_radio_reports_remaining == 0)
	{
		// If the MCU rejects the measured candidate it requests the next search
		// batch instead of issuing CHANNEL_CONFIGURE.  Close that finite scan
		// with the recovered empty-list terminal so MM can select its fallback.
		m_radio_phase = radio_phase::candidate_retry;
		m_radio_reports_remaining = 2;
		m_radio_report_deferred = true;
	}
	else if (packet.type == 0x0c && m_radio_phase == radio_phase::serving_bcch)
	{
		// IDLE_RA form 1 configures the serving-cell receiver. Form 0 carries a
		// CHANNEL REQUEST random-access octet at byte 2 after organic 0x07d1.
		// Complete the former normally; only the latter starts the network access
		// exchange.
		const bool channel_request = packet.length >= 3 &&
				packet.payload[1] == 0 && packet.payload[2] != 0;
		m_radio_access_ra = channel_request ? packet.payload[2] : 0;
		m_radio_access_frame = 0;
		m_radio_phase = channel_request ? radio_phase::random_access : radio_phase::serving_idle_ra;
		m_radio_reports_remaining = channel_request ? 3 : 1;
		m_radio_report_deferred = true;
	}
	else if (packet.type == 0x02 && m_radio_phase == radio_phase::serving_bcch &&
			packet.length >= 9 && packet.payload[8] == 0x60)
	{
		// After serving-cell selection the ROM configures logical channel 0x12,
		// encoded as DSP receive channel 0x60.
		m_radio_phase = radio_phase::serving_channel_change;
		m_radio_reports_remaining = 1;
		m_radio_report_deferred = true;
	}
	else if (packet.type == 0x02 && m_radio_phase == radio_phase::selected_bcch &&
			packet.length >= 9 && packet.payload[8] == 0x60)
	{
		// The channel-change acceptance window closes before the selected search's
		// finite terminal. Suspend that search, acknowledge the requested change,
		// then resume its remaining measurement and terminal reports.
		m_radio_selected_reports_remaining = m_radio_reports_remaining;
		m_radio_phase = radio_phase::selected_bcch_channel_change;
		m_radio_reports_remaining = 1;
		m_radio_wait_ticks = 0;
		m_radio_report_deferred = true;
	}
	else if (packet.type == 0x02 && m_radio_phase == radio_phase::random_access &&
			packet.length >= 9 && packet.payload[8] == 0x80)
	{
		// A matching Immediate Assignment makes RR configure the assigned SDCCH.
		// Complete the same recovered channel-change transaction used for the
		// serving receiver; the firmware owns the subsequent LAPDm establishment.
		m_radio_phase = radio_phase::assigned_channel_change;
		m_radio_reports_remaining = 1;
		m_radio_report_deferred = true;
	}
	else if (packet.type == 0x02 && m_radio_phase == radio_phase::release_deconfigure &&
			packet.length >= 16 && packet.payload[8] == 0x60 &&
			packet.payload[15] == 0x0f)
	{
		// RR Channel Release makes the ROM issue the same CHANNEL_CONFIGURE
		// transaction used to establish channel 0x60, now with the recovered
		// deconfiguration flags. Confirm it at the DSP boundary; the firmware
		// owns the resulting return to idle mode.
		m_radio_phase = radio_phase::release_channel_change;
		m_radio_reports_remaining = 1;
		m_radio_report_deferred = true;
	}
	else if (packet.type == 0x03 &&
			(m_radio_phase == radio_phase::serving_bcch || m_radio_phase == radio_phase::selected_search))
	{
		// DEACTIVATE retires the old receiver.  If the MCU has already queued a
		// newer SEARCH_LIST after completing selection, begin that request only
		// after teardown. It also cancels a pending selected-search terminal; delivering
		// that report after reset makes task 4 discard it in the new controller state.
		if (m_radio_search_requested)
		{
			m_radio_search_requested = false;
			m_radio_phase = radio_phase::selected_search;
			m_radio_reports_remaining = 2;
			m_radio_wait_ticks = 0;
			m_radio_report_deferred = true;
		}
		else
		{
			m_radio_reports_remaining = 0;
			m_radio_wait_ticks = 0;
			m_radio_report_deferred = false;
		}
	}
	else if (packet.type == 0x1a && m_radio_phase == radio_phase::serving_bcch)
	{
		// An explicit measurement request preempts the periodic serving-cell
		// stream. Delaying it until an eight-block BCCH batch drains leaves its
		// terminal queued behind the firmware's next search, which then consumes
		// the stale result as if it belonged to the newer transaction.
		m_radio_search_requested = false;
		m_radio_phase = radio_phase::selected_search;
		m_radio_reports_remaining = 2;
		m_radio_wait_ticks = 0;
		m_radio_report_deferred = true;
	}
	else if (packet.type == 0x1a && m_radio_phase == radio_phase::selected_search && m_radio_reports_remaining == 0)
	{
		m_radio_reports_remaining = 2;
		m_radio_report_deferred = true;
	}
	else if (packet.type == 0x1a && m_radio_phase == radio_phase::candidate_retry && m_radio_reports_remaining == 0)
	{
		m_radio_phase = radio_phase::candidate_measurement;
		m_radio_reports_remaining = 1;
		m_radio_report_deferred = true;
	}
	else if (packet.type == 0x1b && m_radio_phase == radio_phase::lapdm_establish &&
			packet.length >= 7 && packet.payload[1] == 0x80 &&
			packet.payload[2] == 0x01 && packet.payload[3] == 0x3f)
	{
		// SEND_BLOCK has a two-byte DSP channel header followed by LAPDm. A
		// SABM with information invokes contention resolution, whose UA echoes
		// that information field exactly.
		const unsigned l3_length = packet.payload[4] >> 2;
		if (l3_length <= m_radio_contention_l3.size() &&
				packet.length >= 5 + l3_length)
		{
			std::copy_n(packet.payload.begin() + 5, l3_length,
					m_radio_contention_l3.begin());
			m_radio_contention_length = l3_length;
			m_radio_phase = radio_phase::contention_resolution;
			m_radio_reports_remaining = 1;
			m_radio_report_deferred = true;
		}
	}
}

void nokia_dsp_hle_device::emit_radio_report()
{
	u8 payload[166] = { 0 };
	u8 report_type = 0x87; // NO_BCCH_LEFT

	switch (m_radio_phase)
	{
	case radio_phase::candidate_measurement:
		report_type = 0x8b; // ALL_RSSI_RESULTS
		break;
	case radio_phase::candidate_sync:
		report_type = m_radio_reports_remaining == 2 ? 0x8b : 0x80; // result, then SCH block
		break;
	case radio_phase::candidate_channel_change:
		report_type = m_radio_reports_remaining == 2 ? 0x8f : 0x89; // NO_PSW_LEFT, CHANNEL_CHANGED_CNF
		break;
	case radio_phase::candidate_ra_info:
		report_type = 0x84; // RA_INFO
		break;
	case radio_phase::serving_bcch:
		// A camped receiver reports the serving-channel level alongside each
		// decoded BCCH block. Task 11's post-selection measurement otherwise has
		// no completion input and falls through its long timeout.
		report_type = (m_radio_reports_remaining & 1) == 0 ? 0x80 : 0x83;
		break;
	case radio_phase::serving_idle_ra:
		report_type = 0x8c; // IDLE_RA completion
		break;
	case radio_phase::candidate_retry:
		report_type = m_radio_reports_remaining == 2 ? 0x8b : 0x87; // result, then search complete
		break;
	case radio_phase::selected_search:
		// Mode 0x40 performs the initial selected-cell measurement. Mode 0x50 is
		// requested after task 17 accepts the home PLMN. Both searches see the
		// same physical serving cell, so expose its synchronization block after
		// RSSI and let the firmware decide whether to configure it.
		report_type = m_radio_reports_remaining == 2 ? 0x8b :
				(((m_radio_search_mode == 0x00 && m_radio_search_has_arfcn1) ||
					m_radio_search_mode == 0x40 ||
					m_radio_search_mode == 0x50) ?
				0x80 : 0x87);
		break;
	case radio_phase::serving_channel_change:
		report_type = 0x89; // CHANNEL_CHANGED_CNF for post-selection channel 0x60
		break;
	case radio_phase::selected_channel_change:
		report_type = 0x89; // CHANNEL_CHANGED_CNF for selected-PLMN search
		break;
	case radio_phase::selected_bcch:
		// A serving-channel RSSI report follows each received BCCH block.  In
		// controller state 3 the ROM uses it to drain the single pending SI-change
		// slot before the next block can replace that operation.
		report_type = m_radio_reports_remaining == 1 ? 0x87 :
				((m_radio_reports_remaining & 1) == 0 ? 0x83 : 0x80);
		break;
	case radio_phase::selected_ra_info:
		report_type = 0x84; // RA_INFO for selected-PLMN search
		break;
	case radio_phase::selected_bcch_channel_change:
		report_type = 0x89; // requested channel change during selected-cell SI
		break;
	case radio_phase::random_access:
		report_type = m_radio_reports_remaining == 3 ? 0x8c :
				(m_radio_reports_remaining == 2 ? 0x84 : 0x80);
		break;
	case radio_phase::assigned_channel_change:
		report_type = 0x89; // CHANNEL_CHANGED_CNF for the assigned SDCCH
		break;
	case radio_phase::lapdm_establish:
		// The ROM retains a dedicated-channel LAPDm block until the DSP grants a
		// transmit slot. Nokia's recovered MDI vocabulary names type 0x86
		// BLOCK_REQUEST; the ROM decoder accepts subtype 0x80 in controller state
		// 6, while its built-in 0xb0/0xb1 descriptors belong the state-7 paths.
		report_type = 0x86;
		payload[0] = 0x80;
		break;
	case radio_phase::contention_resolution:
		report_type = 0x80; // RECEIVED_BLOCK carrying contention-resolution UA
		break;
	case radio_phase::location_update_accept:
		report_type = 0x80; // RECEIVED_BLOCK carrying Location Updating Accept
		break;
	case radio_phase::rr_channel_release:
		report_type = 0x80; // RECEIVED_BLOCK carrying RR Channel Release
		break;
	case radio_phase::release_channel_change:
		report_type = 0x89; // CHANNEL_CHANGED_CNF for dedicated-channel release
		break;
	default:
		break;
	}

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
					(m_radio_search_round < 2 ? u8(0x93) :
						u8(m_gsm_network->serving_rssi(m_radio_search_round - 2))) : 0x81;
		}
	}

	if (report_type == 0x80)
	{
		// RECEIVED_BLOCK carries channel, BSIC, error, frame number, ARFCN,
		// shift and then a 24-byte GSM L2 block.  Channel 0x40 is SCH; 0x50 is
		// BCCH after the firmware's CHANNEL_CONFIGURE request.
		payload[0] = (m_radio_phase >= radio_phase::contention_resolution &&
				m_radio_phase <= radio_phase::rr_channel_release) ? 0x80 :
				m_radio_phase == radio_phase::random_access ? 0x60 :
				((m_radio_phase < radio_phase::candidate_channel_change ||
					m_radio_phase == radio_phase::selected_search) ? 0x40 : 0x50);
		payload[1] = 0x12; // BSIC of the laboratory cell, for SCH and BCCH.
		const u32 frame_number = m_radio_phase == radio_phase::random_access ? m_radio_access_frame :
				(machine().time().as_ticks(13'000) / 60) % 2'715'648;
		payload[3] = frame_number >> 16;
		payload[4] = frame_number >> 8;
		payload[5] = frame_number;
		payload[6] = 0x00;
		payload[7] = 0x01; // ARFCN 1, matching the selected RSSI candidate.

		if (m_radio_phase == radio_phase::contention_resolution)
		{
			auto *const block = std::begin(payload) + 10;
			std::fill_n(block, 24, 0x2b);
			block[0] = 0x01;
			block[1] = 0x73; // UA with final bit set
			block[2] = (m_radio_contention_length << 2) | 1;
			std::copy_n(m_radio_contention_l3.begin(), m_radio_contention_length,
					block + 3);
		}
		else if (m_radio_phase == radio_phase::location_update_accept)
		{
			auto *const block = std::begin(payload) + 10;
			std::array<u8, 8> mobile_identity{};
			if (m_radio_contention_length >= 18 && m_radio_contention_l3[9] == 8)
				std::copy_n(m_radio_contention_l3.begin() + 10, mobile_identity.size(),
						mobile_identity.begin());
			const auto accept = m_gsm_network->location_update_accept(mobile_identity);
			std::fill_n(block, 24, 0x2b);
			block[0] = 0x03; // network-to-mobile command, SAPI 0
			block[1] = 0x00; // I frame, N(S)=0, N(R)=0
			block[2] = (accept.size() << 2) | 1;
			std::copy(accept.begin(), accept.end(), block + 3);
		}
		else if (m_radio_phase == radio_phase::rr_channel_release)
		{
			auto *const block = std::begin(payload) + 10;
			const auto release = m_gsm_network->channel_release();
			std::fill_n(block, 24, 0x2b);
			block[0] = 0x03; // network-to-mobile command, SAPI 0
			block[1] = 0x02; // I frame, N(S)=1, N(R)=0
			block[2] = (release.size() << 2) | 1;
			std::copy(release.begin(), release.end(), block + 3);
		}
		else if (payload[0] == 0x60)
		{
			const auto assignment = m_gsm_network->immediate_assignment(
					m_radio_access_ra, frame_number);
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
		payload[2] = u8(m_gsm_network->serving_rssi(m_radio_search_round));
	}

	if (report_type == 0x84 && m_radio_phase == radio_phase::random_access)
	{
		// RA_INFO is the DSP's report of the transmitted random-access burst.
		// Task 10 converts the absolute transmit frame into the GSM request
		// reference tuple which task 16 later matches against Immediate Assignment.
		m_radio_access_frame = (machine().time().as_ticks(13'000) / 60) % 2'715'648;
		payload[0] = m_radio_access_ra;
		payload[1] = m_radio_access_frame >> 16;
		payload[2] = m_radio_access_frame >> 8;
		payload[3] = m_radio_access_frame;
	}

	const unsigned payload_length = report_type == 0x8b ? 166 : report_type == 0x80 ? 34 : 8;
	if (!m_transport->enqueue_rx_packet(report_type, payload, payload_length))
		return;

	if (report_type == 0x8b && m_radio_phase >= radio_phase::candidate_measurement)
		++m_radio_search_round;
	++m_radio_reports_sent;
	--m_radio_reports_remaining;
	if (m_radio_phase == radio_phase::candidate_channel_change && report_type == 0x89)
	{
		m_radio_phase = radio_phase::candidate_ra_info;
		m_radio_wait_ticks = 100;
	}
	else if (m_radio_phase == radio_phase::candidate_ra_info && report_type == 0x84)
	{
		m_radio_phase = radio_phase::serving_bcch;
		m_radio_reports_remaining = 8;
		m_radio_wait_ticks = 59;
	}
	else if (m_radio_phase == radio_phase::serving_bcch && report_type == 0x80)
	{
		// Space serving-cell samples at a 51-frame-multiframe cadence. The
		// corresponding RSSI result follows before the next BCCH block.
		m_radio_wait_ticks = 59;
	}
	else if (m_radio_phase == radio_phase::serving_bcch && report_type == 0x83 &&
			m_radio_reports_remaining == 0)
	{
		// A camped GSM cell broadcasts System Information continuously.
		m_radio_reports_remaining = 8;
	}
	else if (m_radio_phase == radio_phase::selected_search && (report_type == 0x87 || report_type == 0x8f))
	{
		m_radio_phase = radio_phase::serving_bcch;
		m_radio_reports_remaining = 8;
		m_radio_wait_ticks = 59;
	}
	else if (m_radio_phase == radio_phase::serving_idle_ra && report_type == 0x8c)
	{
		m_radio_phase = radio_phase::serving_bcch;
		m_radio_reports_remaining = 8;
		m_radio_wait_ticks = 59;
	}
	else if (m_radio_phase == radio_phase::serving_channel_change && report_type == 0x89)
	{
		m_radio_phase = radio_phase::serving_bcch;
		m_radio_reports_remaining = 8;
		m_radio_wait_ticks = 59;
	}
	else if (m_radio_phase == radio_phase::selected_channel_change && report_type == 0x89)
	{
		m_radio_phase = radio_phase::selected_ra_info;
		m_radio_wait_ticks = 100;
	}
	else if (m_radio_phase == radio_phase::selected_ra_info && report_type == 0x84)
	{
		m_radio_phase = radio_phase::selected_bcch;
		// Validate the selected cell across one complete eight-multiframe BCCH
		// schedule. Each block is followed by its serving-channel RSSI result.
		// A usable cell does not also produce NO_BCCH_LEFT: that contradictory
		// terminal can be consumed after the firmware starts its next search and
		// incorrectly fail the newer transaction.
		m_radio_reports_remaining = 16;
		m_radio_wait_ticks = 59;
	}
	else if (m_radio_phase == radio_phase::selected_bcch && report_type == 0x80)
	{
		m_radio_wait_ticks = 59;
	}
	else if (m_radio_phase == radio_phase::selected_bcch && report_type == 0x83)
	{
		// The measurement belongs to the preceding received block; the next BCCH
		// block remains paced by the multiframe delay set on that block.
		if (m_radio_reports_remaining == 0)
		{
			m_radio_phase = radio_phase::serving_bcch;
			m_radio_reports_remaining = 8;
			m_radio_wait_ticks = 59;
		}
	}
	else if (m_radio_phase == radio_phase::selected_bcch && report_type == 0x87)
	{
		m_radio_phase = radio_phase::serving_bcch;
		m_radio_reports_remaining = 8;
		m_radio_wait_ticks = 59;
	}
	else if (m_radio_phase == radio_phase::selected_bcch_channel_change && report_type == 0x89)
	{
		// The accepted logical-channel change retires the selected-cell scan.
		// Firmware immediately issues its next SEARCH_LIST after consuming the RR
		// completion; replaying the pre-change terminal makes that newer request
		// lose ownership and restarts selection indefinitely.
		m_radio_phase = radio_phase::serving_bcch;
		m_radio_reports_remaining = 8;
		m_radio_selected_reports_remaining = 0;
		m_radio_wait_ticks = 59;
	}
	else if (m_radio_phase == radio_phase::random_access && report_type == 0x80)
	{
		// Further progress is firmware-owned: a matching assignment configures
		// the dedicated channel and causes the MCU to transmit LAPDm SABM.
		m_radio_reports_remaining = 0;
	}
	else if (m_radio_phase == radio_phase::assigned_channel_change && report_type == 0x89)
	{
		m_radio_phase = radio_phase::lapdm_establish;
		m_radio_reports_remaining = 1;
		m_radio_report_deferred = true;
	}
	else if (m_radio_phase == radio_phase::lapdm_establish && report_type == 0x86)
	{
		m_radio_reports_remaining = 0;
	}
	else if (m_radio_phase == radio_phase::contention_resolution && report_type == 0x80)
	{
		m_radio_phase = radio_phase::location_update_accept;
		m_radio_reports_remaining = 1;
		m_radio_report_deferred = true;
	}
	else if (m_radio_phase == radio_phase::location_update_accept && report_type == 0x80)
	{
		m_radio_phase = radio_phase::rr_channel_release;
		m_radio_reports_remaining = 1;
	}
	else if (m_radio_phase == radio_phase::rr_channel_release && report_type == 0x80)
	{
		// Firmware owns the LAPDm disconnect and physical-channel teardown which
		// follow the network's RR Channel Release.
		m_radio_phase = radio_phase::release_deconfigure;
		m_radio_reports_remaining = 0;
	}
	else if (m_radio_phase == radio_phase::release_channel_change && report_type == 0x89)
	{
		m_radio_phase = radio_phase::serving_bcch;
		m_radio_reports_remaining = 8;
		m_radio_wait_ticks = 59;
	}
	m_transport->notify_rx();
	if (m_trace_enabled)
		logerror("dsp_hle: radio peer RX type=%02x sequence=%u t=%.6f\n",
				report_type, m_radio_reports_sent, machine().time().as_double());
}

TIMER_CALLBACK_MEMBER(nokia_dsp_hle_device::packet_tick)
{
	if (m_external_service_enabled || m_radio_peer_enabled)
	{
		nokia_dspif_device::packet packet;
		while (m_transport->peek_tx_packet(packet))
		{
			if (m_external_service_enabled && packet.type == 0x05 &&
					packet.length >= 9 && packet.length <= 75)
				m_external_peer->receive_frame(packet.payload.data(), packet.length);
			if (m_external_service_enabled && !m_service_control_completion_sent &&
					packet.type == 0x70 && packet.length == 2 &&
					packet.payload[0] == 0x0d && packet.payload[1] == 0x00)
			{
				if (m_transport->enqueue_rx_packet(0x74, packet.payload.data(), packet.length))
				{
					m_service_control_completion_sent = true;
					m_external_peer->set_service_control_complete();
					m_transport->notify_rx();
				}
			}
			if (m_radio_peer_enabled)
				observe_radio_request(packet);
			if (m_trace_enabled)
			{
				std::string payload_hex;
				for (unsigned index = 0; index < packet.length; ++index)
					payload_hex += util::string_format("%02x", packet.payload[index]);
				logerror("dsp_hle: TX packet type=%02x payload=%u words=%u radio_phase=%s data=%s t=%.6f\n",
						packet.type, packet.length, packet.words, radio_phase_name(m_radio_phase), payload_hex,
						machine().time().as_double());
			}
			m_transport->consume_tx_packet(packet);
		}
		if (m_radio_peer_enabled && m_radio_phase == radio_phase::candidate_sync &&
				m_radio_reports_remaining != 0 && m_radio_wait_ticks != 0)
			--m_radio_wait_ticks;
		if (m_radio_peer_enabled && m_radio_phase == radio_phase::serving_bcch &&
				m_radio_reports_remaining != 0 && m_radio_wait_ticks != 0)
			--m_radio_wait_ticks;
		if (m_radio_peer_enabled && m_radio_phase == radio_phase::selected_bcch &&
				m_radio_reports_remaining != 0 && m_radio_wait_ticks != 0)
			--m_radio_wait_ticks;
		if (m_radio_peer_enabled &&
				(m_radio_phase == radio_phase::candidate_ra_info || m_radio_phase == radio_phase::selected_ra_info) &&
				m_radio_reports_remaining == 0 && m_radio_wait_ticks != 0 &&
				--m_radio_wait_ticks == 0)
			m_radio_reports_remaining = 1;
		if (m_radio_report_deferred)
			m_radio_report_deferred = false;
		else if (m_radio_reports_remaining != 0 &&
				!(m_radio_phase == radio_phase::candidate_sync && m_radio_wait_ticks != 0) &&
				!(m_radio_phase == radio_phase::serving_bcch && m_radio_wait_ticks != 0) &&
				!(m_radio_phase == radio_phase::selected_bcch && m_radio_wait_ticks != 0))
			emit_radio_report();
		if (m_external_service_enabled)
		{
			m_external_peer->tick();
			schedule_response();
		}
	}
	const bool fast_radio_completion = m_radio_peer_enabled &&
			m_radio_phase == radio_phase::candidate_sync && m_radio_reports_remaining == 1;
	m_packet_timer->adjust(fast_radio_completion ? attotime::from_usec(50) :
			attotime::from_msec(m_peer_poll_ms));
}
