// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	constexpr u64 sources[]{0,0x20,0x2021,0xffaa,0x01230021,0x7f00000021ULL,0xff00000021ULL,0xffffffffffULL};
	constexpr u16 unsaturated[]{0,0x20,0x2021,0xffaa,0x21,0x21,0x21,0xffff};
	constexpr u16 unsigned_saturated[]{0,0x20,0x2021,0xffaa,0x21,0xffff,0xffff,0xffff};
	constexpr u16 signed_saturated[]{0,0x20,0x2021,0xffaa,0x21,0xffff,0,0xffff};
	std::vector<clocked_register_case> cases;
	auto append = [&](unsigned pattern,unsigned sst,unsigned sxm,unsigned b,unsigned overlay,unsigned low,unsigned irq,unsigned late) {
		unsigned const seed = pattern + sst + sxm + b + overlay + low + irq + late;
		u32 const base = (seed % 2 ? 3 : 2) << 16, entry = low ? 0x600 : 0x9000;
		u16 const value = sst ? sxm ? signed_saturated[pattern] : unsigned_saturated[pattern] : unsaturated[pattern];
		u16 const old = 0x200 | (overlay ? 0x20 : 0) | sst;
		u16 const st1 = (sxm ? 0x100 : 0) | (!irq && (seed & 2) ? 0x4000 : 0) | (irq ? 0 : 0x800) | 0x13;
		auto initial = initial_case("STLM-PMST-retains-prefetch-and-updates-next-mapping", {0xf495}); initial.program.clear();
		for (u32 bank : {0U,base}) {
			for (unsigned i = 0; i != 24; ++i) initial.program.push_back({bank + entry + i,0xf495});
			initial.program.push_back({bank + entry,u16(b ? 0x891d : 0x881d)});
			initial.program.push_back({bank + entry + 4,u16(bank ? 0xe944 : 0xe911)});
			initial.program.push_back({bank + entry + 5,u16(bank ? 0xe844 : 0xe811)});
			initial.program.push_back({bank + entry + 15,0xf4e1});
		}
		u16 const vector_offset = irq == 17 ? 4 : (15 + irq) * 4;
		u16 const old_vector = (old & 0xff80) | vector_offset, new_vector = (value & 0xff80) | vector_offset;
		if (irq) for (u16 vector : {old_vector,new_vector}) for (u32 bank : {0U,base}) {
			initial.program.insert(initial.program.end(),{{bank + vector,0xf495},{bank + vector + 1,0xf4eb}});
		}
		initial.initial.insert(initial.initial.end(), {{STATE_GENPC,base + entry},{r::STATE_PMST,old},{r::STATE_ST0,0xb9ff},
			{r::STATE_ST1,st1},{r::STATE_SP,0x3000},{r::STATE_A,b ? 0x123456789aULL : 0x12121212ULL},
			{r::STATE_B,b ? 0x12121212ULL : 0xfedcba9876ULL},{r::STATE_BRC,13},{r::STATE_T,0x5aa5},{r::STATE_BK,17},
			{r::STATE_IMR,irq && irq != 17 ? unsigned(1U << (irq - 1)) : 0U}});
		auto old_source = observation("source-not-sampled-before-Execute", {{r::STATE_PMST,old},
			{r::STATE_A,b ? 0x123456789aULL : 0x12121212ULL},{r::STATE_B,b ? 0x12121212ULL : 0xfedcba9876ULL}});
		auto live_source = observation("Execute-live-source-under-old-PMST", {{r::STATE_PMST,old},
			{r::STATE_A,b ? 0x123456789aULL : sources[pattern]},{r::STATE_B,b ? sources[pattern] : 0xfedcba9876ULL}});
		auto published = observation("one-PMST-publication", {{r::STATE_PMST,value},{r::STATE_ST0,0xb9ff},
			{r::STATE_A,b ? 0x123456789aULL : sources[pattern]},{r::STATE_B,b ? sources[pattern] : 0xfedcba9876ULL}});
		auto final = observation("old-prefetch-marker-and-new-subsequent-marker", {{STATE_GENPC,base + entry + 16},
			{r::STATE_A,low && (value & 0x20) ? 0x11U : 0x44U},{r::STATE_B,low && overlay ? 0x11U : 0x44U},
			{r::STATE_PMST,value},{r::STATE_SP,0x3000},{r::STATE_ST0,0xb9ff},{r::STATE_ST1,st1},
			{r::STATE_BRC,13},{r::STATE_T,0x5aa5},{r::STATE_BK,17}});
		for (unsigned ar = 0; ar != 8; ++ar) {
			initial.initial.push_back({int(r::STATE_AR0) + int(ar),0x7000 + ar});
			final.expected.push_back({int(r::STATE_AR0) + int(ar),0x7000 + ar});
		}
		clocked_register_case c{{{0,std::move(initial)},{4,std::move(old_source)},{5,std::move(live_source)},
			{6,std::move(published)},{7,observation("mapping-or-vector-continuation-running", {})},{96,std::move(final)}},std::array<unsigned,4>{3,5,6,12}[seed % 4],96};
		c.actions = {{5,{{int(b ? r::STATE_B : r::STATE_A),sources[pattern]}},true}};
		c.data_events = std::vector<clocked_data_event>{{true,0x1d,value,6}};
		if (irq) {
			int const line = irq == 17 ? INPUT_LINE_NMI : int(irq) - 1;
			c.actions.push_back({late ? 8U : 2U,{},false,{},{{line,ASSERT_LINE},{line,CLEAR_LINE}}});
			std::sort(c.actions.begin(),c.actions.end(),[](auto const &x,auto const &y) { return x.tick < y.tick; });
			u16 const vector = late ? new_vector : old_vector, mapping = late ? value : old;
			u32 const physical = (mapping & 0x20) && vector < 0x8000 ? vector : base | vector;
			c.program_events = {{false,physical,0xf495,late ? 12U : 6U}};
			u16 const continuation = entry + (late ? 7 : 1);
			c.irqs = {{line,late ? 12U : 6U,late ? 12U : 6U,unsigned(late),continuation}};
			c.data_events->insert(c.data_events->end(),{{true,0x2fff,continuation,0,false},{false,0x2fff,continuation,0,false}});
		}
		cases.push_back(std::move(c));
	};
	for (unsigned pattern = 0; pattern != 8; ++pattern)
	for (unsigned sst = 0; sst != 2; ++sst)
	for (unsigned sxm = 0; sxm != 2; ++sxm)
	for (unsigned b = 0; b != 2; ++b)
	for (unsigned overlay = 0; overlay != 2; ++overlay) {
		for (unsigned low = 0; low != 2; ++low) append(pattern,sst,sxm,b,overlay,low,0,0);
		for (unsigned irq = 1; irq != 18; ++irq)
		for (unsigned late = 0; late != 2; ++late) append(pattern,sst,sxm,b,overlay,0,irq,late);
	}
	return cases;
}
}
