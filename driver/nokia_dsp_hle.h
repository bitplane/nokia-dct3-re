// license:BSD-3-Clause
#pragma once

#include "emu.h"
#include "nokia_dspif.h"
#include "nokia_external_service.h"

class nokia_dsp_hle_device : public device_t
{
public:
	nokia_dsp_hle_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock = 0);

	void set_service_enabled(bool enabled) { m_service_enabled = enabled; }
	void set_external_service_enabled(bool enabled) { m_external_service_enabled = enabled; }
	void set_service_delay_ms(unsigned delay) { m_service_delay_ms = delay; }
	void set_peer_poll_ms(unsigned period) { m_peer_poll_ms = period; }
	void set_radio_scenario(unsigned scenario) { m_radio_scenario = scenario; }
	void set_trace_enabled(bool enabled) { m_trace_enabled = enabled; }

	void tx_commit_w(int state);
	void service_pending_w(int state);
	void doorbell_w(int state);
	void bootstrap_fe_w(int state);
	void bootstrap_100_w(int state);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	TIMER_CALLBACK_MEMBER(service_tick);
	TIMER_CALLBACK_MEMBER(packet_tick);
	TIMER_CALLBACK_MEMBER(response_tick);
	void drain_responses();
	void schedule_response();
	void publish_bootstrap_state();

	required_device<nokia_dspif_device> m_transport;
	required_device<nokia_external_service_peer_device> m_external_peer;
	emu_timer *m_service_timer = nullptr;
	emu_timer *m_packet_timer = nullptr;
	emu_timer *m_response_timer = nullptr;
	bool m_service_enabled = false;
	bool m_external_service_enabled = false;
	bool m_trace_enabled = false;
	unsigned m_service_delay_ms = 5;
	unsigned m_peer_poll_ms = 5;
	unsigned m_radio_scenario = 0;
	bool m_service_control_completion_sent = false;
	unsigned m_radio_reports_sent = 0;
	unsigned m_radio_reports_pending = 0;
	unsigned m_radio_sequence_stage = 0;
	unsigned m_bootstrap_exchange_count = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_DSP_HLE, nokia_dsp_hle_device)
