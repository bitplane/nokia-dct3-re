// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned form = 0; form != 6; ++form)
	for (unsigned reg = 0; reg != (form < 2 ? 1U : 8U); ++reg)
	for (unsigned count : {0U,1U,2U,7U})
	for (unsigned prefix : {1U,2U})
	for (u16 destination : {0x2000,0x1000,0x1001}) {
		int const step = form == 3 || form == 5 ? 1 : form == 4 ? -1 : 0;
		u32 const base = (form + reg + count + prefix + destination) % 2 ? 0x20000 : 0;
		unsigned const read = 6 + prefix, finish = read + count + 16;
		u16 const opcode = 0x7000 | (form == 0 ? destination & 0x7f : form == 1 ? 5 :
			(form == 2 ? 0x80 : form == 3 ? 0x90 : form == 4 ? 0x88 : 0x98) | reg);
		u16 const st0 = 0xa600 | (form == 0 ? destination >> 7 : 0), st1 = form == 1 ? 0x4800 : 0x800;
		auto initial = initial_case("repeated-MVKD-forwarding-and-retained-literal", {0xf495});
		initial.program = {{base + 0x600,u16(prefix == 2 ? 0xf070 : 0xec00 | count)},
			{base + 0x600 + prefix,opcode},{base + 0x601 + prefix,0x1000},{base + 0x602 + prefix,0xf4e1}};
		if (prefix == 2) initial.program.push_back({base + 0x601,u16(count)});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_SP,u16(destination - 5)},{r::STATE_A,0x123456789aULL},{r::STATE_B,0x9876543210ULL}});
		auto final = observation("count-drains-with-exact-copy-transactions", {{STATE_GENPC,base + 0x603 + prefix},
			{r::STATE_RC,0},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_SP,u16(destination - 5)},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0x9876543210ULL}});
		for (unsigned ar = 0; ar != 8; ++ar) {
			u16 const pointer = ar == reg ? destination - (form == 5) : 0x3300 + ar;
			initial.initial.push_back({int(r::STATE_AR0) + int(ar),pointer});
			final.expected.push_back({int(r::STATE_AR0) + int(ar),ar == reg ? u16(pointer + step * int(count + 1)) : pointer});
		}
		for (unsigned i = 0; i <= count; ++i) {
			initial.data.push_back({0x1000 + i,u16(0x2468 + i)});
			final.expected_data.push_back({0x1000 + i,u16(i ? 0x2468 + i : 0xcafe)});
		}
		std::vector<clocked_data_event> events;
		u16 previous = 0;
		for (unsigned i = 0; i <= count; ++i) {
			u16 const target = destination + step * int(i);
			bool const bypass = i && u16(destination + step * int(i - 1)) == 0x1000 + i;
			u16 const value = bypass ? previous : 0x2468 + i;
			if (!bypass) events.push_back({false,0x1000 + i,value,read + i});
			if (i) events.push_back({true,u16(destination + step * int(i - 1)),previous,read + i});
			previous = value;
			if (target < 0x1000 || target > 0x1000 + count) initial.data.push_back({target,0xbeef});
			auto found = std::find_if(final.expected_data.begin(),final.expected_data.end(),[&](auto const &word) { return word.address == target; });
			if (found == final.expected_data.end()) final.expected_data.push_back({target,value});
			else found->value = value;
		}
		events.push_back({true,u16(destination + step * int(count)),previous,read + count + 1});
		unsigned const checkpoint = std::array<unsigned,4>{read - 1,read,read + 1,read + count / 2}[(form + reg + count + prefix + destination) % 4];
		clocked_register_case c{{{0,std::move(initial)},{finish,std::move(final)}},checkpoint,finish};
		c.actions = {{read - 1,{},false,{{base + 0x601 + prefix,0xdead}}},
			{read,{},false,{}, {},{{0x1000,0xcafe}}}};
		c.program_events = {{false,base + 0x600 + prefix,opcode,0,false},{false,base + 0x601 + prefix,0x1000,0,false}};
		c.data_events = std::move(events);
		cases.push_back(std::move(c));
	}
	return cases;
}
}
