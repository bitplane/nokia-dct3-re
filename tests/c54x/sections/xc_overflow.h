// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
// XC with a group-1 overflow test, alone or ANDed with a compare on the same
// accumulator (ROM4 a0ad: XC 1,BGT+BOV = fd7e). As for BC and CC, testing
// OVA/OVB consumes that flag whether or not the instruction executes.
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	static constexpr std::array<unsigned,7> compares{0,2,3,4,5,6,7}; // none, GEQ, LT, NEQ, EQ, GT, LEQ
	static constexpr std::array<u64,3> values{0,0x0000001234ULL,0xff00000000ULL};
	for (unsigned polarity = 0; polarity < 2; ++polarity)
	for (unsigned compare : compares)
	for (unsigned b = 0; b < 2; ++b)
	for (unsigned value = 0; value < 3; ++value)
	for (unsigned flags = 0; flags < 4; ++flags)
	{
		u16 const condition = u16(0x60 | (polarity << 4) | (b << 3) | compare);
		u16 const st0 = 0x0e05 | (flags & 1 ? 0x400 : 0) | (flags & 2 ? 0x200 : 0), mask = b ? 0x200 : 0x400;
		u64 const tested = values[value], other = 0x0000005678ULL;
		bool const negative = BIT(tested, 39), zero = !tested;
		bool const compared = std::array<bool,8>{true,false,!negative,negative,!zero,zero,!negative && !zero,negative || zero}[compare];
		bool const take = compared && bool(st0 & mask) == bool(polarity);
		auto initial = initial_case("xc-overflow-condition", {u16(0xfd00 | condition),0xe833,0xf495,0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,st0},{r::STATE_A,b ? other : tested},{r::STATE_B,b ? tested : other}});
		clocked_register_case c{{{0,std::move(initial)}},std::array<unsigned,3>{3,4,6}[cases.size() % 3],24};
		u64 const a = take ? 0x33 : b ? other : tested;
		c.states.push_back({24,observation("combined-predicate-consumes-tested-overflow", {{r::STATE_A,a},
			{r::STATE_B,b ? tested : other},{STATE_GENPC,0x604},{r::STATE_ST0,u16(st0 & ~mask)}})});
		c.data_events = std::vector<clocked_data_event>{};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
