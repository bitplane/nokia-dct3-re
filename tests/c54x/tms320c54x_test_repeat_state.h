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

struct saved_repeat_fixture
{
	unsigned count, inner, snapshot;
	bool after_end, nmi, delayed;
	u32 base;
	u16 sp;
	u16 end() const { return after_end ? 0x609 : 0x60b; }
	u16 saved_count() const { return count - unsigned(after_end && count); }
	u16 saved_st1() const { return 0x900 | (count ? 0x8000 : 0); }
	u16 vector() const { return nmi ? 4 : 0x40; }
};

struct saved_repeat_observations
{
	unsigned outer_reads = 0, outer_writes = 0, leaves = 0, inner_writes = 0;
	unsigned assertions = 0, vectors = 0, mapped_reads = 0, stack_reads = 0, stack_writes = 0, controls = 0, resumed = 0;
	u16 return_pc = 0;
	u64 vector_cycle = 0, inner_cycle = 0, restore_cycle = 0, st1_cycle = 0, resume_cycle = 0;
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
