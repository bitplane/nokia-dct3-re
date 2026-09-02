// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#ifndef MAME_NOKIA_NOKIA_GSM_SESSION_H
#define MAME_NOKIA_NOKIA_GSM_SESSION_H

#include "gsm_sms_transport.h"
#include "nokia_gsm_network.h"

#include <array>

class nokia_gsm_session_device : public device_t
{
public:
	static constexpr unsigned maximum_layer3_length =
			nokia_gsm_network_device::maximum_layer3_length;

	enum class incoming_service : u8
	{
		none,
		call,
		sms,
		smart_message
	};

	enum class downlink_kind : u8
	{
		none,
		authentication_request,
		authentication_reject,
		location_update_accept,
		channel_release,
		cm_service_accept,
		cm_service_reject,
		cipher_mode_command,
		mm_information,
		incoming_call_setup,
		call_proceeding,
		call_alerting,
		call_connect,
		call_disconnect,
		traffic_assignment,
		sapi3_establishment,
		incoming_sms_cp_data,
		sms_cp_ack,
		connect_acknowledge,
		call_release,
		release_complete
	};

	struct downlink_message
	{
		u8 kind = u8(downlink_kind::none);
		u8 sapi = 0;
		unsigned length = 0;
		std::array<u8, maximum_layer3_length> data{};
	};

	nokia_gsm_session_device(const machine_config &mconfig, const char *tag,
			device_t *owner, u32 clock = 0);

	void set_authentication_required(bool required)
	{
		m_authentication_required = required;
	}
	void set_outgoing_decision_delay_ms(unsigned delay)
	{
		m_outgoing_decision_delay_ms = delay;
	}
	void set_outgoing_fallback_enabled(bool enabled)
	{
		m_outgoing_fallback_enabled = enabled;
	}
	bool submit_outgoing_decision(
			u32 request_id,
			nokia_gsm_network_device::outgoing_call_outcome outcome);
	bool submit_outgoing_termination(u32 request_id, u8 cause = 0x10);
	bool submit_incoming_termination(u8 cause = 0x10);
	bool set_incoming_caller(const u8 *digits, unsigned length);
	bool establish_layer3(
			const u8 *information, unsigned length, u16 serving_arfcn);
	bool queue_incoming_page(incoming_service service = incoming_service::none);
	bool incoming_service_admissible(incoming_service service) const;
	downlink_kind contention_resolution_delivered();
	downlink_kind downlink_acknowledged();
	downlink_kind receive_layer3(
			u8 sapi, const u8 *information, unsigned length);
	bool begin_traffic_assignment();

	const downlink_message &pending_downlink() const { return m_pending_downlink; }
	downlink_kind pending_downlink_kind() const
	{
		return downlink_kind(m_pending_downlink.kind);
	}
	std::array<u8, 24> paging_request() const;
	const std::array<u8, 8> &registered_mobile_identity() const
	{
		return m_registered_mobile_identity;
	}
	unsigned registered_mobile_identity_length() const
	{
		return m_registered_mobile_identity_length;
	}
	bool outgoing_request_pending() const { return m_outgoing_request_pending; }
	u32 outgoing_request_id() const { return m_outgoing_request_id; }
	const std::array<u8, 32> &outgoing_called_digits() const
	{
		return m_outgoing_called_digits;
	}
	unsigned outgoing_called_digits_length() const
	{
		return m_outgoing_called_digits_length;
	}
	bool idle() const { return m_state == u8(state::idle); }
	bool incoming_service_queued() const
	{
		return m_state == u8(state::idle) &&
				m_incoming_service != u8(incoming_service::none);
	}
	bool incoming_service_completed() const
	{
		return m_incoming_service_completed;
	}
	bool awaiting_traffic_assignment() const
	{
		return m_state == u8(state::awaiting_traffic_assignment);
	}
	bool call_connected() const
	{
		return m_state == u8(state::incoming_call_active);
	}
	bool outgoing_call_connected() const
	{
		return m_mobile_originated_call &&
				m_state == u8(state::incoming_call_active);
	}
	bool outgoing_call_alerting() const
	{
		return m_mobile_originated_call &&
				m_state == u8(state::outgoing_call_alerting);
	}
	bool incoming_call_connected() const
	{
		return !m_mobile_originated_call && m_incoming_call_answered &&
				m_state == u8(state::incoming_call_active);
	}
	bool incoming_call_alerting() const
	{
		return !m_mobile_originated_call && m_call_alerting;
	}
	gsm::a5::algorithm cipher_algorithm() const
	{
		return gsm::a5::algorithm(m_cipher_algorithm);
	}
	const gsm::a5::key &cipher_key() const { return m_cipher_key; }
	bool cipher_active() const { return m_cipher_active; }
	bool cipher_command_pending() const { return m_cipher_command_pending; }

protected:
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	enum class state : u8
	{
		idle,
		awaiting_paging_response,
		awaiting_contention_resolution,
		awaiting_paging_contention_resolution,
		awaiting_authentication_request_acknowledgement,
		awaiting_authentication_response,
		awaiting_authentication_reject_acknowledgement,
		awaiting_location_update_accept_acknowledgement,
		awaiting_channel_release_acknowledgement,
		awaiting_cm_service_accept_acknowledgement,
		awaiting_cm_service_reject_acknowledgement,
		awaiting_outgoing_call_setup,
		awaiting_call_proceeding_acknowledgement,
		awaiting_outgoing_decision,
		awaiting_call_alerting_acknowledgement,
		awaiting_outgoing_connect_acknowledgement,
		outgoing_call_alerting,
		awaiting_network_disconnect_acknowledgement,
		awaiting_handset_release,
		awaiting_cipher_mode_command_acknowledgement,
		awaiting_mm_information_acknowledgement,
		awaiting_incoming_call_setup_acknowledgement,
		incoming_call_active,
		awaiting_traffic_assignment,
		awaiting_assignment_complete,
		awaiting_connect_acknowledgement,
		awaiting_call_release_acknowledgement,
		awaiting_release_complete,
		awaiting_sms_sapi3_establishment,
		awaiting_sms_cp_data_acknowledgement,
		awaiting_sms_handset_response,
		awaiting_sms_cp_ack_acknowledgement
	};

	void clear_pending_downlink();
	// Close the dedicated channel: every transaction that finishes on a
	// dedicated channel ends this way, so the ordering of the state change
	// against the queued release lives in one place.
	downlink_kind begin_channel_release();
	// Discard mobile-originated call state. Reached both by reset and by the
	// end of a call, so a field added here cannot be missed at one site.
	void clear_outgoing_call_state();
	downlink_kind apply_outgoing_decision();
	downlink_kind apply_outgoing_termination();
	void publish_call_alerting_output();
	void publish_call_active_output();
	void publish_release_waiting_output();
	void clear_dedicated_cipher();
	downlink_kind queue_downlink(
			downlink_kind kind, const u8 *information, unsigned length,
			u8 sapi = 0);
	TIMER_CALLBACK_MEMBER(outgoing_decision_timer);

	required_device<nokia_gsm_network_device> m_network;
	output_finder<> m_call_alerting_output;
	output_finder<> m_call_active_output;
	output_finder<> m_release_waiting_output;
	bool m_authentication_required = false;
	bool m_authentication_for_service = false;
	u8 m_cipher_algorithm = u8(gsm::a5::algorithm::a5_0);
	gsm::a5::key m_cipher_key{};
	bool m_cipher_key_valid = false;
	bool m_cipher_command_pending = false;
	bool m_cipher_active = false;
	u8 m_state = u8(state::idle);
	std::array<u8, maximum_layer3_length> m_established_layer3{};
	unsigned m_established_layer3_length = 0;
	u16 m_serving_arfcn = 1;
	std::array<u8, 8> m_registered_mobile_identity{};
	unsigned m_registered_mobile_identity_length = 0;
	bool m_release_completes_registration = false;
	bool m_call_alerting = false;
	bool m_release_waiting = false;
	bool m_release_complete_received = false;
	bool m_traffic_assignment_issued = false;
	bool m_mobile_originated_call = false;
	bool m_incoming_call_answered = false;
	u8 m_call_transaction = 0;
	bool m_outgoing_request_pending = false;
	u32 m_outgoing_request_id = 0;
	std::array<u8, 32> m_outgoing_called_digits{};
	unsigned m_outgoing_called_digits_length = 0;
	std::array<u8, 20> m_incoming_call_digits{};
	unsigned m_incoming_call_digits_length = 0;
	bool m_outgoing_decision_accepted = false;
	u8 m_outgoing_decision =
			u8(nokia_gsm_network_device::outgoing_call_outcome::connect);
	u32 m_outgoing_decision_request_id = 0;
	u32 m_outgoing_policy_request_id = 0;
	bool m_outgoing_termination_accepted = false;
	u32 m_outgoing_termination_request_id = 0;
	u8 m_outgoing_termination_cause = 0x10;
	unsigned m_outgoing_decision_delay_ms = 0;
	bool m_outgoing_fallback_enabled = true;
	emu_timer *m_outgoing_decision_timer = nullptr;
	u8 m_incoming_service = u8(incoming_service::none);
	u8 m_sms_delivery_index = 0;
	u8 m_sms_cp_transaction = 0;
	u8 m_sms_rp_reference = 0;
	bool m_sms_cp_data_acknowledged = false;
	bool m_sms_rp_acknowledged = false;
	bool m_incoming_service_completed = false;
	downlink_message m_pending_downlink;
};

DECLARE_DEVICE_TYPE(NOKIA_GSM_SESSION, nokia_gsm_session_device)

#endif // MAME_NOKIA_NOKIA_GSM_SESSION_H
