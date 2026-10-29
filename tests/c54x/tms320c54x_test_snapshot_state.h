// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_SNAPSHOT_STATE_H
#define MAME_NOKIA_TMS320C54X_TEST_SNAPSHOT_STATE_H

#include <array>
#include <cstdint>
#include <sstream>

namespace c54x_test {

struct transport_repeat_snapshot
{
	unsigned reads = 0;
	std::array<std::uint16_t, 0x800> transport{};
	std::stringstream saved;
};

// Capture is in the atomic-control sequence; replay is in the restore module.
struct far_transfer_snapshot
{
	bool returning = false;
	std::stringstream saved;
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_SNAPSHOT_STATE_H
