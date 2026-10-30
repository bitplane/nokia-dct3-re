// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto append = [&](char const *name,std::initializer_list<u16> words,std::initializer_list<register_setting> setup,
		std::initializer_list<register_check> checks,std::vector<clocked_data_event> events = {}) {
		auto initial = initial_case(name,{*words.begin()}); unsigned at = 0;
		for (u16 word : words) initial.program[at++].value = word;
		initial.program[at].value = 0xf4e1; initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_ST0,0},{r::STATE_ST1,0x100},{r::STATE_T,0}});
		initial.initial.insert(initial.initial.end(),setup.begin(),setup.end());
		auto final = observation(name,checks); final.expected.push_back({STATE_GENPC,0x601 + unsigned(words.size())});
		for (auto const &e : events) if (e.write) { if (e.address >= 0x60) initial.data.push_back({e.address,0}); final.expected_data.push_back({e.address,e.value}); }
		for (auto const &e : events) if (!e.write && e.address >= 0x60) {
			initial.data.push_back({e.address,e.value});
			if (std::none_of(events.begin(),events.end(),[&](auto const &w) { return w.write && w.address == e.address; })) final.expected_data.push_back({e.address,e.value});
		}
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-absolute-literal-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("literal-running", {})},{48,std::move(final)}},1,48};
		c.pipeline_frontend = false; c.data_events = std::move(events); cases.push_back(std::move(c));
	};
	append("LD-small-B-retains-A",{0xe908},{{r::STATE_A,0x123456},{r::STATE_B,0xffffffffffULL}},{{r::STATE_B,8},{r::STATE_A,0x123456}});
	append("LDM-AR4-B-unsigned",{0x4914},{{r::STATE_AR4,0x8001},{r::STATE_A,0x123456}},
		{{r::STATE_B,0x8001},{r::STATE_AR4,0x8001},{r::STATE_A,0x123456}},{{false,0x14,0x8001,0,false}});
	append("SRL-B-carry",{0xf3ff},{{r::STATE_B,0xff80000003ULL},{r::STATE_A,0x123456}},
		{{r::STATE_B,0x40000001},{r::STATE_A,0x123456},{r::STATE_ST0,0x800,0x800}});
	append("CMPM-absolute",{0x60f8,0xf24,0x1234},{{r::STATE_ST0,0x800}},{{r::STATE_ST0,0x1800,0x1800}},{{false,0xf24,0x1234,0,false}});
	append("SUB-signed-postincrement",{0x0892},{{r::STATE_AR2,0xf20}},{{r::STATE_A,1},{r::STATE_AR2,0xf21}},{{false,0xf20,0xffff,0,false}});
	append("STL-retains-source-pointer",{0x8082},{{r::STATE_A,0xabcd1234},{r::STATE_AR2,0xf30}},
		{{r::STATE_A,0xabcd1234},{r::STATE_AR2,0xf30}},{{true,0xf30,0x1234,0,false}});
	append("LD-signed-circular-wrap",{0x10d2},{{r::STATE_BK,4},{r::STATE_AR2,0xf23}},
		{{r::STATE_A,0xffffff8001ULL},{r::STATE_AR2,0xf20}},{{false,0xf23,0x8001,0,false}});
	append("dual-copy-Y-postincrement",{0xe50b},{{r::STATE_AR2,0xf20},{r::STATE_AR5,0xf33}},
		{{r::STATE_AR2,0xf20},{r::STATE_AR5,0xf34}},{{false,0xf20,0x5a3c,0,false},{true,0xf33,0x5a3c,0,false}});
	append("LD-signed-stationary",{0x1083},{{r::STATE_AR3,0xf35}},{{r::STATE_A,0xffffff8002ULL},{r::STATE_AR3,0xf35}},{{false,0xf35,0x8002,0,false}});
	append("LDU-stationary",{0x1284},{{r::STATE_AR4,0xf36}},{{r::STATE_A,0x8002},{r::STATE_AR4,0xf36}},{{false,0xf36,0x8002,0,false}});
	append("STLM-B-to-T",{0x890e},{{r::STATE_B,0x12345678}},{{r::STATE_T,0x5678},{r::STATE_B,0x12345678}},{{true,0xe,0x5678,0,false}});
	append("SRL-cross-accumulator-carry",{0xf1f4},{{r::STATE_A,0x12345e78},{r::STATE_B,0xffffffff}},
		{{r::STATE_B,0x12345},{r::STATE_A,0x12345e78},{r::STATE_ST0,0x800,0x800}});
	append("AND-register-preserves-B",{0xf280},{{r::STATE_A,0xf0f0},{r::STATE_B,0xff0}},{{r::STATE_A,0xf0},{r::STATE_B,0xff0}});
	append("XOR-literal-preserves-A",{0xf350,0xff},{{r::STATE_A,0xabcd},{r::STATE_B,0x12345678}},{{r::STATE_B,0x12345687},{r::STATE_A,0xabcd}});
	append("MVDK-circular-source-wrap",{0x71d2,0xf40},{{r::STATE_BK,4},{r::STATE_AR2,0xf23}},
		{{r::STATE_AR2,0xf20}},{{false,0xf23,0x5a3c,0,false},{true,0xf40,0x5a3c,0,false}});
	append("ADD-signed-stationary",{0x0083},{{r::STATE_A,1},{r::STATE_AR3,0xf52}},{{r::STATE_A,0xffffffffffULL},{r::STATE_AR3,0xf52}},{{false,0xf52,0xfffe,0,false}});
	append("MVMM-auxiliary",{0xe726},{{r::STATE_AR2,0x4567},{r::STATE_AR6,0}},{{r::STATE_AR2,0x4567},{r::STATE_AR6,0x4567}},
		{{false,0x12,0x4567,0,false},{true,0x16,0x4567,0,false}});
	append("TCR-store-readback",{0x7726,0x10,0x4826},{},{{r::STATE_A,0x10}},{{true,0x26,0x10,0,false},{false,0x26,0x10,0,false}});
	append("MAC-fractional-negative",{0x2883},{{r::STATE_A,5},{r::STATE_AR3,0xf52},{r::STATE_T,0xfffe},{r::STATE_ST1,0x40}},
		{{r::STATE_A,0xfffffffff9ULL},{r::STATE_AR3,0xf52},{r::STATE_T,0xfffe}},{{false,0xf52,3,0,false}});
	append("MPY-dual-fractional",{0xa43a},{{r::STATE_AR5,0xf60},{r::STATE_AR4,0xf70},{r::STATE_ST1,0x40}},
		{{r::STATE_A,0xfffffffff4ULL},{r::STATE_T,0xfffe},{r::STATE_AR5,0xf60},{r::STATE_AR4,0xf71}},
		{{false,0xf60,0xfffe,0,false},{false,0xf70,3,0,false}});
	append("SQDST-fractional",{0xe201},{{r::STATE_A,0x20000},{r::STATE_B,3},{r::STATE_AR2,0xf80},{r::STATE_AR3,0xf81},{r::STATE_ST1,0x140}},
		{{r::STATE_A,0xfffffe0000ULL},{r::STATE_B,11},{r::STATE_AR2,0xf80},{r::STATE_AR3,0xf81}},{{false,0xf80,0xffff,0,false},{false,0xf81,1,0,false}});
	append("MPYR-negative-retains-carry",{0x2282},{{r::STATE_T,0xfffd},{r::STATE_AR2,0xf90},{r::STATE_ST0,0x800},{r::STATE_ST1,0}},
		{{r::STATE_A,0xffffff0000ULL},{r::STATE_T,0xfffd},{r::STATE_AR2,0xf90},{r::STATE_ST0,0x800,0x800}},{{false,0xf90,0x4000,0,false}});
	append("MACR-after-accumulation",{0x2a82},{{r::STATE_A,0x4000},{r::STATE_T,1},{r::STATE_AR2,0xf90},{r::STATE_ST0,0x800},{r::STATE_ST1,0}},
		{{r::STATE_A,0x10000},{r::STATE_T,1},{r::STATE_AR2,0xf90},{r::STATE_ST0,0x800,0x800}},{{false,0xf90,0x4000,0,false}});
	append("MASR-negative-retains-carry",{0x2e82},{{r::STATE_T,3},{r::STATE_AR2,0xf90},{r::STATE_ST0,0x800},{r::STATE_ST1,0}},
		{{r::STATE_A,0xffffff0000ULL},{r::STATE_T,3},{r::STATE_AR2,0xf90},{r::STATE_ST0,0x800,0x800}},{{false,0xf90,0x4000,0,false}});
	append("MPYU-fractional",{0x2482},{{r::STATE_T,0xffff},{r::STATE_AR2,0xf90},{r::STATE_ST1,0x40}},
		{{r::STATE_A,0x1fffc0002ULL},{r::STATE_T,0xffff},{r::STATE_AR2,0xf90}},{{false,0xf90,0xffff,0,false}});
	append("LD-high-ignores-OVM",{0x4482},{{r::STATE_AR2,0xf90},{r::STATE_ST0,0x400},{r::STATE_ST1,0x200}},
		{{r::STATE_A,0x80000000},{r::STATE_AR2,0xf90},{r::STATE_ST0,0x400,0x600}},{{false,0xf90,0x8000,0,false}});
	append("LDM-AR5-A-unsigned",{0x4815},{{r::STATE_A,0xff12345678ULL},{r::STATE_AR5,0x8001},{r::STATE_ST0,0x400}},
		{{r::STATE_A,0x8001},{r::STATE_AR5,0x8001},{r::STATE_ST0,0x400}},{{false,0x15,0x8001,0,false}});
	append("LDM-AR5-B-unsigned",{0x4915},{{r::STATE_A,0x8001},{r::STATE_B,0xff12345678ULL},{r::STATE_AR5,0x8001},{r::STATE_ST0,0x400}},
		{{r::STATE_B,0x8001},{r::STATE_A,0x8001},{r::STATE_AR5,0x8001},{r::STATE_ST0,0x400}},{{false,0x15,0x8001,0,false}});
	append("LDM-indirect-masks-MMR-address",{0x4882},{{r::STATE_AR2,0xff15},{r::STATE_AR5,0x8001}},
		{{r::STATE_A,0x8001},{r::STATE_AR2,0x15}},{{false,0x15,0x8001,0,false}});
	append("STLM-indirect-MMR-postincrement",{0x8892},{{r::STATE_A,0x3456},{r::STATE_AR2,0xff17},{r::STATE_AR7,0}},
		{{r::STATE_AR7,0x3456},{r::STATE_AR2,0x18}},{{true,0x17,0x3456,0,false}});
	return cases;
}
}
