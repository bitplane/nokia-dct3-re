// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_TMS320C54X_TEST_REPEAT_STATE_H
#define MAME_NOKIA_TMS320C54X_TEST_REPEAT_STATE_H

#include "emumem.h"
#include <sstream>

namespace c54x_test {

struct fcala_delay_fixture
{
	bool b, wide;
	unsigned source, checkpoint;
	u32 caller, target;
};

struct fcala_delay_observations
{
	unsigned frames = 0, reads = 0, delays = 0, callee = 0, vectors = 0, irq_reads = 0, target_fetches = 0;
	u16 irq_pc = 0;
};

struct repeat_context_fixture
{
	bool replace, span;
	unsigned count, checkpoint;
	u32 base;
	u16 sp;
	u16 old_end() const { return 0x60b + unsigned(span); }
	u16 start() const { return replace ? span ? 0x1800 : 0x800 : 0x604; }
	u16 end() const { return replace ? start() + 2 : old_end(); }
	unsigned copies() const { return count + 1 + unsigned(replace); }
	unsigned calls() const { return replace ? 1 : count + 1; }
};

struct repeat_context_observations
{
	unsigned body_reads = 0, body_writes = 0, mapped_reads = 0, saved_writes = 0;
	unsigned restore_reads = 0, control_writes = 0, frame_reads = 0, frame_writes = 0;
	unsigned pmst_reads = 0, pmst_saves = 0, pmst_pops = 0, pmst_writes = 0;
};

// Storage only: each family explicitly chooses its checkpoint and counter
// rewinds. Restoring CPU state must not silently restore host observations.
template <typename Observations>
struct replay_fixture
{
	unsigned variant = 0, tick = 0, checkpoint_tick = 0;
	bool replay = false;
	Observations observed{}, snapshot{};
	memory_passthrough_handler read_tap, write_tap, fetch_tap, vector_tap;
	std::stringstream saved;
};

} // namespace c54x_test

#endif // MAME_NOKIA_TMS320C54X_TEST_REPEAT_STATE_H
