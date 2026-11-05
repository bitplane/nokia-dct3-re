// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> external_io_cases(bool extended_forms,bool standard)
{
	std::vector<clocked_register_case> cases;
	for (unsigned form=extended_forms ? 1U : 0U;form<(extended_forms ? 5U : 1U);++form)
	for (unsigned selector=0;selector<8;++selector) for (unsigned multiplier=0;multiplier<(standard ? 1U : 2U);++multiplier)
	for (bool write : {false,true}) for (bool hold : {false,true}) for (unsigned irq=0;irq<3;++irq) {
		unsigned const waits=selector*(multiplier+1), first=(write ? 7U : 6U)+unsigned(form>=1 && form<=3)+unsigned(write && form==3);
		unsigned const complete=first+waits+unsigned(hold && waits>=2)*3;
		bool const extension=form>=1 && form<=3;
		u16 const address=form==3 ? 0x30 : form==4 ? 0x11 : 0x1400;
		u16 const value=write && form==4 ? 0x12 : write || !waits ? 0x1234 : 0xabcd;
		u16 const opcode=(write ? 0x7500 : 0x7400)|(form==1 ? 0xe1 : form==2 || form==3 ? 0xf8 : form==4 ? 0x91 : 0x81);
		u16 const continuation=extension ? 0x603 : 0x602;
		auto initial=initial_case("external-I/O-retains-transfer-through-software-and-READY-waits",{opcode});
		initial.program[1].value=form==1 ? 1 : form==2 || form==3 ? address : 0x125;
		if (extension) initial.program[2].value=0x125;
		initial.program[continuation-0x600].value=0xf4e1;
		initial.program.insert(initial.program.end(),{{4,0xf4e1},{0x40,0xf4e1}});
		initial.initial.insert(initial.initial.end(),{{r::STATE_AR1,form==1 ? 0x13ffU : form==4 ? 0x11U : 0x1400U},
			{r::STATE_ST0,0x1804},{r::STATE_ST1,0},{r::STATE_IMR,1},{r::STATE_PRD,0xffff},{r::STATE_TCR,0x20},{r::STATE_TIM,0xffff}});
		initial.data={{0x1400,0x1234},{0x28,u16(selector<<12)},{0x2b,u16(multiplier)}};
		if (form==3) initial.data.push_back({0x30,0x1234});
		auto final=observation("one-completed-transfer-before-interrupt-dispatch",{{r::STATE_SP,irq ? 0xfffU : 0x1000U}});
		if (!write) final.expected_data.push_back({address,value});
		unsigned const checkpoint=std::array<unsigned,3>{first-1,first+1,complete+1}[(form+selector+multiplier+unsigned(write)+unsigned(hold)+irq)%3];
		clocked_register_case c{{{0,std::move(initial)},{complete+16,std::move(final)}},checkpoint,complete+16};
		c.initial_ready=!hold;
		c.initial_io={{0x125,0x1234}}; c.initial_input_lines={{0,CLEAR_LINE},{INPUT_LINE_NMI,CLEAR_LINE}};
		if (write) {
			clocked_register_action poison{first-1,{}};
			if (form==4) poison.values={{r::STATE_AR1,0xdead}};
			else poison.data={{address,0xdead}};
			c.actions.push_back(std::move(poison));
		}
		clocked_register_action capture{first,{},false,{{0x601,0xbeef}}, {},{{0x28,0},{0x2b,0}}};
		capture.io={{0x125,0xabcd}};
		if (form!=4 || waits) capture.values={{r::STATE_AR1,0x1500}};
		if (extension) capture.program.push_back({0x602,0xdead});
		if (irq) capture.input_lines={{irq==2 ? INPUT_LINE_NMI : 0,ASSERT_LINE}};
		c.actions.push_back(std::move(capture));
		if (hold && waits>=2) { clocked_register_action release{complete-1,{}}; release.ready_level=true; c.actions.push_back(release); }
		c.retained_program_reads={{0x601,u16(form==1 ? 1 : form==2 || form==3 ? address : 0x125)}};
		if (extension) c.retained_program_reads.push_back({0x602,0x125});
		c.io_events=std::vector<clocked_data_event>{{write,0x125,value,complete}};
		c.io_events->front().program_events_before=1+unsigned(extension);
		c.data_events=std::vector<clocked_data_event>{{write ? false : true,address,write ? u16(form==4 ? 0x12 : 0x1234) : value,0,false}};
		if (irq) {
			// A zero-wait request can arrive after the terminal IDLE retires.
			clocked_data_event frame{true,0xfff,0,0,false};
			frame.value_range=std::array<u16,2>{continuation,u16(continuation+1)};
			frame.value_register=r::STATE_RTN;
			c.data_events->push_back(frame);
			c.irqs={{irq==2 ? INPUT_LINE_NMI : 0,complete+1,~0U,1,{}, {},1}};
		}
		cases.push_back(std::move(c));
	}
	return cases;
}
}
