// license:BSD-3-Clause
#pragma once

#include "emu.h"

class nokia_dsp_peer_device : public device_t
{
public:
	nokia_dsp_peer_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	auto fiq0_cb() { return m_fiq0_cb.bind(); }
	auto service_irq_cb() { return m_service_irq_cb.bind(); }

	void set_service_enabled(bool enabled) { m_service_enabled = enabled; }
	void set_contact_enabled(bool enabled) { m_contact_enabled = enabled; }
	void set_service_delay_ms(unsigned delay) { m_service_delay_ms = delay; }
	void set_service_tick_ms(unsigned period) { m_service_tick_ms = period; }
	void set_trace_enabled(bool enabled) { m_trace_enabled = enabled; }

	u16 shared_r(offs_t offset);
	void shared_w(offs_t offset, u16 data, u16 mem_mask = ~0);
	u8 dspif_r(offs_t offset) const;
	void dspif_w(offs_t offset, u8 data);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	TIMER_CALLBACK_MEMBER(service_tick);
	TIMER_CALLBACK_MEMBER(contact_tick);
	bool enqueue_rx_packet(u8 type, const u8 *payload, unsigned payload_length);
	u8 tx_payload_byte(unsigned cursor, unsigned index) const;
	bool enqueue_transport_ack(unsigned cursor, unsigned payload_bytes);
	bool enqueue_contact_frame(u8 command, u8 result, u8 sequence);
	void pulse_fiq0();

	devcb_write_line m_fiq0_cb;
	devcb_write_line m_service_irq_cb;
	emu_timer *m_service_timer = nullptr;
	emu_timer *m_contact_timer = nullptr;
	u16 m_ram[0x800] = { 0 };
	u8 m_dspif[4] = { 0 };
	bool m_service_enabled = false;
	bool m_contact_enabled = false;
	bool m_trace_enabled = false;
	unsigned m_service_delay_ms = 5;
	unsigned m_service_tick_ms = 5;
	bool m_discovery_complete = false;
	bool m_registration_sent = false;
	bool m_registration_acknowledged = false;
	bool m_channel_map_sent = false;
	bool m_channel_map_acknowledged = false;
	bool m_healthy_sent = false;
	bool m_empty_ack_sent = false;
	bool m_contact_completion_sent = false;
	unsigned m_registration_ticks = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_DSP_PEER, nokia_dsp_peer_device)
