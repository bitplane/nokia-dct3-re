// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_DSP_HLE_H
#define MAME_NOKIA_NOKIA_DSP_HLE_H

#include "nokia_dspif.h"
#include "nokia_external_service.h"
#include "nokia_radio_peer.h"

class nokia_dsp_hle_device : public device_t
{
public:
	nokia_dsp_hle_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	void set_service_enabled(bool enabled) { m_service_enabled = enabled; }
	void set_external_service_enabled(bool enabled) { m_external_service_enabled = enabled; }
	void set_service_delay_us(unsigned delay) { m_service_delay_us = delay; }
	void set_peer_poll_ms(unsigned period) { m_peer_poll_ms = period; }
	void set_bootstrap_exchange_limit(unsigned exchanges) { m_bootstrap_exchange_limit = exchanges; }
	void set_bootstrap_ping_pong(bool enabled) { m_bootstrap_ping_pong = enabled; }
	void set_code_block_request(bool enabled) { m_code_block_request = enabled; }
	void set_parked_boot_status(bool enabled, u16 response)
	{
		m_parked_boot_status = enabled;
		m_boot_status_response = response;
	}
	void set_trace_enabled(bool enabled) { m_trace_enabled = enabled; }

	void tx_commit_w(int state);
	void service_pending_w(int state);
	void doorbell_w(int state);
	void shared_002_write_w(int state);
	void shared_0fe_read_w(int state);
	void shared_0fe_write_w(int state);
	void shared_100_read_w(int state);
	void shared_100_write_w(int state);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	TIMER_CALLBACK_MEMBER(service_tick);
	TIMER_CALLBACK_MEMBER(packet_tick);
	TIMER_CALLBACK_MEMBER(response_tick);
	TIMER_CALLBACK_MEMBER(keepalive_tick);
	void drain_responses();
	void schedule_response();
	void publish_bootstrap_state();
	required_device<nokia_dspif_device> m_transport;
	required_device<nokia_external_service_peer_device> m_external_peer;
	required_device<nokia_radio_peer_device> m_radio_peer;
	emu_timer *m_service_timer = nullptr;
	emu_timer *m_packet_timer = nullptr;
	emu_timer *m_response_timer = nullptr;
	emu_timer *m_keepalive_timer = nullptr;
	bool m_service_enabled = false;
	bool m_external_service_enabled = false;
	bool m_trace_enabled = false;
	unsigned m_service_delay_us = 5'000;
	unsigned m_peer_poll_ms = 5;
	bool m_service_control_completion_sent = false;
	unsigned m_bootstrap_exchange_limit = 64;
	unsigned m_bootstrap_exchange_count = 0;
	bool m_bootstrap_ping_pong = false;
	bool m_code_block_request = false;
	bool m_parked_boot_status = false;
	u16 m_boot_status_response = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_DSP_HLE, nokia_dsp_hle_device)

#endif // MAME_NOKIA_NOKIA_DSP_HLE_H
