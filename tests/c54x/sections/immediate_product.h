// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr std::array<u16,8> terms{0,1,0xffff,0x8000,0x7fff,0x1234,0xff80,0x7f00};
	constexpr std::array<u16,8> literals{0,1,0xffff,0x8000,0x7fff,0xf495,0x5678,0xff00};
	constexpr std::array<u64,8> producer_values{0,1,0xffff,0x8000,0x7fffffff,0xff80000000ULL,0x0180008000ULL,0xff7fff8000ULL};
	auto add = [&](unsigned mode, unsigned destination, unsigned flags, u16 term, u16 literal,
		u64 a, u64 b, bool producer, unsigned source, unsigned gap, bool satd) {
		bool const frct = flags & 1, ovm = flags & 2, smul = flags & 4, sxm = flags & 8;
		u16 const pmst = (smul ? 2 : 0) | (satd ? 1 : 0);
		auto signed40 = [](u64 value) { return value >= 0x8000000000ULL ? s64(value)-0x10000000000LL : s64(value); };
		auto clamp32 = [](s64 value) { return std::clamp<s64>(value,-0x80000000LL,0x7fffffffLL); };
		if (producer) {
			u64 const value = source ? b : a;
			// STLM publishes the low word after SATD's signed/unsigned clamp.
			term = satd ? sxm ? u16(clamp32(signed40(value))) : u16(std::min<u64>(value,0xffffffffULL)) : u16(value);
		}
		s64 const left = term >= 0x8000 ? s64(term)-0x10000 : term;
		s64 const right = literal >= 0x8000 ? s64(literal)-0x10000 : literal;
		s64 product = left*right*(frct ? 2 : 1);
		if (mode && smul && frct && ovm) product = clamp32(product);
		s64 const wide = mode ? signed40(mode == 2 ? b : a)+product : product;
		bool const overflow = wide < -0x80000000LL || wide > 0x7fffffffLL;
		u64 const result = u64(ovm && overflow ? clamp32(wide) : wide) & 0xffffffffffULL;
		u16 const st0 = 0x1804 | ((term ^ literal) & 1 ? 0x600 : 0);
		u16 const st1 = 0x800 | (frct ? 0x40 : 0) | (ovm ? 0x200 : 0) | (sxm ? 0x100 : 0) | (producer && (term & 2) ? 0x4000 : 0);
		u16 const result_st0 = st0 | (overflow ? destination ? 0x200 : 0x400 : 0);
		unsigned const prefix = producer ? 1+gap : 0, read = 6+prefix;
		u32 const base = cases.size()%2 ? 0x20000 : 0;
		u16 const opcode = (mode ? 0xf067 | (mode == 2 ? 0x200 : 0) : 0xf066) | (destination << 8);
		auto initial = initial_case("immediate-MPY-MAC-retained-literal-T-and-accumulator",{u16(producer ? 0x880e | (source << 8) : opcode)});
		initial.program[prefix].value = opcode;
		initial.program[prefix+1].value = literal;
		initial.program[prefix+2].value = 0xf4e1;
		for (auto &word : initial.program) word.address += base;
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_T,producer ? 0x9abcU : term},{r::STATE_PMST,pmst},{r::STATE_ST0,st0},{r::STATE_ST1,st1}});
		clocked_register_case c{{{0,std::move(initial)}},4+prefix+unsigned(cases.size()%3),20};
		std::vector<unsigned> boundaries{1,read+1};
		if (producer) boundaries.push_back(6);
		std::sort(boundaries.begin(),boundaries.end());
		boundaries.erase(std::unique(boundaries.begin(),boundaries.end()),boundaries.end());
		for (unsigned tick : boundaries)
			c.states.push_back({tick,observation("multiply-only-publishes-at-Execute",{
				{r::STATE_A,tick > read && !destination ? result : a},{r::STATE_B,tick > read && destination ? result : b},
				{r::STATE_T,tick > read ? 0xbeefU : producer && tick < 6 ? 0x9abcU : term},
				{r::STATE_ST0,tick > read ? result_st0 : st0},{r::STATE_ST1,st1},{r::STATE_PMST,pmst}})});
		c.actions = {{4+prefix,{},false,{{base+0x601+prefix,0xdead}}}, {read,{{r::STATE_T,0xbeef}}}};
		c.retained_program_reads = {{base+0x601+prefix,literal}};
		c.data_events = std::vector<clocked_data_event>{};
		if (producer) c.data_events->push_back({true,0x0e,term,6});
		cases.push_back(std::move(c));
	};
	// Arithmetic bank: every signed boundary pair and FRCT/OVM/SMUL setting.
	for (unsigned mode = 0; mode != 3; ++mode) for (unsigned destination = 0; destination != 2; ++destination)
		for (unsigned flags = 0; flags != 8; ++flags) for (u16 term : terms) for (u16 literal : literals)
			add(mode,destination,flags | ((term ^ literal) & 1 ? 8 : 0),term,literal,0x12345678,0x87654321,false,0,0,false);
	// Transport bank: every producer value/source, SATD/SXM pair and gap.
	// Arithmetic flags/literals are distributed here; their full pair grid is above.
	for (unsigned pattern = 0; pattern != 8; ++pattern) for (unsigned source = 0; source != 2; ++source)
		for (unsigned gap = 0; gap != 3; ++gap) for (unsigned publication = 0; publication != 4; ++publication)
			for (unsigned mode = 0; mode != 3; ++mode) for (unsigned destination = 0; destination != 2; ++destination)
				add(mode,destination,((pattern+gap+publication)%8) | (publication & 1 ? 8 : 0),0,
					literals[(pattern+gap+publication)%8],source ? 0x2222 : producer_values[pattern],
					source ? producer_values[pattern] : 0x1111,true,source,gap,publication & 2);
	return cases;
}
}
