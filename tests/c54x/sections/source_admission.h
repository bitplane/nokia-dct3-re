// license:BSD-3-Clause
// copyright-holders:Gaz
#include "c54x_section_common.h"
#include "tms320c54x_test_domain_schedule.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	for (unsigned index=0;index<domain_total_cases;++index) {
		auto const input=select_domain(index);
		if (input.context&4) continue; // Retain precisely the original staged subset.
		// Base/control inputs differ only in the old before/after checkpoint bit.
		if (!input.checkpoint && (input.context&8)) continue;
		unsigned const mac_start=domain_cases+domain_tc0_cases+domain_control_cases;
		if (index>=mac_start && index<mac_start+domain_mac_source_cases+domain_mac_short_cases) {
			unsigned const ordinal=index-mac_start;
			if (input.checkpoint!=((ordinal%88+ordinal/88%4)%2 ? 5U : 3U)) continue;
		}
		u32 const base=input.context&2 ? 0x20000 : 0;
		auto initial=initial_case("source-admission-before-bus-or-register-effects",{u16(input.instruction.opcode|input.mode)});
		for (auto &word : initial.program) word.address+=base;
		initial.program[1].value=input.instruction.operation && input.mode<0xe0 ? input.instruction.operation : 0x1500;
		initial.program[2].value=input.instruction.operation && input.mode>=0xe0 ? input.instruction.operation : 3;
		initial.program[3].value=0xf4e1;
		if (input.absolute_source!=0xffff) { initial.program[1].value=input.absolute_source; initial.program[2].value=9; }
		else if (input.checkpoint && input.indirect_source==0xffff) initial.program[2].value=0xf4e1;
		if (!input.words.empty()) {
			for (unsigned i=0;i<input.words.size();++i) initial.program[i].value=input.words[i];
		} else if (input.prefix) {
			initial.program[0].value=input.prefix; initial.program[1].value=initial.opcode; initial.program[2].value=0xf4e1;
		}
		std::vector<register_check> preserved{{r::STATE_ST0,input.st0},
			{r::STATE_ST1,input.st1|(input.context&1 ? 0x20U : 0)},
			{r::STATE_A,0x123456789aULL},{r::STATE_B,0xfedcba9876ULL},{r::STATE_T,0xabcd},
			{r::STATE_SP,0x1600},{r::STATE_TRN,0x9876},{r::STATE_BK,0x4321}};
		if (!input.words.empty()) preserved.push_back({r::STATE_PMST,input.pmst});
		if (!input.prefix) preserved.push_back({r::STATE_RC,7});
		for (unsigned i=0;i<8;++i) preserved.push_back({int(r::STATE_AR0+i),
			i==3 && input.indirect_source!=0xffff ? input.indirect_source : input.ar_base+i});
		for (auto const &value : preserved) initial.initial.push_back({value.index,value.value});
		initial.initial.push_back({STATE_GENPC,base+0x600});
		initial.expected=preserved;
		auto final=observation("admission-guard-latched",{{r::STATE_ILLEGAL,1}});
		final.expected.pop_back(); final.expected.insert(final.expected.end(),preserved.begin(),preserved.end());
		unsigned const checkpoint=input.checkpoint ? input.checkpoint : (index%2 ? 10 : 1);
		clocked_register_case c{{{0,std::move(initial)},{20,std::move(final)}},checkpoint,20};
		c.require_idle=false; c.forbid_data_access=true; c.forbid_program_writes=true;
		c.io_events=std::vector<clocked_data_event>{};
		c.pipeline_frontend=!(input.context&4);
		c.reset_cpu_before_case=true;
		cases.push_back(std::move(c));
	}
	return cases;
}
}
