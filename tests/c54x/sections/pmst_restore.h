// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	auto add = [&](unsigned mode, unsigned pattern, unsigned page, bool upper, unsigned selected = 0) {
		bool const pop = mode != 0, boundary = mode == 3, repeat = mode == 4, irq = mode == 5;
		u16 const value = (repeat ? std::array<u16,4>{0,1,0x8000,0xffff} : irq ? std::array<u16,4>{0,0x20,0x2020,0x3020} :
			std::array<u16,4>{0,0x20,0x2020,0xffaa})[pattern];
		u16 const destination = repeat ? 0x1a+selected : 0x1d, entry = boundary ? 0x600 : 0x9000, target = upper ? 0x8700 : 0x700;
		u16 const old = repeat ? 0 : value ^ (irq ? 0x120 : 0x20);
		u32 const base = page << 16;
		auto initial = initial_case("PMST-stack-or-literal-publication-and-next-mapping",{0xf495});
		initial.program.clear();
		for (u32 bank : {0U,base}) {
			for (unsigned i = 0; i != 32; ++i) initial.program.push_back({bank+entry+i,0xf495});
			std::vector<u16> code = pop ? std::vector<u16>{u16(mode == 2 ? 0x8b87 : 0x8a00 | destination),0xf495,0xf495,0xf495,0xf073,target} :
				std::vector<u16>{0x771d,value,0xf495,0xf073,target};
			for (unsigned i = 0; i != code.size(); ++i) initial.program.push_back({bank+entry+i,code[i]});
			initial.program.insert(initial.program.end(),{{bank+target,u16(bank ? 0xe844 : 0xe811)},{bank+target+1,0xf4e1}});
		}
		if (boundary) initial.program.push_back({(old & 0x20 ? base : 0)+entry,0xffff});
		u16 const vector = (value & 0xff80) | ((16+selected)*4);
		if (irq) for (u32 bank : {0U,base}) {
			u16 const old_vector = ((value ^ 0x100) & 0xff80) | ((16+selected)*4);
			initial.program.insert(initial.program.end(),{{bank+old_vector,0xe966},{bank+old_vector+1,0xf4e1},
				{bank+vector,0xe955},{bank+vector+1,0xf4e1}});
		}
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+entry},{r::STATE_PMST,old},{r::STATE_ST0,0xa608},
			{r::STATE_ST1,irq ? 0U : 0x800U},{r::STATE_SP,0x1000},{r::STATE_AR7,0x1d},{r::STATE_A,0},{r::STATE_B,0},
			{r::STATE_BRC,0x1234},{r::STATE_RSA,0x5678},{r::STATE_REA,0x9abc},{r::STATE_IMR,irq ? 1U << selected : 0U}});
		initial.data = {{0x1000,value}};
		auto final = observation("retained-source-mapping-and-stack-effects",{{r::STATE_PMST,repeat ? 0U : value},
			{r::STATE_XPC,page},{r::STATE_SP,pop && !irq ? 0x1001U : 0x1000U},{r::STATE_ST1,0x800},{r::STATE_AR7,0x1d}});
		if (irq) final.expected.push_back({r::STATE_B,0x55});
		else final.expected.push_back({r::STATE_A,!repeat && (value & 0x20) && !upper ? 0x11U : 0x44U});
		if (repeat) final.expected.insert(final.expected.end(),{{r::STATE_BRC,selected == 0 ? value : 0x1234U},
			{r::STATE_RSA,selected == 1 ? value : 0x5678U},{r::STATE_REA,selected == 2 ? value : 0x9abcU}});
		unsigned const choice = cases.size()%4;
		unsigned const checkpoint = (irq ? std::array<unsigned,4>{3,5,6,11} : boundary ? std::array<unsigned,4>{1,2,5,6} :
			pop ? std::array<unsigned,4>{4,5,6,11} : std::array<unsigned,4>{4,6,8,11})[choice];
		clocked_register_case c{{{0,std::move(initial)},{1,observation("PMST-restore-running",{})},{32,std::move(final)}},checkpoint,32};
		c.actions = {{pop ? 5U : 4U,{},false,pop ? std::vector<memory_word>{} : std::vector<memory_word>{{base+0x9001,0xffff}},
			{},pop ? std::vector<memory_word>{{0x1000,u16(value ^ 0xffff)}} : std::vector<memory_word>{}}};
		if (boundary) c.actions.insert(c.actions.begin(),{1,{{r::STATE_PMST,value}}});
		c.data_events = std::vector<clocked_data_event>{};
		if (pop) c.data_events->push_back({false,0x1000,value,5});
		else c.retained_program_reads = {{base+0x9001,value}};
		c.data_events->push_back({true,destination,value,pop ? 6U : 7U});
		if (irq) {
			// Pending-latch conformance, not a physical-pin latency claim.
			c.actions.push_back({upper ? 7U : 4U,{{r::STATE_IFR,1U << selected}}});
			std::sort(c.actions.begin(),c.actions.end(),[](auto const &a,auto const &b) { return a.tick < b.tick; });
			// Early entry retains the third foreground NOP; late entry follows
			// the target marker. These are logical return PCs, not bus latencies.
			c.data_events->push_back({true,0x1000,u16(upper ? target+1 : entry+3),0,false});
			c.irqs = {{int(selected),0,~0U,2}};
			c.program_events = {{false,(value & 0x20 ? 0 : base)+vector,0xe955,0,false}};
		}
		cases.push_back(std::move(c));
	};
	for (unsigned mode = 0; mode != 5; ++mode) for (unsigned pattern = 0; pattern != 4; ++pattern)
		for (unsigned page : {2,3}) for (unsigned upper = 0; upper != 2; ++upper)
			for (unsigned selected = 0; selected != (mode == 4 ? 3U : 1U); ++selected) add(mode,pattern,page,upper,selected);
	for (unsigned pattern = 0; pattern != 4; ++pattern) for (unsigned page : {2,3})
		for (unsigned late = 0; late != 2; ++late) for (unsigned line = 0; line != 16; ++line) add(5,pattern,page,late,line);
	return cases;
}
}
