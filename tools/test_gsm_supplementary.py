import pathlib
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


HARNESS = r'''
#include "gsm_supplementary.h"

#include <cstdio>

int main()
{
	if (!gsm::ss::basic_service_matches_speech(
			gsm::ss::basic_service_kind::none, 0) ||
			!gsm::ss::basic_service_matches_speech(
				gsm::ss::basic_service_kind::teleservice, 0x10) ||
			!gsm::ss::basic_service_matches_speech(
				gsm::ss::basic_service_kind::teleservice, 0x11) ||
			gsm::ss::basic_service_matches_speech(
				gsm::ss::basic_service_kind::teleservice, 0x12) ||
			gsm::ss::basic_service_matches_speech(
				gsm::ss::basic_service_kind::bearer, 0x10))
		return 17;
	const std::uint8_t query[] = {
		0x1b, 0x7b, 0x1c, 0x0d, 0xa1, 0x0b, 0x02, 0x01, 0x01,
		0x02, 0x01, 0x0e, 0x30, 0x03, 0x04, 0x01, 0x21, 0x7f, 0x01, 0x00
	};
	auto request = gsm::ss::parse_register(query, sizeof(query));
	if (!request.valid || request.transaction != 0x1b || request.invoke_id != 1 ||
			request.operation_code != gsm::ss::operation::interrogate_ss ||
			request.service_code != 0x21)
		return 1;
	auto response = gsm::ss::interrogate_result(request, false);
	for (unsigned i = 0; i < response.length; ++i)
		std::printf("%02x", response.data[i]);
	std::puts("");

	std::uint8_t malformed[sizeof(query)];
	for (unsigned i = 0; i < sizeof(query); ++i) malformed[i] = query[i];
	malformed[5] = 0x20;
	if (gsm::ss::parse_register(malformed, sizeof(malformed)).valid)
		return 2;

	const std::uint8_t ussd[] = {
		0x1b, 0x7b, 0x1c, 0x14, 0xa1, 0x12, 0x02, 0x01, 0x01,
		0x02, 0x01, 0x3b, 0x30, 0x0a, 0x04, 0x01, 0x0f,
		0x04, 0x05, 0xaa, 0x98, 0x6c, 0x36, 0x02, 0x7f, 0x01, 0x00
	};
	request = gsm::ss::parse_register(ussd, sizeof(ussd));
	if (!request.valid ||
			request.operation_code != gsm::ss::operation::process_uss_request ||
			request.data_coding_scheme != 0x0f || request.ussd_length != 5)
		return 3;
	response = gsm::ss::process_uss_request_result(request, "OK");
	for (unsigned i = 0; i < response.length; ++i)
		std::printf("%02x", response.data[i]);
	std::puts("");

	const std::uint8_t registration[] = {
		0x1b, 0x7b, 0x1c, 0x14, 0xa1, 0x12, 0x02, 0x01, 0x01,
		0x02, 0x01, 0x0a, 0x30, 0x0a, 0x04, 0x01, 0x21,
		0x84, 0x05, 0x81, 0x55, 0x15, 0x32, 0xf4, 0x7f, 0x01, 0x00
	};
	request = gsm::ss::parse_register(registration, sizeof(registration));
	if (!request.valid || request.operation_code != gsm::ss::operation::register_ss ||
			request.service_code != 0x21 || request.forwarded_number_length != 5)
		return 4;
	response = gsm::ss::forwarding_info_result(request, true, true,
			request.forwarded_number.data(), request.forwarded_number_length);
	for (unsigned i = 0; i < response.length; ++i)
		std::printf("%02x", response.data[i]);
	std::puts("");
	for (std::uint8_t service : {std::uint8_t(0x21), std::uint8_t(0x29),
			std::uint8_t(0x2a), std::uint8_t(0x2b)}) {
		std::uint8_t service_registration[sizeof(registration)];
		for (unsigned i = 0; i < sizeof(registration); ++i)
			service_registration[i] = registration[i];
		service_registration[16] = service;
		request = gsm::ss::parse_register(
				service_registration, sizeof(service_registration));
		if (!request.valid || request.service_code != service ||
				request.operation_code != gsm::ss::operation::register_ss)
			return 20;
	}

	request = gsm::ss::parse_register(registration, sizeof(registration));
	const auto destination = request.forwarded_number;
	const unsigned destination_length = request.forwarded_number_length;
	request = gsm::ss::parse_register(query, sizeof(query));
	response = gsm::ss::interrogate_result(request, true, true,
			destination.data(), destination_length);
	if (response.length != 21 || response.data[14] != 0x81 ||
			response.data[15] != destination_length)
		return 5;
	for (unsigned i = 0; i < destination_length; ++i)
		if (response.data[16 + i] != destination[i]) return 6;
	response = gsm::ss::interrogate_result(request, true, false, nullptr, 0);
	if (response.length != 17 || response.data[16] != 0x04) return 11;

	std::uint8_t control[sizeof(query)];
	for (unsigned i = 0; i < sizeof(query); ++i) control[i] = query[i];
	for (std::uint8_t operation : {std::uint8_t(0x0b), std::uint8_t(0x0c),
			std::uint8_t(0x0d)}) {
		control[11] = operation;
		request = gsm::ss::parse_register(control, sizeof(control));
		if (!request.valid || std::uint8_t(request.operation_code) != operation)
			return 7;
		const bool erased = operation == 0x0b;
		response = gsm::ss::forwarding_info_result(request, !erased,
				operation == 0x0c, erased ? nullptr : destination.data(),
				erased ? 0 : destination_length);
		if (response.length != (erased ? 26U : 33U) ||
				response.data[13] != operation || response.data[23] != 0x84)
			return 8;
	}
	for (std::uint8_t service : {std::uint8_t(0x21), std::uint8_t(0x29),
			std::uint8_t(0x2a), std::uint8_t(0x2b)}) {
		control[16] = service;
		for (std::uint8_t operation : {std::uint8_t(0x0b), std::uint8_t(0x0c),
				std::uint8_t(0x0d), std::uint8_t(0x0e)}) {
			control[11] = operation;
			request = gsm::ss::parse_register(control, sizeof(control));
			if (!request.valid || request.service_code != service ||
					std::uint8_t(request.operation_code) != operation)
				return 18;
		}
	}
	control[16] = 0x21;

	control[11] = 0x55;
	request = gsm::ss::parse_register(control, sizeof(control));
	response = gsm::ss::error_result(request, 0x10);
	if (!request.valid || response.length != 12 || response.data[4] != 0xa3 ||
			response.data[11] != 0x10)
		return 9;
	if (gsm::ss::forwarding_info_result(request, false, false,
			destination.data(), destination.size() + 1).length)
		return 10;

	const std::uint8_t conditional[] = {
		0x1b, 0x7b, 0x1c, 0x1a, 0xa1, 0x18, 0x02, 0x01, 0x02,
		0x02, 0x01, 0x0a, 0x30, 0x10, 0x04, 0x01, 0x2a,
		0x83, 0x01, 0x11, 0x84, 0x05, 0x81, 0x55, 0x15, 0x32, 0xf4,
		0x85, 0x01, 0x14, 0x7f, 0x01, 0x00
	};
	request = gsm::ss::parse_register(conditional, sizeof(conditional));
	if (!request.valid || request.service_code != 0x2a ||
			request.basic_service != gsm::ss::basic_service_kind::teleservice ||
			request.basic_service_code != 0x11 ||
			request.no_reply_condition_time != 20)
		return 12;
	response = gsm::ss::forwarding_info_result(request, true, true,
			request.forwarded_number.data(), request.forwarded_number_length,
			request.basic_service, request.basic_service_code,
			request.no_reply_condition_time);
	if (response.length != 39 || response.data[23] != 0x83 ||
			response.data[36] != 0x87 || response.data[38] != 20)
		return 13;

	const std::uint8_t build_mpty[] = {
		0x83, 0x3a, 0x1c, 0x08, 0xa1, 0x06,
		0x02, 0x01, 0x03, 0x02, 0x01, 0x7c, 0x7f, 0x01, 0x00
	};
	request = gsm::ss::parse_call_related_facility(
			build_mpty, sizeof(build_mpty));
	if (!request.valid || request.transaction != 0x83 || request.invoke_id != 3 ||
			request.operation_code != gsm::ss::operation::build_mpty)
		return 14;
	response = gsm::ss::call_related_result(request);
	if (response.length != 14 || response.data[1] != 0x3a ||
			response.data[4] != 0xa2 || response.data[13] != 0x7c)
		return 15;
	response = gsm::ss::call_related_error(request, 0x14);
	if (response.length != 12 || response.data[4] != 0xa3 ||
			response.data[11] != 0x14)
		return 16;
	response = gsm::ss::reject_result(request, 0x00);
	if (response.length != 12 || response.data[4] != 0xa4 ||
			response.data[9] != 0x80 || response.data[11] != 0x00)
		return 21;
	for (std::uint8_t operation : {std::uint8_t(0x79), std::uint8_t(0x7a),
			std::uint8_t(0x7b), std::uint8_t(0x7c)}) {
		std::uint8_t facility[sizeof(build_mpty)];
		for (unsigned i = 0; i < sizeof(build_mpty); ++i)
			facility[i] = build_mpty[i];
		facility[11] = operation;
		request = gsm::ss::parse_call_related_facility(
				facility, sizeof(facility));
		if (!request.valid || std::uint8_t(request.operation_code) != operation)
			return 19;
	}
	return 0;
}
'''


class GsmSupplementaryTest(unittest.TestCase):
    def test_interrogate_round_trip_and_malformed_length(self):
        with tempfile.TemporaryDirectory() as directory:
            source = pathlib.Path(directory) / "test.cpp"
            binary = pathlib.Path(directory) / "test"
            source.write_text(HARNESS)
            subprocess.run([
                "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-Idriver", str(source), "driver/gsm_supplementary.cpp",
                "-o", str(binary),
            ], cwd=ROOT, check=True)
            result = subprocess.run(
                [str(binary)], cwd=ROOT, check=True, text=True,
                stdout=subprocess.PIPE)
        self.assertEqual(
            "9b2a1c0da20b020101300602010e800100\n"
            "9b2a1c13a211020101300c02013b300704010f0402cf25\n"
            "9b2a1c1da21b020101301602010aa011040121300c300a840105850581551532f4\n",
            result.stdout)


if __name__ == "__main__":
    unittest.main()
