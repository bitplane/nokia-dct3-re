// license:BSD-3-Clause

#include "emu.h"
#include "nokia_gsm_network.h"

DEFINE_DEVICE_TYPE(NOKIA_GSM_NETWORK, nokia_gsm_network_device,
		"nokia_gsm_network", "Nokia DCT3 laboratory GSM network")

namespace {

// Minimum broadcast set for a GSM 900 cell on ARFCN 1. The reserved test PLMN
// 001-01 avoids coupling the synthetic network to handset network-lock data;
// LAC and cell ID are both 1.
constexpr std::array<std::array<u8, 24>, 4> SYSTEM_INFORMATION = {{
	// SI1 Cell Channel Description uses GSM bitmap-0 format. ARFCN 1 is
	// bit 0 of the final octet in that 16-octet field.
	{{ 0x55, 0x06, 0x19,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01,
		0, 0, 0, 0x2b, 0 }},
	{{ 0x59, 0x06, 0x1a, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0xff, 0, 0, 0, 0 }},
	{{ 0x49, 0x06, 0x1b, 0x00, 0x01, 0x00, 0xf1, 0x10,
		0x00, 0x01, 0x40, 0, 0, 0, 0, 0, 0, 0, 0,
		0x2b, 0x2b, 0x2b, 0x2b, 0x2b }},
	{{ 0x31, 0x06, 0x1c, 0x00, 0xf1, 0x10, 0x00, 0x01,
		0, 0, 0, 0, 0, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
		0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b }}
}};

} // anonymous namespace

nokia_gsm_network_device::nokia_gsm_network_device(
		const machine_config &mconfig, const char *tag, device_t *owner, u32 clock) :
	device_t(mconfig, NOKIA_GSM_NETWORK, tag, owner, clock)
{
}

void nokia_gsm_network_device::device_start()
{
}

const std::array<u8, 24> &nokia_gsm_network_device::system_information(unsigned index) const
{
	return SYSTEM_INFORMATION[index % SYSTEM_INFORMATION.size()];
}

std::array<u8, 24> nokia_gsm_network_device::immediate_assignment(
		u8 random_access, u32 frame_number) const
{
	// GSM 04.08 9.1.18 and 10.5.2.30. Assign SDCCH/8 subchannel 0,
	// timeslot 0 on the non-hopping serving carrier, and echo the exact random
	// access octet and reception frame which identify the phone's request.
	const u8 t1_prime = (frame_number / 1326) & 0x1f;
	const u8 t2 = frame_number % 26;
	const u8 t3 = frame_number % 51;
	std::array<u8, 24> block = {
		0x2d, 0x06, 0x3f, 0x00,
		0x20, 0x00, 0x01,
		random_access, u8((t1_prime << 3) | (t3 >> 3)), u8((t3 << 5) | t2),
		0x00, 0x00,
		0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b,
		0x2b, 0x2b, 0x2b, 0x2b, 0x2b, 0x2b
	};
	return block;
}

std::array<u8, 17> nokia_gsm_network_device::location_update_accept(
		const std::array<u8, 8> &mobile_identity) const
{
	// GSM 04.08 9.2.13. The mandatory body is the MM header and Location
	// Area Identification. With no allocated TMSI, the network includes the
	// IMSI mobile-identity IE so the phone discards any stale TMSI. This accepts
	// the subscriber in the laboratory PLMN and LAC advertised by SI3.
	std::array<u8, 17> message = {
		0x05, 0x02, 0x00, 0xf1, 0x10, 0x00, 0x01,
		0x17, 0x08, 0, 0, 0, 0, 0, 0, 0, 0
	};
	std::copy(mobile_identity.begin(), mobile_identity.end(), message.begin() + 9);
	return message;
}

std::array<u8, 3> nokia_gsm_network_device::channel_release() const
{
	// GSM 04.08 9.1.7. Cause 0 is "normal event". Once Location Updating has
	// completed, the network releases the temporary SDCCH and the mobile returns
	// to the already-selected serving cell.
	return { 0x06, 0x0d, 0x00 };
}

s8 nokia_gsm_network_device::serving_rssi(unsigned sample) const
{
	// A real receiver never returns an identical RSSI forever. The ROM retains
	// the previous sample and requires a strict improvement before promoting a
	// background measurement to a usable candidate. Keep the laboratory cell
	// deterministic while representing that measured-signal variation.
	static constexpr std::array<s8, 4> RSSI_PATTERN = { -60, -61, -59, -60 };
	return RSSI_PATTERN[sample % RSSI_PATTERN.size()];
}
