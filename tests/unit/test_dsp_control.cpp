// license:BSD-3-Clause
// copyright-holders:Gaz
#include "nokia_dsp_control_state.h"
#include <cassert>
#include <iostream>

int main()
{
	nokia_dsp_control_state state;
	assert(state.read()==0 && !state.strobe_w(true));
	state.complete(); assert(state.read()==0);
	state.strobe_w(false);
	state.write(0x0080); assert(state.read()==0x0080 && !state.pending);
	state.write(0xc09c); // A software-supplied DONE bit cannot manufacture it.
	assert(state.pending && !state.busy && !state.done && state.read()==0x809c);
	state.complete(); assert(state.pending && !state.done);
	assert(state.strobe_w(true) && state.busy && !state.pending && !state.done);
	assert(!state.strobe_w(true));
	auto restored=state; // All pending logical state is value-owned.
	state.complete(); restored.complete();
	assert(state.read()==0xc09c && restored.read()==state.read());
	state.complete(); assert(state.read()==0xc09c);
	state.write(0x409c); assert(state.read()==0x009c && !state.done);
	state.strobe_w(false);
	state.write(0x809c); assert(state.strobe_w(true));
	state.write(0x009c); state.complete(); assert(state.read()==0x009c);
	state.strobe_w(false);
	state.write(0x801a); assert(!state.pending && !state.strobe_w(true));
	state.complete(); assert(state.read()==0x801a);
	state.reset(); assert(state.read()==0 && !state.pending && !state.busy && !state.done && !state.strobe);
	std::cout << "DSP mode-control handshake: PASS\n";
}
