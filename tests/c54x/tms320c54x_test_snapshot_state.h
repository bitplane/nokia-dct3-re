// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_SNAPSHOT_STATE_H
#define MAME_NOKIA_TMS320C54X_TEST_SNAPSHOT_STATE_H

#include <sstream>

namespace c54x_test {


// Capture is in the atomic-control sequence; replay is in the restore module.
struct far_transfer_snapshot
{
	bool returning = false;
	std::stringstream saved;
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_SNAPSHOT_STATE_H
