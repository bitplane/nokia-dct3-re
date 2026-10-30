// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto append = [&](u16 opcode,u64 a,u64 b,u16 t,u16 st0,u16 st1,u16 x,u16 y,u64 out_a,u64 out_b,
			u16 out_t,u16 flags,u16 mask,u16 stored,bool alias) {
		bool const parallel = (opcode & 0xf000) == 0xd000;
		auto initial = initial_case("atomic-MAC-routing-and-parallel-store", {opcode,0xf4e1});
		initial.program.push_back({0x5ff,0xf495});
		initial.data = {{0xf90,x},{0xf91,y}};
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_A,a},{r::STATE_B,b},{r::STATE_T,t},
			{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_AR4,0xf91},{r::STATE_AR5,0xf90}});
		auto final = observation("independent-routing-rounding-and-store-outcome", {{STATE_GENPC,0x602},{r::STATE_A,out_a},
			{r::STATE_B,out_b},{r::STATE_T,out_t},{r::STATE_ST1,st1},
			{r::STATE_AR5,alias || opcode == 0xddba ? 0xf91U : 0xf90U}},{{0xf90,alias ? stored : x},{0xf91,parallel && !alias ? stored : y}});
		if (!alias) final.expected.push_back({r::STATE_AR4,0xf92});
		if (mask) final.expected.push_back({r::STATE_ST0,flags,mask});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-MAC-snapshot", {{r::STATE_A,a},{r::STATE_B,b}})},
			{2,observation("MAC-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{false,0xf90,x,0,false},
			{parallel,alias ? 0xf90U : 0xf91U,parallel ? stored : y,0,false}};
		cases.push_back(std::move(c));
	};
	for (unsigned parallel = 0; parallel < 2; ++parallel) for (unsigned operation = 0; operation < 4; ++operation)
	for (unsigned source_b = 0; source_b < 2; ++source_b) for (unsigned destination_b = 0; destination_b < 2; ++destination_b) {
		u64 const a = 0x18000,b = 0x28000;
		s64 result = (parallel ? destination_b : source_b) ? b : a;
		result += operation & 2 ? -6 : 6;
		if (operation & 1) result = (result + 0x8000) & ~s64(0xffff);
		append((parallel ? 0xd03a : 0xb03a) | (operation << 10) | (source_b << 9) | (destination_b << 8),
			a,b,parallel ? 2 : 0,0x800,0x100,parallel ? 3 : 2,parallel ? 0 : 3,
			destination_b ? a : u64(result),destination_b ? u64(result) : b,2,0x800,0x800,source_b ? 2 : 1,false);
	}
	append(0xd93a,0x111111,0x1111,0x400,0,0x105,0x4321,0x1234,0x111111,0xfffef38d11ULL,0x400,0,0,0x222,false);
	append(0xddba,0x111111,0x1111,0x400,0,0x101,0x4321,0x1234,0x111111,0xfffef40000ULL,0x400,0,0,0x22,false);
	append(0xd9b7,0x18000,0x28000,2,0x800,0x100,3,0x1234,0x18000,0x27ffa,2,0,0,1,true);
	append(0xb83a,0x18000,0x28000,0,0,0x140,2,3,0x17ff4,0x28000,2,0,0x600,3,false);
	append(0xd93a,0x18000,0x28000,2,0,0x140,3,0,0x18000,0x27ff4,2,0,0x600,1,false);
	append(0xb83a,0x7fffffff,0,0,0,0x300,0xffff,2,0x7fffffff,0,0xffff,0x400,0x600,2,false);
	append(0xd93a,0x18000,0x7fffffff,2,0,0x300,0xffff,0,0x18000,0x7fffffff,2,0x200,0x600,1,false);
	return cases;
}
}
