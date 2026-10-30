// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto append = [&](char const *name, std::initializer_list<u16> words,
		std::initializer_list<register_setting> setup, std::initializer_list<register_check> checks,
		std::vector<clocked_data_event> events = {}, std::initializer_list<memory_word> decoys = {}) {
		auto initial = initial_case(name,{*words.begin()}); unsigned at = 0;
		for (u16 word : words) initial.program[at++].value = word;
		initial.program[at].value = 0xf4e1; initial.program.push_back({0x5ff,0xf495});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,0x5ff},{r::STATE_ST0,0},{r::STATE_ST1,0x100},
			{r::STATE_T,0},{r::STATE_AR2,0xf00},{r::STATE_AR3,0xf90},{r::STATE_AR4,0xf91},{r::STATE_AR5,0xf90}});
		initial.initial.insert(initial.initial.end(),setup.begin(),setup.end());
		initial.data = decoys;
		auto final = observation(name,checks,decoys);
		final.expected.push_back({STATE_GENPC,0x601 + unsigned(words.size())});
		for (auto const &e : events) if (e.write) {
			initial.data.push_back({e.address,0});
			final.expected_data.push_back({e.address,e.value});
		}
		// A read-modify-write source must start with its read operand, not the destination poison.
		for (auto const &e : events) if (!e.write) {
			initial.data.push_back({e.address,e.value});
			if (std::none_of(events.begin(),events.end(),[&](auto const &other) { return other.write && other.address == e.address; }))
				final.expected_data.push_back({e.address,e.value});
		}
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-memory-snapshot", {{STATE_GENPC,0x600}})},
			{2,observation("memory-operation-running", {})},{32,std::move(final)}},1,32};
		c.pipeline_frontend = false; c.data_events = std::move(events); cases.push_back(std::move(c));
	};
	for (unsigned fractional = 0; fractional != 2; ++fractional)
		append("ABDST-old-A-high",{0xe33a},{{r::STATE_A,0xffabcd0000ULL},{r::STATE_T,0x4444},
			{r::STATE_ST1,fractional ? 0x140U : 0x100U}},
			{{r::STATE_A,fractional ? 0xfffffb0000ULL : 0xffffab0000ULL},{r::STATE_B,fractional ? 0xa866U : 0x5433U},
			{r::STATE_T,0x4444},{r::STATE_AR4,0xf92},{r::STATE_AR5,0xf90}},
			{{false,0xf90,u16(fractional ? 0xfffe : 0x55),0,false},{false,0xf91,u16(fractional ? 3 : 0xaa),0,false}});
	for (unsigned form = 0; form != 5; ++form) {
		u16 const opcode = std::array<u16,5>{0x4c83,0x4c93,0x4cf8,0x4cea,0x4c45}[form];
		u32 const source = form == 3 ? 0xf05 : form == 4 ? 0xf45 : 0xf90;
		u16 const value = form == 0 || form == 2 ? 0x6cac : form == 4 ? 0x5678 : 0x1234;
		append("LTD-source-successor-and-address-update",form < 2 || form == 4 ? std::initializer_list<u16>{opcode} :
			std::initializer_list<u16>{opcode,u16(form == 2 ? 0xf90 : 5)},
			{{r::STATE_ST1,form == 4 ? 0x4100U : 0x100U},{r::STATE_SP,0xf00}},
			{{r::STATE_T,value},{r::STATE_AR3,form == 1 ? 0xf91U : 0xf90U},
			{r::STATE_AR2,form == 3 ? 0xf05U : 0xf00U},{r::STATE_AR5,0xf90},{r::STATE_SP,0xf00}},
			{{false,source,value,0,false},{true,source + 1,value,0,false}});
	}
	for (unsigned logical = 0; logical != 2; ++logical)
		append("SUB-half-arithmetic-versus-logical",{0xf43f},{{r::STATE_A,0xfffffffffcULL},{r::STATE_ST1,logical ? 0U : 0x100U}},
			{{r::STATE_A,logical ? 0x7ffffffffeULL : 0xfffffffffeULL}});
	append("XOR-long-shift16-other",{0xf165,0xff},{{r::STATE_A,0xff12340000ULL},{r::STATE_B,0x1234},{r::STATE_ST0,0x800}},
		{{r::STATE_A,0xff12340000ULL},{r::STATE_B,0xff12cb0000ULL},{r::STATE_ST0,0x800,0x800}});
	for (unsigned below = 0; below != 2; ++below)
		append("MACR-negative-half-boundary",{0xb43a},{{r::STATE_A,below ? 0xffffff7fffULL : 0xffffff8000ULL},
			{r::STATE_B,0x1234},{r::STATE_ST0,0x800}},{{r::STATE_A,below ? 0xffffff0000ULL : 0ULL},
			{r::STATE_B,0x1234},{r::STATE_T,0},{r::STATE_AR4,0xf92},{r::STATE_ST0,0x800,0x800}},
			{{false,0xf90,0,0,false},{false,0xf91,1,0,false}});
	for (unsigned cpl = 0; cpl != 2; ++cpl) {
		u32 const source = cpl ? 0xf45 : 0x945; u16 const value = cpl ? 0x5678 : 0x1234;
		append("direct-load-store-DP-versus-SP",{0x1045,0x8046},{{r::STATE_ST0,0x12},
			{r::STATE_ST1,cpl ? 0x4100U : 0x100U},{r::STATE_SP,0xf00}},
			{{r::STATE_A,value},{r::STATE_AR5,0xf90},{r::STATE_SP,0xf00}},
			{{false,source,value,0,false},{true,source + 1,value,0,false}},{{0xf90,0xabcd},{cpl ? 0x946U : 0xf46U,0xbeef}});
	}
	append("ADDM-direct-DP",{0x6b45,1},{{r::STATE_ST0,0x12}},{{r::STATE_AR5,0xf90}},
		{{false,0x945,0x1234,0,false},{true,0x945,0x1235,0,false}});
	append("MPY-direct-DP",{0x2045},{{r::STATE_ST0,0x12},{r::STATE_T,3}},{{r::STATE_A,12},{r::STATE_AR5,0xf90}},
		{{false,0x945,4,0,false}});
	append("MAC-direct-SP",{0x2945},{{r::STATE_ST0,0x12},{r::STATE_ST1,0x4100},{r::STATE_SP,0xf00},
		{r::STATE_A,12},{r::STATE_B,16},{r::STATE_T,3}},{{r::STATE_B,4},{r::STATE_A,12},{r::STATE_AR5,0xf90},{r::STATE_SP,0xf00}},
		{{false,0xf45,0xfffc,0,false}});
	append("SQUR-offset-preincrement",{0x26ea,5},{},{{r::STATE_A,9},{r::STATE_T,0xfffd},{r::STATE_AR2,0xf05}},
		{{false,0xf05,0xfffd,0,false}});
	append("SQUR-direct-B",{0x271e},{{r::STATE_ST0,6},{r::STATE_T,3},{r::STATE_B,0x1f4}},
		{{r::STATE_B,0xe1},{r::STATE_T,0xf}},{{false,0x31e,0xf,0,false}});
	append("SQURA-direct-B",{0x391e},{{r::STATE_ST0,6},{r::STATE_T,3},{r::STATE_B,0x3200000}},
		{{r::STATE_B,0x32000e1},{r::STATE_T,0xf}},{{false,0x31e,0xf,0,false}});
	append("SQURS-direct-A",{0x3a09},{{r::STATE_ST0,6},{r::STATE_A,0x14b5db0},{r::STATE_T,0x8765}},
		{{r::STATE_A,0x320},{r::STATE_T,0x1234}},{{false,0x309,0x1234,0,false}});
	append("ADDC-direct-unsigned-SXM",{0x0605},{{r::STATE_ST0,0x808}},{{r::STATE_A,0x10000},{r::STATE_ST0,0,0x800}},
		{{false,0x405,0xffff,0,false}});
	append("SUBB-direct-inverted-carry",{0x0e05},{{r::STATE_ST0,8},{r::STATE_A,6}},
		{{r::STATE_A,0xffffffffffULL},{r::STATE_ST0,0,0x800}},{{false,0x405,6,0,false}});
	append("ADDM-negative-saturation",{0x6b94,0xfff8},{{r::STATE_AR4,0x100},{r::STATE_ST1,0x300}},
		{{r::STATE_AR4,0x101},{r::STATE_ST0,0x400,0x400}},{{false,0x100,0x8007,0,false},{true,0x100,0x8000,0,false}});
	return cases;
}
}
