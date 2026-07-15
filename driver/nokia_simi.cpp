#include "emu.h"
#include "nokia_simi.h"

#include "nokia_sim_card.h"

#include <algorithm>

DEFINE_DEVICE_TYPE(NOKIA_SIMI, nokia_simi_device, "nokia_simi", "Nokia MAD2 SIMI controller")

namespace {
constexpr u8 SIMI_INT_TX_EMPTY = 0x10;
constexpr u8 SIMI_INT_RX_READY = 0x40;
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
	m_control = 0;
	m_rx_head = m_rx_tail = m_rx_count = 0;
	m_rx_ready = false;
	m_iir = 0;
	m_tx_ready_pending = false;
	m_uart_tx_count = 0;
	m_rx_fifo_control = 0;
	m_tx_fifo_control = 0;
}

u8 nokia_simi_device::control_r() const
{
	return m_control | ((m_enabled && BIT(m_control, 7)) ? 0x40 : 0x00);
}

void nokia_simi_device::control_w(u8 data)
{
	const bool activate = m_enabled && BIT(data, 7) && !BIT(m_control, 7);
	m_control = data;
	if (!activate)
		return;

	m_rx_head = m_rx_tail = m_rx_count = 0;
	m_rx_ready = false;
	m_iir = 0;
	m_tx_ready_pending = false;
	m_uart_tx_count = 0;
	m_card->activate();
	schedule_card_bytes(false);
}

void nokia_simi_device::card_rx_w(u8 data)
{
	if (m_rx_count >= std::size(m_rx_fifo))
		return;
	m_rx_fifo[m_rx_tail] = data;
	m_rx_tail = (m_rx_tail + 1) % std::size(m_rx_fifo);
	m_rx_count++;
}

void nokia_simi_device::schedule_card_bytes(bool tx_complete, attotime delay)
{
	m_rx_ready = false;
	m_tx_ready_pending = tx_complete;
	if (tx_complete || m_rx_count != 0)
		m_rx_timer->adjust(delay);
}

TIMER_CALLBACK_MEMBER(nokia_simi_device::rx_ready)
{
	if (m_tx_ready_pending)
	{
		m_tx_ready_pending = false;
		m_iir |= SIMI_INT_TX_EMPTY;
		m_irq_cb(1);
		m_irq_cb(0);
		m_rx_timer->adjust(attotime::from_usec(100));
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
	if (m_rx_count != 0)
		m_rx_timer->adjust(attotime::from_usec(100));
	else
	{
		m_rx_ready = false;
		m_rx_timer->adjust(attotime::never);
	}
	return data;
}

u8 nokia_simi_device::rx_count_r() const
{
	return m_rx_ready ? std::min<u16>(m_rx_count, 0xff) : 0;
}

void nokia_simi_device::txd_w(u8 data)
{
	if (!m_enabled || !BIT(m_control, 7))
		return;
	if (m_uart_tx_count < std::size(m_uart_tx_fifo))
		m_uart_tx_fifo[m_uart_tx_count++] = data;
}

void nokia_simi_device::tx_fifo_control_w(u8 data)
{
	const u8 old = m_tx_fifo_control;
	m_tx_fifo_control = data;
	if (!m_enabled || !BIT(m_control, 7) || data != 0 || old == 0)
		return;

	for (unsigned i = 0; i < m_uart_tx_count; i++)
		m_card->rx_w(m_uart_tx_fifo[i]);
	m_uart_tx_count = 0;
	schedule_card_bytes(true);
}
