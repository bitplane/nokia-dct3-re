// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_RADIO_PEER_H
#define MAME_NOKIA_NOKIA_RADIO_PEER_H
#include "nokia_dspif.h"
#include "nokia_gsm_network.h"

class nokia_radio_peer_device : public device_t
{
public:
	nokia_radio_peer_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);

	void set_enabled(bool enabled) { m_enabled = enabled; }
	void set_trace_enabled(bool enabled) { m_trace_enabled = enabled; }
	bool enabled() const { return m_enabled; }
	void receive_packet(const nokia_dspif_device::packet &packet);
	void tick();
	bool fast_completion_pending() const;
	const char *phase_name() const;

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	enum phase : u8
	{
		inactive,
		initial_search,
		post_deactivate_search,
		candidate_measurement,
		candidate_sync,
		candidate_channel_change,
		candidate_ra_info,
		serving_bcch,
		serving_idle_ra,
		candidate_retry,
		selected_search,
		serving_channel_change,
		selected_channel_change,
		selected_bcch,
		selected_ra_info,
		selected_bcch_channel_change,
		random_access,
		assigned_channel_change,
		lapdm_establish,
		contention_resolution,
		location_update_accept,
		rr_channel_release,
		release_deconfigure,
		release_channel_change,
		count
	};

	static const char *phase_name(u8 value);
	u8 next_report_type() const;
	void emit_report();
	void advance_after_report(u8 report_type);

	required_device<nokia_dspif_device> m_transport;
	required_device<nokia_gsm_network_device> m_gsm_network;
	bool m_enabled = false;
	bool m_trace_enabled = false;
	unsigned m_reports_sent = 0;
	unsigned m_reports_remaining = 0;
	u8 m_phase = phase::inactive;
	unsigned m_search_round = 0;
	unsigned m_wait_ticks = 0;
	u8 m_search_mode = 0;
	u8 m_access_ra = 0;
	u32 m_access_frame = 0;
	std::array<u8, 20> m_contention_l3{};
	unsigned m_contention_length = 0;
	bool m_search_has_arfcn1 = false;
	bool m_report_deferred = false;
	bool m_search_requested = false;
	unsigned m_selected_reports_remaining = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_RADIO_PEER, nokia_radio_peer_device)

#endif // MAME_NOKIA_NOKIA_RADIO_PEER_H
