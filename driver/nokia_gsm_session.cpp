// license:BSD-3-Clause
// copyright-holders:Sandro Ronco, Gaz

#include "emu.h"
#include "emuopts.h"
#include "nokia_gsm_session.h"

#define LOG_GSM_SESSION (1U << 0)
#define VERBOSE (LOG_GSM_SESSION)
#include "logmacro.h"

DEFINE_DEVICE_TYPE(NOKIA_GSM_SESSION, nokia_gsm_session_device,
		"nokia_gsm_session", "Nokia DCT3 GSM Layer 3 session")

nokia_gsm_session_device::nokia_gsm_session_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_GSM_SESSION, tag, owner, clock),
	m_network(*this, "^gsm_network"),
	m_call_alerting_output(*this, "nokia_gsm_call_alerting"),
	m_call_active_output(*this, "nokia_gsm_call_active"),
	m_release_waiting_output(*this, "nokia_gsm_call_release_waiting_handset")
{
}

void nokia_gsm_session_device::device_start()
{
	m_outgoing_decision_timer =
			timer_alloc(FUNC(nokia_gsm_session_device::outgoing_decision_timer),
					this);
	m_no_reply_timer = timer_alloc(
			FUNC(nokia_gsm_session_device::no_reply_timer), this);
	save_item(NAME(m_state));
	save_item(NAME(m_cipher_algorithm));
	save_item(NAME(m_cipher_key));
	save_item(NAME(m_cipher_key_valid));
	save_item(NAME(m_cipher_command_pending));
	save_item(NAME(m_cipher_active));
	save_item(NAME(m_authentication_for_service));
	save_item(NAME(m_established_layer3));
	save_item(NAME(m_established_layer3_length));
	save_item(NAME(m_serving_arfcn));
	save_item(NAME(m_registered_mobile_identity));
	save_item(NAME(m_registered_mobile_identity_length));
	save_item(NAME(m_release_completes_registration));
	save_item(NAME(m_call_alerting));
	save_item(NAME(m_release_waiting));
	save_item(NAME(m_release_complete_received));
	save_item(NAME(m_traffic_assignment_issued));
	save_item(NAME(m_mobile_originated_call));
	save_item(NAME(m_second_mobile_originated_call));
	save_item(NAME(m_mobile_originated_sms));
	save_item(NAME(m_mobile_originated_supplementary));
	save_item(NAME(m_incoming_call_answered));
	save_item(NAME(m_dtmf_active));
	save_item(NAME(m_dtmf_digit));
	save_item(NAME(m_dtmf_start_count));
	save_item(NAME(m_dtmf_stop_count));
	save_item(NAME(m_call_held));
	save_item(NAME(m_call_hold_count));
	save_item(NAME(m_call_retrieve_count));
	save_item(NAME(m_multiparty_active));
	save_item(NAME(m_multiparty_held));
	save_item(NAME(m_multiparty_operation_count));
	save_item(NAME(m_waiting_call_queued));
	save_item(NAME(m_call_leg_transactions));
	save_item(NAME(m_call_leg_states));
	save_item(NAME(m_releasing_call_leg));
	save_item(NAME(m_call_transaction));
	save_item(NAME(m_outgoing_request_pending));
	save_item(NAME(m_outgoing_request_id));
	save_item(NAME(m_outgoing_called_digits));
	save_item(NAME(m_outgoing_called_digits_length));
	save_item(NAME(m_incoming_call_digits));
	save_item(NAME(m_incoming_call_digits_length));
	save_item(NAME(m_outgoing_decision_accepted));
	save_item(NAME(m_outgoing_decision));
	save_item(NAME(m_outgoing_decision_request_id));
	save_item(NAME(m_outgoing_policy_request_id));
	save_item(NAME(m_outgoing_termination_accepted));
	save_item(NAME(m_outgoing_termination_request_id));
	save_item(NAME(m_outgoing_termination_cause));
	save_item(NAME(m_incoming_service));
	save_item(NAME(m_sms_delivery_index));
	save_item(NAME(m_sms_cp_transaction));
	save_item(NAME(m_sms_rp_reference));
	save_item(NAME(m_sms_cp_data_acknowledged));
	save_item(NAME(m_sms_rp_acknowledged));
	save_item(NAME(m_sms_status_report_requested));
	save_item(NAME(m_sms_submit_message_reference));
	save_item(NAME(m_sms_submit_recipient));
	save_item(NAME(m_sms_submit_recipient_length));
	save_item(NAME(m_outgoing_sms_request_pending));
	save_item(NAME(m_outgoing_sms_request_id));
	save_item(NAME(m_outgoing_sms_decision_accepted));
	save_item(NAME(m_outgoing_sms_decision));
	save_item(NAME(m_outgoing_sms_alphabet));
	save_item(NAME(m_outgoing_sms_user_data));
	save_item(NAME(m_outgoing_sms_user_data_octets));
	save_item(NAME(m_outgoing_sms_user_data_length));
	save_item(NAME(m_ussd_request_pending));
	save_item(NAME(m_ussd_request_id));
	save_item(NAME(m_ussd_policy_request_id));
	save_item(NAME(m_ussd_transaction));
	save_item(NAME(m_ussd_invoke_id));
	save_item(NAME(m_ussd_data_coding_scheme));
	save_item(NAME(m_ussd_data));
	save_item(NAME(m_ussd_data_length));
	save_item(NAME(m_incoming_service_completed));
	save_item(NAME(m_incoming_call_forwarded));
	save_item(NAME(m_handover_target_arfcn));
	save_item(NAME(m_handover_reference));
	save_item(NAME(m_pending_downlink.kind));
	save_item(NAME(m_pending_downlink.sapi));
	save_item(NAME(m_pending_downlink.length));
	save_item(NAME(m_pending_downlink.data));
	machine().save().register_postload(
			save_prepost_delegate(
				FUNC(nokia_gsm_session_device::publish_call_alerting_output),
				this));
	machine().save().register_postload(
			save_prepost_delegate(
				FUNC(nokia_gsm_session_device::publish_call_active_output),
				this));
	machine().save().register_postload(
			save_prepost_delegate(
				FUNC(nokia_gsm_session_device::publish_release_waiting_output),
				this));
}

void nokia_gsm_session_device::device_reset()
{
	m_state = u8(state::idle);
	m_cipher_algorithm = u8(gsm::a5::algorithm::a5_0);
	m_cipher_key.fill(0);
	m_cipher_key_valid = false;
	m_cipher_command_pending = false;
	m_cipher_active = false;
	m_authentication_for_service = false;
	m_established_layer3.fill(0);
	m_established_layer3_length = 0;
	m_serving_arfcn = 1;
	m_registered_mobile_identity.fill(0);
	m_registered_mobile_identity_length = 0;
	m_release_completes_registration = false;
	m_call_alerting = false;
	m_release_waiting = false;
	m_release_complete_received = false;
	m_traffic_assignment_issued = false;
	m_mobile_originated_call = false;
	m_second_mobile_originated_call = false;
	m_mobile_originated_sms = false;
	m_mobile_originated_supplementary = false;
	m_incoming_call_answered = false;
	m_dtmf_active = false;
	m_dtmf_digit = 0;
	m_dtmf_start_count = 0;
	m_dtmf_stop_count = 0;
	m_call_held = false;
	m_call_hold_count = 0;
	m_call_retrieve_count = 0;
	m_multiparty_active = false;
	m_multiparty_held = false;
	m_multiparty_operation_count = 0;
	m_waiting_call_queued = false;
	m_call_leg_transactions.fill(0);
	m_call_leg_states.fill(u8(call_leg_state::inactive));
	m_releasing_call_leg = 0xff;
	m_call_transaction = 0;
	m_outgoing_request_pending = false;
	m_outgoing_request_id = 0;
	m_incoming_call_digits.fill(0);
	m_incoming_call_digits_length = 0;
	clear_outgoing_call_state();
	// Reset additionally restores the default decision. Clearing a call in
	// flight deliberately leaves the last decision in place, so this stays
	// outside the shared helper.
	m_outgoing_decision =
			u8(nokia_gsm_network_device::outgoing_call_outcome::connect);
	publish_call_alerting_output();
	publish_call_active_output();
	publish_release_waiting_output();
	m_incoming_service = u8(incoming_service::none);
	m_sms_delivery_index = 0;
	m_sms_cp_transaction = 0;
	m_sms_rp_reference = 0;
	m_sms_cp_data_acknowledged = false;
	m_sms_rp_acknowledged = false;
	m_sms_status_report_requested = false;
	m_sms_submit_message_reference = 0;
	m_sms_submit_recipient.fill(0);
	m_sms_submit_recipient_length = 0;
	m_outgoing_sms_request_pending = false;
	m_outgoing_sms_request_id = 0;
	m_outgoing_sms_decision_accepted = false;
	m_outgoing_sms_user_data.fill(0);
	m_outgoing_sms_user_data_octets = 0;
	m_outgoing_sms_user_data_length = 0;
	m_ussd_request_pending = false;
	m_ussd_request_id = 0;
	m_ussd_policy_request_id = 0;
	m_ussd_transaction = 0;
	m_ussd_invoke_id = 0;
	m_ussd_data_coding_scheme = 0;
	m_ussd_data.fill(0);
	m_ussd_data_length = 0;
	m_incoming_service_completed = false;
	m_incoming_call_forwarded = false;
	m_no_reply_timer->adjust(attotime::never);
	m_handover_target_arfcn = 0;
	m_handover_reference = 0;
	clear_pending_downlink();
}

void nokia_gsm_session_device::clear_dedicated_cipher()
{
	if (m_cipher_active)
		LOGMASKED(LOG_GSM_SESSION,
				"gsm_cipher: event=cleared algorithm=%u t=%.6f\n",
				m_cipher_algorithm, machine().time().as_double());
	m_cipher_active = false;
	m_cipher_command_pending = false;
}

void nokia_gsm_session_device::radio_link_failed()
{
	// A network-side LAPDm N200 failure retires only the dedicated-channel
	// transaction. The registered mobile identity and serving cell belong to
	// idle MM state and survive the link, just as they do after Channel Release.
	const auto identity = m_registered_mobile_identity;
	const unsigned identity_length = m_registered_mobile_identity_length;
	const u16 serving_arfcn = m_serving_arfcn;
	device_reset();
	m_registered_mobile_identity = identity;
	m_registered_mobile_identity_length = identity_length;
	m_serving_arfcn = serving_arfcn;
}

bool nokia_gsm_session_device::establish_layer3(
		const u8 *information, unsigned length, u16 serving_arfcn)
{
	if (length < 2 || length > maximum_layer3_length)
		return false;

	const bool location_update_request =
			m_state == u8(state::idle) &&
			(information[0] & 0x0f) == 0x05 &&
			(information[1] & 0x3f) == 0x08;
	const bool paging_response =
			m_state == u8(state::awaiting_paging_response) &&
			(information[0] & 0x0f) == 0x06 &&
			(information[1] & 0x3f) == 0x27;
	const bool cm_service_request =
			m_state == u8(state::idle) &&
			(information[0] & 0x0f) == 0x05 &&
			(information[1] & 0x3f) == 0x24 &&
			length >= 6 &&
			((information[2] & 0x0f) == 0x01 ||
				(information[2] & 0x0f) == 0x04 ||
				(information[2] & 0x0f) == 0x08);
	if (!location_update_request && !paging_response && !cm_service_request)
		return false;
	if (cm_service_request)
	{
		// GSM 04.08 9.2.9: both accepted mobile-originated services carry the
		// same classmark and mobile identity geometry. Their service type is
		// retained separately so SMS cannot accidentally enter call control.
		const unsigned classmark_length = information[3];
		const unsigned identity_length_offset = 4 + classmark_length;
		if (identity_length_offset >= length)
			return false;
		const unsigned identity_length = information[identity_length_offset];
		if (identity_length == 0 || identity_length > 8 ||
				identity_length_offset + 1 + identity_length > length)
			return false;
	}

	m_established_layer3.fill(0);
	std::copy_n(information, length, m_established_layer3.begin());
	m_established_layer3_length = length;
	m_serving_arfcn = serving_arfcn;
	clear_pending_downlink();
	m_release_completes_registration = location_update_request;
	m_mobile_originated_call = cm_service_request &&
			(information[2] & 0x0f) == 0x01;
	m_mobile_originated_sms = cm_service_request &&
			(information[2] & 0x0f) == 0x04;
	m_mobile_originated_supplementary = cm_service_request &&
			(information[2] & 0x0f) == 0x08;
	m_incoming_service_completed = false;
	m_call_transaction = 0;
	m_traffic_assignment_issued = false;
	m_state = u8(paging_response ?
			state::awaiting_paging_contention_resolution :
			state::awaiting_contention_resolution);
	return true;
}

bool nokia_gsm_session_device::queue_incoming_page(incoming_service service)
{
	if (m_state == u8(state::awaiting_paging_response))
		return true;
	if (m_state != u8(state::idle) || m_registered_mobile_identity_length != 8)
		return false;
	const bool continuing_queued_service =
			m_incoming_service_completed &&
			m_incoming_service == u8(service);
	m_incoming_service = u8(service);
	m_traffic_assignment_issued = false;
	m_incoming_call_answered = false;
	m_incoming_service_completed = false;
	if (!continuing_queued_service)
		m_sms_delivery_index = 0;
	m_state = u8(state::awaiting_paging_response);
	return true;
}

void nokia_gsm_session_device::cancel_queued_incoming_service()
{
	if (m_state != u8(state::idle) &&
			m_state != u8(state::awaiting_paging_response))
		return;
	m_state = u8(state::idle);
	m_incoming_service = u8(incoming_service::none);
	m_incoming_service_completed = false;
	m_incoming_call_digits.fill(0);
	m_incoming_call_digits_length = 0;
	clear_pending_downlink();
}

bool nokia_gsm_session_device::set_incoming_caller(
		const u8 *digits, unsigned length)
{
	if (length == 0 || length > m_incoming_call_digits.size() ||
			m_state != u8(state::idle))
		return false;
	std::copy_n(digits, length, m_incoming_call_digits.begin());
	m_incoming_call_digits_length = length;
	return true;
}

bool nokia_gsm_session_device::queue_waiting_call(
		const u8 *digits, unsigned length,
		bool malformed_bearer, bool duplicate)
{
	if (m_state != u8(state::incoming_call_active) ||
			(duplicate ? !m_waiting_call_queued : m_waiting_call_queued) ||
			m_pending_downlink.kind != u8(downlink_kind::none) ||
			length == 0 || length > 20)
		return false;
	// A waiting call is a second network-originated CC transaction on the
	// existing main link. It does not page or establish another RR connection.
	const auto setup = m_network->incoming_call_setup(
			digits, length, 0x10, malformed_bearer);
	if (!duplicate)
	{
		m_waiting_call_queued = true;
		m_call_leg_transactions[1] = setup.data[0];
		set_call_leg_state(1, call_leg_state::alerting);
	}
	LOGMASKED(LOG_GSM_SESSION,
			"gsm_session: waiting call SETUP transaction=%02x malformed=%u duplicate=%u t=%.6f\n",
			setup.data[0], malformed_bearer, duplicate,
			machine().time().as_double());
	return queue_downlink(downlink_kind::incoming_call_setup,
			setup.data.data(), setup.length) != downlink_kind::none;
}

bool nokia_gsm_session_device::incoming_service_admissible(
		incoming_service service) const
{
	return !incoming_service_diverted(service) &&
			(service != incoming_service::sms ||
			 m_network->incoming_sms_admissible(m_sms_delivery_index));
}

bool nokia_gsm_session_device::incoming_service_diverted(
		incoming_service service) const
{
	return service == incoming_service::call &&
			(m_network->speech_forwarding_active(
				nokia_gsm_network_device::forwarding_condition::unconditional) ||
			 (live_call_leg_count() != 0 && m_network->speech_forwarding_active(
				nokia_gsm_network_device::forwarding_condition::busy)));
}

nokia_gsm_session_device::downlink_kind
nokia_gsm_session_device::contention_resolution_delivered()
{
	if (m_state == u8(state::awaiting_paging_contention_resolution))
	{
		if (m_incoming_service != u8(incoming_service::none))
		{
			if (m_network->cipher_algorithm() !=
							gsm::a5::algorithm::a5_0 &&
					!m_cipher_key_valid)
			{
				const auto request = m_network->authentication_request();
				m_authentication_for_service = true;
				m_state =
						u8(state::awaiting_authentication_request_acknowledgement);
				return queue_downlink(downlink_kind::authentication_request,
						request.data(), request.size());
			}
			// Exercise the firmware's real DSP cipher-control publication.
			// A5/0 remains explicitly clear; A5/1 becomes pending only with
			// an authenticated live key context.
			const auto information = m_network->cipher_mode_command();
			m_cipher_algorithm = u8(m_network->cipher_algorithm());
			m_cipher_command_pending =
					m_network->cipher_algorithm() != gsm::a5::algorithm::a5_0 &&
					m_cipher_key_valid;
			m_state = u8(state::awaiting_cipher_mode_command_acknowledgement);
			return queue_downlink(downlink_kind::cipher_mode_command,
					information.data(), information.size());
		}
		return begin_channel_release();
	}

	if (m_state != u8(state::awaiting_contention_resolution))
		return downlink_kind::none;

	if ((m_established_layer3[0] & 0x0f) == 0x05 &&
			(m_established_layer3[1] & 0x3f) == 0x24)
	{
		if (m_mobile_originated_call &&
				m_network->configured_outgoing_call_outcome() ==
				nokia_gsm_network_device::outgoing_call_outcome::
						service_reject)
		{
			const auto reject = m_network->cm_service_reject();
			m_state = u8(state::awaiting_cm_service_reject_acknowledgement);
			return queue_downlink(downlink_kind::cm_service_reject,
					reject.data(), reject.size());
		}
		if (m_network->cipher_algorithm() !=
				gsm::a5::algorithm::a5_0)
		{
			if (!m_cipher_key_valid)
			{
				const auto request = m_network->authentication_request();
				m_authentication_for_service = true;
				m_state =
						u8(state::awaiting_authentication_request_acknowledgement);
				return queue_downlink(downlink_kind::authentication_request,
						request.data(), request.size());
			}
			const auto command = m_network->cipher_mode_command();
			m_cipher_algorithm = u8(m_network->cipher_algorithm());
			m_cipher_command_pending = true;
			m_state = u8(state::awaiting_cipher_mode_command_acknowledgement);
			return queue_downlink(downlink_kind::cipher_mode_command,
					command.data(), command.size());
		}
		const auto accept = m_network->cm_service_accept();
		m_state = u8(state::awaiting_cm_service_accept_acknowledgement);
		return queue_downlink(downlink_kind::cm_service_accept,
				accept.data(), accept.size());
	}

	if (m_authentication_required)
	{
		m_cipher_key.fill(0);
		m_cipher_key_valid = false;
		m_authentication_for_service = false;
		const auto request = m_network->authentication_request();
		m_state = u8(state::awaiting_authentication_request_acknowledgement);
		return queue_downlink(downlink_kind::authentication_request,
				request.data(), request.size());
	}

	const auto accept = m_network->location_update_accept(
			m_established_layer3.data(), m_established_layer3_length,
			m_serving_arfcn);
	m_state = u8(state::awaiting_location_update_accept_acknowledgement);
	return queue_downlink(downlink_kind::location_update_accept,
			accept.data(), accept.size());
}

std::array<u8, 24> nokia_gsm_session_device::paging_request() const
{
	return m_network->paging_request(
			m_registered_mobile_identity.data(),
			m_registered_mobile_identity_length);
}

nokia_gsm_session_device::downlink_kind
nokia_gsm_session_device::downlink_acknowledged()
{
	if (m_state == u8(state::awaiting_cm_service_accept_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::cm_service_accept))
	{
		clear_pending_downlink();
		m_state = u8(m_mobile_originated_sms ?
				state::awaiting_mobile_sms_sapi3_establishment :
				m_mobile_originated_supplementary ?
				state::awaiting_supplementary_request :
				state::awaiting_outgoing_call_setup);
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_second_cm_service_accept_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::cm_service_accept))
	{
		clear_pending_downlink();
		m_state = u8(state::awaiting_second_outgoing_call_setup);
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_cm_service_reject_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::cm_service_reject))
	{
		return begin_channel_release();
	}

	if (m_state == u8(state::awaiting_supplementary_release_acknowledgement) &&
			m_pending_downlink.kind ==
					u8(downlink_kind::supplementary_release_complete))
	{
		return begin_channel_release();
	}
	if (m_state == u8(state::awaiting_supplementary_facility_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::supplementary_facility))
	{
		clear_pending_downlink();
		m_state = u8(state::awaiting_supplementary_response);
		return downlink_kind::none;
	}
	if (m_state == u8(state::awaiting_network_ussd_register_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::supplementary_register))
	{
		clear_pending_downlink();
		m_state = u8(state::awaiting_network_ussd_response);
		return downlink_kind::none;
	}
	if (m_state == u8(state::awaiting_network_ussd_release_acknowledgement) &&
			m_pending_downlink.kind ==
				u8(downlink_kind::supplementary_release_complete))
	{
		return begin_channel_release();
	}

	if (m_state == u8(state::awaiting_call_proceeding_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::call_proceeding))
	{
		if (m_outgoing_decision_accepted)
			return apply_outgoing_decision();
		clear_pending_downlink();
		m_state = u8(state::awaiting_outgoing_decision);
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_call_alerting_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::call_alerting))
	{
		if (nokia_gsm_network_device::outgoing_call_outcome(
					m_outgoing_decision) ==
				nokia_gsm_network_device::outgoing_call_outcome::no_answer)
		{
			clear_pending_downlink();
			m_state = u8(state::outgoing_call_alerting);
			if (m_outgoing_termination_accepted)
				return apply_outgoing_termination();
			return downlink_kind::none;
		}
		const auto connect = m_network->call_connect(m_call_transaction);
		m_state = u8(state::awaiting_outgoing_connect_acknowledgement);
		return queue_downlink(downlink_kind::call_connect,
				connect.data(), connect.size());
	}

	if (m_state == u8(state::awaiting_network_disconnect_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::call_disconnect))
	{
		clear_pending_downlink();
		m_state = u8(state::awaiting_handset_release);
		m_release_waiting = true;
		publish_release_waiting_output();
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_outgoing_connect_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::call_connect))
	{
		clear_pending_downlink();
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_authentication_request_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::authentication_request))
	{
		clear_pending_downlink();
		m_state = u8(state::awaiting_authentication_response);
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_authentication_reject_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::authentication_reject))
	{
		const auto release = m_network->channel_release();
		m_release_completes_registration = false;
		m_state = u8(state::awaiting_channel_release_acknowledgement);
		return queue_downlink(downlink_kind::channel_release,
				release.data(), release.size());
	}

	if (m_state == u8(state::awaiting_location_update_accept_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::location_update_accept))
	{
		return begin_channel_release();
	}

	if (m_state == u8(state::awaiting_incoming_call_setup_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::incoming_call_setup))
	{
		clear_pending_downlink();
		m_state = u8(state::incoming_call_active);
		publish_call_active_output();
		return downlink_kind::none;
	}

	if (m_state == u8(state::incoming_call_active) && m_waiting_call_queued &&
			m_pending_downlink.kind == u8(downlink_kind::incoming_call_setup))
	{
		clear_pending_downlink();
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_traffic_assignment) &&
			m_pending_downlink.kind == u8(downlink_kind::traffic_assignment))
	{
		// The mobile may acknowledge this I frame before locally releasing the
		// old SDCCH. Assignment completion itself belongs to the new link.
		clear_pending_downlink();
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_cipher_mode_command_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::cipher_mode_command))
	{
		// Supply deterministic network time before entering call control or
		// SAPI-3 SMS, matching the ordering expected by this firmware.
		const auto information = m_network->mm_information();
		m_state = u8(state::awaiting_mm_information_acknowledgement);
		return queue_downlink(downlink_kind::mm_information,
				information.data(), information.size());
	}

	if (m_state == u8(state::awaiting_mm_information_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::mm_information))
	{
		if (m_incoming_service == u8(incoming_service::call))
		{
			static constexpr std::array<u8, 7> fixture_digits =
					{ 5, 5, 5, 1, 2, 3, 4 };
			const u8 *const digits = m_incoming_call_digits_length ?
					m_incoming_call_digits.data() : fixture_digits.data();
			const unsigned digit_count = m_incoming_call_digits_length ?
					m_incoming_call_digits_length : fixture_digits.size();
			const auto setup = m_network->incoming_call_setup(digits, digit_count);
			m_call_transaction = setup.data[0];
			m_call_leg_transactions[0] = setup.data[0];
			set_call_leg_state(0, call_leg_state::alerting);
			m_state = u8(state::awaiting_incoming_call_setup_acknowledgement);
			return queue_downlink(downlink_kind::incoming_call_setup,
					setup.data.data(), setup.length);
		}
		if (m_incoming_service == u8(incoming_service::sms) ||
				m_incoming_service == u8(incoming_service::status_report) ||
				m_incoming_service == u8(incoming_service::smart_message))
		{
			clear_pending_downlink();
			m_state = u8(state::awaiting_sms_sapi3_establishment);
			return queue_downlink(downlink_kind::sapi3_establishment,
					nullptr, 0, 3);
		}
		if (m_incoming_service == u8(incoming_service::ussd_request) ||
				m_incoming_service == u8(incoming_service::ussd_notification))
		{
			const bool notification = m_incoming_service ==
					u8(incoming_service::ussd_notification);
			const auto request = notification ?
					(m_network->host_incoming_ussd_pending() ?
							m_network->host_incoming_ussd() :
							gsm::ss::network_unstructured_uss_notify(
									0x0b, 1, "Nokia test network")) :
					gsm::ss::network_unstructured_uss_request(
							0x0b, 1, "Enter reply");
			m_state = u8(state::awaiting_network_ussd_register_acknowledgement);
			LOGMASKED(LOG_GSM_SESSION,
					"gsm_ss: network_initiated operation=%s transaction=0b "
					"invoke=1 dcs=0f t=%.6f\n",
					notification ? "notify" : "request",
					machine().time().as_double());
			return queue_downlink(downlink_kind::supplementary_register,
					request.data.data(), request.length);
		}
		if (m_mobile_originated_call)
		{
			const auto accept = m_network->cm_service_accept();
			m_state = u8(state::awaiting_cm_service_accept_acknowledgement);
			return queue_downlink(downlink_kind::cm_service_accept,
					accept.data(), accept.size());
		}
	}

	if (m_state == u8(state::awaiting_sms_sapi3_establishment) &&
			m_pending_downlink.kind == u8(downlink_kind::sapi3_establishment))
	{
		m_state = u8(state::awaiting_sms_cp_data_acknowledgement);
		if (m_incoming_service == u8(incoming_service::smart_message))
		{
			const auto data = m_network->incoming_smart_message_cp_data(
					m_sms_delivery_index);
			if (data.length >= 5)
			{
				m_sms_cp_transaction = data.data[0];
				m_sms_rp_reference = data.data[4];
			}
			m_sms_cp_data_acknowledged = false;
			m_sms_rp_acknowledged = false;
			return queue_downlink(downlink_kind::incoming_sms_cp_data,
					data.data.data(), data.length, 3);
		}
		const auto data = m_incoming_service == u8(incoming_service::status_report) ?
				m_network->sms_status_report_cp_data(
						m_sms_submit_message_reference,
						m_sms_submit_recipient.data(),
						m_sms_submit_recipient_length) :
				m_network->incoming_sms_cp_data(m_sms_delivery_index);
		if (data.length >= 5)
		{
			m_sms_cp_transaction = data.data[0];
			m_sms_rp_reference = data.data[4];
		}
		m_sms_cp_data_acknowledged = false;
		m_sms_rp_acknowledged = false;
		return queue_downlink(downlink_kind::incoming_sms_cp_data,
				data.data.data(), data.length, 3);
	}

	if (m_state == u8(state::awaiting_sms_cp_data_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::incoming_sms_cp_data))
	{
		clear_pending_downlink();
		m_state = u8(state::awaiting_sms_handset_response);
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_sms_cp_ack_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::sms_cp_ack))
	{
		return begin_channel_release();
	}

	if (m_state == u8(state::awaiting_mobile_sms_cp_ack_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::sms_cp_ack))
	{
		const auto outcome = m_outgoing_sms_fallback_enabled ?
				m_network->configured_outgoing_sms_outcome() :
				nokia_gsm_network_device::outgoing_sms_outcome(
						m_outgoing_sms_decision);
		if (!m_outgoing_sms_fallback_enabled &&
				!m_outgoing_sms_decision_accepted)
		{
			clear_pending_downlink();
			m_state = u8(state::awaiting_mobile_sms_host_decision);
			return downlink_kind::none;
		}
		if (outcome ==
				nokia_gsm_network_device::outgoing_sms_outcome::rp_silence)
		{
			clear_pending_downlink();
			m_state = u8(state::awaiting_mobile_sms_timeout);
			return downlink_kind::none;
		}
		m_state = u8(state::awaiting_mobile_sms_final_cp_ack);
		if (outcome ==
				nokia_gsm_network_device::outgoing_sms_outcome::rp_error)
		{
			const auto error = m_network->sms_rp_error(
					m_sms_cp_transaction, m_sms_rp_reference, 21);
			return queue_downlink(downlink_kind::outgoing_sms_rp_error,
					error.data(), error.size(), 3);
		}
		const auto acknowledge = m_network->sms_rp_ack(
				m_sms_cp_transaction, m_sms_rp_reference);
		m_sms_rp_acknowledged = true;
		return queue_downlink(downlink_kind::outgoing_sms_rp_ack,
				acknowledge.data(), acknowledge.size(), 3);
	}

	if (m_state == u8(state::awaiting_connect_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::connect_acknowledge))
	{
		clear_pending_downlink();
		m_state = u8(state::incoming_call_active);
		publish_call_active_output();
		return downlink_kind::none;
	}

	if (m_state == u8(state::incoming_call_active) &&
			(m_pending_downlink.kind ==
					u8(downlink_kind::start_dtmf_acknowledge) ||
			 m_pending_downlink.kind ==
					u8(downlink_kind::stop_dtmf_acknowledge) ||
			 m_pending_downlink.kind ==
					u8(downlink_kind::call_hold_acknowledge) ||
			 m_pending_downlink.kind ==
					u8(downlink_kind::call_retrieve_acknowledge) ||
			 m_pending_downlink.kind ==
					u8(downlink_kind::multiparty_facility_result)))
	{
		clear_pending_downlink();
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_call_release_acknowledgement) &&
			(m_pending_downlink.kind == u8(downlink_kind::call_release) ||
				m_pending_downlink.kind == u8(downlink_kind::release_complete)))
	{
		if (m_releasing_call_leg < maximum_call_legs)
		{
			set_call_leg_state(m_releasing_call_leg, call_leg_state::inactive);
			if (live_call_leg_count() < 2)
			{
				m_multiparty_active = false;
				m_multiparty_held = false;
			}
			m_releasing_call_leg = 0xff;
			if (live_call_leg_count() != 0)
			{
				clear_pending_downlink();
				m_state = u8(state::incoming_call_active);
				publish_call_active_output();
				return downlink_kind::none;
			}
		}
		// Close the dedicated RR channel after RELEASE is acknowledged. The
		// handset emits CC Release Complete while that release is in flight.
		return begin_channel_release();
	}

	if (m_state == u8(state::awaiting_channel_release_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::channel_release))
	{
		if (m_release_completes_registration &&
				m_established_layer3_length >= 18 &&
				m_established_layer3[9] == 8)
		{
			std::copy_n(m_established_layer3.begin() + 10,
					m_registered_mobile_identity.size(),
					m_registered_mobile_identity.begin());
			m_registered_mobile_identity_length =
					m_registered_mobile_identity.size();
		}
		clear_pending_downlink();
		m_incoming_service_completed =
				m_incoming_service != u8(incoming_service::none);
		bool more_ordinary_sms = false;
		if (m_sms_rp_acknowledged &&
				m_incoming_service == u8(incoming_service::sms) &&
				m_sms_delivery_index + 1 <
						m_network->incoming_sms_message_count())
		{
			const auto next = m_network->incoming_sms_cp_data(
					m_sms_delivery_index + 1);
			const auto current = m_network->incoming_sms_cp_data(
					m_sms_delivery_index);
			// GSM 04.11 RP-MR correlates an RP transaction. A queued replay
			// must repeat both the reference and the complete CP/RP payload;
			// RP-MR alone may be reused by a later distinct transaction.
			const bool exact_replay =
					next.length == current.length &&
					next.length >= 5 &&
					next.data[4] == m_sms_rp_reference &&
					std::equal(
							next.data.begin(), next.data.begin() + next.length,
							current.data.begin());
			more_ordinary_sms =
					next.length >= 5 &&
					!exact_replay;
			if (!more_ordinary_sms)
				LOGMASKED(LOG_GSM_SESSION,
						"gsm_sms: duplicate queued rp_reference=%02x "
						"suppressed t=%.6f\n",
						m_sms_rp_reference,
						machine().time().as_double());
		}
		const bool more_sms_deliveries =
				m_sms_rp_acknowledged &&
				((m_incoming_service == u8(incoming_service::smart_message) &&
				m_sms_delivery_index + 1 <
						m_network->incoming_smart_message_part_count()) ||
				more_ordinary_sms);
		if (more_sms_deliveries)
			++m_sms_delivery_index;
		if (m_incoming_service == u8(incoming_service::call))
		{
			if (!m_release_complete_received)
			{
				m_state = u8(state::awaiting_release_complete);
				return downlink_kind::none;
			}
			m_release_complete_received = false;
		}
		m_established_layer3.fill(0);
		m_established_layer3_length = 0;
		m_release_completes_registration = false;
		m_mobile_originated_call = false;
		const bool queue_status_report = m_mobile_originated_sms &&
				m_sms_status_report_requested && m_sms_rp_acknowledged;
		m_mobile_originated_sms = false;
		m_outgoing_sms_request_pending = false;
		m_outgoing_sms_decision_accepted = false;
		m_mobile_originated_supplementary = false;
		m_ussd_request_pending = false;
		m_call_transaction = 0;
		m_outgoing_request_pending = false;
		clear_outgoing_call_state();
		if (queue_status_report)
		{
			m_incoming_service = u8(incoming_service::status_report);
			m_incoming_service_completed = false;
		}
		else if (!more_sms_deliveries)
		{
			m_incoming_service = u8(incoming_service::none);
			m_incoming_call_digits_length = 0;
			m_sms_delivery_index = 0;
		}
		m_sms_cp_transaction = 0;
		m_sms_rp_reference = 0;
		m_sms_cp_data_acknowledged = false;
		m_sms_rp_acknowledged = false;
		if (!queue_status_report &&
				m_incoming_service != u8(incoming_service::status_report))
		{
			m_sms_status_report_requested = false;
			m_sms_submit_recipient_length = 0;
		}
		m_state = u8(state::idle);
		publish_call_active_output();
		clear_dedicated_cipher();
		return downlink_kind::release_complete;
	}

	return downlink_kind::none;
}

nokia_gsm_session_device::downlink_kind
nokia_gsm_session_device::receive_layer3(
		u8 sapi, const u8 *information, unsigned length)
{
	if (length < 2)
		return downlink_kind::none;

	const u8 protocol_discriminator = information[0] & 0x0f;
	const u8 message_type = information[1] & 0x3f;
	if (sapi == 0 && m_state == u8(state::awaiting_handover_result) &&
			protocol_discriminator == 0x06 && message_type == 0x2c)
	{
		clear_pending_downlink();
		m_serving_arfcn = m_handover_target_arfcn;
		m_handover_target_arfcn = 0;
		m_handover_reference = 0;
		m_state = u8(state::incoming_call_active);
		LOGMASKED(LOG_GSM_SESSION,
				"gsm_session: handover complete serving=%u t=%.6f\n",
				m_serving_arfcn, machine().time().as_double());
		return downlink_kind::none;
	}
	if (sapi == 0 && m_state == u8(state::awaiting_handover_result) &&
			protocol_discriminator == 0x06 && message_type == 0x28)
	{
		clear_pending_downlink();
		m_handover_target_arfcn = 0;
		m_handover_reference = 0;
		m_state = u8(state::incoming_call_active);
		LOGMASKED(LOG_GSM_SESSION,
				"gsm_session: handover failure serving=%u t=%.6f\n",
				m_serving_arfcn, machine().time().as_double());
		return downlink_kind::none;
	}
	if (sapi == 0 && m_state == u8(state::awaiting_supplementary_request) &&
			protocol_discriminator == 0x0b && message_type == 0x3b)
	{
		const gsm::ss::request request =
				gsm::ss::parse_register(information, length);
		if (!request.valid)
			return downlink_kind::none;
		gsm::ss::message response;
		nokia_gsm_network_device::forwarding_condition forwarding_condition;
		const bool forwarding_service = [&]() {
			switch (request.service_code)
			{
			case 0x21:
				forwarding_condition = nokia_gsm_network_device::
						forwarding_condition::unconditional;
				return true;
			case 0x29:
				forwarding_condition = nokia_gsm_network_device::
						forwarding_condition::busy;
				return true;
			case 0x2a:
				forwarding_condition = nokia_gsm_network_device::
						forwarding_condition::no_reply;
				return true;
			case 0x2b:
				forwarding_condition = nokia_gsm_network_device::
						forwarding_condition::not_reachable;
				return true;
			default:
				return false;
			}
		}();
		if (forwarding_service &&
				request.operation_code == gsm::ss::operation::interrogate_ss)
		{
			const bool active = m_network->forwarding_active(forwarding_condition);
			response = gsm::ss::interrogate_result(request,
					m_network->forwarding_registered(forwarding_condition), active,
					m_network->forwarding_number(forwarding_condition).data(),
					m_network->forwarding_number_length(forwarding_condition),
					m_network->forwarding_basic_service(forwarding_condition),
					m_network->forwarding_basic_service_code(forwarding_condition),
					m_network->forwarding_no_reply_time(forwarding_condition));
			LOGMASKED(LOG_GSM_SESSION,
					"gsm_ss: request=interrogate transaction=%02x invoke=%u "
					"service=%02x active=%u t=%.6f\n",
					request.transaction, request.invoke_id, request.service_code,
					active ? 1 : 0,
					machine().time().as_double());
		}
		else if (request.operation_code ==
				gsm::ss::operation::process_uss_request)
		{
			if (!m_ussd_fallback_enabled)
			{
				m_ussd_request_pending = true;
				m_ussd_request_id = ++m_ussd_policy_request_id;
				m_ussd_transaction = request.transaction;
				m_ussd_invoke_id = request.invoke_id;
				m_ussd_data_coding_scheme = request.data_coding_scheme;
				m_ussd_data.fill(0);
				std::copy_n(request.ussd.data(), request.ussd_length,
						m_ussd_data.begin());
				m_ussd_data_length = u8(request.ussd_length);
				m_state = u8(state::awaiting_mobile_ussd_host_response);
				LOGMASKED(LOG_GSM_SESSION,
						"gsm_ss: request=ussd_host id=%u transaction=%02x invoke=%u dcs=%02x packed_length=%u t=%.6f\n",
						m_ussd_request_id, request.transaction, request.invoke_id,
						request.data_coding_scheme, request.ussd_length,
						machine().time().as_double());
				return downlink_kind::none;
			}
			const auto outcome = m_network->configured_ussd_outcome();
			switch (outcome)
			{
			case nokia_gsm_network_device::ussd_outcome::success:
				response = gsm::ss::process_uss_request_result(
						request, "Nokia test network");
				break;
			case nokia_gsm_network_device::ussd_outcome::return_error:
				response = gsm::ss::error_result(request, 0x22);
				break;
			case nokia_gsm_network_device::ussd_outcome::reject:
				response = gsm::ss::reject_result(request, 0x00);
				break;
			case nokia_gsm_network_device::ussd_outcome::silence:
				break;
			case nokia_gsm_network_device::ussd_outcome::continuation_request:
				response = gsm::ss::unstructured_uss_request(
						request, request.invoke_id + 1, "Enter reply");
				break;
			}
			LOGMASKED(LOG_GSM_SESSION,
					"gsm_ss: request=ussd transaction=%02x invoke=%u "
					"dcs=%02x packed_length=%u outcome=%u t=%.6f\n",
					request.transaction, request.invoke_id,
					request.data_coding_scheme, request.ussd_length,
					unsigned(outcome),
					machine().time().as_double());
		}
		else if (forwarding_service &&
				request.operation_code == gsm::ss::operation::register_ss)
		{
			m_network->register_forwarding(forwarding_condition,
					request.forwarded_number.data(),
					request.forwarded_number_length, request.basic_service,
					request.basic_service_code, request.no_reply_condition_time);
			response = gsm::ss::forwarding_info_result(request, true, true,
					m_network->forwarding_number(forwarding_condition).data(),
					m_network->forwarding_number_length(forwarding_condition),
					m_network->forwarding_basic_service(forwarding_condition),
					m_network->forwarding_basic_service_code(forwarding_condition),
					m_network->forwarding_no_reply_time(forwarding_condition));
			LOGMASKED(LOG_GSM_SESSION,
					"gsm_ss: request=register transaction=%02x invoke=%u "
					"service=%02x number_length=%u active=1 t=%.6f\n",
					request.transaction, request.invoke_id, request.service_code,
					request.forwarded_number_length,
					machine().time().as_double());
		}
		else if (forwarding_service &&
				request.operation_code == gsm::ss::operation::deactivate_ss)
		{
			m_network->deactivate_forwarding(forwarding_condition);
			response = gsm::ss::forwarding_info_result(request,
					m_network->forwarding_registered(forwarding_condition), false,
					m_network->forwarding_number(forwarding_condition).data(),
					m_network->forwarding_number_length(forwarding_condition),
					m_network->forwarding_basic_service(forwarding_condition),
					m_network->forwarding_basic_service_code(forwarding_condition),
					m_network->forwarding_no_reply_time(forwarding_condition));
			LOGMASKED(LOG_GSM_SESSION,
					"gsm_ss: request=deactivate transaction=%02x invoke=%u "
					"service=%02x active=0 t=%.6f\n",
					request.transaction, request.invoke_id, request.service_code,
					machine().time().as_double());
		}
		else if (forwarding_service &&
				request.operation_code == gsm::ss::operation::activate_ss)
		{
			if (m_network->activate_forwarding(forwarding_condition))
				response = gsm::ss::forwarding_info_result(request, true, true,
						m_network->forwarding_number(forwarding_condition).data(),
						m_network->forwarding_number_length(forwarding_condition),
						m_network->forwarding_basic_service(forwarding_condition),
						m_network->forwarding_basic_service_code(forwarding_condition),
						m_network->forwarding_no_reply_time(forwarding_condition));
			else
				response = gsm::ss::error_result(request, 0x11);
			LOGMASKED(LOG_GSM_SESSION,
					"gsm_ss: request=activate transaction=%02x invoke=%u "
					"service=%02x registered=%u active=%u t=%.6f\n",
					request.transaction, request.invoke_id, request.service_code,
					m_network->forwarding_registered(forwarding_condition) ? 1 : 0,
					m_network->forwarding_active(forwarding_condition) ? 1 : 0,
					machine().time().as_double());
		}
		else if (forwarding_service &&
				request.operation_code == gsm::ss::operation::erase_ss)
		{
			m_network->erase_forwarding(forwarding_condition);
			response = gsm::ss::forwarding_info_result(request, false, false,
					nullptr, 0);
			LOGMASKED(LOG_GSM_SESSION,
					"gsm_ss: request=erase transaction=%02x invoke=%u "
					"service=%02x registered=0 active=0 t=%.6f\n",
					request.transaction, request.invoke_id, request.service_code,
					machine().time().as_double());
		}
		else if (request.operation_code !=
				gsm::ss::operation::process_uss_request)
		{
			response = gsm::ss::error_result(request, 0x10);
			LOGMASKED(LOG_GSM_SESSION,
					"gsm_ss: request=unsupported transaction=%02x invoke=%u "
					"operation=%02x service=%02x error=10 t=%.6f\n",
					request.transaction, request.invoke_id,
					unsigned(request.operation_code), request.service_code,
					machine().time().as_double());
		}
		else
			return downlink_kind::none;
		if (!response.length)
			return downlink_kind::none;
		const bool continued = m_network->configured_ussd_outcome() ==
				nokia_gsm_network_device::ussd_outcome::continuation_request &&
				request.operation_code == gsm::ss::operation::process_uss_request;
		m_state = u8(continued ?
				state::awaiting_supplementary_facility_acknowledgement :
				state::awaiting_supplementary_release_acknowledgement);
		return queue_downlink(continued ? downlink_kind::supplementary_facility :
				downlink_kind::supplementary_release_complete,
				response.data.data(), response.length);
	}
	if (sapi == 0 && m_state == u8(state::awaiting_supplementary_response) &&
			protocol_discriminator == 0x0b)
	{
		std::string information_hex;
		for (unsigned index = 0; index < length; ++index)
			information_hex += util::string_format("%02x", information[index]);
		LOGMASKED(LOG_GSM_SESSION,
				"gsm_ss: continued handset_response message=%02x length=%u "
				"data=%s t=%.6f\n", message_type, length,
				information_hex.c_str(),
				machine().time().as_double());
		if (message_type == 0x2a)
			return begin_channel_release();
		return downlink_kind::none;
	}
	if (sapi == 0 && m_state == u8(state::awaiting_network_ussd_response) &&
			protocol_discriminator == 0x0b)
	{
		std::string information_hex;
		for (unsigned index = 0; index < length; ++index)
			information_hex += util::string_format("%02x", information[index]);
		LOGMASKED(LOG_GSM_SESSION,
				"gsm_ss: network_initiated handset_response message=%02x "
				"length=%u data=%s t=%.6f\n", message_type, length,
				information_hex.c_str(), machine().time().as_double());
		if (message_type == 0x3a)
		{
			const auto release = gsm::ss::network_release_complete(0x0b);
			m_state = u8(state::awaiting_network_ussd_release_acknowledgement);
			return queue_downlink(downlink_kind::supplementary_release_complete,
					release.data.data(), release.length);
		}
		if (message_type == 0x2a)
			return begin_channel_release();
		return downlink_kind::none;
	}
	if (sapi == 0 && m_state == u8(state::incoming_call_active) &&
			protocol_discriminator == 0x05 && message_type == 0x24 &&
			live_call_leg_count() == 1)
	{
		// GSM 04.08 permits a new CM service transaction on an existing
		// dedicated connection. A handset can use it after HOLD when the user
		// starts another call; no paging, RACH or second RR assignment occurs.
		m_second_mobile_originated_call = true;
		LOGMASKED(LOG_GSM_SESSION,
				"gsm_session: second outgoing CM service request transaction=%02x "
				"live_legs=%u t=%.6f\n",
				information[0], live_call_leg_count(),
				machine().time().as_double());
		const auto accept = m_network->cm_service_accept();
		m_state = u8(state::awaiting_second_cm_service_accept_acknowledgement);
		return queue_downlink(downlink_kind::cm_service_accept,
				accept.data(), accept.size());
	}
	if (sapi == 0 && m_state == u8(state::incoming_call_active) &&
			protocol_discriminator == 0x03 && message_type == 0x3a)
	{
		const gsm::ss::request request =
				gsm::ss::parse_call_related_facility(information, length);
		if (!request.valid)
			return downlink_kind::none;
		const int selected_leg = call_leg_index(request.transaction);
		bool accepted = false;
		switch (request.operation_code)
		{
		case gsm::ss::operation::build_mpty:
			accepted = !m_multiparty_active && live_call_leg_count() == 2 &&
					std::count(m_call_leg_states.begin(), m_call_leg_states.end(),
						u8(call_leg_state::active)) == 1 &&
					std::count(m_call_leg_states.begin(), m_call_leg_states.end(),
						u8(call_leg_state::held)) == 1;
			if (accepted)
			{
				for (unsigned leg = 0; leg < maximum_call_legs; ++leg)
					set_call_leg_state(leg, call_leg_state::active);
				m_multiparty_active = true;
				m_multiparty_held = false;
			}
			break;
		case gsm::ss::operation::hold_mpty:
			accepted = m_multiparty_active && !m_multiparty_held;
			if (accepted)
			{
				for (unsigned leg = 0; leg < maximum_call_legs; ++leg)
					set_call_leg_state(leg, call_leg_state::held);
				m_multiparty_held = true;
			}
			break;
		case gsm::ss::operation::retrieve_mpty:
			accepted = m_multiparty_active && m_multiparty_held;
			if (accepted)
			{
				for (unsigned leg = 0; leg < maximum_call_legs; ++leg)
					set_call_leg_state(leg, call_leg_state::active);
				m_multiparty_held = false;
			}
			break;
		case gsm::ss::operation::split_mpty:
			accepted = m_multiparty_active && selected_leg >= 0;
			if (accepted)
			{
				for (unsigned leg = 0; leg < maximum_call_legs; ++leg)
					set_call_leg_state(leg,
							int(leg) == selected_leg ? call_leg_state::active :
							call_leg_state::held);
				m_multiparty_active = false;
				m_multiparty_held = false;
			}
			break;
		default:
			break;
		}
		if (accepted)
			++m_multiparty_operation_count;
		LOGMASKED(LOG_GSM_SESSION,
				"gsm_session: multiparty operation=%02x transaction=%02x "
				"accepted=%u active=%u held=%u count=%u t=%.6f\n",
				unsigned(request.operation_code), request.transaction,
				accepted ? 1 : 0, m_multiparty_active ? 1 : 0,
				m_multiparty_held ? 1 : 0, m_multiparty_operation_count,
				machine().time().as_double());
		const gsm::ss::message response = accepted ?
				gsm::ss::call_related_result(request) :
				gsm::ss::call_related_error(request, 0x14);
		return queue_downlink(downlink_kind::multiparty_facility_result,
				response.data.data(), response.length);
	}
	if (sapi == 0 && protocol_discriminator == 0x06 &&
			message_type == 0x32)
	{
		if (gsm::a5::activation_allowed(
				gsm::a5::algorithm(m_cipher_algorithm),
				m_cipher_key_valid, m_cipher_command_pending, sapi,
				information, length))
		{
			m_cipher_active = true;
			m_cipher_command_pending = false;
			LOGMASKED(LOG_GSM_SESSION,
					"gsm_cipher: event=activated algorithm=%u t=%.6f\n",
					m_cipher_algorithm, machine().time().as_double());
		}
		return downlink_kind::none;
	}
	if (sapi == 0 &&
			(m_state == u8(state::awaiting_outgoing_call_setup) ||
			 m_state == u8(state::awaiting_second_outgoing_call_setup)) &&
			protocol_discriminator == 0x03 && message_type == 0x05)
	{
		const bool second_call =
				m_state == u8(state::awaiting_second_outgoing_call_setup);
		bool speech_bearer = false;
		bool called_party = false;
		for (unsigned offset = 2; offset < length;)
		{
			const u8 identifier = information[offset];
			if (BIT(identifier, 7))
			{
				++offset;
				continue;
			}
			if (offset + 1 >= length)
				return downlink_kind::none;
			const unsigned value_length = information[offset + 1];
			if (offset + 2 + value_length > length)
				return downlink_kind::none;
			if (identifier == 0x04 && value_length >= 1)
				speech_bearer = (information[offset + 2] & 0x07) == 0x00;
			else if (identifier == 0x5e && value_length >= 2)
			{
				called_party = true;
				m_outgoing_called_digits.fill(0);
				m_outgoing_called_digits_length = 0;
				for (unsigned index = offset + 3;
						index < offset + 2 + value_length; ++index)
				{
					for (const u8 digit :
							{ u8(information[index] & 0x0f),
								u8(information[index] >> 4) })
					{
						if (digit == 0x0f)
							break;
						if (digit > 9 ||
								m_outgoing_called_digits_length >=
										m_outgoing_called_digits.size())
							return downlink_kind::none;
						m_outgoing_called_digits[
								m_outgoing_called_digits_length++] = digit;
					}
				}
			}
			offset += 2 + value_length;
		}
		if (!speech_bearer || !called_party ||
				m_outgoing_called_digits_length == 0 ||
				BIT(information[0], 7))
			return downlink_kind::none;
		m_call_transaction = information[0];
		const unsigned leg = second_call ? 1 : 0;
		m_call_leg_transactions[leg] = information[0];
		set_call_leg_state(leg, call_leg_state::alerting);
		if (second_call)
		{
			LOGMASKED(LOG_GSM_SESSION,
					"gsm_session: second outgoing SETUP transaction=%02x leg=%u "
					"digits=%u t=%.6f\n",
					information[0], leg, m_outgoing_called_digits_length,
					machine().time().as_double());
		}
		++m_outgoing_request_id;
		if (m_outgoing_request_id == 0)
			++m_outgoing_request_id;
		m_outgoing_request_pending = true;
		m_outgoing_decision_accepted = false;
		m_outgoing_decision_request_id = 0;
		if (m_outgoing_fallback_enabled)
		{
			m_outgoing_policy_request_id = m_outgoing_request_id;
			const auto outcome = m_network->configured_outgoing_call_outcome();
			if (m_outgoing_decision_delay_ms == 0)
				submit_outgoing_decision(m_outgoing_request_id, outcome);
			else
				m_outgoing_decision_timer->adjust(
						attotime::from_msec(m_outgoing_decision_delay_ms));
		}
		const auto proceeding =
				m_network->call_proceeding(m_call_transaction);
		m_state = u8(state::awaiting_call_proceeding_acknowledgement);
		return queue_downlink(downlink_kind::call_proceeding,
				proceeding.data(), proceeding.size());
	}

	if (sapi == 0 && m_state == u8(state::awaiting_authentication_response))
	{
		if (m_network->authentication_response_valid(information, length))
		{
			m_cipher_key = m_network->authentication_result().kc;
			m_cipher_key_valid = true;
			if (m_authentication_for_service)
			{
				m_authentication_for_service = false;
				const auto command = m_network->cipher_mode_command();
				m_cipher_algorithm = u8(m_network->cipher_algorithm());
				m_cipher_command_pending =
						m_network->cipher_algorithm() !=
								gsm::a5::algorithm::a5_0;
				m_state =
						u8(state::awaiting_cipher_mode_command_acknowledgement);
				return queue_downlink(downlink_kind::cipher_mode_command,
						command.data(), command.size());
			}
			const auto accept = m_network->location_update_accept(
					m_established_layer3.data(),
					m_established_layer3_length, m_serving_arfcn);
			m_state = u8(state::awaiting_location_update_accept_acknowledgement);
			return queue_downlink(downlink_kind::location_update_accept,
					accept.data(), accept.size());
		}

		const auto reject = m_network->authentication_reject();
		m_authentication_for_service = false;
		m_cipher_key.fill(0);
		m_cipher_key_valid = false;
		m_state = u8(state::awaiting_authentication_reject_acknowledgement);
		return queue_downlink(downlink_kind::authentication_reject,
				reject.data(), reject.size());
	}

	if (sapi == 0 &&
			(m_state == u8(state::incoming_call_active) ||
			m_state == u8(state::outgoing_call_alerting) ||
			m_state == u8(state::awaiting_traffic_assignment) ||
			m_state == u8(state::awaiting_assignment_complete) ||
			m_state == u8(state::awaiting_incoming_call_setup_acknowledgement)) &&
			protocol_discriminator == 0x03)
	{
		if (message_type == 0x08)
		{
			// Some handsets repeat Call Confirmed after the dedicated channel
			// assignment has completed.  It is still the same CC transaction,
			// so it must not start a second RR assignment.
			if (m_state != u8(state::incoming_call_active) ||
					m_traffic_assignment_issued)
				return downlink_kind::none;
			m_traffic_assignment_issued = true;
			const auto assignment =
					m_network->traffic_assignment(m_serving_arfcn);
			m_state = u8(state::awaiting_traffic_assignment);
			return queue_downlink(downlink_kind::traffic_assignment,
					assignment.data(), assignment.size());
		}
		if (message_type == 0x01)
		{
			m_call_alerting = true;
			publish_call_alerting_output();
			if (!m_mobile_originated_call && m_network->speech_forwarding_active(
					nokia_gsm_network_device::forwarding_condition::no_reply))
			{
				const u8 configured = m_network->forwarding_no_reply_time(
						nokia_gsm_network_device::forwarding_condition::no_reply);
				m_no_reply_timer->adjust(attotime::from_seconds(
						configured ? configured : 20));
			}
			return downlink_kind::none;
		}
		if (message_type == 0x07)
		{
			m_no_reply_timer->adjust(attotime::never);
			const int leg = call_leg_index(information[0]);
			if (leg >= 0)
				set_call_leg_state(unsigned(leg), call_leg_state::active);
			m_incoming_call_answered = true;
			m_call_alerting = false;
			publish_call_alerting_output();
			const auto acknowledge =
					m_network->connect_acknowledge(information[0]);
			m_state = u8(state::awaiting_connect_acknowledgement);
			return queue_downlink(downlink_kind::connect_acknowledge,
					acknowledge.data(), acknowledge.size());
		}
		if (message_type == 0x25)
		{
			const int leg = call_leg_index(information[0]);
			if (leg < 0)
				return downlink_kind::none;
			m_releasing_call_leg = u8(leg);
			m_call_alerting = false;
			publish_call_alerting_output();
			const auto release = m_network->call_release(information[0]);
			m_state = u8(state::awaiting_call_release_acknowledgement);
			return queue_downlink(downlink_kind::call_release,
					release.data(), release.size());
		}
		if (m_state == u8(state::incoming_call_active) &&
				message_type == 0x35 && length == 4 &&
				information[2] == 0x2c)
		{
			m_dtmf_active = true;
			m_dtmf_digit = information[3];
			++m_dtmf_start_count;
			LOGMASKED(LOG_GSM_SESSION,
					"gsm_session: DTMF start digit=%02x count=%u t=%.6f\n",
					m_dtmf_digit, m_dtmf_start_count,
					machine().time().as_double());
			const auto acknowledge = m_network->start_dtmf_acknowledge(
					information[0], m_dtmf_digit);
			return queue_downlink(downlink_kind::start_dtmf_acknowledge,
					acknowledge.data(), acknowledge.size());
		}
		if (m_state == u8(state::incoming_call_active) &&
				message_type == 0x31 && length == 2)
		{
			m_dtmf_active = false;
			++m_dtmf_stop_count;
			LOGMASKED(LOG_GSM_SESSION,
					"gsm_session: DTMF stop digit=%02x count=%u t=%.6f\n",
					m_dtmf_digit, m_dtmf_stop_count,
					machine().time().as_double());
			const auto acknowledge =
					m_network->stop_dtmf_acknowledge(information[0]);
			return queue_downlink(downlink_kind::stop_dtmf_acknowledge,
					acknowledge.data(), acknowledge.size());
		}
		if (m_state == u8(state::incoming_call_active) &&
				message_type == 0x18 && length == 2)
		{
			const int leg = call_leg_index(information[0]);
			if (leg < 0 || m_call_leg_states[leg] != u8(call_leg_state::active))
				return downlink_kind::none;
			set_call_leg_state(unsigned(leg), call_leg_state::held);
			++m_call_hold_count;
			LOGMASKED(LOG_GSM_SESSION,
					"gsm_session: call held count=%u transaction=%02x leg=%d t=%.6f\n",
					m_call_hold_count, information[0], leg,
					machine().time().as_double());
			const auto acknowledge =
					m_network->call_hold_acknowledge(information[0]);
			return queue_downlink(downlink_kind::call_hold_acknowledge,
					acknowledge.data(), acknowledge.size());
		}
		if (m_state == u8(state::incoming_call_active) &&
				message_type == 0x1c && length == 2)
		{
			const int leg = call_leg_index(information[0]);
			if (leg < 0 || m_call_leg_states[leg] != u8(call_leg_state::held))
				return downlink_kind::none;
			set_call_leg_state(unsigned(leg), call_leg_state::active);
			++m_call_retrieve_count;
			LOGMASKED(LOG_GSM_SESSION,
					"gsm_session: call retrieved count=%u transaction=%02x leg=%d t=%.6f\n",
					m_call_retrieve_count, information[0], leg,
					machine().time().as_double());
			const auto acknowledge =
					m_network->call_retrieve_acknowledge(information[0]);
			return queue_downlink(downlink_kind::call_retrieve_acknowledge,
					acknowledge.data(), acknowledge.size());
		}
	}

	if (sapi == 0 && m_state == u8(state::awaiting_handset_release) &&
			protocol_discriminator == 0x03)
	{
		m_release_waiting = false;
		publish_release_waiting_output();
		if (message_type == 0x2d)
		{
			const auto complete =
					m_network->call_release_complete(information[0]);
			m_state = u8(state::awaiting_call_release_acknowledgement);
			return queue_downlink(downlink_kind::release_complete,
					complete.data(), complete.size());
		}
		if (message_type == 0x2a)
		{
			return begin_channel_release();
		}
	}

	if (sapi == 0 &&
			m_state == u8(state::awaiting_network_disconnect_acknowledgement) &&
			protocol_discriminator == 0x03 && message_type == 0x2a)
	{
		// A handset may answer network DISCONNECT with Release Complete before
		// the LAPDm acknowledgement for the downlink reaches this state machine.
		// The CC response itself is sufficient to close the transaction.
		clear_pending_downlink();
		m_release_complete_received = true;
		return begin_channel_release();
	}

	if (sapi == 0 &&
			m_state == u8(state::awaiting_assignment_complete) &&
			protocol_discriminator == 0x06 && message_type == 0x29)
	{
		if (m_mobile_originated_call)
		{
			m_call_alerting = true;
			publish_call_alerting_output();
			const auto alerting =
					m_network->call_alerting(m_call_transaction);
			m_state = u8(state::awaiting_call_alerting_acknowledgement);
			return queue_downlink(downlink_kind::call_alerting,
					alerting.data(), alerting.size());
		}
		m_state = u8(state::incoming_call_active);
		publish_call_active_output();
		return downlink_kind::none;
	}

	if (sapi == 0 &&
			m_state == u8(state::awaiting_outgoing_connect_acknowledgement) &&
			protocol_discriminator == 0x03 && message_type == 0x0f)
	{
		clear_pending_downlink();
		m_call_alerting = false;
		m_release_waiting = false;
		publish_call_alerting_output();
		publish_release_waiting_output();
		m_state = u8(state::incoming_call_active);
		const int leg = call_leg_index(m_call_transaction);
		if (leg >= 0)
			set_call_leg_state(unsigned(leg), call_leg_state::active);
		publish_call_active_output();
		if (m_outgoing_termination_accepted)
			return apply_outgoing_termination();
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_release_complete) &&
			protocol_discriminator == 0x03 &&
			message_type == 0x2a)
	{
		m_established_layer3.fill(0);
		m_established_layer3_length = 0;
		m_release_completes_registration = false;
		m_call_alerting = false;
		m_traffic_assignment_issued = false;
		m_mobile_originated_call = false;
		m_mobile_originated_sms = false;
		m_mobile_originated_supplementary = false;
		m_ussd_request_pending = false;
		m_incoming_call_answered = false;
		m_release_complete_received = false;
		m_call_transaction = 0;
		m_waiting_call_queued = false;
		m_second_mobile_originated_call = false;
		m_call_leg_transactions.fill(0);
		m_call_leg_states.fill(u8(call_leg_state::inactive));
		m_releasing_call_leg = 0xff;
		m_call_held = false;
		m_multiparty_active = false;
		m_multiparty_held = false;
		m_outgoing_request_pending = false;
		clear_outgoing_call_state();
		publish_call_alerting_output();
		m_incoming_service_completed =
				m_incoming_service != u8(incoming_service::none);
		m_incoming_service = u8(incoming_service::none);
		m_incoming_call_digits_length = 0;
		m_state = u8(state::idle);
		publish_call_active_output();
		clear_dedicated_cipher();
		return downlink_kind::release_complete;
	}

	if (sapi == 3 && m_state == u8(state::awaiting_sms_handset_response) &&
			protocol_discriminator == 0x09)
	{
		const auto sms = gsm::sms::parse_uplink(
				information, length,
				m_sms_cp_transaction, m_sms_rp_reference);
		// CP-ACK confirms CP-DATA delivery. The following CP-DATA carries the
		// independently correlated RP response.
		if (sms.kind == gsm::sms::uplink_kind::cp_ack)
		{
			m_sms_cp_data_acknowledged = true;
			return downlink_kind::none;
		}
		if (m_sms_cp_data_acknowledged &&
				(sms.kind == gsm::sms::uplink_kind::rp_ack ||
					sms.kind == gsm::sms::uplink_kind::rp_error))
		{
			m_sms_rp_acknowledged =
					sms.kind == gsm::sms::uplink_kind::rp_ack;
			const auto acknowledge = m_network->sms_cp_ack(information[0]);
			m_state = u8(state::awaiting_sms_cp_ack_acknowledgement);
			return queue_downlink(downlink_kind::sms_cp_ack,
					acknowledge.data(), acknowledge.size(), 3);
		}
	}

	if (sapi == 3 && m_state == u8(state::awaiting_mobile_sms_submit))
	{
		const auto submit = gsm::sms::parse_submit(information, length);
		if (!submit.valid)
			return downlink_kind::none;
		m_sms_cp_transaction = submit.cp_transaction;
		m_sms_rp_reference = submit.rp_reference;
		m_sms_status_report_requested = submit.status_report_requested;
		m_sms_submit_message_reference = submit.message_reference;
		m_sms_submit_recipient = submit.destination_digits;
		m_sms_submit_recipient_length = submit.destination_digit_count;
		++m_outgoing_sms_request_id;
		if (m_outgoing_sms_request_id == 0)
			++m_outgoing_sms_request_id;
		m_outgoing_sms_request_pending = true;
		m_outgoing_sms_decision_accepted = false;
		m_outgoing_sms_alphabet = u8(submit.data_alphabet);
		m_outgoing_sms_user_data.fill(0);
		m_outgoing_sms_user_data_length = u8(submit.user_data_length);
		m_outgoing_sms_user_data_octets = u8(length - submit.user_data_offset);
		std::copy_n(information + submit.user_data_offset,
				m_outgoing_sms_user_data_octets,
				m_outgoing_sms_user_data.begin());
		if (machine().options().verbose())
		{
			std::string smsc;
			std::string destination;
			for (unsigned index = 0; index < submit.service_center_digit_count;
					++index)
				smsc += char('0' + submit.service_center_digits[index]);
			for (unsigned index = 0; index < submit.destination_digit_count;
					++index)
				destination += char('0' + submit.destination_digits[index]);
			LOGMASKED(LOG_GSM_SESSION,
					"gsm_sms_submit: cp=%02x rp=%02x smsc=%s destination=%s "
					"alphabet=%u user_length=%u outcome=%u status_report=%u "
					"t=%.6f\n",
					submit.cp_transaction, submit.rp_reference, smsc.c_str(),
					destination.c_str(), unsigned(submit.data_alphabet),
					submit.user_data_length,
					unsigned(m_network->configured_outgoing_sms_outcome()),
					submit.status_report_requested ? 1 : 0,
					machine().time().as_double());
		}
		if (m_network->configured_outgoing_sms_outcome() ==
				nokia_gsm_network_device::outgoing_sms_outcome::cp_silence)
		{
			m_state = u8(state::awaiting_mobile_sms_timeout);
			return downlink_kind::none;
		}
		const auto acknowledge = m_network->sms_cp_ack(information[0]);
		m_state = u8(state::awaiting_mobile_sms_cp_ack_acknowledgement);
		return queue_downlink(downlink_kind::sms_cp_ack,
				acknowledge.data(), acknowledge.size(), 3);
	}

	if (sapi == 3 && m_state == u8(state::awaiting_mobile_sms_final_cp_ack))
	{
		const auto acknowledge = gsm::sms::parse_uplink(
				information, length,
				u8(m_sms_cp_transaction ^ 0x80), m_sms_rp_reference);
		if (acknowledge.kind == gsm::sms::uplink_kind::cp_ack)
			return begin_channel_release();
	}

	return downlink_kind::none;
}

void nokia_gsm_session_device::mobile_sms_sapi3_established()
{
	if (m_state == u8(state::awaiting_mobile_sms_sapi3_establishment))
		m_state = u8(state::awaiting_mobile_sms_submit);
}

bool nokia_gsm_session_device::submit_outgoing_sms_decision(
		u32 request_id,
		nokia_gsm_network_device::outgoing_sms_outcome outcome)
{
	if (m_outgoing_sms_fallback_enabled || !m_outgoing_sms_request_pending ||
			request_id == 0 || request_id != m_outgoing_sms_request_id ||
			m_outgoing_sms_decision_accepted)
		return false;
	m_outgoing_sms_decision_accepted = true;
	m_outgoing_sms_decision = u8(outcome);
	if (m_state != u8(state::awaiting_mobile_sms_host_decision))
		return true;

	m_state = u8(state::awaiting_mobile_sms_final_cp_ack);
	if (outcome == nokia_gsm_network_device::outgoing_sms_outcome::rp_silence)
	{
		m_state = u8(state::awaiting_mobile_sms_timeout);
		return true;
	}
	if (outcome == nokia_gsm_network_device::outgoing_sms_outcome::rp_error)
	{
		const auto error = m_network->sms_rp_error(
				m_sms_cp_transaction, m_sms_rp_reference, 21);
		queue_downlink(downlink_kind::outgoing_sms_rp_error,
				error.data(), error.size(), 3);
		return true;
	}
	const auto acknowledge = m_network->sms_rp_ack(
			m_sms_cp_transaction, m_sms_rp_reference);
	m_sms_rp_acknowledged = true;
	queue_downlink(downlink_kind::outgoing_sms_rp_ack,
			acknowledge.data(), acknowledge.size(), 3);
	return true;
}

bool nokia_gsm_session_device::submit_ussd_response(
		u32 request_id, host_ussd_outcome outcome, u8 data_coding_scheme,
		const u8 *data, unsigned length, u8 error_code)
{
	if (m_ussd_fallback_enabled || !m_ussd_request_pending ||
			request_id == 0 || request_id != m_ussd_request_id ||
			m_state != u8(state::awaiting_mobile_ussd_host_response))
		return false;

	gsm::ss::request request;
	request.valid = true;
	request.transaction = m_ussd_transaction;
	request.invoke_id = m_ussd_invoke_id;
	request.operation_code = gsm::ss::operation::process_uss_request;
	request.data_coding_scheme = m_ussd_data_coding_scheme;
	gsm::ss::message response;
	switch (outcome)
	{
	case host_ussd_outcome::success:
		response = gsm::ss::process_uss_request_result(
				request, data_coding_scheme, data, length);
		break;
	case host_ussd_outcome::return_error:
		response = gsm::ss::error_result(request, error_code);
		break;
	case host_ussd_outcome::reject:
		response = gsm::ss::reject_result(request, error_code);
		break;
	}
	if (!response.length)
		return false;
	m_state = u8(state::awaiting_supplementary_release_acknowledgement);
	return queue_downlink(downlink_kind::supplementary_release_complete,
			response.data.data(), response.length) != downlink_kind::none;
}

bool nokia_gsm_session_device::submit_outgoing_decision(
		u32 request_id,
		nokia_gsm_network_device::outgoing_call_outcome outcome)
{
	if (!m_outgoing_request_pending || request_id == 0 ||
			request_id != m_outgoing_request_id ||
			m_outgoing_decision_accepted ||
			outcome ==
					nokia_gsm_network_device::outgoing_call_outcome::
							service_reject)
		return false;
	m_outgoing_decision_accepted = true;
	m_outgoing_decision = u8(outcome);
	m_outgoing_decision_request_id = request_id;
	if (machine().options().verbose())
		LOGMASKED(LOG_GSM_SESSION,
				"gsm_session: outgoing decision queued id=%u outcome=%u t=%.6f\n",
				request_id, unsigned(outcome), machine().time().as_double());
	if (m_state == u8(state::awaiting_outgoing_decision))
		apply_outgoing_decision();
	return true;
}

bool nokia_gsm_session_device::submit_outgoing_termination(
		u32 request_id, u8 cause)
{
	const bool call_in_progress =
			m_state == u8(state::awaiting_call_proceeding_acknowledgement) ||
			m_state == u8(state::awaiting_outgoing_decision) ||
			m_state == u8(state::awaiting_traffic_assignment) ||
			m_state == u8(state::awaiting_assignment_complete) ||
			m_state == u8(state::awaiting_call_alerting_acknowledgement) ||
			m_state == u8(state::awaiting_outgoing_connect_acknowledgement) ||
			m_state == u8(state::outgoing_call_alerting) ||
			m_state == u8(state::incoming_call_active);
	if (!m_mobile_originated_call || !m_outgoing_request_pending ||
			request_id == 0 || request_id != m_outgoing_request_id ||
			!m_outgoing_decision_accepted ||
			m_outgoing_termination_accepted || !call_in_progress ||
			cause == 0 || cause > 0x7f)
		return false;
	m_outgoing_termination_accepted = true;
	m_outgoing_termination_request_id = request_id;
	m_outgoing_termination_cause = cause;
	if (machine().options().verbose())
		LOGMASKED(LOG_GSM_SESSION,
				"gsm_session: outgoing termination queued id=%u cause=%u t=%.6f\n",
				request_id, cause, machine().time().as_double());
	if (m_state == u8(state::outgoing_call_alerting) ||
			m_state == u8(state::incoming_call_active))
		apply_outgoing_termination();
	return true;
}

bool nokia_gsm_session_device::submit_incoming_termination(u8 cause)
{
	if (m_mobile_originated_call || cause == 0 || cause > 0x7f ||
			(m_state != u8(state::incoming_call_active) &&
			 m_state != u8(state::awaiting_traffic_assignment) &&
			 m_state != u8(state::awaiting_assignment_complete) &&
			 m_state != u8(state::awaiting_incoming_call_setup_acknowledgement)))
	{
		LOGMASKED(LOG_GSM_SESSION,
				"gsm_session: incoming termination rejected state=%u mobile=%u "
				"cause=%u t=%.6f\n", m_state, m_mobile_originated_call, cause,
				machine().time().as_double());
		return false;
	}
	m_no_reply_timer->adjust(attotime::never);
	m_call_alerting = false;
	publish_call_alerting_output();
	const auto disconnect = m_network->call_disconnect(m_call_transaction, cause);
	m_state = u8(state::awaiting_network_disconnect_acknowledgement);
	return queue_downlink(downlink_kind::call_disconnect,
			disconnect.data(), disconnect.size()) != downlink_kind::none;
}

nokia_gsm_session_device::downlink_kind
nokia_gsm_session_device::apply_outgoing_termination()
{
	if (!m_outgoing_termination_accepted ||
			m_outgoing_termination_request_id != m_outgoing_request_id)
		return downlink_kind::none;
	if (machine().options().verbose())
		LOGMASKED(LOG_GSM_SESSION,
				"gsm_session: outgoing termination consumed id=%u cause=%u t=%.6f\n",
				m_outgoing_termination_request_id,
				m_outgoing_termination_cause, machine().time().as_double());
	m_call_alerting = false;
	publish_call_alerting_output();
	const auto disconnect = m_network->call_disconnect(
			m_call_transaction, m_outgoing_termination_cause);
	m_state = u8(state::awaiting_network_disconnect_acknowledgement);
	return queue_downlink(downlink_kind::call_disconnect,
			disconnect.data(), disconnect.size());
}

nokia_gsm_session_device::downlink_kind
nokia_gsm_session_device::apply_outgoing_decision()
{
	if (!m_outgoing_decision_accepted ||
			m_outgoing_decision_request_id != m_outgoing_request_id)
		return downlink_kind::none;
	if (machine().options().verbose())
		LOGMASKED(LOG_GSM_SESSION,
				"gsm_session: outgoing decision consumed id=%u outcome=%u t=%.6f\n",
				m_outgoing_decision_request_id, m_outgoing_decision,
				machine().time().as_double());
	const auto outcome =
			nokia_gsm_network_device::outgoing_call_outcome(
					m_outgoing_decision);
	if (outcome == nokia_gsm_network_device::outgoing_call_outcome::busy)
	{
		const auto disconnect =
				m_network->call_disconnect(m_call_transaction, 0x11);
		m_state = u8(state::awaiting_network_disconnect_acknowledgement);
		return queue_downlink(downlink_kind::call_disconnect,
				disconnect.data(), disconnect.size());
	}
	if (m_second_mobile_originated_call)
	{
		// The original call already owns the TCH. Continue the second CC
		// transaction directly from CALL PROCEEDING to ALERTING/CONNECT.
		const auto alerting = m_network->call_alerting(m_call_transaction);
		m_state = u8(state::awaiting_call_alerting_acknowledgement);
		return queue_downlink(downlink_kind::call_alerting,
				alerting.data(), alerting.size());
	}
	m_traffic_assignment_issued = true;
	const auto assignment = m_network->traffic_assignment(m_serving_arfcn);
	m_state = u8(state::awaiting_traffic_assignment);
	return queue_downlink(downlink_kind::traffic_assignment,
			assignment.data(), assignment.size());
}

TIMER_CALLBACK_MEMBER(nokia_gsm_session_device::outgoing_decision_timer)
{
	if (m_outgoing_policy_request_id != 0)
		submit_outgoing_decision(
				m_outgoing_policy_request_id,
				m_network->configured_outgoing_call_outcome());
}

TIMER_CALLBACK_MEMBER(nokia_gsm_session_device::no_reply_timer)
{
	if (!m_mobile_originated_call && m_call_alerting &&
			m_network->speech_forwarding_active(
				nokia_gsm_network_device::forwarding_condition::no_reply) &&
			submit_incoming_termination(0x10))
	{
		m_incoming_call_forwarded = true;
		LOGMASKED(LOG_GSM_SESSION,
				"gsm_ss: incoming call forwarded condition=no-reply "
				"destination_length=%u t=%.6f\n",
				m_network->forwarding_number_length(
					nokia_gsm_network_device::forwarding_condition::no_reply),
				machine().time().as_double());
	}
}

void nokia_gsm_session_device::publish_call_alerting_output()
{
	m_call_alerting_output = m_call_alerting;
}

void nokia_gsm_session_device::publish_call_active_output()
{
	m_call_active_output = m_state == u8(state::incoming_call_active);
}

void nokia_gsm_session_device::publish_release_waiting_output()
{
	m_release_waiting_output = m_release_waiting;
}

bool nokia_gsm_session_device::begin_traffic_assignment()
{
	if (m_state != u8(state::awaiting_traffic_assignment))
		return false;
	clear_pending_downlink();
	m_state = u8(state::awaiting_assignment_complete);
	return true;
}

bool nokia_gsm_session_device::begin_handover(
		u16 target_arfcn, u8 target_bsic, u8 reference)
{
	if (m_state != u8(state::incoming_call_active) ||
			m_pending_downlink.kind != u8(downlink_kind::none) ||
			target_arfcn == m_serving_arfcn)
		return false;

	const auto command =
			m_network->handover_command(target_arfcn, target_bsic, reference);
	m_handover_target_arfcn = target_arfcn;
	m_handover_reference = reference;
	m_state = u8(state::awaiting_handover_result);
	queue_downlink(downlink_kind::handover_command,
			command.data(), command.size());
	LOGMASKED(LOG_GSM_SESSION,
			"gsm_session: handover command target=%u reference=%02x t=%.6f\n",
			target_arfcn, reference, machine().time().as_double());
	return true;
}

bool nokia_gsm_session_device::handover_channel_configured(
		u16 target_arfcn, u8 reference)
{
	if (m_state != u8(state::awaiting_handover_result) ||
			m_pending_downlink.kind != u8(downlink_kind::handover_command) ||
			target_arfcn != m_handover_target_arfcn ||
			reference != m_handover_reference)
		return false;

	// The old LAPDm link acknowledges Handover Command, but that link-layer ACK
	// does not complete the RR procedure. Channel configuration is the first
	// observable handset acceptance; completion still belongs to the target.
	clear_pending_downlink();
	return true;
}

bool nokia_gsm_session_device::handover_rollback_completed(
		u16 restored_arfcn)
{
	if (m_state != u8(state::awaiting_handover_result) ||
			restored_arfcn != m_serving_arfcn)
		return false;

	clear_pending_downlink();
	m_handover_target_arfcn = 0;
	m_handover_reference = 0;
	m_state = u8(state::incoming_call_active);
	LOGMASKED(LOG_GSM_SESSION,
			"gsm_session: handover rollback serving=%u t=%.6f\n",
			m_serving_arfcn, machine().time().as_double());
	return true;
}

nokia_gsm_session_device::downlink_kind
nokia_gsm_session_device::handover_access_received()
{
	if (m_state != u8(state::awaiting_handover_result) ||
			m_pending_downlink.kind != u8(downlink_kind::none))
		return downlink_kind::none;
	const auto information = m_network->physical_information(0);
	return queue_downlink(downlink_kind::physical_information,
			information.data(), information.size());
}

nokia_gsm_session_device::downlink_kind
nokia_gsm_session_device::queue_downlink(
		downlink_kind kind, const u8 *information, unsigned length, u8 sapi)
{
	if (length > m_pending_downlink.data.size())
		return downlink_kind::none;
	clear_pending_downlink();
	m_pending_downlink.kind = u8(kind);
	m_pending_downlink.sapi = sapi;
	m_pending_downlink.length = length;
	if (length != 0)
		std::copy_n(information, length, m_pending_downlink.data.begin());
	return kind;
}

void nokia_gsm_session_device::clear_pending_downlink()
{
	m_pending_downlink.kind = u8(downlink_kind::none);
	m_pending_downlink.sapi = 0;
	m_pending_downlink.length = 0;
	m_pending_downlink.data.fill(0);
}

int nokia_gsm_session_device::call_leg_index(u8 transaction) const
{
	const u8 identity = transaction & 0x70;
	for (unsigned index = 0; index < maximum_call_legs; ++index)
		if (m_call_leg_states[index] != u8(call_leg_state::inactive) &&
				(m_call_leg_transactions[index] & 0x70) == identity)
			return int(index);
	return -1;
}

unsigned nokia_gsm_session_device::live_call_leg_count() const
{
	return unsigned(std::count_if(
			m_call_leg_states.begin(), m_call_leg_states.end(),
			[](u8 value) { return value != u8(call_leg_state::inactive); }));
}

void nokia_gsm_session_device::set_call_leg_state(
		unsigned index, call_leg_state state)
{
	if (index < maximum_call_legs)
		m_call_leg_states[index] = u8(state);
	m_call_held = std::any_of(
			m_call_leg_states.begin(), m_call_leg_states.end(),
			[](u8 value) { return value == u8(call_leg_state::held); });
}

nokia_gsm_session_device::downlink_kind
nokia_gsm_session_device::begin_channel_release()
{
	// The state moves before the release is queued so that an acknowledgement
	// arriving for this downlink is attributed to the release, not to the
	// transaction that ended.
	const auto release = m_network->channel_release();
	m_state = u8(state::awaiting_channel_release_acknowledgement);
	return queue_downlink(downlink_kind::channel_release,
			release.data(), release.size());
}

void nokia_gsm_session_device::clear_outgoing_call_state()
{
	// The decision itself is not cleared here: a call being torn down keeps
	// the outcome that ended it, and only reset restores the default.
	m_outgoing_called_digits.fill(0);
	m_outgoing_called_digits_length = 0;
	m_outgoing_decision_accepted = false;
	m_outgoing_decision_request_id = 0;
	m_outgoing_policy_request_id = 0;
	m_outgoing_termination_accepted = false;
	m_outgoing_termination_request_id = 0;
	m_outgoing_termination_cause = 0x10;
	m_outgoing_decision_timer->adjust(attotime::never);
}
