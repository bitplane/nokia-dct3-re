// license:BSD-3-Clause
// copyright-holders:Gaz
#include "nokia_rf_lab_tx.h"
#include <cassert>
#include <iostream>

int main()
{
	for (unsigned count:{0U,1U,14U,15U,16U})
	{
		nokia_rf_lab_tx state;
		std::array<std::uint16_t,15> output{};
		state.write(0xffff); assert(!state.count && !state.release(output));
		state.control(0x0ac3);
		for (unsigned i=0;i<count;++i) state.write(0x1000+i);
		auto restored=state;
		assert(state.control(0x06c3) && restored.control(0x0600));
		assert(state.ready==(count==15) && restored.ready==state.ready);
		assert(state.release(output)==(count==15));
		std::array<std::uint16_t,15> replay{};
		assert(restored.release(replay)==(count==15));
		if (count==15) assert(replay==output);
		if (count==15) for (unsigned i=0;i<15;++i) assert(output[i]==0x1000+i);
		assert(!state.release(output));
		state.reset(); assert(!state.loading && !state.ready && !state.count);
	}
	nokia_rf_lab_tx state;
	state.control(0x0a00); for (unsigned i=0;i<8;++i) state.write(i);
	auto restored=state;
	for (unsigned i=8;i<15;++i) restored.write(i);
	assert(restored.control(0x0600) && restored.ready);
	std::array<std::uint16_t,15> replay{};
	assert(restored.release(replay));
	for (unsigned i=0;i<15;++i) assert(replay[i]==i);
	state.control(0x0400); assert(!state.control(0x0600) && !state.ready);
	restored.control(0x0a00); restored.write(7);
	assert(restored.control(0x0600) && !restored.ready);
	restored.reset(); assert(!restored.ready && !restored.loading && !restored.count);
	state.control(0x0a00); state.write(1);
	assert(!state.release(replay)); // Missing the edge cannot delay the old load.
	for (unsigned i=0;i<15;++i) state.write(i);
	assert(!state.control(0x0600) && !state.ready && !state.count);
	std::cout << "Finite RF laboratory transmit framing: PASS\n";
}
