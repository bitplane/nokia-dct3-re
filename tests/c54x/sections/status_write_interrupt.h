// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	// Memory writes cannot set INTM in the current implementation. Its clearing
	// policy remains legacy/unverified; this is not a silicon timing claim.
	for (unsigned stlm=0; stlm<2; ++stlm) for (unsigned wide=0; wide<2; ++wide)
	for (unsigned old=0; old<2; ++old) for (unsigned mask=0; mask<2; ++mask)
	for (unsigned enable=0; enable<2; ++enable) for (unsigned nmi=0; nmi<2; ++nmi)
	for (unsigned arrival : {4U,6U,8U}) {
		unsigned const index=cases.size(), line=nmi ? INPUT_LINE_NMI : 0;
		u32 const base=index%2 ? 0x20000 : 0, vector=base+(nmi ? 4 : 0x40);
		u16 const status=mask ? 0x800 : 0;
		bool const accepted=nmi || (enable && !(old && mask));
		auto initial=initial_case("status-publication-before-IRQ-drain",{u16(stlm ? 0x8907 : 0x730b)});
		initial.program.clear();
		for (unsigned i=0;i<64;++i) initial.program.push_back({base+0x600+i,u16(i>=4 ? 0xf4e1 : 0xf495)});
		initial.program[0].value=stlm ? 0x8907 : 0x730b;
		initial.program[1].value=stlm ? 0xf495 : 7;
		initial.program[2].value=wide ? 0x770e : 0x880e;
		initial.program[3].value=wide ? 0x55 : 0xf495;
		initial.program.insert(initial.program.end(),{{vector,0xf495},{vector+1,0xf4eb}});
		initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_ST0,0x2000},
			{r::STATE_ST1,old ? 0x800U : 0U},{r::STATE_IMR,enable},{r::STATE_A,0x55},
			{r::STATE_B,0x1234560000ULL|status},{r::STATE_T,0}});
		auto final=observation("one-foreground-marker-balanced-frame-and-pending-mask",{{r::STATE_A,0x55},
			{r::STATE_B,0x1234560000ULL|status},{r::STATE_T,0x55},{r::STATE_SP,0x1000},
			{r::STATE_ST1,old && mask && !accepted ? 0x800U : 0U},
			{r::STATE_IFR,unsigned(!nmi && !accepted),1}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("status-transaction-running",{})},{96,std::move(final)}},
			std::array<unsigned,3>{5,7,12}[index%3],96};
		c.actions={{arrival,{},false,{},{{int(line),ASSERT_LINE}}},{arrival+1,{},false,{},{{int(line),CLEAR_LINE}}}};
		c.require_idle=false; // A masked pending input may wake successive IDLEs without service.
		c.data_events=std::vector<clocked_data_event>{};
		if (!stlm) c.data_events->push_back({false,0xb,status,5});
		c.data_events->push_back({true,7,status,6});
		c.data_events->push_back({true,0xe,0x55,0,false});
		if (accepted) {
			for (bool write : {true,false}) {
				clocked_data_event frame{write,0xfff,0,0,false};
				frame.value_range=std::array<u16,2>{u16(stlm ? 0x601 : 0x602),0x621};
				frame.value_register=r::STATE_RTN;
				c.data_events->push_back(frame);
			}
			unsigned const selection=arrival==4 ? (wide || (!nmi && old) ? 6 : 5) : arrival+1;
			c.irqs.push_back({int(line),7,95,unsigned(stlm ? 1 : 2)+unsigned((wide ? 7U : 8U)<selection+3)});
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
