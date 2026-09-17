// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "emuopts.h"
#include "nokia_mbus.h"
#include "nokia_mbus_terminal.h"

#define LOG_TERMINAL (1U << 0)
#define VERBOSE (LOG_TERMINAL)
#include "logmacro.h"

#include <algorithm>

DEFINE_DEVICE_TYPE(NOKIA_MBUS_TERMINAL, nokia_mbus_terminal_device,
		"nokia_mbus_terminal", "Nokia M2BUS terminal")

namespace {
const attotime BYTE_TIME = attotime::from_hz(960); // 10 bits at 9,600 baud
const attotime ACK_IDLE = attotime::from_usec(2'500);
const attotime FRAME_IDLE = attotime::from_msec(3);
constexpr u8 PHONE_NODE = 0x00;
constexpr u8 TERMINAL_NODE = 0x1d;
}

nokia_mbus_terminal_device::nokia_mbus_terminal_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_MBUS_TERMINAL, tag, owner, clock),
	m_mbus(*this, "^mbus")
{
}

void nokia_mbus_terminal_device::device_start()
{
	m_trace = machine().options().verbose();
	m_tx_timer = timer_alloc(FUNC(nokia_mbus_terminal_device::transmit_byte), this);
	save_item(NAME(m_enabled));
	save_item(NAME(m_startup_sent));
	save_item(NAME(m_ack_queued));
	save_item(NAME(m_rx));
	save_item(NAME(m_rx_length));
	save_item(NAME(m_queue));
	save_item(NAME(m_queue_length));
	save_item(NAME(m_queue_idle));
	save_item(NAME(m_queue_head));
	save_item(NAME(m_queue_tail));
	save_item(NAME(m_tx_offset));
}

void nokia_mbus_terminal_device::device_reset()
{
	m_startup_sent = false;
	m_ack_queued = false;
	m_rx_length = 0;
	m_queue_head = 0;
	m_queue_tail = 0;
	m_tx_offset = 0;
	m_tx_timer->adjust(attotime::never);
}

u8 nokia_mbus_terminal_device::checksum(const u8 *data, unsigned length)
{
	u8 result = 0;
	for (unsigned i = 0; i < length; ++i)
		result ^= data[i];
	return result;
}

void nokia_mbus_terminal_device::receive_phone_byte(u8 data)
{
	if (!m_enabled)
		return;

	// M2BUS is single-wire. If the phone wins arbitration while this endpoint
	// is part-way through a frame, the partial frame is unusable. Restart it
	// only after the phone has left the line idle; each further phone byte moves
	// that deadline forward by one character.
	if (m_queue_head != m_queue_tail)
	{
		m_tx_offset = 0;
		m_tx_timer->adjust(m_queue_idle[m_queue_head]);
	}

	if (m_rx_length == 0 && data != 0x1f)
		return;
	if (m_rx_length >= MAX_FRAME)
		m_rx_length = 0;
	m_rx[m_rx_length++] = data;

	if (m_rx_length >= 6)
	{
		// NAM-2's checksum-valid wire frame stores the payload length high byte
		// first (00 01 for its one-byte D0 body).
		const unsigned expected = 8 + (unsigned(m_rx[4]) << 8) + m_rx[5];
		if (expected > MAX_FRAME)
			m_rx_length = 0;
		else if (m_rx_length == expected)
			accept_phone_frame();
	}
}

void nokia_mbus_terminal_device::accept_phone_frame()
{
	const unsigned length = m_rx_length;
	m_rx_length = 0;
	if (length < 6 || checksum(m_rx.data(), length) != 0 || m_rx[2] != PHONE_NODE)
		return;

	// An ACK reverses the acknowledged frame's endpoints. The initial phone
	// registration is broadcast to ff, while later replies target node 1d.
	u8 ack[] = { 0x1f, m_rx[2], m_rx[1], 0x7f,
		m_rx[length - 2], 0x00 };
	ack[std::size(ack) - 1] = checksum(ack, std::size(ack) - 1);
	if (!m_ack_queued)
	{
		// A repeated phone frame pre-empts a normal terminal frame. ACK traffic
		// has the shorter protocol idle and therefore wins arbitration.
		if (m_queue_head != m_queue_tail && m_queue[m_queue_head][3] != 0x7f)
		{
			std::copy_n(ack, std::size(ack), m_queue[m_queue_head].begin());
			m_queue_length[m_queue_head] = std::size(ack);
			m_queue_idle[m_queue_head] = ACK_IDLE;
			m_tx_offset = 0;
			m_startup_sent = false;
			m_ack_queued = true;
			m_tx_timer->adjust(ACK_IDLE);
		}
		else
			m_ack_queued = queue_frame(ack, std::size(ack), ACK_IDLE);
	}

	if (m_trace)
		LOGMASKED(LOG_TERMINAL, "mbus_terminal: phone_frame type=%02x command=%02x length=%u sequence=%02x startup=%u t=%.9f\n",
				m_rx[3], length > 6 ? m_rx[6] : 0xff, length,
				m_rx[length - 2], m_startup_sent,
				machine().time().as_double());
}

bool nokia_mbus_terminal_device::queue_frame(const u8 *data, unsigned length, attotime idle)
{
	const bool was_empty = m_queue_head == m_queue_tail;
	const u8 next = (m_queue_tail + 1) % QUEUE_SIZE;
	if (length == 0 || length > MAX_FRAME || next == m_queue_head)
		return false;
	std::copy_n(data, length, m_queue[m_queue_tail].begin());
	m_queue_length[m_queue_tail] = length;
	m_queue_idle[m_queue_tail] = idle;
	m_queue_tail = next;
	if (was_empty)
		start_next_frame();
	return true;
}

void nokia_mbus_terminal_device::start_next_frame()
{
	if (m_queue_head == m_queue_tail)
	{
		m_tx_timer->adjust(attotime::never);
		return;
	}
	m_tx_offset = 0;
	m_tx_timer->adjust(m_queue_idle[m_queue_head]);
}

TIMER_CALLBACK_MEMBER(nokia_mbus_terminal_device::transmit_byte)
{
	if (m_queue_head == m_queue_tail)
		return;

	// MAD2 has a one-byte receive holding register. Retrying after one physical
	// character preserves flow control without inventing a controller FIFO.
	if (!m_mbus->receive_byte(m_queue[m_queue_head][m_tx_offset]))
	{
		m_tx_timer->adjust(BYTE_TIME);
		return;
	}

	if (++m_tx_offset < m_queue_length[m_queue_head])
		m_tx_timer->adjust(BYTE_TIME);
	else
	{
		const bool ack_complete = m_queue[m_queue_head][3] == 0x7f;
		if (m_trace)
			LOGMASKED(LOG_TERMINAL, "mbus_terminal: tx_complete type=%02x length=%u t=%.9f\n",
					m_queue[m_queue_head][3], m_queue_length[m_queue_head],
					machine().time().as_double());
		m_queue_head = (m_queue_head + 1) % QUEUE_SIZE;
		if (ack_complete)
		{
			m_ack_queued = false;
			if (!m_startup_sent)
			{
				u8 startup[] = { 0x1f, PHONE_NODE, TERMINAL_NODE, 0xd0,
					0x00, 0x01, 0x04, 0x08, 0x00 };
				startup[std::size(startup) - 1] = checksum(startup, std::size(startup) - 1);
				m_startup_sent = queue_frame(startup, std::size(startup), FRAME_IDLE);
			}
		}
		start_next_frame();
	}
}
