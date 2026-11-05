// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_C5416_BUS
#define C54X_SECTION_INSTRUCTION_EVENTS
#include "c54x_section_common.h"
#include <algorithm>
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned context=0;context<3;++context) for (unsigned count=0;count<(context ? 1U : 4U);++count)
	for (unsigned write=0;write<2;++write) {
		unsigned const transfers=context ? 1 : count+1, body=context==1 ? 2 : 1;
		auto initial=initial_case("waited-port-control-defers-pending-IRQ",{u16(context==0 ? 0xec00|count : context==1 ? 0xfa45 : 0xf6bb)});
		initial.program.clear();
		for (unsigned i=0;i<64;++i) for (u32 base : {0U,0x600U,0x700U}) initial.program.push_back({base+i,0xf495});
		// Keep instruction construction separate from the vector/NOP padding.
		initial.program.push_back({0x600,u16(context==0 ? 0xec00|count : context==1 ? 0xfa45 : 0xf6bb)});
		if (context==1) initial.program.push_back({0x601,0x700});
		initial.program.insert(initial.program.end(),{{0x600+body,u16(write ? 0x7591 : 0x7491)},{0x601+body,0x125}});
		initial.program.push_back({0x40,0xf4e1}); // Stop in a real handler, not unmapped fall-through.
		initial.data={{0x1400,0x1234},{0x1401,0x1234},{0x1402,0x1234},{0x1403,0x1234},{0x28,0x7000},{0x2b,0}};
		initial.initial.insert(initial.initial.end(),{{r::STATE_A,0},{r::STATE_ST0,0x1804},
			{r::STATE_ST1,context==2 ? 0x800U : 0U},{r::STATE_AR1,0x1400},{r::STATE_IMR,1}});
		auto final=observation("all-transfers-before-single-IRQ-at-owned-continuation",{{r::STATE_AR1,0x1400+transfers},
			{r::STATE_RTN,context==1 ? 0x702U : context==2 ? 0x606U : 0x603U},{r::STATE_SP,0xfff}});
		clocked_register_case c{{{0,std::move(initial)},{1,observation("atomic-control-running",{})},{64,std::move(final)}},4,64};
		if (context==1) {
			c.states.insert(c.states.begin()+2,{6,observation("retained-branch-predicate-input",{{r::STATE_A,0}})});
			c.program_events={{false,0x601,0x700,0,false},{false,0x700,0xf495,0,false}};
		}
		c.initial_io={{0x125,0x1234}};
		c.observed_data_space=AS_IO;
		c.data_events=std::vector<clocked_data_event>(transfers,{bool(write),0x125,0x1234,0,false});
		for (unsigned i=1;i<transfers;++i) (*c.data_events)[i].cycles_from_previous=9;
		c.irqs={{0,context ? 0U : 18+count*9,context ? ~0U : 18+count*9,transfers}};
		// CPU input delivery is queued. Settle it before the clock-4 snapshot;
		// the branch's retained extension already protects this earlier edge.
		c.actions={{3,{},false,{},{{0,ASSERT_LINE}}}};
		if (!context && count==3) {
			auto retained_bus=[&](clocked_register_case &sample,bool ready_hold) {
				constexpr u16 words[]{0x1234,0x2468,0x1357,0xabcd};
				auto action_at=[&sample](unsigned tick) -> clocked_register_action & {
					auto action=std::find_if(sample.actions.begin(),sample.actions.end(),[tick](auto const &item) { return item.tick==tick; });
					if (action!=sample.actions.end()) return *action;
					sample.actions.push_back({tick,{}});
					return sample.actions.back();
				};
				sample.observed_data_space=AS_DATA;
				sample.io_events=sample.data_events;
				sample.data_events=std::vector<clocked_data_event>{};
				for (unsigned i=0;i<4;++i) {
					(*sample.io_events)[i].value=words[i];
					(*sample.io_events)[i].cycle=(write ? 15 : 14)+i*9+(ready_hold && i ? 4 : 0);
					(*sample.io_events)[i].check_cycle=true;
					(*sample.io_events)[i].cycles_from_previous.reset();
					sample.data_events->push_back({!bool(write),0x1400+i,words[i],0,false});
					sample.states.back().observation.expected_data.push_back({0x1400+i,u16(write && i==1 ? 0xdead : words[i])});
					if (write) {
						for (auto &word : sample.states.front().observation.data)
							if (word.address==0x1400+i) word.value=words[i];
					}
					if (!write && i<3) {
						unsigned const read=14+i*9+(ready_hold && i ? 4 : 0);
						action_at(read+1).io={{0x125,words[i+1]}};
					}
				}
				if (write) action_at(16).data={{0x1401,0xdead}};
				else action_at(ready_hold ? 27U : 23U).io={{0x125,0xdead}};
				sample.data_events->push_back({true,0xfff,0x603,0,false});
				sample.irqs.front().io_events_before=4;
				std::stable_sort(sample.actions.begin(),sample.actions.end(),[](auto const &left,auto const &right) {
					return left.tick<right.tick;
				});
			};
			auto waiting=c;
			waiting.checkpoint=write ? 18 : 17;
			retained_bus(waiting,false);
			cases.push_back(std::move(waiting));
			auto ready=c;
			ready.checkpoint=write ? 25 : 24;
			clocked_register_action hold{write ? 16U : 15U,{}};
			hold.ready_level=false;
			clocked_register_action release{write ? 27U : 26U,{}};
			release.ready_level=true;
			ready.actions.push_back(hold); ready.actions.push_back(release);
			(*ready.data_events)[1].cycles_from_previous=13;
			ready.irqs.front().earliest=ready.irqs.front().latest=49;
			retained_bus(ready,true);
			cases.push_back(std::move(ready));
			auto captured=c;
			captured.checkpoint=write ? 16 : 23;
			retained_bus(captured,false);
			cases.push_back(std::move(captured));
		}
		cases.push_back(std::move(c));
	}
	for (bool write : {false,true}) {
		u16 const opcode=write ? 0x7510 : 0x7410;
		auto initial=initial_case("older-mapped-port-before-younger-repeat",{opcode,0x125,0xec03,0xf495,0xf4e1});
		initial.initial.insert(initial.initial.end(),{{r::STATE_ST0,0},{r::STATE_AR0,write ? 0x5aa5U : 0xdeadU}});
		initial.data={{0x28,0}};
		clocked_register_case older{{{0,std::move(initial)},{32,observation("repeat-ownership-does-not-reject-older-port",{
			{r::STATE_AR0,0x5aa5},{r::STATE_RC,0},{STATE_GENPC,0x605}})}},5,32};
		older.initial_io={{0x125,0x5aa5}};
		older.data_events=std::vector<clocked_data_event>{{!write,0x10,0x5aa5,write ? 6U : 7U}};
		older.io_events=std::vector<clocked_data_event>{{write,0x125,0x5aa5,write ? 7U : 6U}};
		older.instruction_events=std::vector<memory_word>{{0x600,opcode},{0x602,0xec03},{0x603,0xf495},
			{0x603,0xf495},{0x603,0xf495},{0x603,0xf495},{0x604,0xf4e1}};
		cases.push_back(std::move(older));
	}
	return cases;
}
}
