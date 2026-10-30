// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto append = [&](u16 source, u16 value, u16 literal, unsigned form, unsigned tc, unsigned initial_tc,
			bool mapped, bool excluded, unsigned seed) {
		unsigned const read = form == 2 ? 7 : 6, words = form == 2 ? 3 : 2;
		unsigned const pointer = mapped ? (source == 0x17 ? 6 : 7) : 2;
		u16 const st1 = 0x800 | (seed & 1 ? 0x4000 : 0);
		u32 const base = seed & 2 ? 0x20000 : 0;
		u64 a = 0x11223344ULL, b = 0x55667788ULL;
		if (!mapped && !excluded) {
			unsigned const lane = (source - 8) % 3, shift = lane * 16;
			u64 &accumulator = source < 11 ? a : b;
			accumulator = (accumulator & ~(0xffffULL << shift)) | (u64(value) << shift);
		}
		auto initial = initial_case("mapped-CMPM-and-TC-branch", {u16(form == 2 ? 0x60f8 : form == 1 ? 0x6080 | pointer : 0x6000 | source)});
		if (form == 2) initial.program[1].value = source;
		initial.program[words - 1].value = literal;
		initial.program[words].value = 0xf820 | (tc << 4);
		initial.program[words + 1].value = 0x700;
		initial.program[words + 2].value = 0xe811;
		initial.program[words + 3].value = 0xf4e1;
		for (auto &entry : initial.program) entry.address += base;
		initial.program.push_back({base + 0x700,0xe855}); initial.program.push_back({base + 0x701,0xf4e1});
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + 0x600},{r::STATE_A,a ^ 0xffffffffffULL},
			{r::STATE_B,b ^ 0xffffffffffULL},{r::STATE_ST0,0xa600U | (initial_tc << 12)},{r::STATE_ST1,st1},
			{r::STATE_SP,0},{r::STATE_T,0xbeef},{r::STATE_TRN,0xabcd}});
		for (unsigned i = 0; i < 8; ++i) initial.initial.push_back({int(r::STATE_AR0) + int(i),i == pointer ? source : 0x1200 + i * 0x100});
		bool const equal = value == literal, taken = equal == bool(tc);
		auto before = observation("TC-stays-old-through-Read", {{r::STATE_ST0,initial_tc << 12,0x1000}});
		auto final = observation("comparison-and-branch-use-captured-operands", {{r::STATE_A,excluded ? a ^ 0xffffffffffULL : taken ? 0x55U : 0x11U},
			{r::STATE_B,b ^ 0xffffffffffULL},{r::STATE_ST0,0xa600U | ((excluded ? initial_tc : unsigned(equal)) << 12)},
			{r::STATE_ST1,st1},{r::STATE_SP,0}});
		if (!excluded) final.expected.push_back({STATE_GENPC,base + (taken ? 0x702 : 0x604 + words)});
		if (mapped) {
			final.expected.push_back({r::STATE_T,!excluded && source == 14 ? u16(value ^ 0xffff) : 0xbeefU});
			final.expected.push_back({r::STATE_TRN,!excluded && source == 15 ? u16(value ^ 0xffff) : 0xabcdU});
			for (unsigned i = 0; i < 8; ++i) final.expected.push_back({int(r::STATE_AR0) + int(i),
				!excluded && source == 16 + i ? u16(value ^ 0xffff) : i == pointer ? source : 0x1200 + i * 0x100});
		}
		if (excluded) {
			for (auto &check : final.expected) if (check.index == r::STATE_ILLEGAL) check.value = 1;
			for (auto &check : before.expected) if (check.index == r::STATE_ILLEGAL) check.value = 1;
		}
		clocked_register_case c{{{0,std::move(initial)},{excluded ? read - 1 : 1,std::move(before)},
			{excluded ? read : read + 1,observation("comparison-published", {{r::STATE_ST0,(excluded ? initial_tc : unsigned(equal)) << 12,0x1000}})},
			{30,std::move(final)}},excluded ? read - 2 + seed % 2 : read - 1 + seed % 3,30};
		if (excluded) {
			for (auto &check : c.states[2].observation.expected) if (check.index == r::STATE_ILLEGAL) check.value = 1;
		} else {
			auto registers = [&](bool poison) {
				std::vector<register_setting> values{{r::STATE_A,poison ? a ^ 0xffffffffffULL : a},{r::STATE_B,poison ? b ^ 0xffffffffffULL : b}};
				if (mapped) values.push_back({source == 14 ? int(r::STATE_T) : source == 15 ? int(r::STATE_TRN) : int(r::STATE_AR0) + source - 16,
					poison ? u16(value ^ 0xffff) : value});
				return values;
			};
			c.actions = {{read - 1,registers(false)},{read,registers(true),false,{{base + 0x600 + words - 1,0xdead}}}};
			if (form == 2) c.actions.back().program.push_back({base + 0x601,0xdead});
		}
		c.require_idle = !excluded;
		c.data_events = std::vector<clocked_data_event>{};
		if (!excluded) c.data_events->push_back({false,source,value,read});
		c.program_events = {{false,base + 0x601,form == 2 ? source : literal,0,false}};
		if (form == 2) c.program_events.push_back({false,base + 0x602,literal,0,false});
		cases.push_back(std::move(c));
	};
	for (unsigned mapped = 0; mapped < 2; ++mapped) for (unsigned lane = 0; lane < (mapped ? 10U : 6U); ++lane)
	for (unsigned pattern = 0; pattern < (mapped ? 8U : 4U); ++pattern) for (unsigned literal = 0; literal < (mapped ? 6U : 2U); ++literal)
	for (unsigned form = 0; form < 3; ++form) for (unsigned tc = 0; tc < 2; ++tc) for (unsigned initial_tc = 0; initial_tc < 2; ++initial_tc) {
		u16 const value = mapped ? std::array<u16,8>{0,1,0x10,0x7f,0x80,0x7fff,0x8000,0xffff}[pattern] :
			std::array<u16,4>{0,1,0x7f,u16(lane % 3 == 2 ? 0x80 : 0x8000)}[pattern];
		append((mapped ? 14 : 8) + lane,value,mapped ? std::array<u16,6>{0,1,0x10,0x7f,0x8000,0xffff}[literal] : literal ? 0x7f : 1,
			form,tc,initial_tc,mapped,false,lane + pattern + literal + form + tc + initial_tc);
	}
	for (unsigned index = 0; index < 80; ++index) for (unsigned form = 0; form < 3; ++form) for (unsigned initial_tc = 0; initial_tc < 2; ++initial_tc)
		append(index < 8 ? index : index + 0x10,0,1,form,0,initial_tc,true,true,index + form + initial_tc);
	return cases;
}
}
