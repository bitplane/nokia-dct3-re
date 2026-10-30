// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct row { u16 opcode; unsigned pointer; u16 start,finish,high_address,low_address; bool a; };
	constexpr row rows[]{
		{0x4f81,1,0xd00,0xd00,0xd00,0xd01,false},
		{0x4f93,3,0xe00,0xe02,0xe00,0xe01,false},
		{0x4fd3,3,0xe02,0xe00,0xe02,0xe03,false},
		{0x4fd2,2,0xe12,0xe10,0xe12,0xe13,false},
		{0x4f90,0,0xe22,0xe24,0xe22,0xe23,false},
		{0x4e82,2,0xe22,0xe22,0xe22,0xe23,true},
		{0x4e95,5,0xe22,0xe24,0xe22,0xe23,true},
		{0x4f8d,5,0xe23,0xe21,0xe23,0xe22,false}
	};
	std::vector<clocked_register_case> cases;
	for (auto const &v : rows) {
		auto initial = initial_case("DST-two-word-addressing", {0xf495,v.opcode,0xf4e1});
		u64 const b = v.opcode == 0x4f81 || v.opcode == 0x4f93 ? 0xff12345678ULL : 0xff76543210ULL;
		u16 const high = v.a ? 0xabcd : u16(b >> 16), low = v.a ? 0xef01 : u16(b);
		u32 const first = std::min(v.high_address,v.low_address), last = std::max(v.high_address,v.low_address);
		initial.data = {{first - 1,0xbeef},{first,0},{last,0},{last + 1,0xcafe}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,0xabcdef01},{r::STATE_B,b},
			{r::STATE_ST0,0xaa5},{r::STATE_ST1,0x100},{r::STATE_BK,4}});
		for (unsigned ar = 0; ar != 8; ++ar) initial.initial.push_back({int(r::STATE_AR0) + int(ar),ar == v.pointer ? v.start : 0x1100U + ar});
		auto final = observation("ordered-halves-and-independent-pointer-update", {{STATE_GENPC,0x603},
			{r::STATE_A,0xabcdef01},{r::STATE_B,b},{r::STATE_ST0,0xaa5},{r::STATE_ST1,0x100},{r::STATE_BK,4}},
			{{v.high_address,high},{v.low_address,low},{first - 1,0xbeef},{last + 1,0xcafe}});
		for (unsigned ar = 0; ar != 8; ++ar) final.expected.push_back({int(r::STATE_AR0) + int(ar),ar == v.pointer ? v.finish : 0x1100U + ar});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("before-DST", {{STATE_GENPC,0x601}})},
			{2,observation("DST-running", {})},{16,std::move(final)}},1,16};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{true,v.high_address,high,0,false},{true,v.low_address,low,0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
