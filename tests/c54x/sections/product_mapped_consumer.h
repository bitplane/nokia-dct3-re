// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr std::array<u16,4> operands{3,0xffff,0x8000,0x7fff};
	for (u16 term : operands) for (u16 literal : operands) for (unsigned producer = 0; producer != 2; ++producer)
		for (unsigned consumer = 0; consumer != 2; ++consumer) for (unsigned part = 0; part != 3; ++part)
			for (unsigned gap = 0; gap != 2; ++gap) for (unsigned flags = 0; flags != 4; ++flags) {
				s64 const left = term >= 0x8000 ? s64(term)-0x10000 : term;
				s64 const right = literal >= 0x8000 ? s64(literal)-0x10000 : literal;
				s64 const wide = left*right*(flags & 1 ? 2 : 1);
				u64 const product = u64(flags & 2 ? std::clamp<s64>(wide,-0x80000000LL,0x7fffffffLL) : wide) & 0xffffffffffULL;
				// AG/BG are eight-bit memory-mapped guard fields, not signed words.
				u16 const sampled = part == 2 ? (product >> 32) & 0xff : product >> (16*part);
				u16 const address = (producer ? 0xb : 8)+part, st1 = 0x800 | (flags & 1 ? 0x40 : 0) | (flags & 2 ? 0x200 : 0);
				u32 const base = cases.size()%2 ? 0x20000 : 0;
				auto initial = initial_case("MPY-to-mapped-accumulator-LDM",{u16(0xf066 | (producer << 8)),literal});
				initial.program[2+gap].value = 0x4800 | (consumer << 8) | address;
				initial.program[3+gap].value = 0xf4e1;
				for (auto &word : initial.program) word.address += base;
				initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_T,term},
					{r::STATE_A,0x12345678},{r::STATE_B,0x87654321},{r::STATE_ST0,0x1804},{r::STATE_ST1,st1}});
				unsigned const read = 7+gap;
				u64 a = consumer ? producer ? 0x12345678 : 0x012345abcdULL : sampled;
				u64 b = consumer ? sampled : producer ? 0x012345abcdULL : 0x87654321;
				clocked_register_case c{{{0,std::move(initial)},{1,observation("before-product",{})},
					{read+1,observation("LDM-retains-published-field-after-source-poison",{{r::STATE_A,a},{r::STATE_B,b},
						{r::STATE_T,0x7777},{r::STATE_ST1,st1}})}},read-1+unsigned(cases.size()%2),16};
				c.actions = {{6,{{r::STATE_T,0x7777}}},{read,{{producer ? r::STATE_B : r::STATE_A,0x012345abcdULL}}}};
				c.data_events = std::vector<clocked_data_event>{{false,address,sampled,read}};
				c.retained_program_reads = {{base+0x601,literal}};
				cases.push_back(std::move(c));
			}
	return cases;
}
}
