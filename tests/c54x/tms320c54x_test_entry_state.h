// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_ENTRY_STATE_H
#define MAME_NOKIA_TMS320C54X_TEST_ENTRY_STATE_H

#include "emumem.h"
#include <cstdint>
#include <sstream>

namespace c54x_test {

struct interrupt_entry_fixture
{
	unsigned vector_case = 0, restore_case = 0;
	unsigned reads = 0, acknowledgements = 0, stack_writes = 0;
	int acknowledged_source = -1;
	std::uint64_t fetch_cycle = 0, stack_cycle = 0;
	memory_passthrough_handler fetch_tap, stack_tap;
	// Host observations are rewound explicitly at the selected replay boundary,
	// not implicitly by restoring the CPU snapshot stored here.
	std::stringstream saved;
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_ENTRY_STATE_H
