// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	constexpr u16 values[]{0,1,0xffff,0x8000,0x7fff,0xfde5,2,10};
	std::vector<clocked_register_case> cases;
	for (unsigned operation = 0; operation != 4; ++operation) for (unsigned destination = 0; destination != 2; ++destination)
		for (unsigned sxm = 0; sxm != 2; ++sxm) for (unsigned other = 0; other != 2; ++other)
			for (unsigned gap = 0; gap != 3; ++gap) for (unsigned pattern = 0; pattern != 8; ++pattern) {
				u16 const sample = values[pattern], operand = other ? 0x55aa : sample;
				u32 const base = (pattern ^ destination ^ operation) & 1 ? 0x20000 : 0;
				u64 a = 0x123456789aULL, b = 0xfeabcd9876ULL;
				u64 &result = destination ? b : a;
				if (!operation) result = u64(sxm ? s64(s16(operand)) : s64(operand)) & 0xffffffffffULL;
				else if (operation == 1) result &= operand;
				else if (operation == 2) result |= operand;
				else result ^= operand;
				auto initial = initial_case("PORTR-to-LD-AND-OR-XOR-forwarding-and-alias-control",{0x7482,0x125});
				initial.program[2+gap].value = std::array<u16,4>{0x1000,0x1800,0x1a00,0x1c00}[operation] | (destination << 8) | (other ? 0x83 : 0x82);
				initial.program[3+gap].value = 0xf4e1;
				for (auto &word : initial.program) word.address += base;
				initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},
					{r::STATE_T,0xbeef},{r::STATE_AR2,0x1000},{r::STATE_AR3,0x1100},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800U | (sxm << 8)}});
				initial.data = {{0x1000,0xa55a},{0x1100,0x55aa}};
				auto final = observation("retained-incoming-sample-with-real-destination-write",{{r::STATE_A,a},{r::STATE_B,b},
					{r::STATE_T,0xbeef},{r::STATE_AR2,0x1000},{r::STATE_AR3,0x1100},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800U | (sxm << 8)}},
					{{0x1000,sample},{0x1100,0x55aa}});
				clocked_register_case c{{{0,std::move(initial)},{1,observation("port-consumer-running",{})},{24,std::move(final)}},
					std::array<unsigned,4>{4,6,7,11}[(pattern+gap+destination+sxm+operation+other)%4],24};
				c.initial_io = {{0x125,sample}};
				c.io_events = std::vector<clocked_data_event>{{false,0x125,sample,6}};
				c.program_events = {{false,base+0x601,0x125,0,false}};
				c.data_events = std::vector<clocked_data_event>{};
				// Same-location zero-gap reads bypass the pending EB write (SPRU131G7.3.2).
				if (!gap && other) c.data_events->push_back({false,0x1100,0x55aa,7});
				c.data_events->push_back({true,0x1000,sample,7});
				if (gap) c.data_events->push_back({false,u32(other ? 0x1100 : 0x1000),operand,7+gap});
				c.actions = {{6,{}}}; c.actions.back().io = {{0x125,u16(sample ^ 0xffff)}};
				cases.push_back(std::move(c));
			}
	for (unsigned sample_index = 0; sample_index != 8; ++sample_index) for (unsigned request_index = 0; request_index != 8; ++request_index) {
		u16 const sample = values[sample_index], request = values[request_index];
		u64 const a = u16(~sample) & request;
		u32 const base = (sample_index ^ request_index) & 1 ? 0x20000 : 0;
		auto initial = initial_case("PORTR-load-complement-PORTR-AND-mask-chain",{0x7482,0x125,0x1082,0xf493,0x7482,0x126,0x1882,0xf130,2,0xf4e1});
		for (auto &word : initial.program) word.address += base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfeabcd9876ULL},
			{r::STATE_T,0xbeef},{r::STATE_AR2,0x1000},{r::STATE_AR3,0x1100},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800}});
		initial.data = {{0x1000,0xa55a},{0x1100,0x55aa}};
		auto final = observation("two-retained-port-samples-compose-without-refetch",{{r::STATE_A,a},{r::STATE_B,a & 2},
			{r::STATE_T,0xbeef},{r::STATE_AR2,0x1000},{r::STATE_AR3,0x1100},{r::STATE_ST0,0xa608},{r::STATE_ST1,0x800}},
			{{0x1000,request},{0x1100,0x55aa}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("port-chain-running",{})},{24,std::move(final)}},
			std::array<unsigned,4>{4,6,7,11}[(sample_index+request_index)%4],24};
		c.initial_io = {{0x125,sample},{0x126,request}};
		c.io_events = std::vector<clocked_data_event>{{false,0x125,sample,6},{false,0x126,request,10}};
		c.data_events = std::vector<clocked_data_event>{{true,0x1000,sample,7},{true,0x1000,request,11}};
		c.program_events = {{false,base+0x601,0x125,0,false},{false,base+0x605,0x126,0,false}};
		c.actions = {{6,{}},{10,{}}}; c.actions[0].io = {{0x125,u16(sample ^ 0xffff)}}; c.actions[1].io = {{0x126,u16(request ^ 0xffff)}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
