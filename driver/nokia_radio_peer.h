// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_RADIO_PEER_H
#define MAME_NOKIA_NOKIA_RADIO_PEER_H
#include "gsm_a5.h"
#include "gsm_tch_f_l1.h"
#include "gsm_xcch_l1.h"
#include "nokia_dspif.h"
#include "nokia_gsm_network.h"
#include "nokia_gsm_session.h"
#include "nokia_gsm_voice_peer.h"
#include "nokia_lapdm_link.h"

#include <array>

class nokia_radio_peer_device : public device_t
{
public:
	enum class acquisition_strategy : u8
	{
		none,
		bitmap_multistage,
		candidate_window,
		autonomous_band_scan
	};

	enum class neighbour_bsic_encoding : u8
	{
		none,
		direct,
		low_six_bits
	};

	enum class neighbour_arfcn_encoding : u8
	{
		direct_octet,
		topology_low_octet
	};

	struct protocol_contract
	{
		acquisition_strategy acquisition = acquisition_strategy::none;
		u8 traffic_release_parameter = 0;
		u8 assigned_channel_confirmation = 0;
		unsigned mm_information_settle_ticks = 0;
		bool repeat_empty_assigned_uplink = false;
		u8 serving_sch_request_type = 0;
		bool neighbour_instruction_establishes_candidate = false;
		bool serving_list_commits_receiver = false;
		neighbour_arfcn_encoding neighbour_instruction_arfcn =
				neighbour_arfcn_encoding::direct_octet;
		neighbour_bsic_encoding neighbour_instruction_bsic =
				neighbour_bsic_encoding::none;

		constexpr bool enabled() const
		{
			return acquisition != acquisition_strategy::none;
		}
	};

	// GSM 06.10/ETSI TS 46.010 full-rate speech: one 20 ms, 260-bit
	// parameter frame in the conventional 33-octet serial representation.
	static constexpr unsigned speech_frame_octets = 33;
	using speech_frame = std::array<u8, speech_frame_octets>;
	enum class speech_delivery : u8
	{
		none,
		good,
		bad
	};

	nokia_radio_peer_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);

	void set_enabled(bool enabled) { m_enabled = enabled; }
	void set_protocol_contract(protocol_contract contract)
	{
		m_protocol = contract;
	}
	void set_page_after_registration(bool enabled)
	{
		m_page_after_registration = enabled;
	}
	void set_page_requires_reselection(bool enabled)
	{
		m_page_requires_reselection = enabled;
	}
	void set_incoming_call_after_registration(bool enabled)
	{
		m_incoming_call_after_registration = enabled;
	}
	void set_incoming_sms_after_registration(bool enabled)
	{
		m_incoming_sms_after_registration = enabled;
	}
	void set_incoming_smart_message_after_registration(bool enabled)
	{
		m_incoming_smart_message_after_registration = enabled;
	}
	bool queue_host_incoming_call(const u8 *digits, unsigned length);
	bool traffic_channel_active() const { return m_traffic_channel_active; }
	void set_speech_loopback(bool enabled) { m_speech_loopback = enabled; }
	void set_downlink_tch_burst_error_profile(
			unsigned period, unsigned span)
	{
		m_downlink_tch_burst_error_period = period;
		m_downlink_tch_burst_error_span = span;
	}
	void set_uplink_tch_burst_error_profile(unsigned period, unsigned span)
	{
		m_uplink_tch_burst_error_period = period;
		m_uplink_tch_burst_error_span = span;
	}
	void set_lab_voice_source(bool enabled)
	{
		m_voice_peer->set_lab_test_source(enabled);
		m_lab_voice_source = enabled;
	}
	void set_host_voice_peer(bool enabled) { m_host_voice_peer = enabled; }
	bool enabled() const { return m_enabled; }
	void receive_packet(const nokia_dspif_device::packet &packet);
	void tick();
	bool fast_completion_pending() const;
	const char *phase_name() const;
	bool speech_channel_active() const;
	bool queue_downlink_speech(const speech_frame &frame);
	speech_delivery take_downlink_speech(speech_frame &frame);
	bool submit_uplink_speech(const speech_frame &frame);
	bool take_uplink_speech(speech_frame &frame);
	bool queue_downlink_sacch(
			const gsm::tch_f::packed_control_block &block);
	bool submit_uplink_sacch(
			const gsm::tch_f::packed_control_block &block);

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	// Scoped so that neither a bare integer nor another enum can be written
	// into the phase. Ordering is load-bearing: several tests ask whether the
	// phase lies within a contiguous block, so entries must not be reordered.
	enum class phase : u8
	{
		inactive,
		initial_search,
		post_deactivate_search,
		candidate_measurement,
		candidate_sync,
		candidate_channel_change,
		candidate_ra_info,
		serving_bcch,
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
		location_update_ack_request,
		location_update_acknowledgement,
		rr_channel_release,
		channel_release_uplink_request,
		channel_release_acknowledgement,
		release_deconfigure,
		release_channel_change,
		service_downlink,
		service_uplink_request,
		service_uplink_wait,
		service_uplink_acknowledgement,
		traffic_channel_change,
		traffic_lapdm_establish,
		traffic_contention_resolution,
		traffic_release_acknowledgement,
		candidate_terminal_control,
		serving_sch_observation,
		service_sapi3_contention_resolution,
		count
	};

	static const char *phase_name(u8 value);
	// The phase is held as u8 because MAME's save system takes only fundamental
	// types; these keep every use site typed regardless.
	phase current_phase() const { return phase(m_phase); }
	void set_phase(phase next) { m_phase = u8(next); }
	enum class search_request : u8
	{
		none,
		bitmap_multistage,
		candidate_window,
		autonomous_band_scan
	};
	enum class paging_schedule : u8
	{
		monitored,
		transmitted
	};
	// Answer an untargeted search from the receivable topology.
	void populate_search_from_receivable_cells(u8 mode);
	// Move the receiver to another carrier, returning the carrier it left.
	// Invalidates both decoded BCCH contexts, which belonged to that carrier.
	u16 retune_receiver(u16 arfcn);
	// Adopt the configured receiver as the serving cell. Only the part every
	// commit shares; what a particular commit implies about validation, the
	// candidate BCCH and the DSC counter stays at the call site.
	void commit_receiver_as_serving();
	// Begin a fresh TS 45.008 downlink-signalling observation interval.
	void restart_downlink_signalling_counter();
	void trace_layer3_uplink(const char *direction);
	void enter_release_deconfigure();
	search_request decode_search_request(
			const nokia_dspif_device::packet &packet);
	bool decode_candidate_window(
			const nokia_dspif_device::packet &packet, bool ignore_zero);
	bool decode_neighbour_measurement_list(
			const nokia_dspif_device::packet &packet);
	bool decode_neighbour_measurement_instruction(
			const nokia_dspif_device::packet &packet);
	bool decode_serving_sch_request(
			const nokia_dspif_device::packet &packet);
	bool uses_candidate_window() const;
	bool handle_search_request(search_request request);
	bool handle_acquisition_packet(
			const nokia_dspif_device::packet &packet);
	bool phase_waits() const;
	u8 next_report_type() const;
	unsigned serving_cycle_reports() const;
	bool serving_pch_report() const;
	u32 paging_frame_number(
			u32 minimum_frame_number, paging_schedule schedule) const;
	void encode_measurement_report(u8 *payload) const;
	void encode_channel_confirmation(u8 *payload) const;
	void encode_random_access_info(u8 *payload);
	void emit_report();
	void advance_after_report(u8 report_type);
	static constexpr unsigned speech_queue_depth = 8;
	bool speech_queue_push(
			std::array<speech_frame, speech_queue_depth> &queue,
			u8 &head, u8 &count, const speech_frame &frame);
	bool speech_queue_pop(
			std::array<speech_frame, speech_queue_depth> &queue,
			u8 &head, u8 &count, speech_frame &frame);
	bool queue_downlink_delivery(const speech_frame &frame, bool good);
	void clear_speech_queues();
	void reset_l1_pipeline();
	void prepare_l1_save();
	void restore_l1_block_kinds();
	nokia_lapdm_link_device::uplink_result receive_lapdm_uplink(
			const u8 *frame, unsigned length);
	void deliver_lapdm_downlink(
			const std::array<u8, nokia_lapdm_link_device::frame_length> &frame,
			u8 *payload, u32 reference_frame);
	bool apply_active_cipher(gsm::tch_f::burst_payload &payload,
			u32 frame_number, gsm::a5::direction direction) const;
	TIMER_CALLBACK_MEMBER(burst_tick);

	required_device<nokia_dspif_device> m_transport;
	required_device<nokia_gsm_network_device> m_gsm_network;
	required_device<nokia_gsm_session_device> m_gsm_session;
	required_device<nokia_gsm_voice_peer_device> m_voice_peer;
	required_device<nokia_lapdm_link_device> m_lapdm_link;
	emu_timer *m_burst_timer = nullptr;
	bool m_enabled = false;
	protocol_contract m_protocol;
	bool m_trace_enabled = false;
	unsigned m_reports_sent = 0;
	unsigned m_reports_remaining = 0;
	u8 m_phase = u8(phase::inactive);
	unsigned m_search_round = 0;
	unsigned m_idle_measurement_sample = 0;
	unsigned m_wait_ticks = 0;
	u8 m_search_mode = 0;
	std::array<u16, 40> m_search_arfcns{};
	u8 m_search_arfcn_count = 0;
	std::array<u16, 32> m_neighbour_arfcns{};
	u8 m_neighbour_arfcn_count = 0;
	bool m_neighbour_bcch_pending = false;
	u16 m_neighbour_bcch_arfcn = 0xffff;
	u8 m_neighbour_instruction_mode = 0;
	u8 m_neighbour_instruction_bsic = 0;
	u16 m_last_neighbour_instruction_arfcn = 0xffff;
	u8 m_last_neighbour_instruction_bsic = 0;
	unsigned m_neighbour_resume_wait_ticks = 0;
	u8 m_access_ra = 0;
	u32 m_access_frame = 0;
	bool m_search_has_serving_arfcn = false;
	u16 m_serving_arfcn = 1;
	u16 m_receiver_arfcn = 1;
	u8 m_receiver_bsic = 0x12;
	u16 m_sch_observation_arfcn = 0xffff;
	bool m_candidate_bcch_valid = false;
	bool m_reselection_validation_pending = false;
	s16 m_downlink_signalling_count = 45;
	bool m_downlink_signalling_failed = false;
	bool m_serving_loss_pending = false;
	bool m_report_deferred = false;
	bool m_search_requested = false;
	unsigned m_selected_reports_remaining = 0;
	bool m_registered = false;
	bool m_idle_common_control_active = false;
	bool m_page_after_registration = false;
	bool m_page_requires_reselection = false;
	bool m_has_reselected = false;
	bool m_incoming_call_after_registration = false;
	bool m_incoming_sms_after_registration = false;
	bool m_incoming_smart_message_after_registration = false;
	bool m_host_incoming_call_pending = false;
	bool m_speech_loopback = false;
	bool m_lab_voice_source = false;
	bool m_host_voice_peer = false;
	bool m_pch_fill_delivered = false;
	bool m_page_transmitted = false;
	bool m_traffic_channel_active = false;
	unsigned m_downlink_offset = 0;
	bool m_followup_downlink_opportunity = false;
	std::array<speech_frame, speech_queue_depth> m_downlink_speech{};
	std::array<u8, speech_queue_depth> m_downlink_speech_good{};
	std::array<speech_frame, speech_queue_depth> m_uplink_speech{};
	u8 m_downlink_speech_head = 0;
	u8 m_downlink_speech_count = 0;
	u8 m_uplink_speech_head = 0;
	u8 m_uplink_speech_count = 0;
	u64 m_uplink_speech_received = 0;
	u32 m_tdma_frame_number = 0;
	u32 m_bcch_frame_number = 0;
	bool m_bcch_frame_valid = false;
	bool m_l1_traffic_active = false;
	gsm::tch_f::diagonal_transmitter m_uplink_transmitter;
	gsm::tch_f::diagonal_receiver m_network_receiver;
	gsm::tch_f::diagonal_transmitter m_downlink_transmitter;
	gsm::tch_f::diagonal_receiver m_handset_receiver;
	gsm::tch_f::sacch_transmitter m_uplink_sacch_transmitter;
	gsm::tch_f::sacch_receiver m_network_sacch_receiver;
	gsm::tch_f::sacch_transmitter m_downlink_sacch_transmitter;
	gsm::tch_f::sacch_receiver m_handset_sacch_receiver;
	std::array<u8, gsm::tch_f::diagonal_transmitter::queue_depth>
			m_uplink_l1_block_kinds{};
	std::array<u8, gsm::tch_f::diagonal_transmitter::queue_depth>
			m_downlink_l1_block_kinds{};
	u64 m_uplink_facch_blocks = 0;
	u64 m_downlink_facch_blocks = 0;
	u64 m_uplink_bad_speech_blocks = 0;
	u64 m_downlink_bad_speech_blocks = 0;
	u64 m_sacch_slots = 0;
	unsigned m_uplink_tch_burst_error_period = 0;
	unsigned m_uplink_tch_burst_error_span = 0;
	u64 m_uplink_tch_bursts = 0;
	u64 m_uplink_tch_bursts_impaired = 0;
	unsigned m_downlink_tch_burst_error_period = 0;
	unsigned m_downlink_tch_burst_error_span = 0;
	u64 m_downlink_tch_bursts = 0;
	u64 m_downlink_tch_bursts_impaired = 0;
	u64 m_uplink_ciphered_bursts = 0;
	u64 m_downlink_ciphered_bursts = 0;
	u64 m_uplink_ciphered_xcch_blocks = 0;
	u64 m_downlink_ciphered_xcch_blocks = 0;
};

DECLARE_DEVICE_TYPE(NOKIA_RADIO_PEER, nokia_radio_peer_device)

#endif // MAME_NOKIA_NOKIA_RADIO_PEER_H
