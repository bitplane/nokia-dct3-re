// license:BSD-3-Clause
#include "emu.h"
#include "nokia_service_transport.h"

DEFINE_DEVICE_TYPE(NOKIA_SERVICE_TRANSPORT, nokia_service_transport_device,
		"nokia_service_transport", "Nokia DCT3 service transport")

nokia_service_transport_device::nokia_service_transport_device(
		const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, NOKIA_SERVICE_TRANSPORT, tag, owner, clock),
	m_cpu(*this, "^maincpu"),
	m_channel_empty_cb(*this)
{
}

void nokia_service_transport_device::device_start()
{
	m_channel_empty_timer = timer_alloc(FUNC(nokia_service_transport_device::channel_empty), this);
	save_item(NAME(m_responder_enabled));
	save_item(NAME(m_channel_drain_enabled));
	save_item(NAME(m_response_posted));
	save_item(NAME(m_response_delay_ms));
	save_item(NAME(m_drain_delay_us));
}

void nokia_service_transport_device::device_reset()
{
	m_response_posted = false;
	m_channel_empty_timer->adjust(attotime::never);
}

void nokia_service_transport_device::configure(
		bool responder, bool channel_drain, unsigned response_delay_ms, unsigned drain_delay_us)
{
	m_responder_enabled = responder;
	m_channel_drain_enabled = channel_drain;
	m_response_delay_ms = response_delay_ms;
	m_drain_delay_us = drain_delay_us;
}

bool nokia_service_transport_device::response_ready() const
{
	return m_responder_enabled && !m_response_posted &&
			machine().time() >= attotime::from_msec(m_response_delay_ms);
}

void nokia_service_transport_device::write_response(uint32_t address)
{
	// Contact-service inbound message: node-0x18 transport class, command 0x64,
	// healthy completion 0x05. Allocation and mailbox ownership remain firmware-side.
	static constexpr uint8_t response[0x14] = {
		0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00,
		0x64, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
	};
	address_space &space = m_cpu->space(AS_PROGRAM);
	for (unsigned i = 0; i < std::size(response); i++)
		space.write_byte(address + i, response[i]);
}

void nokia_service_transport_device::response_posted()
{
	m_response_posted = true;
}

void nokia_service_transport_device::channel_busy()
{
	if (m_channel_drain_enabled)
		m_channel_empty_timer->adjust(attotime::from_usec(m_drain_delay_us));
}

TIMER_CALLBACK_MEMBER(nokia_service_transport_device::channel_empty)
{
	m_channel_empty_cb(1);
}
