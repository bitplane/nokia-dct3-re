// license:BSD-3-Clause
// copyright-holders:Gaz

#include "gsm_supplementary.h"

namespace gsm::ss
{

bool basic_service_matches_speech(basic_service_kind kind, std::uint8_t code)
{
	// An omitted BasicService selects all services. GSM 02.30 uses teleservice
	// groups 0x10 (all speech transmission services) and 0x11 (telephony).
	return kind == basic_service_kind::none ||
			(kind == basic_service_kind::teleservice &&
			 (code == 0x10 || code == 0x11));
}

namespace
{

bool read_tlv(const std::uint8_t *data, unsigned limit, unsigned &offset,
		std::uint8_t tag, unsigned &value_offset, unsigned &value_length)
{
	if (offset + 2 > limit || data[offset] != tag || (data[offset + 1] & 0x80))
		return false;
	value_length = data[offset + 1];
	value_offset = offset + 2;
	if (value_offset + value_length > limit)
		return false;
	offset = value_offset + value_length;
	return true;
}

unsigned pack_gsm7(const char *text, std::uint8_t *packed, unsigned capacity)
{
	if (!text)
		return 0;
	unsigned septets = 0;
	while (text[septets] && septets < maximum_ussd_length)
		++septets;
	const unsigned bytes = (septets * 7 + 7) / 8;
	if (bytes > capacity)
		return 0;
	for (unsigned index = 0; index < bytes; ++index)
		packed[index] = 0;
	for (unsigned index = 0; index < septets; ++index)
	{
		const unsigned bit = index * 7;
		const std::uint8_t value = std::uint8_t(text[index]) & 0x7f;
		packed[bit / 8] |= value << (bit & 7);
		if ((bit & 7) > 1 && bit / 8 + 1 < bytes)
			packed[bit / 8 + 1] |= value >> (8 - (bit & 7));
	}
	return bytes;
}

} // anonymous namespace

request parse_register(const std::uint8_t *data, unsigned length)
{
	request result;
	if (!data || length < 9 || (data[0] & 0x0f) != 0x0b ||
			(data[1] & 0x3f) != 0x3b)
		return result;

	unsigned offset = 2;
	unsigned facility_offset = 0;
	unsigned facility_length = 0;
	if (!read_tlv(data, length, offset, 0x1c,
			facility_offset, facility_length))
		return result;

	unsigned component = facility_offset;
	unsigned invoke_offset = 0;
	unsigned invoke_length = 0;
	if (!read_tlv(data, facility_offset + facility_length, component, 0xa1,
			invoke_offset, invoke_length) ||
			component != facility_offset + facility_length)
		return result;

	unsigned field = invoke_offset;
	unsigned value = 0;
	unsigned value_length = 0;
	if (!read_tlv(data, invoke_offset + invoke_length, field, 0x02,
			value, value_length) || value_length != 1)
		return result;
	result.invoke_id = data[value];
	if (!read_tlv(data, invoke_offset + invoke_length, field, 0x02,
			value, value_length) || value_length != 1)
		return request{};
	result.operation_code = operation(data[value]);

	if (field < invoke_offset + invoke_length)
	{
		unsigned sequence_offset = 0;
		unsigned sequence_length = 0;
		if (!read_tlv(data, invoke_offset + invoke_length, field, 0x30,
				sequence_offset, sequence_length) ||
				field != invoke_offset + invoke_length)
			return request{};
		unsigned parameter = sequence_offset;
		if (!read_tlv(data, sequence_offset + sequence_length, parameter, 0x04,
				value, value_length) || value_length != 1)
			return request{};
		if (result.operation_code == operation::process_uss_request)
		{
			result.data_coding_scheme = data[value];
			if (!read_tlv(data, sequence_offset + sequence_length, parameter, 0x04,
					value, value_length) ||
					value_length > result.ussd.size())
				return request{};
			result.ussd_length = value_length;
			for (unsigned index = 0; index < value_length; ++index)
				result.ussd[index] = data[value + index];
		}
		else
		{
			result.service_code = data[value];
			while (parameter < sequence_offset + sequence_length)
			{
				const std::uint8_t tag = data[parameter];
				if (tag == 0x82 || tag == 0x83)
				{
					if (!read_tlv(data, sequence_offset + sequence_length,
							parameter, tag, value, value_length) || value_length != 1)
						return request{};
					result.basic_service = tag == 0x82 ?
							basic_service_kind::bearer : basic_service_kind::teleservice;
					result.basic_service_code = data[value];
				}
				else if (tag == 0x84 &&
						result.operation_code == operation::register_ss)
				{
					if (!read_tlv(data, sequence_offset + sequence_length,
							parameter, tag, value, value_length) ||
							value_length == 0 ||
							value_length > result.forwarded_number.size())
						return request{};
					result.forwarded_number_length = value_length;
					for (unsigned index = 0; index < value_length; ++index)
						result.forwarded_number[index] = data[value + index];
				}
				else if (tag == 0x85 &&
						result.operation_code == operation::register_ss)
				{
					if (!read_tlv(data, sequence_offset + sequence_length,
							parameter, tag, value, value_length) || value_length != 1 ||
							data[value] < 5 || data[value] > 30)
						return request{};
					result.no_reply_condition_time = data[value];
				}
				else
					return request{};
			}
		}
		if (parameter != sequence_offset + sequence_length)
			return request{};
	}

	result.transaction = data[0];
	result.valid = true;
	return result;
}

request parse_call_related_facility(const std::uint8_t *data, unsigned length)
{
	request result;
	if (!data || length < 10 || (data[0] & 0x0f) != 0x03 ||
			(data[1] & 0x3f) != 0x3a)
		return result;

	unsigned offset = 2;
	unsigned facility_offset = 0;
	unsigned facility_length = 0;
	if (!read_tlv(data, length, offset, 0x1c,
			facility_offset, facility_length))
		return result;
	// The SS version indicator may follow the Facility IE, but no additional
	// call-related information element is accepted before it.
	if (offset != length &&
			(offset + 3 != length || data[offset] != 0x7f || data[offset + 1] != 1))
		return result;

	unsigned component = facility_offset;
	unsigned invoke_offset = 0;
	unsigned invoke_length = 0;
	if (!read_tlv(data, facility_offset + facility_length, component, 0xa1,
			invoke_offset, invoke_length) || component != facility_offset + facility_length)
		return result;
	unsigned field = invoke_offset;
	unsigned value = 0;
	unsigned value_length = 0;
	if (!read_tlv(data, invoke_offset + invoke_length, field, 0x02,
			value, value_length) || value_length != 1)
		return result;
	result.invoke_id = data[value];
	if (!read_tlv(data, invoke_offset + invoke_length, field, 0x02,
			value, value_length) || value_length != 1 ||
			field != invoke_offset + invoke_length)
		return request{};
	result.operation_code = operation(data[value]);
	if (result.operation_code != operation::build_mpty &&
			result.operation_code != operation::hold_mpty &&
			result.operation_code != operation::retrieve_mpty &&
			result.operation_code != operation::split_mpty)
		return request{};
	result.transaction = data[0];
	result.valid = true;
	return result;
}

message call_related_result(const request &request)
{
	message result;
	if (!request.valid ||
			(request.operation_code != operation::build_mpty &&
			 request.operation_code != operation::hold_mpty &&
			 request.operation_code != operation::retrieve_mpty &&
			 request.operation_code != operation::split_mpty))
		return result;
	const std::uint8_t encoded[] = {
		std::uint8_t(request.transaction ^ 0x80), 0x3a,
		0x1c, 0x0a, 0xa2, 0x08,
		0x02, 0x01, request.invoke_id,
		0x30, 0x03, 0x02, 0x01, std::uint8_t(request.operation_code)
	};
	result.length = sizeof(encoded);
	for (unsigned index = 0; index < result.length; ++index)
		result.data[index] = encoded[index];
	return result;
}

message call_related_error(const request &request, std::uint8_t error_code)
{
	message result;
	if (!request.valid)
		return result;
	const std::uint8_t encoded[] = {
		std::uint8_t(request.transaction ^ 0x80), 0x3a,
		0x1c, 0x08, 0xa3, 0x06,
		0x02, 0x01, request.invoke_id,
		0x02, 0x01, error_code
	};
	result.length = sizeof(encoded);
	for (unsigned index = 0; index < result.length; ++index)
		result.data[index] = encoded[index];
	return result;
}

message forwarding_info_result(const request &request, bool registered,
		bool active, const std::uint8_t *forwarded_number,
		unsigned number_length, basic_service_kind basic_service,
		std::uint8_t basic_service_code,
		std::uint8_t no_reply_condition_time)
{
	message result;
	if (!request.valid ||
			(request.operation_code != operation::register_ss &&
			 request.operation_code != operation::erase_ss &&
			 request.operation_code != operation::activate_ss &&
			 request.operation_code != operation::deactivate_ss &&
			 request.operation_code != operation::interrogate_ss) ||
			number_length > maximum_forwarded_number_length ||
			(number_length && !forwarded_number) ||
			(no_reply_condition_time &&
				(no_reply_condition_time < 5 || no_reply_condition_time > 30)))
		return result;
	const unsigned feature_length =
			(basic_service != basic_service_kind::none ? 3 : 0) + 3 +
			(number_length ? 2 + number_length : 0) +
			(no_reply_condition_time ? 3 : 0);
	const unsigned feature_list_length = 2 + feature_length;
	const unsigned forwarding_info_length = 3 + 2 + feature_list_length;
	const unsigned result_sequence_length = 3 + 2 + forwarding_info_length;
	const unsigned component_length = 3 + 2 + result_sequence_length;
	const unsigned facility_length = 2 + component_length;
	if (2 + 2 + facility_length > result.data.size())
		return message{};

	unsigned offset = 0;
	result.data[offset++] = request.transaction ^ 0x80;
	result.data[offset++] = 0x2a;
	result.data[offset++] = 0x1c;
	result.data[offset++] = facility_length;
	result.data[offset++] = 0xa2;
	result.data[offset++] = component_length;
	result.data[offset++] = 0x02;
	result.data[offset++] = 0x01;
	result.data[offset++] = request.invoke_id;
	result.data[offset++] = 0x30;
	result.data[offset++] = result_sequence_length;
	result.data[offset++] = 0x02;
	result.data[offset++] = 0x01;
	result.data[offset++] = std::uint8_t(request.operation_code);
	result.data[offset++] = 0xa0;
	result.data[offset++] = forwarding_info_length;
	result.data[offset++] = 0x04;
	result.data[offset++] = 0x01;
	result.data[offset++] = request.service_code;
	result.data[offset++] = 0x30;
	result.data[offset++] = feature_list_length;
	result.data[offset++] = 0x30;
	result.data[offset++] = feature_length;
	if (basic_service != basic_service_kind::none)
	{
		result.data[offset++] = basic_service == basic_service_kind::bearer ?
				0x82 : 0x83;
		result.data[offset++] = 0x01;
		result.data[offset++] = basic_service_code;
	}
	result.data[offset++] = 0x84;
	result.data[offset++] = 0x01;
	result.data[offset++] = registered ? (active ? 0x05 : 0x04) : 0x00;
	if (number_length)
	{
		result.data[offset++] = 0x85;
		result.data[offset++] = number_length;
		for (unsigned index = 0; index < number_length; ++index)
			result.data[offset++] = forwarded_number[index];
	}
	if (no_reply_condition_time)
	{
		result.data[offset++] = 0x87;
		result.data[offset++] = 0x01;
		result.data[offset++] = no_reply_condition_time;
	}
	result.length = offset;
	return result;
}

message error_result(const request &request, std::uint8_t error_code)
{
	message result;
	if (!request.valid)
		return result;
	const std::uint8_t encoded[] = {
		std::uint8_t(request.transaction ^ 0x80), 0x2a,
		0x1c, 0x08, 0xa3, 0x06,
		0x02, 0x01, request.invoke_id,
		0x02, 0x01, error_code
	};
	result.length = sizeof(encoded);
	for (unsigned index = 0; index < result.length; ++index)
		result.data[index] = encoded[index];
	return result;
}

message reject_result(const request &request, std::uint8_t problem_code)
{
	message result;
	if (!request.valid)
		return result;
	const std::uint8_t encoded[] = {
		std::uint8_t(request.transaction ^ 0x80), 0x2a,
		0x1c, 0x08, 0xa4, 0x06,
		0x02, 0x01, request.invoke_id,
		0x80, 0x01, problem_code
	};
	result.length = sizeof(encoded);
	for (unsigned index = 0; index < result.length; ++index)
		result.data[index] = encoded[index];
	return result;
}

message process_uss_request_result(const request &request, const char *response)
{
	message result;
	if (!request.valid ||
			request.operation_code != operation::process_uss_request)
		return result;

	std::array<std::uint8_t, maximum_ussd_length> packed{};
	const unsigned packed_length = pack_gsm7(
			response, packed.data(), packed.size());
	if (!packed_length)
		return result;

	const unsigned result_sequence_length = 3 + 2 + packed_length;
	const unsigned operation_sequence_length = 3 + 2 + result_sequence_length;
	const unsigned component_length = 3 + 2 + operation_sequence_length;
	const unsigned facility_length = 2 + component_length;
	const unsigned total_length = 2 + 2 + facility_length;
	if (total_length > result.data.size())
		return message{};

	unsigned offset = 0;
	result.data[offset++] = request.transaction ^ 0x80;
	result.data[offset++] = 0x2a;
	result.data[offset++] = 0x1c;
	result.data[offset++] = facility_length;
	result.data[offset++] = 0xa2;
	result.data[offset++] = component_length;
	result.data[offset++] = 0x02;
	result.data[offset++] = 0x01;
	result.data[offset++] = request.invoke_id;
	result.data[offset++] = 0x30;
	result.data[offset++] = operation_sequence_length;
	result.data[offset++] = 0x02;
	result.data[offset++] = 0x01;
	result.data[offset++] = std::uint8_t(operation::process_uss_request);
	result.data[offset++] = 0x30;
	result.data[offset++] = result_sequence_length;
	result.data[offset++] = 0x04;
	result.data[offset++] = 0x01;
	result.data[offset++] = request.data_coding_scheme;
	result.data[offset++] = 0x04;
	result.data[offset++] = packed_length;
	for (unsigned index = 0; index < packed_length; ++index)
		result.data[offset++] = packed[index];
	result.length = offset;
	return result;
}

message unstructured_uss_request(const request &request,
		std::uint8_t invoke_id, const char *text)
{
	message result;
	if (!request.valid ||
			request.operation_code != operation::process_uss_request)
		return result;

	std::array<std::uint8_t, maximum_ussd_length> packed{};
	const unsigned packed_length = pack_gsm7(text, packed.data(), packed.size());
	if (!packed_length)
		return result;
	const unsigned parameter_length = 3 + 2 + packed_length;
	const unsigned component_length = 3 + 3 + 2 + parameter_length;
	const unsigned facility_length = 2 + component_length;
	if (4 + facility_length > result.data.size())
		return message{};

	unsigned offset = 0;
	result.data[offset++] = request.transaction ^ 0x80;
	result.data[offset++] = 0x3a;
	result.data[offset++] = 0x1c;
	result.data[offset++] = facility_length;
	result.data[offset++] = 0xa1;
	result.data[offset++] = component_length;
	result.data[offset++] = 0x02;
	result.data[offset++] = 0x01;
	result.data[offset++] = invoke_id;
	result.data[offset++] = 0x02;
	result.data[offset++] = 0x01;
	result.data[offset++] = std::uint8_t(operation::unstructured_uss_request);
	result.data[offset++] = 0x30;
	result.data[offset++] = parameter_length;
	result.data[offset++] = 0x04;
	result.data[offset++] = 0x01;
	result.data[offset++] = request.data_coding_scheme;
	result.data[offset++] = 0x04;
	result.data[offset++] = packed_length;
	for (unsigned index = 0; index < packed_length; ++index)
		result.data[offset++] = packed[index];
	result.length = offset;
	return result;
}

namespace
{

message network_unstructured_uss(
		std::uint8_t transaction, std::uint8_t invoke_id,
		operation operation_code, const char *text, bool version_indicator)
{
	message result;
	std::array<std::uint8_t, maximum_ussd_length> packed{};
	const unsigned packed_length = pack_gsm7(text, packed.data(), packed.size());
	if (!packed_length)
		return result;
	const unsigned parameter_length = 3 + 2 + packed_length;
	const unsigned component_length = 3 + 3 + 2 + parameter_length;
	const unsigned facility_length = 2 + component_length;
	const unsigned version_length = version_indicator ? 3 : 0;
	if (4 + facility_length + version_length > result.data.size())
		return message{};

	unsigned offset = 0;
	result.data[offset++] = transaction;
	result.data[offset++] = 0x3b;
	result.data[offset++] = 0x1c;
	result.data[offset++] = facility_length;
	result.data[offset++] = 0xa1;
	result.data[offset++] = component_length;
	result.data[offset++] = 0x02;
	result.data[offset++] = 0x01;
	result.data[offset++] = invoke_id;
	result.data[offset++] = 0x02;
	result.data[offset++] = 0x01;
	result.data[offset++] = std::uint8_t(operation_code);
	result.data[offset++] = 0x30;
	result.data[offset++] = parameter_length;
	result.data[offset++] = 0x04;
	result.data[offset++] = 0x01;
	result.data[offset++] = 0x0f;
	result.data[offset++] = 0x04;
	result.data[offset++] = packed_length;
	for (unsigned index = 0; index < packed_length; ++index)
		result.data[offset++] = packed[index];
	if (version_indicator)
	{
		result.data[offset++] = 0x7f;
		result.data[offset++] = 0x01;
		result.data[offset++] = 0x00;
	}
	result.length = offset;
	return result;
}

} // anonymous namespace

message network_unstructured_uss_request(
		std::uint8_t transaction, std::uint8_t invoke_id, const char *text)
{
	return network_unstructured_uss(transaction, invoke_id,
			operation::unstructured_uss_request, text, true);
}

message network_unstructured_uss_notify(
		std::uint8_t transaction, std::uint8_t invoke_id, const char *text)
{
	return network_unstructured_uss(transaction, invoke_id,
			operation::unstructured_uss_notify, text, false);
}

message network_release_complete(std::uint8_t transaction)
{
	message result;
	result.data[0] = transaction;
	result.data[1] = 0x2a;
	result.length = 2;
	return result;
}

message interrogate_result(const request &request, bool active)
{
	return interrogate_result(request, active, active, nullptr, 0);
}

message interrogate_result(const request &request, bool registered, bool active,
		const std::uint8_t *forwarded_number, unsigned number_length,
		basic_service_kind basic_service, std::uint8_t basic_service_code,
		std::uint8_t no_reply_condition_time)
{
	message result;
	if (!request.valid || request.operation_code != operation::interrogate_ss ||
			number_length > maximum_forwarded_number_length ||
			(number_length && !forwarded_number))
		return result;

	if (basic_service == basic_service_kind::none && !no_reply_condition_time &&
			active && number_length)
	{
		const unsigned result_sequence_length = 5 + number_length;
		const unsigned component_length = 3 + 2 + result_sequence_length;
		const unsigned facility_length = 2 + component_length;
		unsigned offset = 0;
		result.data[offset++] = request.transaction ^ 0x80;
		result.data[offset++] = 0x2a;
		result.data[offset++] = 0x1c;
		result.data[offset++] = facility_length;
		result.data[offset++] = 0xa2;
		result.data[offset++] = component_length;
		result.data[offset++] = 0x02;
		result.data[offset++] = 0x01;
		result.data[offset++] = request.invoke_id;
		result.data[offset++] = 0x30;
		result.data[offset++] = result_sequence_length;
		result.data[offset++] = 0x02;
		result.data[offset++] = 0x01;
		result.data[offset++] = std::uint8_t(operation::interrogate_ss);
		result.data[offset++] = 0x81;
		result.data[offset++] = number_length;
		for (unsigned index = 0; index < number_length; ++index)
			result.data[offset++] = forwarded_number[index];
		result.length = offset;
		return result;
	}
	if (basic_service != basic_service_kind::none || no_reply_condition_time)
		return forwarding_info_result(request, registered, active,
				forwarded_number, number_length, basic_service,
				basic_service_code, no_reply_condition_time);

	// InterrogateSS-Res selects the SS-Status choice when no active destination
	// is available. Bit 0 is active and bit 2 is registered.
	const std::uint8_t status = registered ? (active ? 0x05 : 0x04) : 0x00;
	const std::uint8_t encoded[] = {
		std::uint8_t(request.transaction ^ 0x80), 0x2a,
		0x1c, 0x0d,
		0xa2, 0x0b,
		0x02, 0x01, request.invoke_id,
		0x30, 0x06,
		0x02, 0x01, std::uint8_t(operation::interrogate_ss),
		0x80, 0x01, status
	};
	result.length = sizeof(encoded);
	for (unsigned index = 0; index < result.length; ++index)
		result.data[index] = encoded[index];
	return result;
}

} // namespace gsm::ss
