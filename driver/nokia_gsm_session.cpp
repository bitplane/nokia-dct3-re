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
	m_release_waiting_output(*this, "nokia_gsm_call_release_waiting_handset")
{
}

void nokia_gsm_session_device::device_start()
{
	m_outgoing_decision_timer =
			timer_alloc(FUNC(nokia_gsm_session_device::outgoing_decision_timer),
					this);
	save_item(NAME(m_state));
	save_item(NAME(m_cipher_algorithm));
	save_item(NAME(m_cipher_key));
	save_item(NAME(m_cipher_key_valid));
	save_item(NAME(m_cipher_command_pending));
	save_item(NAME(m_cipher_active));
	save_item(NAME(m_authentication_for_service));
	save_item(NAME(m_established_layer3));
	save_item(NAME(m_established_layer3_length));
	save_item(NAME(m_registered_mobile_identity));
	save_item(NAME(m_registered_mobile_identity_length));
	save_item(NAME(m_release_completes_registration));
	save_item(NAME(m_call_alerting));
	save_item(NAME(m_release_waiting));
	save_item(NAME(m_traffic_assignment_issued));
	save_item(NAME(m_mobile_originated_call));
	save_item(NAME(m_call_transaction));
	save_item(NAME(m_outgoing_request_pending));
	save_item(NAME(m_outgoing_request_id));
	save_item(NAME(m_outgoing_called_digits));
	save_item(NAME(m_outgoing_called_digits_length));
	save_item(NAME(m_outgoing_decision_accepted));
	save_item(NAME(m_outgoing_decision));
	save_item(NAME(m_outgoing_decision_request_id));
	save_item(NAME(m_outgoing_policy_request_id));
	save_item(NAME(m_outgoing_termination_accepted));
	save_item(NAME(m_outgoing_termination_request_id));
	save_item(NAME(m_outgoing_termination_cause));
	save_item(NAME(m_incoming_service));
	save_item(NAME(m_smart_message_part_index));
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
	m_registered_mobile_identity.fill(0);
	m_registered_mobile_identity_length = 0;
	m_release_completes_registration = false;
	m_call_alerting = false;
	m_release_waiting = false;
	m_traffic_assignment_issued = false;
	m_mobile_originated_call = false;
	m_call_transaction = 0;
	m_outgoing_request_pending = false;
	m_outgoing_request_id = 0;
	m_outgoing_called_digits.fill(0);
	m_outgoing_called_digits_length = 0;
	m_outgoing_decision_accepted = false;
	m_outgoing_decision =
			u8(nokia_gsm_network_device::outgoing_call_outcome::connect);
	m_outgoing_decision_request_id = 0;
	m_outgoing_policy_request_id = 0;
	m_outgoing_termination_accepted = false;
	m_outgoing_termination_request_id = 0;
	m_outgoing_termination_cause = 0x10;
	m_outgoing_decision_timer->adjust(attotime::never);
	publish_call_alerting_output();
	publish_release_waiting_output();
	m_incoming_service = u8(incoming_service::none);
	m_smart_message_part_index = 0;
	clear_pending_downlink();
}

bool nokia_gsm_session_device::establish_layer3(
		const u8 *information, unsigned length)
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
			(information[2] & 0x0f) == 0x01;
	if (!location_update_request && !paging_response && !cm_service_request)
		return false;
	if (cm_service_request)
	{
		// GSM 04.08 9.2.9: accept only a structurally complete mobile-
		// originating call request. Other CM service types belong to their own
		// sessions and must not accidentally enter call control.
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
	clear_pending_downlink();
	m_release_completes_registration = location_update_request;
	m_mobile_originated_call = cm_service_request;
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
	m_incoming_service = u8(service);
	m_traffic_assignment_issued = false;
	if (service != incoming_service::smart_message)
		m_smart_message_part_index = 0;
	m_state = u8(state::awaiting_paging_response);
	return true;
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
		const auto release = m_network->channel_release();
		m_state = u8(state::awaiting_channel_release_acknowledgement);
		return queue_downlink(downlink_kind::channel_release,
				release.data(), release.size());
	}

	if (m_state != u8(state::awaiting_contention_resolution))
		return downlink_kind::none;

	if ((m_established_layer3[0] & 0x0f) == 0x05 &&
			(m_established_layer3[1] & 0x3f) == 0x24)
	{
		if (m_network->configured_outgoing_call_outcome() ==
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
			m_established_layer3.data(), m_established_layer3_length);
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
		m_state = u8(state::awaiting_outgoing_call_setup);
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_cm_service_reject_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::cm_service_reject))
	{
		const auto release = m_network->channel_release();
		m_state = u8(state::awaiting_channel_release_acknowledgement);
		return queue_downlink(downlink_kind::channel_release,
				release.data(), release.size());
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
		const auto release = m_network->channel_release();
		m_state = u8(state::awaiting_channel_release_acknowledgement);
		return queue_downlink(downlink_kind::channel_release,
				release.data(), release.size());
	}

	if (m_state == u8(state::awaiting_incoming_call_setup_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::incoming_call_setup))
	{
		clear_pending_downlink();
		m_state = u8(state::incoming_call_active);
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
			const auto setup = m_network->incoming_call_setup();
			m_state = u8(state::awaiting_incoming_call_setup_acknowledgement);
			return queue_downlink(downlink_kind::incoming_call_setup,
					setup.data(), setup.size());
		}
		if (m_incoming_service == u8(incoming_service::sms) ||
				m_incoming_service == u8(incoming_service::smart_message))
		{
			clear_pending_downlink();
			m_state = u8(state::awaiting_sms_sapi3_establishment);
			return queue_downlink(downlink_kind::sapi3_establishment,
					nullptr, 0, 3);
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
					m_smart_message_part_index);
			return queue_downlink(downlink_kind::incoming_sms_cp_data,
					data.data.data(), data.length, 3);
		}
		const auto data = m_network->incoming_sms_cp_data();
		return queue_downlink(downlink_kind::incoming_sms_cp_data,
				data.data(), data.size(), 3);
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
		const auto release = m_network->channel_release();
		m_state = u8(state::awaiting_channel_release_acknowledgement);
		return queue_downlink(downlink_kind::channel_release,
				release.data(), release.size());
	}

	if (m_state == u8(state::awaiting_connect_acknowledgement) &&
			m_pending_downlink.kind == u8(downlink_kind::connect_acknowledge))
	{
		clear_pending_downlink();
		m_state = u8(state::incoming_call_active);
		return downlink_kind::none;
	}

	if (m_state == u8(state::awaiting_call_release_acknowledgement) &&
			(m_pending_downlink.kind == u8(downlink_kind::call_release) ||
				m_pending_downlink.kind == u8(downlink_kind::release_complete)))
	{
		// Close the dedicated RR channel after RELEASE is acknowledged. The
		// handset emits CC Release Complete while that release is in flight.
		const auto release = m_network->channel_release();
		m_state = u8(state::awaiting_channel_release_acknowledgement);
		return queue_downlink(downlink_kind::channel_release,
				release.data(), release.size());
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
		const bool more_smart_message_parts =
				m_incoming_service == u8(incoming_service::smart_message) &&
				m_smart_message_part_index + 1 <
						m_network->incoming_smart_message_part_count();
		if (more_smart_message_parts)
			++m_smart_message_part_index;
		if (m_incoming_service == u8(incoming_service::call))
		{
			m_state = u8(state::awaiting_release_complete);
			return downlink_kind::none;
		}
		m_established_layer3.fill(0);
		m_established_layer3_length = 0;
		m_release_completes_registration = false;
		m_mobile_originated_call = false;
		m_call_transaction = 0;
		m_outgoing_request_pending = false;
		m_outgoing_called_digits.fill(0);
		m_outgoing_called_digits_length = 0;
		m_outgoing_decision_accepted = false;
		m_outgoing_decision_request_id = 0;
		m_outgoing_policy_request_id = 0;
		m_outgoing_termination_accepted = false;
		m_outgoing_termination_request_id = 0;
		m_outgoing_termination_cause = 0x10;
		m_outgoing_decision_timer->adjust(attotime::never);
		if (!more_smart_message_parts)
		{
			m_incoming_service = u8(incoming_service::none);
			m_smart_message_part_index = 0;
		}
		m_state = u8(state::idle);
		m_cipher_active = false;
		m_cipher_command_pending = false;
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
	if (sapi == 0 && m_state == u8(state::awaiting_outgoing_call_setup) &&
			protocol_discriminator == 0x03 && message_type == 0x05)
	{
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
					m_established_layer3_length);
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
			const auto assignment = m_network->traffic_assignment();
			m_state = u8(state::awaiting_traffic_assignment);
			return queue_downlink(downlink_kind::traffic_assignment,
					assignment.data(), assignment.size());
		}
		if (message_type == 0x01)
		{
			m_call_alerting = true;
			publish_call_alerting_output();
			return downlink_kind::none;
		}
		if (message_type == 0x07)
		{
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
			m_call_alerting = false;
			publish_call_alerting_output();
			const auto release = m_network->call_release(information[0]);
			m_state = u8(state::awaiting_call_release_acknowledgement);
			return queue_downlink(downlink_kind::call_release,
					release.data(), release.size());
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
			const auto release = m_network->channel_release();
			m_state = u8(state::awaiting_channel_release_acknowledgement);
			return queue_downlink(downlink_kind::channel_release,
					release.data(), release.size());
		}
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
		m_call_transaction = 0;
		m_outgoing_request_pending = false;
		m_outgoing_called_digits.fill(0);
		m_outgoing_called_digits_length = 0;
		m_outgoing_decision_accepted = false;
		m_outgoing_decision_request_id = 0;
		m_outgoing_policy_request_id = 0;
		m_outgoing_termination_accepted = false;
		m_outgoing_termination_request_id = 0;
		m_outgoing_termination_cause = 0x10;
		m_outgoing_decision_timer->adjust(attotime::never);
		publish_call_alerting_output();
		m_incoming_service = u8(incoming_service::none);
		m_state = u8(state::idle);
		m_cipher_active = false;
		m_cipher_command_pending = false;
		return downlink_kind::release_complete;
	}

	if (sapi == 3 && m_state == u8(state::awaiting_sms_handset_response) &&
			protocol_discriminator == 0x09)
	{
		// CP-ACK confirms CP-DATA delivery. The following CP-DATA carries the
		// RP-ACK for the current multipart message reference.
		if (message_type == 0x04)
			return downlink_kind::none;
		if (message_type == 0x01 && length >= 6 &&
				information[2] == 0x01 && information[3] == 0x02 &&
				information[4] == 0x02 &&
				information[5] == 0x40 + m_smart_message_part_index)
		{
			const auto acknowledge = m_network->sms_cp_ack(information[0]);
			m_state = u8(state::awaiting_sms_cp_ack_acknowledgement);
			return queue_downlink(downlink_kind::sms_cp_ack,
					acknowledge.data(), acknowledge.size(), 3);
		}
	}

	return downlink_kind::none;
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
	m_traffic_assignment_issued = true;
	const auto assignment = m_network->traffic_assignment();
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

void nokia_gsm_session_device::publish_call_alerting_output()
{
	m_call_alerting_output = m_call_alerting;
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
