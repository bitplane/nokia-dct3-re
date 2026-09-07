// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_GSM_SUPPLEMENTARY_H
#define MAME_NOKIA_GSM_SUPPLEMENTARY_H

#include <array>
#include <cstdint>

namespace gsm::ss
{

constexpr unsigned maximum_message_length = 64;
constexpr unsigned maximum_ussd_length = 40;
constexpr unsigned maximum_forwarded_number_length = 16;

enum class operation : std::uint8_t
{
	unknown = 0,
	register_ss = 0x0a,
	erase_ss = 0x0b,
	activate_ss = 0x0c,
	deactivate_ss = 0x0d,
	interrogate_ss = 0x0e,
	process_uss_request = 0x3b,
	unstructured_uss_request = 0x3c,
	unstructured_uss_notify = 0x3d,
	split_mpty = 0x79,
	retrieve_mpty = 0x7a,
	hold_mpty = 0x7b,
	build_mpty = 0x7c
};

enum class basic_service_kind : std::uint8_t
{
	none,
	bearer,
	teleservice
};

struct request
{
	bool valid = false;
	std::uint8_t transaction = 0;
	std::uint8_t invoke_id = 0;
	operation operation_code = operation::unknown;
	std::uint8_t service_code = 0;
	basic_service_kind basic_service = basic_service_kind::none;
	std::uint8_t basic_service_code = 0;
	std::uint8_t no_reply_condition_time = 0;
	std::uint8_t data_coding_scheme = 0;
	unsigned ussd_length = 0;
	std::array<std::uint8_t, maximum_ussd_length> ussd{};
	unsigned forwarded_number_length = 0;
	std::array<std::uint8_t, maximum_forwarded_number_length> forwarded_number{};
};

struct message
{
	unsigned length = 0;
	std::array<std::uint8_t, maximum_message_length> data{};
};

bool basic_service_matches_speech(basic_service_kind kind, std::uint8_t code);
request parse_register(const std::uint8_t *data, unsigned length);
request parse_call_related_facility(const std::uint8_t *data, unsigned length);
message call_related_result(const request &request);
message call_related_error(const request &request, std::uint8_t error_code);
message interrogate_result(const request &request, bool active);
message interrogate_result(const request &request, bool registered, bool active,
		const std::uint8_t *forwarded_number, unsigned number_length,
		basic_service_kind basic_service = basic_service_kind::none,
		std::uint8_t basic_service_code = 0,
		std::uint8_t no_reply_condition_time = 0);
message forwarding_info_result(const request &request, bool registered,
		bool active, const std::uint8_t *forwarded_number,
		unsigned number_length,
		basic_service_kind basic_service = basic_service_kind::none,
		std::uint8_t basic_service_code = 0,
		std::uint8_t no_reply_condition_time = 0);
message error_result(const request &request, std::uint8_t error_code);
message reject_result(const request &request, std::uint8_t problem_code);
message process_uss_request_result(const request &request,
		const char *response);
message process_uss_request_result(const request &request,
		std::uint8_t data_coding_scheme, const std::uint8_t *packed,
		unsigned packed_length);
message unstructured_uss_request(const request &request,
		std::uint8_t invoke_id, const char *text);
message network_unstructured_uss_request(
		std::uint8_t transaction, std::uint8_t invoke_id, const char *text);
message network_unstructured_uss_notify(
		std::uint8_t transaction, std::uint8_t invoke_id, const char *text);
message network_unstructured_uss_notify(
		std::uint8_t transaction, std::uint8_t invoke_id,
		std::uint8_t data_coding_scheme, const std::uint8_t *packed,
		unsigned packed_length);
message network_release_complete(std::uint8_t transaction);

} // namespace gsm::ss

#endif // MAME_NOKIA_GSM_SUPPLEMENTARY_H
