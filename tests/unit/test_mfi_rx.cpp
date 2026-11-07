// license:BSD-3-Clause
// copyright-holders:Gaz
#include "nokia_mfi_rx_state.h"
#include <cassert>
#include <iostream>

int main()
{
	nokia_mfi_rx_state state;
	assert(!state.clock(1,6) && !state.count);
	state.enable(true);
	for (unsigned block=0; block<8; ++block)
	{
		for (unsigned word=0; word<32; ++word)
		{
			unsigned const index=block*32+word;
			assert(state.clock(index,6*(index+1))==(word==31));
		}
		auto restored=state;
		for (unsigned word=0; word<32; ++word)
		{
			unsigned const index=block*32+word;
			assert(state.source_ticks[state.head]==6*(index+1));
			assert(state.read(false)==index && state.count==32-word);
			assert(state.read()==index && restored.read()==index);
		}
		assert(state.consumed==restored.consumed && !state.count);
	}
	assert(state.produced==256 && state.consumed==256);
	assert(state.read()==255 && state.underruns==1); // Retention, never padding.
	state.enable(false); assert(!state.clock(99,999));
	state.enable(true);
	for (unsigned i=0; i<64; ++i) state.clock(i,i);
	assert(!state.clock(64,64) && state.overruns==1 && state.fault);
	assert(!state.clock(65,65) && state.produced==64); // No silent overwrite.
	state.reset(); assert(!state.count && !state.enabled && !state.produced && !state.fault);
	state.enable(true);
	for (unsigned block=0; block<3; ++block)
	{
		for (unsigned word=0; word<96; ++word) state.clock(block*96+word,1000+block*96+word,96);
		assert(state.count==96 && !state.fault);
		auto restored=state;
		for (unsigned word=0; word<96; ++word)
		{
			assert(state.source_ticks[state.head]==1000+block*96+word);
			assert(state.read()==block*96+word && restored.read()==block*96+word);
		}
	}
	for (unsigned word=0; word<96; ++word) state.clock(word,word,96);
	assert(!state.clock(96,96,96) && state.fault && state.produced==384);
	std::cout << "MFI raw transport: PASS blocks=8 provenance=256 snapshot=8 staging96=3 underrun=1 overrun=1 reset=1 physical_capacity_claim=0\n";
}
