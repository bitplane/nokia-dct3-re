// license:BSD-3-Clause

#include "emu.h"
#include "nokia_gsm_network.h"

DEFINE_DEVICE_TYPE(NOKIA_GSM_NETWORK, nokia_gsm_network_device,
		"nokia_gsm_network", "Nokia DCT3 laboratory GSM network")

namespace {

// Minimum broadcast set for a GSM 900 cell on ARFCN 1. The identity is the
// reserved test PLMN 001-01, LAC 1, cell 1; 0x2b is the GSM padding octet.
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
