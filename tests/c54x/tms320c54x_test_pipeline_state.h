// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_PIPELINE_STATE_H
#define MAME_NOKIA_TMS320C54X_TEST_PIPELINE_STATE_H

#include "emumem.h"
#include <sstream>
#include <utility>
#include <vector>

namespace c54x_test {

struct instruction_observer_fixture
{
	unsigned variant = 0, tick = 0;
	bool replay = false;
	std::vector<std::pair<offs_t, u16>> observed, snapshot;
	std::stringstream saved;
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_PIPELINE_STATE_H
