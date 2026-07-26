// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#include "emu.h"
#include "nokia_simi.h"

#include "nokia_sim_card.h"

#include <algorithm>

DEFINE_DEVICE_TYPE(NOKIA_SIMI, nokia_simi_device, "nokia_simi", "Nokia MAD2 SIMI controller")

namespace {
constexpr u8 SIMI_INT_TX_EMPTY = 0x10;
constexpr u8 SIMI_INT_RX_READY = 0x40;
// The firmware answers ATR TA1=0x05 with PPS ff 00 ff, retaining default
// parameters. Model one ten-bit character at 9,600 bit/s throughout.
const attotime SIMI_CHARACTER_TIME = attotime::from_hz(960);
}

nokia_simi_device::nokia_simi_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_SIMI, tag, owner, clock),
	m_card(*this, "^sim_card"),
	m_irq_cb(*this)
{
}

void nokia_simi_device::device_start()
{
	m_rx_timer = timer_alloc(FUNC(nokia_simi_device::rx_ready), this);
	save_item(NAME(m_enabled));
	save_item(NAME(m_card_present));
	save_item(NAME(m_clock_enabled));
	save_item(NAME(m_control));
	save_item(NAME(m_rx_fifo));
	save_item(NAME(m_rx_head));
	save_item(NAME(m_rx_tail));
	save_item(NAME(m_rx_count));
	save_item(NAME(m_rx_ready));
	save_item(NAME(m_iir));
	save_item(NAME(m_tx_ready_pending));
	save_item(NAME(m_uart_tx_fifo));
	save_item(NAME(m_uart_tx_count));
	save_item(NAME(m_rx_fifo_control));
	save_item(NAME(m_tx_fifo_control));
}

void nokia_simi_device::device_reset()
{
	m_rx_timer->adjust(attotime::never);
	m_clock_enabled = false;
	m_control = 0;
	m_rx_head = m_rx_tail = m_rx_count = 0;
	m_rx_ready = false;
	m_iir = 0;
	m_tx_ready_pending = false;
	m_uart_tx_count = 0;
	m_rx_fifo_control = 0;
	m_tx_fifo_control = 0;
}

void nokia_simi_device::set_clock_enabled(int state)
{
	const bool enabled = bool(state);
	if (m_clock_enabled == enabled)
		return;
	m_clock_enabled = enabled;
	if (!enabled)
	{
		// Clock gating freezes the controller and suppresses delivery. Register,
		// FIFO and card protocol state survive for the next gated transaction.
		m_rx_timer->adjust(attotime::never);
	}
	else if (m_tx_ready_pending || (!m_rx_ready && m_rx_count != 0))
	{
		m_rx_timer->adjust(SIMI_CHARACTER_TIME);
	}
}

u8 nokia_simi_device::control_r() const
{
	return m_control | ((m_enabled && m_clock_enabled && BIT(m_control, 7)) ? 0x40 : 0x00);
}

void nokia_simi_device::control_w(u8 data)
{
	const bool activate = m_enabled && m_clock_enabled && BIT(data, 7) && !BIT(m_control, 7);
	m_control = data;
	if (!activate)
		return;

	m_rx_head = m_rx_tail = m_rx_count = 0;
	m_rx_ready = false;
	m_iir = 0;
	m_tx_ready_pending = false;
	m_uart_tx_count = 0;
	if (m_card_present)
		m_card->activate();
	// Deliver ATR outside the control-register write so its FIQ cannot
	// re-enter the firmware's activation routine.
	schedule_card_bytes(false, m_card_present);
}

void nokia_simi_device::card_rx_w(u8 data)
{
	if (m_rx_count >= std::size(m_rx_fifo))
		return;
	m_rx_fifo[m_rx_tail] = data;
	m_rx_tail = (m_rx_tail + 1) % std::size(m_rx_fifo);
	m_rx_count++;
}

void nokia_simi_device::schedule_card_bytes(
		bool tx_complete, bool response_added)
{
	// A card cannot answer while the CPU is still writing the final transmit
	// byte. Deferring receive-ready also prevents the firmware's FIQ handler
	// from observing the previous transaction descriptor during that write.
	if (response_added)
	{
		m_rx_ready = false;
		m_tx_ready_pending = tx_complete;
	}
	else if (tx_complete && !m_tx_ready_pending)
	{
		// A TX-only completion must not hide an unread RX character.
		m_tx_ready_pending = true;
	}
	else
	{
		return;
	}

	if (m_tx_ready_pending || (response_added && m_rx_count != 0))
		m_rx_timer->adjust(SIMI_CHARACTER_TIME);
}

TIMER_CALLBACK_MEMBER(nokia_simi_device::rx_ready)
{
	if (!m_enabled || !m_clock_enabled)
		return;
	if (m_tx_ready_pending)
	{
		m_tx_ready_pending = false;
		m_iir |= SIMI_INT_TX_EMPTY;
		m_irq_cb(1);
		m_irq_cb(0);
		m_rx_timer->adjust(SIMI_CHARACTER_TIME);
		return;
	}
	if (m_rx_count != 0)
	{
		m_rx_ready = true;
		m_iir |= SIMI_INT_RX_READY;
		m_irq_cb(1);
		m_irq_cb(0);
	}
}

u8 nokia_simi_device::rxd_r()
{
	if (!m_rx_ready || m_rx_count == 0)
		return 0;
	const u8 data = m_rx_fifo[m_rx_head];
	m_rx_head = (m_rx_head + 1) % std::size(m_rx_fifo);
	m_rx_count--;
	m_rx_ready = false;
	// SIMI services one received character per FIQ. Present subsequent
	// characters at serial cadence instead of treating a response as one edge
	// with a pre-filled FIFO.
	if (m_rx_count != 0)
		m_rx_timer->adjust(SIMI_CHARACTER_TIME);
	else
	{
		m_rx_timer->adjust(attotime::never);
	}
	return data;
}

u8 nokia_simi_device::rx_count_r() const
{
	// The response queue is private serialization state. Firmware sees the one
	// character currently transferred into the SIMI receive holding register.
	return m_rx_ready && m_rx_count != 0 ? 1 : 0;
}

void nokia_simi_device::rx_fifo_control_w(u8 data)
{
	// Activation programs 0x18, 0x12 and 0x06; receive rewrites 0x06 before
	// each drain. No recovered firmware path assigns a destructive flush side
	// effect, so this remains a configuration latch.
	m_rx_fifo_control = data;
}

void nokia_simi_device::txd_w(u8 data)
{
	if (!m_enabled || !m_clock_enabled || !BIT(m_control, 7))
		return;
	if (m_uart_tx_count < std::size(m_uart_tx_fifo))
		m_uart_tx_fifo[m_uart_tx_count++] = data;
}

void nokia_simi_device::tx_fifo_control_w(u8 data)
{
	const u8 old = m_tx_fifo_control;
	m_tx_fifo_control = data;
	if (!m_enabled || !m_clock_enabled || !BIT(m_control, 7) || data != 0 || old == 0)
		return;

	const u16 rx_count_before = m_rx_count;
	if (m_card_present)
		for (unsigned i = 0; i < m_uart_tx_count; i++)
			m_card->rx_w(m_uart_tx_fifo[i]);
	m_uart_tx_count = 0;
	schedule_card_bytes(true, m_rx_count != rx_count_before);
}
