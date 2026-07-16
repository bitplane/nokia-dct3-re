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
	save_item(NAME(m_service_enabled));
	save_item(NAME(m_external_service_enabled));
	save_item(NAME(m_service_delay_ms));
	save_item(NAME(m_service_tick_ms));
	save_item(NAME(m_service_control_completion_sent));
}

void nokia_dsp_hle_device::device_reset()
{
	m_service_timer->adjust(attotime::never);
	m_packet_timer->adjust(attotime::never);
	m_service_control_completion_sent = false;
}

void nokia_dsp_hle_device::tx_commit_w(int state)
{
	if (state && m_external_service_enabled)
		m_packet_timer->adjust(attotime::from_usec(100));
}

void nokia_dsp_hle_device::service_pending_w(int state)
{
	if (state && m_service_enabled)
		m_service_timer->adjust(attotime::from_msec(m_service_delay_ms));
}

void nokia_dsp_hle_device::doorbell_w(int state)
{
	if (state && m_trace_enabled)
		logerror("dsp_hle: doorbell pending=%04x t=%.6f\n",
				m_transport->service_pending(), machine().time().as_double());
}

u16 nokia_dsp_hle_device::bootstrap_r(offs_t offset, u16 backing)
{
	offset &= 0x7ff;
	if (offset <= (0x004 / 2))
		return 1;
	if (offset == (0x0e0 / 2))
		return 0;
	if (offset == (0x0fe / 2))
		return 1;
	if (offset == (0x100 / 2) &&
			(m_transport->shared_value(0x1c8 / 2) < (0x100 / 2) ||
			 m_transport->shared_value(0x1ca / 2) < (0x100 / 2)))
		return 1;
	return backing;
}

TIMER_CALLBACK_MEMBER(nokia_dsp_hle_device::service_tick)
{
	m_transport->complete_service();
	m_service_timer->adjust(attotime::from_msec(m_service_tick_ms));
}

void nokia_dsp_hle_device::drain_responses()
{
	nokia_external_service_peer_device::response response;
	while (m_external_peer->peek_response(response))
	{
		if (!m_transport->enqueue_rx_packet(response.type, response.payload.data(), response.length))
			break;
		m_external_peer->consume_response();
		if (response.notify)
			m_transport->notify_rx();
	}
}

TIMER_CALLBACK_MEMBER(nokia_dsp_hle_device::packet_tick)
{
	if (m_external_service_enabled)
	{
		nokia_dspif_device::packet packet;
		while (m_transport->peek_tx_packet(packet))
		{
			if (packet.type == 0x05 && packet.length >= 9 && packet.length <= 75)
				m_external_peer->receive_frame(packet.payload.data(), packet.length);
			if (!m_service_control_completion_sent && packet.type == 0x70 && packet.length == 2 &&
					packet.payload[0] == 0x0d && packet.payload[1] == 0x00)
			{
				const u8 completion[2] = { 0x0d, 0x00 };
				if (m_transport->enqueue_rx_packet(0x74, completion, std::size(completion)))
				{
					m_service_control_completion_sent = true;
					m_external_peer->set_service_control_complete();
					m_transport->notify_rx();
				}
			}
			if (m_trace_enabled)
				logerror("dsp_hle: TX packet type=%02x payload=%u words=%u t=%.6f\n",
						packet.type, packet.length, packet.words, machine().time().as_double());
			m_transport->consume_tx_packet(packet);
		}
		m_external_peer->tick();
		drain_responses();
	}
	m_packet_timer->adjust(attotime::from_msec(m_service_tick_ms));
}
