// license:BSD-3-Clause
#include "emu.h"
#include "nokia_dsp_peer.h"

#include <algorithm>

DEFINE_DEVICE_TYPE(NOKIA_DSP_PEER, nokia_dsp_peer_device, "nokia_dsp_peer", "Nokia DCT3 DSP mailbox peer")

namespace {
constexpr unsigned SVC_PENDING = 0x0e4 / 2;
constexpr unsigned TX_PRODUCER = 0x0a4 / 2;
constexpr unsigned TX_CONSUMER = 0x0a6 / 2;
constexpr unsigned TX_WORDS = 0x52;
constexpr unsigned RX_START = 0x100 / 2;
constexpr unsigned RX_END = 0x1c8 / 2;
constexpr unsigned RX_PRODUCER = 0x1c8 / 2;
constexpr unsigned RX_CONSUMER = 0x1ca / 2;
constexpr u8 DISCOVERY_NODE = 0x02;
constexpr unsigned SERVICE_START_DELAY_TICKS = 36;
}

nokia_dsp_peer_device::nokia_dsp_peer_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_DSP_PEER, tag, owner, clock),
	m_fiq0_cb(*this),
	m_service_irq_cb(*this)
{
}

void nokia_dsp_peer_device::device_start()
{
	m_service_timer = timer_alloc(FUNC(nokia_dsp_peer_device::service_tick), this);
	m_external_service_timer = timer_alloc(FUNC(nokia_dsp_peer_device::external_service_tick), this);
	save_item(NAME(m_ram));
	save_item(NAME(m_dspif));
	save_item(NAME(m_service_enabled));
	save_item(NAME(m_external_service_enabled));
	save_item(NAME(m_trace_enabled));
	save_item(NAME(m_service_delay_ms));
	save_item(NAME(m_service_tick_ms));
	save_item(NAME(m_discovery_complete));
	save_item(NAME(m_registration_sent));
	save_item(NAME(m_registration_acknowledged));
	save_item(NAME(m_channel_map_sent));
	save_item(NAME(m_channel_map_acknowledged));
	save_item(NAME(m_healthy_sent));
	save_item(NAME(m_empty_ack_sent));
	save_item(NAME(m_service_control_completion_sent));
	save_item(NAME(m_registration_ticks));
}

void nokia_dsp_peer_device::device_reset()
{
	std::fill(std::begin(m_ram), std::end(m_ram), 0);
	std::fill(std::begin(m_dspif), std::end(m_dspif), 0);
	// Power-on bootstrap-ready words. Once the firmware initializes the RX ring,
	// 0x100 becomes ordinary shared storage.
	m_ram[0x0fe / 2] = 1;
	m_ram[0x100 / 2] = 1;
	m_service_timer->adjust(attotime::never);
	m_external_service_timer->adjust(attotime::never);
	m_discovery_complete = false;
	m_registration_sent = false;
	m_registration_acknowledged = false;
	m_channel_map_sent = false;
	m_channel_map_acknowledged = false;
	m_healthy_sent = false;
	m_empty_ack_sent = false;
	m_service_control_completion_sent = false;
	m_registration_ticks = 0;
}

u16 nokia_dsp_peer_device::shared_r(offs_t offset)
{
	offset &= 0x7ff;
	const unsigned byte_offset = offset << 1;
	if (offset <= (0x004 / 2)) return 1;
	if (offset == (0x0e0 / 2)) return 0;
	if (offset == (0x0fe / 2)) return 1;
	if (offset == (0x100 / 2) && (m_ram[RX_PRODUCER] < RX_START || m_ram[RX_CONSUMER] < RX_START))
		return 1;
	const u16 value = m_ram[offset];
	if (m_trace_enabled && (byte_offset == 0x0a4 || byte_offset == 0x0a6 ||
			byte_offset == 0x1c8 || byte_offset == 0x1ca))
		logerror("dsp_boundary: RAM R off=%03x data=%04x t=%.6f\n", byte_offset, value, machine().time().as_double());
	return value;
}

void nokia_dsp_peer_device::shared_w(offs_t offset, u16 data, u16 mem_mask)
{
	offset &= 0x7ff;
	COMBINE_DATA(&m_ram[offset]);
	// Ring producer commits and shared-service pending counts are independent
	// transport work sources. Not every service-transport ring commit is followed by DSPIF
	// command 4, so retain their observed scheduling boundaries.
	if (m_external_service_enabled && offset == TX_PRODUCER)
		m_external_service_timer->adjust(attotime::from_usec(100));
	if (m_service_enabled && offset == SVC_PENDING && data != 0)
		m_service_timer->adjust(attotime::from_msec(m_service_delay_ms));
}

u8 nokia_dsp_peer_device::dspif_r(offs_t offset) const
{
	return m_dspif[offset & 3];
}

void nokia_dsp_peer_device::dspif_w(offs_t offset, u8 data)
{
	offset &= 3;
	m_dspif[offset] = data;
	if (offset != 1)
		return;

	const u16 command = (u16(m_dspif[0]) << 8) | m_dspif[1];
	if (m_trace_enabled)
		logerror("dsp_boundary: DSPIF command=%04x pending=%04x tx=%02x/%02x t=%.6f\n",
				command, m_ram[SVC_PENDING], m_ram[TX_CONSUMER], m_ram[TX_PRODUCER],
				machine().time().as_double());
}

void nokia_dsp_peer_device::pulse_fiq0()
{
	m_fiq0_cb(1);
	m_fiq0_cb(0);
}

bool nokia_dsp_peer_device::enqueue_rx_packet(u8 type, const u8 *payload, unsigned payload_length)
{
	const unsigned words = (payload_length + 3) / 2;
	unsigned producer = m_ram[RX_PRODUCER];
	const unsigned consumer = m_ram[RX_CONSUMER];
	if (producer < RX_START || producer >= RX_END || consumer < RX_START || consumer >= RX_END)
		return false;
	const unsigned free_words = consumer > producer ? consumer - producer - 1 :
			(RX_END - producer) + (consumer - RX_START) - 1;
	if (words == 0 || words > free_words)
		return false;
	auto put = [&](u16 value) {
		m_ram[producer] = value;
		if (++producer == RX_END) producer = RX_START;
	};
	put((payload_length << 8) | type);
	for (unsigned i = 0; i < payload_length; i += 2)
		put((u16(payload[i]) << 8) | (i + 1 < payload_length ? payload[i + 1] : 0));
	m_ram[RX_PRODUCER] = producer;
	return true;
}

u8 nokia_dsp_peer_device::tx_payload_byte(unsigned cursor, unsigned index) const
{
	const u16 packed = m_ram[(cursor + 1 + (index / 2)) % TX_WORDS];
	return BIT(index, 0) ? packed : packed >> 8;
}

bool nokia_dsp_peer_device::enqueue_transport_ack(unsigned cursor, unsigned payload_bytes)
{
	const u8 response[9] = {
		0x1e, tx_payload_byte(cursor, 2), tx_payload_byte(cursor, 1), 0x7f, 0x00, 0x02,
		tx_payload_byte(cursor, 3), u8(tx_payload_byte(cursor, payload_bytes - 1) & 0x07),
		tx_payload_byte(cursor, 8)
	};
	if (!enqueue_rx_packet(0x8e, response, std::size(response))) return false;
	if (m_trace_enabled)
		logerror("dsp_boundary: RX transport ACK class=%02x sequence=%02x command=%02x t=%.6f\n",
				response[6], response[7], response[8], machine().time().as_double());
	pulse_fiq0();
	return true;
}

bool nokia_dsp_peer_device::enqueue_service_frame(u8 command, u8 result, u8 sequence)
{
	const u8 frame[12] = {
		0x1e, 0x00, DISCOVERY_NODE, 0x40, 0x00, 0x06, 0x00, 0x01,
		command, result, 0x01, sequence
	};
	if (!enqueue_rx_packet(0x8e, frame, std::size(frame))) return false;
	if (m_trace_enabled)
		logerror("dsp_boundary: RX service command=%02x result=%02x sequence=%02x t=%.6f\n",
				command, result, sequence, machine().time().as_double());
	pulse_fiq0();
	return true;
}

TIMER_CALLBACK_MEMBER(nokia_dsp_peer_device::service_tick)
{
	m_ram[SVC_PENDING] = 0;
	m_service_irq_cb(1);
	m_service_irq_cb(0);
	m_service_timer->adjust(attotime::from_msec(m_service_tick_ms));
}

TIMER_CALLBACK_MEMBER(nokia_dsp_peer_device::external_service_tick)
{
	if (m_external_service_enabled)
	{
		unsigned cursor = m_ram[TX_CONSUMER] % TX_WORDS;
		const unsigned producer = m_ram[TX_PRODUCER] % TX_WORDS;
		while (cursor != producer)
		{
			const u16 header = m_ram[cursor];
			const unsigned payload_bytes = header >> 8;
			const unsigned words = (payload_bytes + 3) / 2;
			const unsigned available = producer >= cursor ? producer - cursor : TX_WORDS - cursor + producer;
			if (words == 0 || words > available) break;

			if ((header & 0xff) == 0x05 && payload_bytes >= 10)
			{
				const u8 frame_class = tx_payload_byte(cursor, 3);
				const u8 command = tx_payload_byte(cursor, 8);
				if (m_registration_sent && !m_registration_acknowledged && frame_class == 0x40 && command == 0x64)
					m_registration_acknowledged = true;
				else if (frame_class == 0x40 && command == 0x70 && tx_payload_byte(cursor, 9) != 0)
					m_channel_map_acknowledged = true;
				if (frame_class == 0x40) enqueue_transport_ack(cursor, payload_bytes);
				if (m_channel_map_acknowledged && payload_bytes <= 32 && frame_class == 0x00 &&
						command == 0x5f && tx_payload_byte(cursor, 9) == 0x00)
					enqueue_transport_ack(cursor, payload_bytes);
				if (m_healthy_sent && !m_empty_ack_sent && payload_bytes <= 32 && frame_class == 0x00 &&
						command == 0x62 && tx_payload_byte(cursor, 9) == 0x2a)
					m_empty_ack_sent = enqueue_transport_ack(cursor, payload_bytes);
			}

			if (!m_service_control_completion_sent && (header & 0xff) == 0x70 && payload_bytes == 2)
			{
				const u16 request = m_ram[(cursor + 1) % TX_WORDS];
				const u8 completion[2] = { 0x0d, 0x00 };
				if (request == 0x0d00 && enqueue_rx_packet(0x74, completion, std::size(completion)))
				{
					m_service_control_completion_sent = true;
					pulse_fiq0();
				}
			}

			if ((header & 0xff) == 0x05 && payload_bytes >= 9 && payload_bytes <= 32)
			{
				u8 response[32];
				for (unsigned i = 0; i < payload_bytes; i++) response[i] = tx_payload_byte(cursor, i);
				if (response[0] == 0x1e && response[3] == 0xd0 && response[6] == 0x01)
				{
					response[1] = response[2];
					response[2] = DISCOVERY_NODE;
					if (enqueue_rx_packet(0x8e, response, payload_bytes))
					{
						response[6] = 0x04;
						response[8] = (response[8] & ~0x27) | ((response[8] + 1) & 0x07);
						enqueue_rx_packet(0x8e, response, payload_bytes);
						m_discovery_complete = true;
						pulse_fiq0();
					}
				}
			}

			if (m_trace_enabled)
				logerror("dsp_boundary: TX packet type=%02x payload=%u words=%u ring=%02x t=%.6f\n",
						header & 0xff, payload_bytes, words, cursor, machine().time().as_double());
			cursor = (cursor + words) % TX_WORDS;
		}
		m_ram[TX_CONSUMER] = cursor;
	}

	if (m_external_service_enabled && m_discovery_complete && m_service_control_completion_sent && !m_registration_sent)
	{
		if (++m_registration_ticks >= SERVICE_START_DELAY_TICKS)
			m_registration_sent = enqueue_service_frame(0x64, 0x01, 0x42);
	}
	else if (m_external_service_enabled && m_registration_sent && !m_channel_map_sent)
	{
		u8 frame[75] = { 0 };
		frame[0] = 0x1e; frame[2] = DISCOVERY_NODE; frame[3] = 0x40;
		frame[5] = 0x45; frame[7] = 0x01; frame[8] = 0x70;
		frame[9 + (0x5f >> 3)] = 0x01;
		frame[9 + (0x62 >> 3)] = 0x20;
		frame[73] = 0x01; frame[74] = 0x43;
		if (enqueue_rx_packet(0x8e, frame, std::size(frame)))
		{
			m_channel_map_sent = true;
			pulse_fiq0();
		}
	}
	else if (m_external_service_enabled && m_channel_map_sent && !m_healthy_sent)
	{
		m_healthy_sent = enqueue_service_frame(0x64, 0x05, 0x44);
	}

	m_external_service_timer->adjust(attotime::from_msec(m_service_tick_ms));
}
