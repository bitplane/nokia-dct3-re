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
	m_radio_peer(*this, "^radio_peer")
{
}

void nokia_dsp_hle_device::device_start()
{
	m_trace_enabled = machine().options().verbose();
	m_service_timer = timer_alloc(FUNC(nokia_dsp_hle_device::service_tick), this);
	m_packet_timer = timer_alloc(FUNC(nokia_dsp_hle_device::packet_tick), this);
	m_response_timer = timer_alloc(FUNC(nokia_dsp_hle_device::response_tick), this);
	m_keepalive_timer = timer_alloc(FUNC(nokia_dsp_hle_device::keepalive_tick), this);
	save_item(NAME(m_service_enabled));
	save_item(NAME(m_external_service_enabled));
	save_item(NAME(m_service_delay_us));
	save_item(NAME(m_peer_poll_ms));
	save_item(NAME(m_service_control_completion_sent));
	save_item(NAME(m_bootstrap_exchange_limit));
	save_item(NAME(m_bootstrap_exchange_count));
	save_item(NAME(m_bootstrap_ping_pong));
	save_item(NAME(m_code_block_request));
	save_item(NAME(m_parked_boot_status));
	save_item(NAME(m_boot_status_response));
}

void nokia_dsp_hle_device::device_reset()
{
	m_service_timer->adjust(attotime::never);
	m_packet_timer->adjust(attotime::never);
	m_response_timer->adjust(attotime::never);
	m_keepalive_timer->adjust(m_service_enabled ? attotime::from_seconds(1) : attotime::never,
			0, attotime::from_seconds(1));
	m_service_control_completion_sent = false;
	m_bootstrap_exchange_count = 0;
	publish_bootstrap_state();
}

void nokia_dsp_hle_device::publish_bootstrap_state()
{
	// Transition timing is not recovered. Publish the minimum reset-visible DSP
	// state synchronously, through DSPIF-owned shared RAM, before the MCU starts.
	m_transport->peer_shared_w(0x0e0 / 2, 0);
	if (!m_bootstrap_ping_pong)
	{
		m_transport->peer_shared_w(0x0fe / 2, 1);
		m_transport->peer_shared_w(0x100 / 2, 1);
	}
}

void nokia_dsp_hle_device::tx_commit_w(int state)
{
	if (state && (m_external_service_enabled || m_radio_peer->enabled()))
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
		m_transport->peer_shared_w(0x0e0 / 2, 0);
	if (state && m_trace_enabled)
		LOGMASKED(LOG_DSP_HLE, "dsp_hle: doorbell pending=%04x t=%.6f\n",
				m_transport->service_pending(), machine().time().as_double());
}

void nokia_dsp_hle_device::shared_002_write_w(int state)
{
	if (state && m_parked_boot_status && m_transport->shared_word(0x002 / 2) == 0xffff)
		m_transport->peer_shared_w(0x002 / 2, m_boot_status_response);
}

void nokia_dsp_hle_device::shared_0fe_read_w(int state)
{
	if (state && m_bootstrap_ping_pong && m_transport->shared_word(0x0fe / 2) != 0)
		m_transport->peer_shared_w(0x0fe / 2, 0);
}

void nokia_dsp_hle_device::shared_0fe_write_w(int state)
{
	if (!state)
		return;
	const u16 token = m_transport->shared_word(0x0fe / 2);
	if (m_bootstrap_ping_pong)
	{
		m_transport->peer_shared_w(0x0fe / 2, 0);
		m_transport->peer_shared_w(0x100 / 2, token != 0 ? token : 1);
	}
	else if (token == 0)
		m_transport->peer_shared_w(0x0fe / 2, 1);
}

void nokia_dsp_hle_device::shared_100_read_w(int state)
{
	if (state && m_bootstrap_ping_pong && m_transport->shared_word(0x100 / 2) != 0 &&
			m_transport->shared_word(0x1ca / 2) == 0)
		m_transport->peer_shared_w(0x100 / 2, 0);
}

void nokia_dsp_hle_device::shared_100_write_w(int state)
{
	if (!state)
		return;
	const u16 token = m_transport->shared_word(0x100 / 2);
	if (m_bootstrap_ping_pong && m_transport->shared_word(0x1ca / 2) == 0)
	{
		m_transport->peer_shared_w(0x100 / 2, 0);
		m_transport->peer_shared_w(0x0fe / 2, token != 0 ? token : 1);
	}
	else if (token == 0)
	{
		m_transport->peer_shared_w(0x100 / 2, 1);
		if (++m_bootstrap_exchange_count == m_bootstrap_exchange_limit)
		{
			for (offs_t offset = 0; offset <= (0x004 / 2); offset++)
				m_transport->peer_shared_w(offset, 1);
			if (m_trace_enabled)
				LOGMASKED(LOG_DSP_HLE, "dsp_hle: bootstrap ready exchanges=%u t=%.6f\n",
						m_bootstrap_exchange_count, machine().time().as_double());
		}
	}
}

TIMER_CALLBACK_MEMBER(nokia_dsp_hle_device::service_tick)
{
	if (m_code_block_request)
		m_transport->peer_shared_w(0x0e2 / 2, 1);
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
TIMER_CALLBACK_MEMBER(nokia_dsp_hle_device::packet_tick)
{
	if (m_external_service_enabled || m_radio_peer->enabled())
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
			if (m_radio_peer->enabled())
				m_radio_peer->receive_packet(packet);
			if (m_trace_enabled)
			{
				std::string payload_hex;
				for (unsigned index = 0; index < packet.length; ++index)
					payload_hex += util::string_format("%02x", packet.payload[index]);
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
