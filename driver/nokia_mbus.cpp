#include "emu.h"
#include "nokia_mbus.h"

DEFINE_DEVICE_TYPE(NOKIA_MBUS, nokia_mbus_device, "nokia_mbus", "Nokia MAD2 MBUS controller")

namespace {
constexpr u8 CTRL_RESET = 0x80;
constexpr u8 CTRL_TX_ENABLE = 0x20;
constexpr u8 CTRL_RX_ENABLE = 0x40;
constexpr u8 STATUS_TX_READY = 0x10;
constexpr u8 STATUS_RX_READY = 0x20;
constexpr u8 STATUS_LINE_IDLE = 0xc0;
}

nokia_mbus_device::nokia_mbus_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_MBUS, tag, owner, clock),
	m_tx_cb(*this),
	m_fiq2_cb(*this),
	m_fiq3_cb(*this)
{
}

void nokia_mbus_device::device_start()
{
	m_byte_timer = timer_alloc(FUNC(nokia_mbus_device::byte_complete), this);
	m_fiq3_timer = timer_alloc(FUNC(nokia_mbus_device::fiq3_event), this);
	save_item(NAME(m_control));
	save_item(NAME(m_status_latch));
	save_item(NAME(m_data));
	save_item(NAME(m_tx_data));
	save_item(NAME(m_rx_ready));
	save_item(NAME(m_tx_ready));
	save_item(NAME(m_tx_pending));
}

void nokia_mbus_device::device_reset()
{
	m_control = 0;
	m_status_latch = 0;
	m_data = 0;
	m_tx_data = 0;
	m_rx_ready = false;
	m_tx_ready = false;
	m_tx_pending = false;
	m_trace_count = 0;
	m_byte_timer->adjust(attotime::never);
	m_fiq3_timer->adjust(attotime::never);
}

u8 nokia_mbus_device::status() const
{
	return STATUS_LINE_IDLE | (m_status_latch & 0x0f) |
			(m_tx_ready ? STATUS_TX_READY : 0) |
			(m_rx_ready ? STATUS_RX_READY : 0);
}

u8 nokia_mbus_device::read(offs_t offset)
{
	switch (offset & 3)
	{
	case 0:
		return m_control & ~CTRL_RESET;
	case 1:
		return status();
	case 2:
	{
		const u8 result = m_data;
		m_rx_ready = false;
		trace_event("rx_read", result);
		return result;
	}
	default:
		return 0xff;
	}
}

void nokia_mbus_device::write(offs_t offset, u8 data)
{
	switch (offset & 3)
	{
	case 0:
	{
		const u8 old = m_control;
		if (data & CTRL_RESET)
		{
			m_control = 0;
			m_status_latch = 0;
			m_rx_ready = false;
			m_tx_ready = false;
			m_tx_pending = false;
			m_byte_timer->adjust(attotime::never);
		}
		else
		{
			m_control = data;
			if ((data & CTRL_TX_ENABLE) && !(old & CTRL_TX_ENABLE))
				schedule_byte();
			else if ((old & CTRL_RX_ENABLE) && !(data & CTRL_RX_ENABLE))
				schedule_byte();
		}
		trace_event("control", data);
		break;
	}
	case 1:
		m_status_latch = data & 0x0f;
		// Both recovered 3210 initializers set the low status bits and bit 7
		// before entering receive mode. The source clock for this FIQ3 event is
		// not recovered, so preserve the observed calibrated delay explicitly.
		if (BIT(data, 7))
			m_fiq3_timer->adjust(m_byte_delay);
		trace_event("status", data);
		break;
	case 2:
		m_tx_data = data;
		m_tx_pending = true;
		m_tx_ready = false;
		schedule_byte();
		trace_event("tx_load", data);
		break;
	}
}

bool nokia_mbus_device::receive_byte(u8 data)
{
	if (!(m_control & CTRL_RX_ENABLE) || m_rx_ready)
		return false;
	m_data = data;
	m_rx_ready = true;
	trace_event("rx_ready", data);
	m_fiq2_cb(1);
	return true;
}

void nokia_mbus_device::schedule_byte()
{
	m_byte_timer->adjust(m_byte_delay);
}

TIMER_CALLBACK_MEMBER(nokia_mbus_device::byte_complete)
{
	if (m_tx_pending)
	{
		m_tx_pending = false;
		m_tx_cb(m_tx_data);
		trace_event("tx_complete", m_tx_data);
	}
	if (m_control & CTRL_TX_ENABLE)
	{
		m_tx_ready = true;
		m_status_latch &= ~0x07;
		m_fiq2_cb(1);
	}
}

TIMER_CALLBACK_MEMBER(nokia_mbus_device::fiq3_event)
{
	trace_event("fiq3");
	m_fiq3_cb(1);
}

void nokia_mbus_device::trace_event(const char *event, u8 data)
{
	if (m_trace && m_trace_count++ < 8192)
		logerror("mbus_device: event=%s data=%02x ctrl=%02x status=%02x rx=%u tx=%u t=%.9f\n",
				event, data, m_control, status(), m_rx_ready, m_tx_pending,
				machine().time().as_double());
}
