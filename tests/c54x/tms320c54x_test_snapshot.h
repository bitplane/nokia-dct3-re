// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_SNAPSHOT_H
#define MAME_NOKIA_TMS320C54X_TEST_SNAPSHOT_H

#include <sstream>

namespace c54x_test {

// Serialization only: callers retain their assertions, observation-counter
// rewinds, CPU suspension and continuation. Never snapshot host observations
// implicitly along with guest state.
template <typename Machine>
auto capture_snapshot(Machine &machine, std::stringstream &saved)
{
	saved.str({});
	saved.clear();
	return machine.save().write_stream(saved);
}

template <typename Machine, typename Timer>
auto restore_snapshot(Machine &machine, Timer &timer, std::stringstream &saved)
{
	saved.clear();
	saved.seekg(0);
	// A loaded timer may be immediately due. Preserve the fixtures' original
	// ordering: cancel before load; the caller explicitly enables it afterwards.
	timer.adjust(attotime::never);
	return machine.save().read_stream(saved);
}

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_SNAPSHOT_H
