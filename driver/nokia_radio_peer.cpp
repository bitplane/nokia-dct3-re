// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz
#include "emu.h"
#include "nokia_radio_peer.h"

DEFINE_DEVICE_TYPE(NOKIA_RADIO_PEER, nokia_radio_peer_device,
		"nokia_radio_peer", "Nokia DCT3 radio peer HLE")

nokia_radio_peer_device::nokia_radio_peer_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_RADIO_PEER, tag, owner, clock),
	m_transport(*this, "^dspif"),
	m_gsm_network(*this, "^gsm_network")
{
}

void nokia_radio_peer_device::device_start()
{
	save_item(NAME(m_enabled));
	save_item(NAME(m_reports_sent));
	save_item(NAME(m_reports_remaining));
	save_item(NAME(m_phase));
	save_item(NAME(m_search_round));
	save_item(NAME(m_wait_ticks));
	save_item(NAME(m_search_mode));
	save_item(NAME(m_access_ra));
	save_item(NAME(m_access_frame));
	save_item(NAME(m_contention_l3));
	save_item(NAME(m_contention_length));
	save_item(NAME(m_search_has_arfcn1));
	save_item(NAME(m_report_deferred));
	save_item(NAME(m_search_requested));
	save_item(NAME(m_selected_reports_remaining));
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
	m_contention_l3.fill(0);
	m_contention_length = 0;
	m_search_has_arfcn1 = false;
	m_report_deferred = false;
	m_search_requested = false;
	m_selected_reports_remaining = 0;
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
		"contention_resolution", "location_update_accept", "rr_channel_release",
		"release_deconfigure", "release_channel_change"
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
		0xff, 0x89, 0x86, 0x80, 0x80, 0x80, 0x87, 0x89
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
			packet.payload[15] == 0x0f)
	{
		// RR Channel Release makes the ROM issue the same CHANNEL_CONFIGURE
		// transaction used to establish channel 0x60, now with the recovered
		// deconfiguration flags. Confirm it at the DSP boundary; the firmware
		// owns the resulting return to idle mode.
		m_phase = phase::release_channel_change;
		m_reports_remaining = 1;
		m_report_deferred = true;
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
			packet.length >= 7 && packet.payload[1] == 0x80 &&
			packet.payload[2] == 0x01 && packet.payload[3] == 0x3f)
	{
		// SEND_BLOCK has a two-byte DSP channel header followed by LAPDm. A
		// SABM with information invokes contention resolution, whose UA echoes
		// that information field exactly.
		const unsigned l3_length = packet.payload[4] >> 2;
		if (l3_length <= m_contention_l3.size() &&
				packet.length >= 5 + l3_length)
		{
			std::copy_n(packet.payload.begin() + 5, l3_length,
					m_contention_l3.begin());
			m_contention_length = l3_length;
			m_phase = phase::contention_resolution;
			m_reports_remaining = 1;
			m_report_deferred = true;
		}
	}
}

void nokia_radio_peer_device::emit_report()
{
	u8 payload[166] = { 0 };
	const u8 report_type = next_report_type();
	if (m_phase == phase::lapdm_establish)
		payload[0] = 0x80; // BLOCK_REQUEST subtype accepted in controller state 6.

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
		// RECEIVED_BLOCK carries channel, BSIC, error, frame number, ARFCN,
		// shift and then a 24-byte GSM L2 block.  Channel 0x40 is SCH; 0x50 is
		// BCCH after the firmware's CHANNEL_CONFIGURE request.
		payload[0] = (m_phase >= phase::contention_resolution &&
				m_phase <= phase::rr_channel_release) ? 0x80 :
				m_phase == phase::random_access ? 0x60 :
				((m_phase < phase::candidate_channel_change ||
					m_phase == phase::selected_search) ? 0x40 : 0x50);
		payload[1] = 0x12; // BSIC of the laboratory cell, for SCH and BCCH.
		const u32 frame_number = m_phase == phase::random_access ? m_access_frame :
				(machine().time().as_ticks(13'000) / 60) % 2'715'648;
		payload[3] = frame_number >> 16;
		payload[4] = frame_number >> 8;
		payload[5] = frame_number;
		payload[6] = 0x00;
		payload[7] = 0x01; // ARFCN 1, matching the selected RSSI candidate.

		if (m_phase == phase::contention_resolution)
		{
			auto *const block = std::begin(payload) + 10;
			std::fill_n(block, 24, 0x2b);
			block[0] = 0x01;
			block[1] = 0x73; // UA with final bit set
			block[2] = (m_contention_length << 2) | 1;
			std::copy_n(m_contention_l3.begin(), m_contention_length,
					block + 3);
		}
		else if (m_phase == phase::location_update_accept)
		{
			auto *const block = std::begin(payload) + 10;
			std::array<u8, 8> mobile_identity{};
			if (m_contention_length >= 18 && m_contention_l3[9] == 8)
				std::copy_n(m_contention_l3.begin() + 10, mobile_identity.size(),
						mobile_identity.begin());
			const auto accept = m_gsm_network->location_update_accept(mobile_identity);
			std::fill_n(block, 24, 0x2b);
			block[0] = 0x03; // network-to-mobile command, SAPI 0
			block[1] = 0x00; // I frame, N(S)=0, N(R)=0
			block[2] = (accept.size() << 2) | 1;
			std::copy(accept.begin(), accept.end(), block + 3);
		}
		else if (m_phase == phase::rr_channel_release)
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
	if (!m_transport->enqueue_rx_packet(report_type, payload, payload_length))
		return;

	if (report_type == 0x8b && m_phase >= phase::candidate_measurement)
		++m_search_round;
	++m_reports_sent;
	--m_reports_remaining;
	advance_after_report(report_type);
	m_transport->notify_rx();
	if (m_trace_enabled)
		logerror("dsp_hle: radio peer RX type=%02x sequence=%u t=%.6f\n",
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
		m_reports_remaining = 8;
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
		m_reports_remaining = 8;
	}
	else if (m_phase == phase::selected_search && (report_type == 0x87 || report_type == 0x8f))
	{
		m_phase = phase::serving_bcch;
		m_reports_remaining = 8;
		m_wait_ticks = 59;
	}
	else if (m_phase == phase::serving_idle_ra && report_type == 0x8c)
	{
		m_phase = phase::serving_bcch;
		m_reports_remaining = 8;
		m_wait_ticks = 59;
	}
	else if (m_phase == phase::serving_channel_change && report_type == 0x89)
	{
		m_phase = phase::serving_bcch;
		m_reports_remaining = 8;
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
			m_reports_remaining = 8;
			m_wait_ticks = 59;
		}
	}
	else if (m_phase == phase::selected_bcch && report_type == 0x87)
	{
		m_phase = phase::serving_bcch;
		m_reports_remaining = 8;
		m_wait_ticks = 59;
	}
	else if (m_phase == phase::selected_bcch_channel_change && report_type == 0x89)
	{
		// The accepted logical-channel change retires the selected-cell scan.
		// Firmware immediately issues its next SEARCH_LIST after consuming the RR
		// completion; replaying the pre-change terminal makes that newer request
		// lose ownership and restarts selection indefinitely.
		m_phase = phase::serving_bcch;
		m_reports_remaining = 8;
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
		m_phase = phase::location_update_accept;
		m_reports_remaining = 1;
		m_report_deferred = true;
	}
	else if (m_phase == phase::location_update_accept && report_type == 0x80)
	{
		m_phase = phase::rr_channel_release;
		m_reports_remaining = 1;
	}
	else if (m_phase == phase::rr_channel_release && report_type == 0x80)
	{
		// Firmware owns the LAPDm disconnect and physical-channel teardown which
		// follow the network's RR Channel Release.
		m_phase = phase::release_deconfigure;
		m_reports_remaining = 0;
	}
	else if (m_phase == phase::release_channel_change && report_type == 0x89)
	{
		m_phase = phase::serving_bcch;
		m_reports_remaining = 8;
		m_wait_ticks = 59;
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
	if ((m_phase == phase::candidate_ra_info || m_phase == phase::selected_ra_info) &&
			m_reports_remaining == 0 && m_wait_ticks != 0 && --m_wait_ticks == 0)
		m_reports_remaining = 1;
	if (m_report_deferred)
		m_report_deferred = false;
	else if (m_reports_remaining != 0 &&
			!(m_phase == phase::candidate_sync && m_wait_ticks != 0) &&
			!(m_phase == phase::serving_bcch && m_wait_ticks != 0) &&
			!(m_phase == phase::selected_bcch && m_wait_ticks != 0))
		emit_report();
}

bool nokia_radio_peer_device::fast_completion_pending() const
{
	return m_enabled && m_phase == phase::candidate_sync && m_reports_remaining == 1;
}
