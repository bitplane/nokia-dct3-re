// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto append = [&](char const *name,u16 opcode,u16 source,u16 word,u16 destination,u16 stored,
			std::initializer_list<register_setting> setup,std::initializer_list<register_check> checks) {
		auto initial = initial_case(name,{opcode,0xf4e1});
		initial.program.push_back({0x5ff,0xf495}); initial.data = {{source,word}};
		if (source != destination) initial.data.push_back({destination,0xdead});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_ST0,0},{r::STATE_ST1,0}});
		initial.initial.insert(initial.initial.end(),setup.begin(),setup.end());
		auto final = observation(name,checks,{{destination,stored}});
		final.expected.push_back({STATE_GENPC,0x602});
		if (source != destination) final.expected_data.push_back({source,word});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-parallel-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("parallel-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false;
		c.data_events = std::vector<clocked_data_event>{{false,source,word,0,false},{true,destination,stored,0,false}};
		cases.push_back(std::move(c));
	};
	append("ST-MPY-fractional",0xcdb9,0x300,0x4000,0x200,0x8421,
		{{r::STATE_A,0xff84211234ULL},{r::STATE_B,0},{r::STATE_T,0x4000},{r::STATE_AR3,0x200},{r::STATE_AR5,0x300},{r::STATE_ST1,0x140}},
		{{r::STATE_A,0xff84211234ULL},{r::STATE_B,0x20000000},{r::STATE_T,0x4000},{r::STATE_AR3,0x201},{r::STATE_AR5,0x301}});
	append("ST-MPY-old-destination",0xcfb9,0x300,3,0x200,0x1234,
		{{r::STATE_A,0xabcdef},{r::STATE_B,0x12345678},{r::STATE_T,2},{r::STATE_AR3,0x200},{r::STATE_AR5,0x300},{r::STATE_ST1,0x100}},
		{{r::STATE_A,0xabcdef},{r::STATE_B,6},{r::STATE_T,2},{r::STATE_AR3,0x201},{r::STATE_AR5,0x301}});
	append("ST-ADD-opposite",0xc131,0x300,2,0x200,0x1234,
		{{r::STATE_A,0x12345678},{r::STATE_B,0xdead},{r::STATE_AR3,0x200},{r::STATE_AR5,0x300},{r::STATE_ST1,0x100}},
		{{r::STATE_A,0x12345678},{r::STATE_B,0x12365678},{r::STATE_AR3,0x200},{r::STATE_AR5,0x300}});
	append("ST-SUB-circular-ASM",0xc5f5,0x300,0x8001,0x1ff,0x842,
		{{r::STATE_A,0xff84210000ULL},{r::STATE_B,0x10000001},{r::STATE_AR0,2},{r::STATE_BK,4},{r::STATE_AR3,0x1ff},{r::STATE_AR5,0x300},{r::STATE_ST1,0x101}},
		{{r::STATE_A,0xff84210000ULL},{r::STATE_B,0xfffbe00000ULL},{r::STATE_AR3,0x1fe},{r::STATE_AR5,0x302}});
	append("ST-LD-signed",0xca31,0x300,0x8001,0x200,0x1234,
		{{r::STATE_A,0},{r::STATE_B,0x12345678},{r::STATE_AR3,0x200},{r::STATE_AR5,0x300},{r::STATE_ST1,0x100}},
		{{r::STATE_A,0xff80010000ULL},{r::STATE_B,0x12345678},{r::STATE_AR3,0x200},{r::STATE_AR5,0x300}});
	append("ST-LD-T-same-cell",0xe411,0x200,0x80ff,0x200,0x1234,
		{{r::STATE_A,0x12345678},{r::STATE_T,0},{r::STATE_AR3,0x200},{r::STATE_ST1,0x100}},
		{{r::STATE_A,0x12345678},{r::STATE_T,0x80ff},{r::STATE_AR3,0x200}});
	for (unsigned b = 0; b < 2; ++b) append("ST-LD-T-circular",b ? 0xe6e9 : 0xe4e9,0x203,0x80ff,0x300,b ? 0x8765 : 0x1234,
		{{r::STATE_A,0x12345678},{r::STATE_B,0x87654321},{r::STATE_T,0},{r::STATE_AR0,1},{r::STATE_BK,4},
		 {r::STATE_AR3,0x300},{r::STATE_AR4,0x203},{r::STATE_ST0,0xaa5},{r::STATE_ST1,0x100}},
		{{r::STATE_A,0x12345678},{r::STATE_B,0x87654321},{r::STATE_T,0x80ff},{r::STATE_AR3,0x301},{r::STATE_AR4,0x200},{r::STATE_ST0,0xaa5}});
	append("ST-ADD-saturates",0xc131,0x300,1,0x200,0x7fff,
		{{r::STATE_A,0x7fffffff},{r::STATE_B,0},{r::STATE_ST0,0x800},{r::STATE_ST1,0x300},{r::STATE_AR3,0x200},{r::STATE_AR5,0x300}},
		{{r::STATE_B,0x7fffffff},{r::STATE_ST0,0x200,0xe00}});
	append("ST-SUB-saturates",0xc531,0x300,0x8000,0x200,1,
		{{r::STATE_A,0x10000},{r::STATE_B,0},{r::STATE_ST0,0},{r::STATE_ST1,0x300},{r::STATE_AR3,0x200},{r::STATE_AR5,0x300}},
		{{r::STATE_B,0xff80000000ULL},{r::STATE_ST0,0xa00,0xe00}});
	return cases;
}
}
