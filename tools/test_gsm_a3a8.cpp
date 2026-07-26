#include "../driver/gsm_a3a8.h"
#include "../driver/gsm_mm_authentication.h"

#include <algorithm>
#include <cassert>
#include <iostream>

int main()
{
	// FIPS-197 AES-128 example, projected through the explicit TS 55.205
	// section-5 SRES/Kc selection.
	const gsm::a3a8::block ki = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
	};
	const gsm::a3a8::block rand = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
	};
	const auto result = gsm::a3a8::aes_example(ki, rand);
	const std::array<std::uint8_t, 4> expected_sres = {
		0x69, 0xc4, 0xe0, 0xd8
	};
	const std::array<std::uint8_t, 8> expected_kc = {
		0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a
	};
	assert(result.sres == expected_sres);
	assert(result.kc == expected_kc);

	const auto request = gsm::mm::authentication::request(0, rand);
	assert(request[0] == 0x05 && request[1] == 0x12 && request[2] == 0x00);
	assert(std::equal(rand.begin(), rand.end(), request.begin() + 3));
	std::array<std::uint8_t, 6> response = {
		0x05, 0x14,
		expected_sres[0], expected_sres[1],
		expected_sres[2], expected_sres[3]
	};
	assert(gsm::mm::authentication::response_valid(
			result, response.data(), response.size()));
	response.back() ^= 1;
	assert(!gsm::mm::authentication::response_valid(
			result, response.data(), response.size()));
	assert(!gsm::mm::authentication::response_valid(
			result, response.data(), response.size() - 1));
	assert(gsm::mm::authentication::reject() ==
			(std::array<std::uint8_t, 2>{ 0x05, 0x11 }));
	std::cout << "GSM A3/A8 example tests passed\n";
}
