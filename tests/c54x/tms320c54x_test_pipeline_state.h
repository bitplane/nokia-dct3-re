// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_PIPELINE_STATE_H
#define MAME_NOKIA_TMS320C54X_TEST_PIPELINE_STATE_H

#include "tms320c54x_test_repeat_state.h"
#include <utility>
#include <vector>

namespace c54x_test {

struct frontend_entry_fixture
{
	bool memory = false, memory_b = false, nmi = false, replay = false;
	u64 expected_cycle = 0;
	unsigned operand_reads = 0, vector_reads = 0, acks = 0, drained_fetches = 0;
	memory_passthrough_handler operand_tap, fetch_tap, vector_tap, stack_tap;
	std::stringstream saved;
};

struct instruction_observer_fixture
{
	unsigned variant = 0, tick = 0;
	bool replay = false;
	std::vector<std::pair<offs_t, u16>> observed, snapshot;
	std::stringstream saved;
};

struct rptbd_observations
{
	unsigned reads = 0, writes = 0, fetches = 0, delay_reads = 0, delay_writes = 0;
	unsigned acks = 0, stack_reads = 0, stack_writes = 0;
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_PIPELINE_STATE_H
