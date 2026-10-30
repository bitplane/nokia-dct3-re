// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned returning = 0; returning != 2; ++returning) for (unsigned form = 0; form != 4; ++form)
		for (unsigned nmi = 0; nmi != 2; ++nmi) {
			u32 const base = (returning+form+nmi)%2 ? 0x20000 : 0, vector = base+(nmi ? 4 : 0x40);
			u16 const opcode = std::array<u16,4>{0x4a08,0x4b62,0x8a0e,0x8a18}[form];
			u16 const frame = form < 2 ? 0xffe : form == 2 ? 0x1000 : 0x10ff, source = form == 0 ? 8 : form == 1 ? 0x62 : 0x1000;
			u16 const word = form == 3 ? 0x1100 : 0x1234;
			auto initial = initial_case("foreground-stack-operation-before-IRQ-reservation",{opcode,u16(returning && form < 2 ? 0x8a0e : 0xf4e1),0xf4e1});
			for (auto &entry : initial.program) entry.address += base;
			initial.program.insert(initial.program.end(),{{vector,0xe955},{vector+1,u16(returning ? 0xf4eb : 0xf4e1)}});
			initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_SP,0x1000},{r::STATE_A,0x1234},
				{r::STATE_B,0},{r::STATE_T,0},{r::STATE_ST0,0},{r::STATE_ST1,0},{r::STATE_IMR,1}});
			initial.data = {{0x62,0x1234},{0x1000,word},{0xffe,0xdead},{0xfff,0xbeef},{0x10ff,0xdead}};
			u16 const final_sp = returning ? form < 2 ? 0x1000 : form == 2 ? 0x1001 : 0x1100 : frame;
			auto final = observation("separate-foreground-and-interrupt-stack-ownership",{{r::STATE_SP,final_sp},{r::STATE_B,0x55},
				{r::STATE_RTN,0x601},{r::STATE_A,0x1234},{r::STATE_T,form == 2 || (returning && form < 2) ? 0x1234U : 0U},
				{r::STATE_ST0,0},{r::STATE_ST1,returning ? 0U : 0x800U}},{{frame,0x601}});
			if (form < 2) final.expected_data.push_back({0xfff,0x1234});
			clocked_register_case c{{{0,std::move(initial)},{1,observation("stack-interrupt-running",{})},
				{5,observation("older-SP-update-before-interrupt-reservation",{{r::STATE_SP,frame}})},
				{6,observation("publication-and-return-running",{})},{32,std::move(final)}},
				returning ? std::array<unsigned,5>{4,5,6,10,14}[(form+nmi)%5] : std::array<unsigned,3>{4,5,6}[(form+nmi)%3],32};
			c.actions = {{2,nmi ? std::vector<register_setting>{} : std::vector<register_setting>{{r::STATE_IFR,1}},false,{},
				nmi ? std::vector<register_setting>{{INPUT_LINE_NMI,ASSERT_LINE}} : std::vector<register_setting>{}}};
			c.data_events = std::vector<clocked_data_event>{{false,source,word,5}};
			// POPM SP is a Read-owned address-generator publication, unlike T.
			c.data_events->push_back({true,u32(form < 2 ? 0xfff : form == 2 ? 0xe : 0x18),word,form == 3 ? 5U : 6U});
			c.data_events->push_back({true,frame,0x601,7});
			if (returning) {
				c.data_events->push_back({false,frame,0x601,0,false});
				if (form < 2) c.data_events->insert(c.data_events->end(),{{false,0xfff,0x1234,0,false},{true,0xe,0x1234,0,false}});
			}
			c.irqs = {{nmi ? INPUT_LINE_NMI : 0,6,6,form == 3 ? 2U : 1U,0x601}};
			c.program_events = {{false,vector,0xe955,0,false}};
			cases.push_back(std::move(c));
		}
	return cases;
}
}
