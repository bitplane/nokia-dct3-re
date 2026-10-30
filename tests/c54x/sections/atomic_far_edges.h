// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_HIGH_PROGRAM
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto append = [&](char const *name,u32 start,std::vector<memory_word> program,std::vector<memory_word> data,
			std::initializer_list<register_setting> setup,std::initializer_list<register_check> checks,
			std::vector<clocked_data_event> events,std::initializer_list<memory_word> memory = {}) {
		auto initial = initial_case(name,{0xf495}); initial.program = std::move(program);
		initial.program.push_back({start - 1,0xf495}); initial.data = std::move(data);
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,start - 1},{r::STATE_SP,0x400},{r::STATE_ST0,0},{r::STATE_ST1,0x800}});
		initial.initial.insert(initial.initial.end(),setup.begin(),setup.end());
		auto final = observation(name,checks,memory);
		clocked_register_case c{{{0,std::move(initial)},{1,observation("pre-far-snapshot", {{STATE_GENPC,start}})},
			{2,observation("far-running", {})},{48,std::move(final)}},1,48};
		c.pipeline_frontend = false; c.data_events = std::move(events); cases.push_back(std::move(c));
	};
	for (unsigned delayed = 0; delayed < 2; ++delayed) {
		std::vector<memory_word> program{{0x20600,u16(delayed ? 0xfb83 : 0xf983)},{0x20601,0x700},
			{0x20602,u16(delayed ? 0xe901 : 0xf4e1)},{0x20603,0xe902},{0x20604,0xf4e1},{0x30700,0xe807},{0x30701,0xf4e4}};
		u16 const continuation = delayed ? 0x604 : 0x602;
		append("atomic-FCALL-FRET-page-frame",0x20600,std::move(program),{}, {},
			{{r::STATE_A,7},{r::STATE_B,delayed ? 2U : 0U},{r::STATE_SP,0x400},{STATE_GENPC,0x20000U + continuation + 1}},
			{{true,0x3ff,continuation,0,false},{true,0x3fe,2,0,false},{false,0x3fe,2,0,false},{false,0x3ff,continuation,0,false}},
			{{0x3fe,2},{0x3ff,continuation}});
	}
	append("FBACCD-mask-and-two-word-delay",0x20600,{{0x20600,0xf7e6},{0x20601,0xf020},{0x20602,0x1234},{0x7f0800,0xf4e1}},
		{},{{r::STATE_B,0x12347f0800ULL}},{{r::STATE_A,0x1234},{r::STATE_B,0x12347f0800ULL},{STATE_GENPC,0x7f0801},{r::STATE_SP,0x400}},{});
	append("FCALA-highest-caller-page",0x7f0600,{{0x7f0600,0xf5e7},{0x7f0601,0xf4e1},{0x20700,0xe805},{0x20701,0xf4e4}},
		{},{{r::STATE_B,0x20700}},{{r::STATE_A,5},{STATE_GENPC,0x7f0602},{r::STATE_SP,0x400}},
		{{true,0x3ff,0x601,0,false},{true,0x3fe,0x7f,0,false},{false,0x3fe,0x7f,0,false},{false,0x3ff,0x601,0,false}},{{0x3fe,0x7f},{0x3ff,0x601}});
	append("FRETED-masks-page-and-enables-interrupts",0x30700,{{0x30700,0xf6e5},{0x30701,0xe901},{0x30702,0xe902},{0x20800,0xf4e1}},
		{{0x3fe,0x82},{0x3ff,0x800}},{{r::STATE_SP,0x3fe}},{{r::STATE_B,2},{STATE_GENPC,0x20801},{r::STATE_SP,0x400},{r::STATE_ST1,0,0x800}},
		{{false,0x3fe,0x82,0,false},{false,0x3ff,0x800,0,false}});
	append("sequential-PC-wrap-keeps-XPC",0x2ffff,{{0x2ffff,0xe803},{0x20000,0xf4e1},{0x30000,0xe809}},
		{}, {},{{r::STATE_A,3},{STATE_GENPC,0x20001},{r::STATE_XPC,2}},{});
	append("mapped-XPC-write-affects-next-fetch",0x20600,{{0x20600,0x771e},{0x20601,0xffff},{0x7f0602,0x481e},{0x7f0603,0xf4e1}},
		{}, {},{{r::STATE_A,0x7f},{STATE_GENPC,0x7f0604},{r::STATE_XPC,0x7f}},{{true,0x1e,0xffff,0,false},{false,0x1e,0x7f,0,false}});
	for (u16 opcode : {0xf882U,0xfa82U,0xf4e6U,0xf5e6U,0xf6e6U,0xf7e6U,
		0xf982U,0xfb82U,0xf4e7U,0xf5e7U,0xf6e7U,0xf7e7U,0xf4e4U,0xf6e4U,0xf4e5U,0xf6e5U}) {
		bool const immediate = (opcode & 0xfc80) == 0xf880, returning = (opcode & 0xfdfe) == 0xf4e4;
		bool const call = !returning && (immediate ? opcode & 0x100 : opcode & 1), delayed = opcode & 0x200;
		unsigned const continuation = 0x600 + (immediate ? 2 : 1) + (delayed ? 2 : 0);
		std::vector<memory_word> program{{0x10600,opcode}};
		unsigned cursor = 0x10601;
		if (immediate) program.push_back({cursor++,0x700});
		if (delayed) { program.push_back({cursor++,0xe905}); program.push_back({cursor++,0xe906}); }
		program.push_back({cursor,0xf4e1}); program.push_back({0x20700,0xe807});
		program.push_back({0x20701,u16(call ? 0xf4e4 : 0xf4e1)});
		std::vector<clocked_data_event> events;
		if (returning) events = {{false,0x3fe,2,0,false},{false,0x3ff,0x700,0,false}};
		if (call) events = {{true,0x3ff,u16(continuation),0,false},{true,0x3fe,1,0,false},
			{false,0x3fe,1,0,false},{false,0x3ff,u16(continuation),0,false}};
		append("all-atomic-far-transfer-encodings",0x10600,std::move(program),{{0x3fe,2},{0x3ff,0x700}},
			{{r::STATE_SP,returning ? 0x3feU : 0x400U},{r::STATE_A,0x20700},{r::STATE_B,0x20700}},
			{{r::STATE_A,7},{r::STATE_B,delayed ? 6U : 0x20700U},{r::STATE_SP,0x400},
			 {STATE_GENPC,call ? 0x10000U + continuation + 1 : 0x20702U},
			 {r::STATE_ST1,returning && (opcode & 1) ? 0U : 0x800U}},std::move(events),
			{{0x3fe,u16(call ? 1 : 2)},{0x3ff,u16(call ? continuation : 0x700)}});
	}
	return cases;
}
}
