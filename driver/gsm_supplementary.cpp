// license:BSD-3-Clause
// copyright-holders:Gaz

#include "gsm_supplementary.h"

namespace gsm::ss
{

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
		result.service_code = data[value];
	}

	result.transaction = data[0];
	result.valid = true;
	return result;
}

message interrogate_result(const request &request, bool active)
{
	message result;
	if (!request.valid || request.operation_code != operation::interrogate_ss)
		return result;

	// GSM 04.80 ReturnResult carries the reflected invoke id, operation code,
	// and an InterrogateSS-Res SS-Status.  Bit 0 is active, bit 2 registered.
	const std::uint8_t status = active ? 0x05 : 0x00;
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
