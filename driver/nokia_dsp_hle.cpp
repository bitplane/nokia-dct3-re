// license:BSD-3-Clause
#include "emu.h"
#include "nokia_dsp_hle.h"

DEFINE_DEVICE_TYPE(NOKIA_DSP_HLE, nokia_dsp_hle_device, "nokia_dsp_hle", "Nokia DCT3 DSP HLE")

nokia_dsp_hle_device::nokia_dsp_hle_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_DSP_HLE, tag, owner, clock),
	m_transport(*this, "^dspif"),
	m_external_peer(*this, "^external_service_peer")
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
	save_item(NAME(m_radio_reports_pending));
	save_item(NAME(m_radio_sequence_stage));
	save_item(NAME(m_radio_search_round));
	save_item(NAME(m_radio_wait_ticks));
	save_item(NAME(m_radio_report_deferred));
	save_item(NAME(m_radio_search_requested));
	save_item(NAME(m_bootstrap_exchange_count));
}

void nokia_dsp_hle_device::device_reset()
{
	m_service_timer->adjust(attotime::never);
	m_packet_timer->adjust(attotime::never);
	m_response_timer->adjust(attotime::never);
	m_service_control_completion_sent = false;
	m_radio_reports_sent = 0;
	m_radio_reports_pending = 0;
	m_radio_sequence_stage = 0;
	m_radio_search_round = 0;
	m_radio_wait_ticks = 0;
	m_radio_report_deferred = false;
	m_radio_search_requested = false;
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

void nokia_dsp_hle_device::observe_radio_request(const nokia_dspif_device::packet &packet)
{
	if (packet.type == 0x1a && m_radio_sequence_stage == 0 && m_radio_reports_pending == 0)
	{
		// Initial SEARCH_LIST attempts end empty.  The firmware responds by
		// narrowing its own channel bitmap and publishing the next request.
		m_radio_sequence_stage = 1;
		m_radio_reports_pending = 2;
	}
	else if (packet.type == 0x03 && m_radio_sequence_stage == 1 && m_radio_reports_pending == 0)
	{
		m_radio_sequence_stage = 2;
		m_radio_reports_pending = 2;
	}
	else if (packet.type == 0x1a && m_radio_sequence_stage == 2 && m_radio_reports_pending == 0)
	{
		m_radio_sequence_stage = 3;
		m_radio_reports_pending = 1;
	}
	else if (packet.type == 0x1a && m_radio_sequence_stage == 3 && m_radio_reports_pending == 0)
	{
		if (m_radio_search_round >= 3)
		{
			m_radio_sequence_stage = 4;
			m_radio_reports_pending = 2;
			m_radio_report_deferred = true;
		}
		else
			m_radio_reports_pending = 1;
	}
	else if (packet.type == 0x02 && m_radio_sequence_stage == 4 && m_radio_reports_pending == 0)
	{
		// SCH reception makes the ROM issue CHANNEL_CONFIGURE.  Complete that
		// transaction while its channel-change acceptance window is open.
		m_radio_sequence_stage = 5;
		m_radio_reports_pending = 2;
		m_radio_report_deferred = true;
	}
	else if (packet.type == 0x1a && m_radio_sequence_stage == 4 && m_radio_reports_pending == 0)
	{
		// If the MCU rejects the measured candidate it requests the next search
		// batch instead of issuing CHANNEL_CONFIGURE.  Close that finite scan
		// with the recovered empty-list terminal so MM can select its fallback.
		m_radio_sequence_stage = 9;
		m_radio_reports_pending = 2;
		m_radio_report_deferred = true;
	}
	else if (packet.type == 0x0c && m_radio_sequence_stage == 7)
	{
		// RA_INFO makes the MCU issue IDLE_RA; type 0x8c is its recovered
		// completion family.  Only after it is accepted do BCCH blocks begin.
		m_radio_sequence_stage = 8;
		m_radio_reports_pending = 1;
		m_radio_report_deferred = true;
	}
	else if (packet.type == 0x1a && m_radio_sequence_stage == 7)
	{
		// Mobility Management can request another search while the current
		// serving cell's BCCH batch is still being delivered.  Preserve that
		// request and begin it once the batch has drained.
		m_radio_search_requested = true;
		if (m_trace_enabled)
			logerror("dsp_hle: queued repeated SEARCH_LIST during BCCH t=%.6f\n",
					machine().time().as_double());
	}
	else if (packet.type == 0x1a && m_radio_sequence_stage == 9 && m_radio_reports_pending == 0)
	{
		m_radio_sequence_stage = 3;
		m_radio_reports_pending = 1;
		m_radio_report_deferred = true;
	}
}

void nokia_dsp_hle_device::emit_radio_report()
{
	u8 payload[166] = { 0 };
	u8 report_type = 0x87; // NO_BCCH_LEFT

	switch (m_radio_sequence_stage)
	{
	case 3:
		report_type = 0x8b; // ALL_RSSI_RESULTS
		break;
	case 4:
		report_type = m_radio_reports_pending == 2 ? 0x8b : 0x80; // result, then SCH block
		break;
	case 5:
		report_type = m_radio_reports_pending == 2 ? 0x8f : 0x89; // NO_PSW_LEFT, CHANNEL_CHANGED_CNF
		break;
	case 6:
		report_type = 0x84; // RA_INFO
		break;
	case 7:
		report_type = 0x80; // BCCH RECEIVED_BLOCK
		break;
	case 8:
		report_type = 0x8c; // IDLE_RA completion
		break;
	case 9:
		report_type = m_radio_reports_pending == 2 ? 0x8b : 0x87; // result, then search complete
		break;
	default:
		break;
	}

	if (report_type == 0x8b)
	{
		// ALL_RSSI_RESULTS begins with a two-byte list header followed by forty
		// four-byte records: big-endian ARFCN, flags and signed RSSI.  Only ARFCN
		// 1 exists.  Two -109 dBm baselines followed by -60 dBm let the ROM set
		// candidate flags 0x0e and satisfy its own 0x212048 predicate.
		payload[1] = 0x10;
		for (unsigned result = 0; result < 40; ++result)
		{
			payload[2 + result * 4] = result == 0 ? 0x00 : 0xff;
			payload[3 + result * 4] = result == 0 ? 0x01 : 0xff;
			payload[5 + result * 4] = result == 0 ?
					(m_radio_search_round < 2 ? 0x93 : 0xc4) : 0x81;
		}
	}

	if (report_type == 0x80)
	{
		// RECEIVED_BLOCK carries channel, BSIC, error, frame number, ARFCN,
		// shift and then a 24-byte GSM L2 block.  Channel 0x40 is SCH; 0x50 is
		// BCCH after the firmware's CHANNEL_CONFIGURE request.
		payload[0] = m_radio_sequence_stage >= 5 ? 0x50 : 0x40;
		if (m_radio_sequence_stage >= 5)
			payload[1] = 0x12;

		static constexpr u8 si1[] = {
			0x55, 0x06, 0x19, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x2b
		};
		static constexpr u8 si2[] = {
			0x59, 0x06, 0x1a, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0xff, 0, 0, 0, 0
		};
		static constexpr u8 si3[] = {
			0x49, 0x06, 0x1b, 0x00, 0x01, 0x00, 0xf1, 0x10,
			0x00, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0x2b, 0x2b, 0x2b, 0x2b, 0x2b
		};
		static constexpr u8 si4[] = {
			0x31, 0x06, 0x1c, 0x00, 0xf1, 0x10, 0x00, 0x01,
			0, 0, 0, 0, 0, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
			0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b
		};

		if (m_radio_sequence_stage != 7 || m_radio_reports_pending == 2)
			std::copy(std::begin(si3), std::end(si3), std::begin(payload) + 10);
		else if (m_radio_reports_pending == 4)
			std::copy(std::begin(si1), std::end(si1), std::begin(payload) + 10);
		else if (m_radio_reports_pending == 3)
			std::copy(std::begin(si2), std::end(si2), std::begin(payload) + 10);
		else
			std::copy(std::begin(si4), std::end(si4), std::begin(payload) + 10);
	}

	const unsigned payload_length = report_type == 0x8b ? 166 : report_type == 0x80 ? 34 : 8;
	if (!m_transport->enqueue_rx_packet(report_type, payload, payload_length))
		return;

	if (report_type == 0x8b && m_radio_sequence_stage >= 3)
		++m_radio_search_round;
	++m_radio_reports_sent;
	--m_radio_reports_pending;
	if (m_radio_sequence_stage == 5 && report_type == 0x89)
	{
		m_radio_sequence_stage = 6;
		m_radio_wait_ticks = 100;
	}
	else if (m_radio_sequence_stage == 6 && report_type == 0x84)
	{
		m_radio_sequence_stage = 7;
		m_radio_reports_pending = 4;
		m_radio_wait_ticks = 59;
	}
	else if (m_radio_sequence_stage == 8 && report_type == 0x8c)
	{
		m_radio_sequence_stage = 7;
		m_radio_reports_pending = 4;
	}
	else if (m_radio_sequence_stage == 7 && report_type == 0x80 && m_radio_reports_pending != 0)
	{
		// Space SI blocks at a 51-frame-multiframe cadence rather than filling
		// the MDIRCV ring in a single firmware receive turn.
		m_radio_wait_ticks = 59;
	}
	else if (m_radio_sequence_stage == 7 && report_type == 0x80 && m_radio_search_requested)
	{
		m_radio_search_requested = false;
		m_radio_sequence_stage = 9;
		m_radio_reports_pending = 2;
		m_radio_report_deferred = true;
		if (m_trace_enabled)
			logerror("dsp_hle: starting queued repeated SEARCH_LIST t=%.6f\n",
					machine().time().as_double());
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
				logerror("dsp_hle: TX packet type=%02x payload=%u words=%u data=%s t=%.6f\n",
						packet.type, packet.length, packet.words, payload_hex,
						machine().time().as_double());
			}
			m_transport->consume_tx_packet(packet);
		}
		if (m_radio_peer_enabled && m_radio_sequence_stage == 4 &&
				m_radio_reports_pending != 0 && m_radio_wait_ticks != 0)
			--m_radio_wait_ticks;
		if (m_radio_peer_enabled && m_radio_sequence_stage == 7 &&
				m_radio_reports_pending != 0 && m_radio_wait_ticks != 0)
			--m_radio_wait_ticks;
		if (m_radio_peer_enabled && m_radio_sequence_stage == 6 &&
				m_radio_reports_pending == 0 && m_radio_wait_ticks != 0 &&
				--m_radio_wait_ticks == 0)
			m_radio_reports_pending = 1;
		if (m_radio_report_deferred)
			m_radio_report_deferred = false;
		else if (m_radio_reports_pending != 0 &&
				!(m_radio_sequence_stage == 4 && m_radio_wait_ticks != 0) &&
				!(m_radio_sequence_stage == 7 && m_radio_wait_ticks != 0))
			emit_radio_report();
		if (m_external_service_enabled)
		{
			m_external_peer->tick();
			schedule_response();
		}
	}
	const bool fast_radio_completion = m_radio_peer_enabled &&
			m_radio_sequence_stage == 4 && m_radio_reports_pending == 1;
	m_packet_timer->adjust(fast_radio_completion ? attotime::from_usec(50) :
			attotime::from_msec(m_peer_poll_ms));
}
