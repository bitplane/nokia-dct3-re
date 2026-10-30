// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_HIGH_PROGRAM
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned near = 0; near != 2; ++near) for (unsigned producer = 0; producer != 3; ++producer)
		for (unsigned b = 0; b != 2; ++b) for (unsigned caller : {0,2,0x7f}) for (unsigned hint : {0,3,0x7f})
			for (unsigned mode = 0; mode != 3; ++mode) {
				unsigned const interrupt = producer ? 0 : mode, prefix = producer ? producer+mode : 0;
				unsigned const sample = 5+prefix+(producer == 2 ? 4 : 0);
				u32 const base = caller << 16, target = (near ? caller : hint) << 16, destination = target | 0x700, table = (hint << 16) | 0x900;
				u64 const accumulator = producer == 1 ? 0xbad0 : 0x5500000000ULL | (hint << 16) | (producer == 2 ? 0x900 : 0x700);
				u64 const other = b ? producer == 2 ? 0x5500000000ULL | table : 0x1234 : 0x2345;
				u16 const marker = b ? u16(other) : 0xbeef, st0 = producer == 1 ? 0xa628 : 0xa608;
				u16 const st1 = 0x100 | (interrupt == 1 ? 0 : 0x800), opcode = (near ? 0xf4e2 : 0xf4e6) | (b << 8);
				auto initial = initial_case("BACC-FBACC-retained-target-without-call-frame",{0xf495});
				for (auto &word : initial.program) word.address += base;
				if (producer == 1) initial.program[0].value = b ? 0x5700 : 0x5600;
				if (producer == 2) { initial.program[0].value = 0x7ef8; initial.program[1].value = b ? 0xb : 8; initial.program.push_back({table,0x700}); }
				initial.program[prefix].value = opcode;
				initial.program[prefix+1].value = 0x880f; // Flushed sequential path must not publish TRN.
				initial.program.insert(initial.program.end(),{{destination,0x880e},{destination+1,0xf4e1},{target+4,0xf4eb},{target+0x40,0xf4eb}});
				initial.initial.insert(initial.initial.end(),{{STATE_GENPC,base+0x600},{r::STATE_A,b ? other : accumulator},
					{r::STATE_B,b ? accumulator : other},{r::STATE_T,0xaaaa},{r::STATE_TRN,0x1357},{r::STATE_SP,0x1000},
					{r::STATE_RTN,0x5555},{r::STATE_ST0,st0},{r::STATE_ST1,st1},{r::STATE_IMR,interrupt == 1 ? 1U : 0U}});
				initial.data = {{0xfff,0xaaaa},{0x1400,u16(0x8000 | hint)},{0x1401,0x700}};
				auto final = observation("target-once-no-call-frame-and-flushed-sequential-word",{{STATE_GENPC,destination+2},
					{r::STATE_A,b ? other : 0xdeadbeef},{r::STATE_B,b ? 0xdeadbeef : other},{r::STATE_T,marker},{r::STATE_TRN,0x1357},
					{r::STATE_SP,0x1000},{r::STATE_RTN,interrupt ? 0x700U : 0x5555U},{r::STATE_ST0,st0},{r::STATE_ST1,st1 & ~(interrupt ? 0x800U : 0U)}});
				final.expected_data = {{0xfff,u16(interrupt ? 0x700 : 0xaaaa)}};
				clocked_register_case c{{{0,std::move(initial)},{1,observation("branch-running",{})},{64,std::move(final)}},
					std::array<unsigned,4>{4,sample,sample+1,sample+3}[(near+producer+b+caller+hint+mode)%4],64};
				c.data_events = std::vector<clocked_data_event>{};
				if (producer == 1) c.data_events->insert(c.data_events->end(),{{false,0x1400,u16(0x8000 | hint),5},{false,0x1401,0x700,5}});
				if (producer == 2) c.data_events->push_back({true,u32(b ? 0xb : 8),0x700,11});
				if (interrupt) {
					c.data_events->insert(c.data_events->end(),{{true,0xfff,0x700,0,false},{false,0xfff,0x700,0,false}});
					c.irqs = {{interrupt == 2 ? INPUT_LINE_NMI : 0,0,~0U,0,0x700}};
					c.actions.push_back({3,{},false,{},{{interrupt == 2 ? INPUT_LINE_NMI : 0,ASSERT_LINE},{interrupt == 2 ? INPUT_LINE_NMI : 0,CLEAR_LINE}}});
				}
				c.data_events->push_back({true,0xe,marker,sample+7,!interrupt});
				c.program_events = {{false,base+0x600+prefix,opcode,0,false}};
				if (producer == 2) c.program_events.push_back({false,table,0x700,10});
				c.program_events.push_back({false,destination,0x880e,sample+3,!interrupt});
				if (interrupt) c.program_events.push_back({false,destination,0x880e,0,false});
				if (producer == 1) c.actions.push_back({5,{},false,{}, {},{{0x1400,0xdead},{0x1401,0xbeef}}});
				if (producer == 2) c.actions.push_back({10,{},false,{{table,0xdead}}});
				c.actions.push_back({sample,{{b ? r::STATE_B : r::STATE_A,0xdeadbeef}}});
				cases.push_back(std::move(c));
			}
	return cases;
}
}
