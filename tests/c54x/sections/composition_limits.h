// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
#include "tms320c54x_test_composition_limits.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	using k=composition_limit;
	for (auto const [kind,count] : {std::pair{k::interrupt_pop,24U}, {k::clock_move,24U},
		{k::delay_store,16U}, {k::block_xc,18U}, {k::delayed_block,28U}})
		for (unsigned input=0;input<count;++input) {
			auto c=composition_limit_case<r>(input,kind);
			c.identity=cases.size();
			cases.push_back(std::move(c));
		}
	return cases;
}
}
