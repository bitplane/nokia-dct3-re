// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz
#include "emu.h"
#include "nokia_external_service.h"

#include <algorithm>

DEFINE_DEVICE_TYPE(NOKIA_EXTERNAL_SERVICE_PEER, nokia_external_service_peer_device,
		"nokia_external_service", "Nokia DCT3 external service peer")

nokia_external_service_peer_device::nokia_external_service_peer_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_EXTERNAL_SERVICE_PEER, tag, owner, clock)
{
}

void nokia_external_service_peer_device::device_start()
{
	save_item(NAME(m_enabled));
	save_item(NAME(m_discovery_complete));
	save_item(NAME(m_service_control_complete));
	save_item(NAME(m_registration_sent));
	save_item(NAME(m_registration_acknowledged));
	save_item(NAME(m_channel_map_sent));
	save_item(NAME(m_channel_map_acknowledged));
	save_item(NAME(m_empty_ack_sent));
	save_item(NAME(m_registration_ticks));
	save_item(NAME(m_queue_type));
	save_item(NAME(m_queue_length));
	save_item(NAME(m_queue_notify));
	save_item(NAME(m_queue_payload));
	save_item(NAME(m_queue_head));
	save_item(NAME(m_queue_tail));
}

void nokia_external_service_peer_device::device_reset()
{
	m_discovery_complete = false;
	m_service_control_complete = false;
	m_registration_sent = false;
	m_registration_acknowledged = false;
	m_channel_map_sent = false;
	m_channel_map_acknowledged = false;
	m_empty_ack_sent = false;
	m_registration_ticks = 0;
	m_queue_head = 0;
	m_queue_tail = 0;
}

bool nokia_external_service_peer_device::queue_response(
		const u8 *payload, unsigned length, bool notify)
{
	const u8 next = (m_queue_tail + 1) % QUEUE_SIZE;
	if (length == 0 || length > 75 || next == m_queue_head)
		return false;
	m_queue_type[m_queue_tail] = 0x8e;
	m_queue_length[m_queue_tail] = length;
	m_queue_notify[m_queue_tail] = notify;
	std::copy_n(payload, length, m_queue_payload[m_queue_tail]);
	m_queue_tail = next;
	return true;
}

bool nokia_external_service_peer_device::queue_service_frame(u8 command, u8 result, u8 sequence)
{
	const u8 frame[12] = {
		0x1e, 0x00, DISCOVERY_NODE, 0x40, 0x00, 0x06, 0x00, 0x01,
		command, result, 0x01, sequence
	};
	if (!queue_response(frame, std::size(frame)))
		return false;
	if (m_trace_enabled)
		logerror("external_service: response command=%02x result=%02x sequence=%02x t=%.6f\n",
				command, result, sequence, machine().time().as_double());
	return true;
}

bool nokia_external_service_peer_device::queue_transport_ack(const u8 *payload, unsigned length)
{
	const u8 ack[9] = {
		0x1e, payload[2], payload[1], 0x7f, 0x00, 0x02,
		payload[3], u8(payload[length - 1] & 0x07), payload[8]
	};
	return queue_response(ack, std::size(ack));
}

void nokia_external_service_peer_device::receive_frame(const u8 *payload, unsigned length)
{
	if (!m_enabled || length < 9 || length > 75 || payload[0] != 0x1e)
		return;

	const u8 frame_class = payload[3];
	const u8 command = payload[8];
	if (frame_class == 0x40 && length >= 10)
	{
		if (m_registration_sent && !m_registration_acknowledged && command == 0x64)
			m_registration_acknowledged = true;
		else if (command == 0x70 && payload[9] != 0)
			m_channel_map_acknowledged = true;
		queue_transport_ack(payload, length);
	}
	if (m_channel_map_acknowledged && length >= 10 && length <= 32 && frame_class == 0x00 &&
			command == 0x5f && payload[9] == 0x00)
	{
		queue_transport_ack(payload, length);
	}
	if (m_channel_map_acknowledged && !m_empty_ack_sent && length >= 10 && length <= 32 && frame_class == 0x00 &&
			command == 0x62 && payload[9] == 0x2a)
	{
		m_empty_ack_sent = queue_transport_ack(payload, length);
	}

	if (payload[3] == 0xd0 && payload[6] == 0x01)
	{
		u8 response[75];
		std::copy_n(payload, length, response);
		response[1] = response[2];
		response[2] = DISCOVERY_NODE;
		if (queue_response(response, length, false))
		{
			response[6] = 0x04;
			response[8] = (response[8] & ~0x27) | ((response[8] + 1) & 0x07);
			if (queue_response(response, length, true))
				m_discovery_complete = true;
		}
	}
}

void nokia_external_service_peer_device::set_service_control_complete()
{
	m_service_control_complete = true;
}

void nokia_external_service_peer_device::tick()
{
	if (!m_enabled)
		return;
	if (m_discovery_complete && m_service_control_complete && !m_registration_sent)
	{
		if (++m_registration_ticks >= SERVICE_START_DELAY_TICKS)
			m_registration_sent = queue_service_frame(0x64, 0x01, 0x42);
	}
	else if (m_registration_acknowledged && !m_channel_map_sent)
	{
		u8 frame[75] = { 0 };
		frame[0] = 0x1e; frame[2] = DISCOVERY_NODE; frame[3] = 0x40;
		frame[5] = 0x45; frame[7] = 0x01; frame[8] = 0x70;
		frame[9 + (0x5f >> 3)] = 0x01;
		frame[9 + (0x62 >> 3)] = 0x20;
		frame[73] = 0x01; frame[74] = 0x43;
		m_channel_map_sent = queue_response(frame, std::size(frame));
	}
}

bool nokia_external_service_peer_device::peek_response(response &result) const
{
	if (m_queue_head == m_queue_tail)
		return false;
	result.type = m_queue_type[m_queue_head];
	result.length = m_queue_length[m_queue_head];
	result.notify = m_queue_notify[m_queue_head];
	std::copy_n(m_queue_payload[m_queue_head], result.length, result.payload.begin());
	return true;
}

void nokia_external_service_peer_device::consume_response()
{
	if (m_queue_head != m_queue_tail)
		m_queue_head = (m_queue_head + 1) % QUEUE_SIZE;
}
