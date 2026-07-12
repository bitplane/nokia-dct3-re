// license:BSD-3-Clause
#ifndef MAME_NOKIA_NOKIA_SERVICE_TRANSPORT_H
#define MAME_NOKIA_NOKIA_SERVICE_TRANSPORT_H

#pragma once

class nokia_service_transport_device : public device_t
{
public:
	nokia_service_transport_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

	auto channel_empty_cb() { return m_channel_empty_cb.bind(); }
	void configure(bool responder, bool channel_drain, unsigned response_delay_ms, unsigned drain_delay_us);
	bool response_ready() const;
	void write_response(uint32_t address);
	void response_posted();
	void channel_busy();

protected:
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:
	TIMER_CALLBACK_MEMBER(channel_empty);

	required_device<cpu_device> m_cpu;
	devcb_write_line m_channel_empty_cb;
	emu_timer *m_channel_empty_timer = nullptr;
	bool m_responder_enabled = false;
	bool m_channel_drain_enabled = false;
	bool m_response_posted = false;
	unsigned m_response_delay_ms = 450;
	unsigned m_drain_delay_us = 1;
};

DECLARE_DEVICE_TYPE(NOKIA_SERVICE_TRANSPORT, nokia_service_transport_device)

#endif
