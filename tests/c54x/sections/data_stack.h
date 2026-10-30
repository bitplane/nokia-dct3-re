// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned pop = 0; pop < 2; ++pop) for (unsigned form = 0; form < 4; ++form)
	for (u16 value : {0x123U,0xbeefU}) for (unsigned point : {3U,4U,5U,6U})
	{
		u16 const operand = form == 1 ? 0x1020 : 0x470;
		u16 const opcode = pop ? std::array<u16,4>{0x8b70,0x8b20,0x8b83,0x8b93}[form] :
			std::array<u16,4>{0x4b70,0x4b20,0x4b82,0x4b92}[form];
		auto initial = initial_case("data-stack", {opcode});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0x208},{r::STATE_ST1,form == 1 ? 0x4900 : 0x900},
			{r::STATE_AR2,0x470},{r::STATE_AR3,0x470},{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL}});
		initial.expected.insert(initial.expected.end(), {{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL},
			{r::STATE_ST0,0x208},{r::STATE_ST1,form == 1 ? 0x4900U : 0x900U},
			{r::STATE_SP,0x1000},{r::STATE_AR2,0x470},{r::STATE_AR3,0x470}});
		initial.data = pop ? std::vector<memory_word>{{operand,0xaaaa},{0x1000,value}} :
			std::vector<memory_word>{{operand,value},{0xfff,0xaaaa}};
		auto address = observation("address", {}); address.expected = initial.expected;
		if (!pop) address.expected[5].value = 0xfff;
		if (form == 3) address.expected[pop ? 7 : 6].value = 0x471;
		auto read = address;
		if (pop) read.expected[5].value = 0x1001;
		auto write = read; write.expected_data = {{pop ? operand : 0xfff,value}};
		clocked_register_case c{{{0,std::move(initial)},{4,std::move(address)},
			{5,std::move(read)},{6,std::move(write)}},point,10};
		c.require_idle = false; c.check_pipeline_clock = true;
		clocked_register_action poison{5,{}};
		poison.data = {{pop ? 0x1000 : operand,0xdead}}; c.actions.push_back(std::move(poison));
		c.data_events = std::vector<clocked_data_event>{{false,pop ? 0x1000U : operand,value,5},
			{true,pop ? operand : 0xfffU,value,6}};
		cases.push_back(std::move(c));
	}
	// One separated Category-I compiler-mode consumer; zero-gap predictions retired.
	for (unsigned pop = 0; pop < 2; ++pop) for (unsigned point : {4U,5U,6U})
	{
		auto initial = initial_case("stack-consumer", {u16(pop ? 0x8b83 : 0x4b82),0xf495,0x1027});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0x208},{r::STATE_ST1,0x4900},
			{r::STATE_AR2,0x470},{r::STATE_AR3,0x470},{r::STATE_B,0x2233445566ULL}});
		initial.data = {{pop ? 0x1000 : 0x470,0xbeef},{pop ? 0x470 : 0xfff,0xaaaa},
			{0x1027,0x1111},{pop ? 0x1028 : 0x1026,0x2222}};
		clocked_register_case c{{{0,std::move(initial)},
			{8,observation("consumer", {{r::STATE_A,0x2222},{r::STATE_B,0x2233445566ULL},
				{r::STATE_SP,pop ? 0x1001U : 0xfffU},{r::STATE_ST0,0x208},{r::STATE_ST1,0x4900}})}},point,10};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.data_events = std::vector<clocked_data_event>{{false,pop ? 0x1000U : 0x470U,0xbeef,5},
			{true,pop ? 0x470U : 0xfffU,0xbeef,6},{false,pop ? 0x1028U : 0x1026U,0x2222,7}};
		cases.push_back(std::move(c));
	}
	// Adjacent ADDM -> PSHD bypass, including16-bit wrap and sticky carry.
	for (unsigned wrap = 0; wrap < 2; ++wrap) for (unsigned point : {6U,7U,8U})
	{
		u16 const value = wrap ? 0xffff : 0x1234, sum = wrap ? 1 : 0x1236;
		auto initial = initial_case("addm-push", {0x6b82,2,0x4b92});
		initial.initial.insert(initial.initial.end(), {{r::STATE_ST0,0x208},{r::STATE_ST1,0x900},
			{r::STATE_AR2,0x470},{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL}});
		initial.data = {{0x470,value},{0xfff,0xaaaa}};
		clocked_register_case c{{{0,std::move(initial)},
			{10,observation("forwarded", {{r::STATE_SP,0xfff},{r::STATE_AR2,0x471},
				{r::STATE_ST0,wrap ? 0xa08U : 0x208U},{r::STATE_ST1,0x900},
				{r::STATE_A,0x123456789aULL},{r::STATE_B,0x2233445566ULL}}, {{0x470,sum},{0xfff,sum}})}},point,10};
		c.require_idle = false; c.check_pipeline_clock = true;
		c.data_events = std::vector<clocked_data_event>{{false,0x470,value,6},{true,0x470,sum,7},{true,0xfff,sum,8}};
		cases.push_back(std::move(c));
	}
	return cases;
}
}
