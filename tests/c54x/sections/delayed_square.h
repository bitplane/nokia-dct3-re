// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	constexpr u16 words[]{0,1,0xffff,0x7fff,0x8000,0x8001,0x1234,0xf495};
	constexpr u64 seeds[]{0,1,0x7fffffff,0xff80000000ULL,0x017fffffffULL,
		0xff7fffffffULL,0x1234567833ULL,0x80aa001234ULL};
	// SPRU172C SQUR/SQURA/SQURS: signed square, FRCT/OVM and sticky OV;
	// SMUL is not a listed modifier. The MPYA layout consumes signed A[32:16].
	for (unsigned family = 0; family < 3; ++family)
	for (unsigned layout = 0; layout < (family == 2 ? 5U : 4U); ++layout)
	for (unsigned destination = 0; destination < 2; ++destination)
	for (unsigned pattern = 0; pattern < 8; ++pattern)
	for (unsigned flags = 0; flags < 8; ++flags)
	for (unsigned form = 0; form < 3; ++form)
	{
		bool const fractional = flags & 1, saturate = flags & 2;
		unsigned const operations = layout < 2 ? 1 : 2;
		unsigned const count = std::array<unsigned,3>{0,1,7}[(pattern + flags + form) % 3];
		u16 const st0 = 0xa820 | (pattern & 1 ? 0x600 : 0);
		u16 const st1 = 0x800 | (fractional ? 0x40 : 0) | (saturate ? 0x200 : 0) | (pattern & 2 ? 0x100 : 0);
		u16 const pmst = flags & 4 ? 2 : 0;
		std::array<u64,2> out{seeds[pattern],seeds[pattern] ^ 0x5555555555ULL};
		u16 status = st0;
		std::array<u16,2> opcodes{0xf495,0xf495}, memory{}, addresses{};
		std::vector<clocked_register_state> arithmetic;
		for (unsigned i = 0; i < operations; ++i)
		{
			unsigned const slot = layout == 1 ? 1 : i;
			unsigned const target = family == 2 && layout == 4 && i ? 1 : destination ^ unsigned(layout == 3 && i);
			memory[i] = words[(pattern + (form == 1 ? 0 : i * 3)) % 8];
			addresses[i] = (form ? 0x5000 : 0x1060) + (form == 1 ? 0 : i);
			u16 const opcode = family == 2 ? layout == 4 && i ? 0x3100 : 0x2600 | (target << 8) :
				0x3800 | (family << 9) | (target << 8);
			opcodes[slot] = opcode | (form == 0 ? 0x60 + i : form == 1 ? 0x85 : 0x95);
			s64 const sample = s16(memory[i]);
			s64 const old = out[target] & 0x8000000000ULL ? s64(out[target]) - (s64(1) << 40) : s64(out[target]);
			s64 const high = s64((out[0] >> 16) & 0x1ffff) - ((out[0] & 0x100000000ULL) ? 0x20000 : 0);
			s64 const product = (family == 2 && layout == 4 && i ? high : sample) * sample * (fractional ? 2 : 1);
			s64 const sum = family == 2 ? product : family ? old - product : old + product;
			bool const overflow = sum < -2147483648LL || sum > 2147483647LL;
			status |= overflow ? target ? 0x200 : 0x400 : 0;
			out[target] = u64(saturate && overflow ? std::clamp(sum,s64(-2147483648LL),s64(2147483647LL)) : sum) & 0xffffffffffULL;
			arithmetic.push_back({8 + slot,observation("delay-result", {{r::STATE_A,out[0]},{r::STATE_B,out[1]},
				{r::STATE_ST0,status}})});
		}
		auto initial = initial_case("delayed-square", {0xf272,0x605,opcodes[0],opcodes[1],0x8092,0x8193,0xf4e1});
		initial.initial.insert(initial.initial.end(), {{r::STATE_PMST,pmst},{r::STATE_ST0,st0},{r::STATE_ST1,st1},
			{r::STATE_A,seeds[pattern]},{r::STATE_B,seeds[pattern] ^ 0x5555555555ULL},
			{r::STATE_T,0x5555},{r::STATE_SP,0x3000},{r::STATE_BRC,count},
			{r::STATE_AR2,0x6000},{r::STATE_AR3,0x6100},{r::STATE_AR5,0x5000}});
		for (unsigned i = 0; i < operations; ++i) initial.data.push_back({addresses[i],memory[i]});
		auto final = observation("body-observed", {{r::STATE_A,out[0]},{r::STATE_B,out[1]},
			{r::STATE_ST0,status},{r::STATE_ST1,st1},{r::STATE_PMST,pmst},{r::STATE_T,0xdead},
			{r::STATE_BRC,0},{r::STATE_AR2,0x6001 + count},{r::STATE_AR3,0x6101 + count},
			{r::STATE_AR5,0x7000},{r::STATE_SP,0x3000}});
		clocked_register_case c{{{0,std::move(initial)}},
			std::array<unsigned,4>{5,7,8,9}[(family + layout + destination + pattern + flags + form) % 4],64};
		unsigned const last = layout == 1 || operations == 2 ? 8 : 7;
		for (unsigned tick = 6; tick <= 9; ++tick)
		{
			auto step = observation("delay-publication", {});
			for (auto const &state : arithmetic) if (state.tick == tick) step = state.observation;
			if (tick == last - 1) step.expected.push_back({r::STATE_AR5,0x5000 + (form == 2 ? operations : 0)});
			if (tick == 7 && layout != 1) step.expected.push_back({r::STATE_T,memory[0]});
			if (tick == 8) step.expected.push_back({r::STATE_T,last == 8 ? unsigned(memory[operations - 1]) : 0xdeadU});
			if (tick == 9) step.expected.push_back({r::STATE_T,0xdead});
			c.states.push_back({tick,std::move(step)});
		}
		c.states.push_back({64,std::move(final)});
		c.actions = {{6,{},false,{{0x602,0xdead},{0x603,0xbeef}}}};
		// Keep mutations ordered even when AR poisoning shares the opcode-poison tick.
		if (last == 7) c.actions.back().values = {{r::STATE_AR5,0x7000}};
		else c.actions.push_back({7,{{r::STATE_AR5,0x7000}}});
		c.actions.push_back({last,{{r::STATE_T,0xdead}}});
		for (unsigned i = 0; i < operations; ++i) c.actions.back().data.push_back({addresses[i],0xdead});
		c.clear_pipeline_before_restore = true;
		c.program_events = {{false,0x601,0x605,0,false},{false,0x602,opcodes[0],0,false},{false,0x603,opcodes[1],0,false}};
		c.data_events = std::vector<clocked_data_event>{};
		for (unsigned i = 0; i < operations; ++i)
			c.data_events->push_back({false,addresses[i],memory[i],7 + (layout == 1 ? 1 : i)});
		for (unsigned i = 0; i <= count; ++i) for (unsigned dst = 0; dst < 2; ++dst)
		{
			c.data_events->push_back({true,(dst ? 0x6100U : 0x6000U) + i,u16(out[dst]),0,false});
			c.states.back().observation.expected_data.push_back({(dst ? 0x6100U : 0x6000U) + i,u16(out[dst])});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
