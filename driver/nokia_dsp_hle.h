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
	void set_service_tick_ms(unsigned period) { m_service_tick_ms = period; }
	void set_trace_enabled(bool enabled) { m_trace_enabled = enabled; }

	void tx_commit_w(int state);
	void service_pending_w(int state);
	void doorbell_w(int state);
	u16 bootstrap_r(offs_t offset, u16 backing);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	TIMER_CALLBACK_MEMBER(service_tick);
	TIMER_CALLBACK_MEMBER(packet_tick);
	void drain_responses();

	required_device<nokia_dspif_device> m_transport;
	required_device<nokia_external_service_peer_device> m_external_peer;
	emu_timer *m_service_timer = nullptr;
	emu_timer *m_packet_timer = nullptr;
	bool m_service_enabled = false;
	bool m_external_service_enabled = false;
	bool m_trace_enabled = false;
	unsigned m_service_delay_ms = 5;
	unsigned m_service_tick_ms = 5;
	bool m_service_control_completion_sent = false;
};

DECLARE_DEVICE_TYPE(NOKIA_DSP_HLE, nokia_dsp_hle_device)
