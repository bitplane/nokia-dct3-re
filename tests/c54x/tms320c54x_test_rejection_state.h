// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_REJECTION_STATE_H
#define MAME_NOKIA_TMS320C54X_TEST_REJECTION_STATE_H

#include "emumem.h"
#include <sstream>

namespace c54x_test {

// Each rejection family owns an instance. These are host observations, not CPU
// save items: replay deliberately retains them to detect duplicate bus effects.
struct operand_rejection_fixture
{
	explicit operand_rejection_fixture(unsigned phase) : phase(phase) { }

	bool handles_phase(unsigned value) const { return value == phase || value == phase + 10; }

	unsigned const phase;
	unsigned variant = 0, tick = 0;
	bool replay = false;
	struct effects
	{
		unsigned reads = 0, writes = 0, program_writes = 0, io_accesses = 0;
	} observed;
	memory_passthrough_handler data_read_tap, data_write_tap, program_tap, io_tap;
	std::stringstream saved;
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_REJECTION_STATE_H
