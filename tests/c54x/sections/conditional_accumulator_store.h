// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	struct boundary { u64 value; int shift; u16 logical, arithmetic; };
	constexpr boundary bounds[]{
		{0x0180012345ULL,0,0x8001,0x8001},{0xfe7ffe1234ULL,0,0x7ffe,0x7ffe},
		{0x8000000001ULL,-16,0x0080,0xff80},{0xffffffffffULL,-16,0x00ff,0xffff},
		{0xffffffffffULL,-1,0xffff,0xffff},{0x0080012345ULL,15,0x91a2,0x91a2}};
	std::vector<clocked_register_case> cases;
	for (unsigned index = 0; index < 168; ++index) {
		bool const edge = index >= 72;
		unsigned const v = edge ? index - 72 : index;
		unsigned const condition = v % 12, sign = (v / 12) % 3;
		bool const source_b = edge ? v & 1 : v / 36;
		unsigned const mode = edge ? 2 : (condition + sign + unsigned(source_b)) % 4;
		unsigned const shift = edge ? bounds[v / 16].shift & 31 : (condition + sign) & 1;
		bool const predicates[]{sign != 2,sign == 2,sign != 0,sign == 0,sign == 1,sign != 1};
		bool const take = edge ? !(v & 8) : predicates[condition % 6];
		u64 const tested = sign == 0 ? 0 : sign == 1 ? 0x12340000ULL : 0xffedcc0000ULL;
		u64 const a = edge ? source_b ? 1 : bounds[v / 16].value : condition < 6 ? tested : 0x23450000;
		u64 const b = edge ? source_b ? bounds[v / 16].value : 1 : condition < 6 ? 0x23450000 : tested;
		u16 const word = edge ? (v & 2 ? bounds[v / 16].arithmetic : bounds[v / 16].logical) :
			u16(((source_b == (condition >= 6)) ? (sign == 0 ? 0 : sign == 1 ? 0x1234 : 0xedcc) : 0x2345) << shift);
		u16 const opcode = edge ? 0x9e80 | (source_b ? 0x100 : 0) | (source_b ? 0 : 8) | (take ? 6 : 3) :
			0x9e00 | (source_b ? 0x100 : 0) | (mode << 6) | (condition < 6 ? 2 + condition : 10 + condition - 6);
		u16 const st0 = edge ? 0x1c00 : 0;
		u16 const st1 = edge ? 0x800 | (v & 2 ? 0x100 : 0) | shift : 0x900 | shift;
		auto initial = initial_case("conditional-accumulator-store", {opcode,0xf4e1});
		initial.data = {{0x500,0xa55a}};
		initial.initial.insert(initial.initial.end(), {{r::STATE_A,a},{r::STATE_B,b},{r::STATE_AR0,3},
			{r::STATE_AR2,0x500},{r::STATE_BK,0},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_PMST,edge && (v & 4) ? 1U : 0U}});
		constexpr unsigned delta[]{0,0xffff,1,3};
		clocked_register_case c{{{0,std::move(initial)}},index & 1 ? 3U : 1U,24};
		c.pipeline_frontend = false;
		c.states.push_back({24,observation("predicate-store-and-unconditional-address-update", {{r::STATE_A,a},{r::STATE_B,b},
			{r::STATE_AR2,u16(0x500 + delta[mode])},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{STATE_GENPC,0x602}},
			{{0x500,take ? word : u16(0xa55a)}})});
		// Atomic backend reads and writes back even for a false predicate.
		// This protects current bus behavior, not a measured silicon claim.
		c.data_events = std::vector<clocked_data_event>{{false,0x500,0xa55a,0,false},
			{true,0x500,take ? word : u16(0xa55a),0,false}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
