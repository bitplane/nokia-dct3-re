// license:BSD-3-Clause
// copyright-holders:Gaz
#define C54X_SECTION_XF
#define C54X_SECTION_XF_RESTORE
#include "c54x_section_common.h"
namespace c54x_test {
inline std::vector<clocked_register_case> section_cases()
{
	std::vector<clocked_register_case> cases;
	std::vector<u16> words{0,0xffff};
	for(unsigned bit=0;bit<16;++bit){words.push_back(1U<<bit);words.push_back(~u16(1U<<bit));}
	for(bool masked : {false,true})for(u16 mask : {u16(0xffff),u16(0x00ff),u16(0xff00),u16(0x0800)})for(u16 word : words){
		u16 const before=0x6300|(masked ? 0x800 : 0),merged=(before&~mask)|(word&mask);
		// Memory may clear INTM, but cannot set it; debugger import is separate.
		u16 const expected=(merged&~u16(0xc00))|(before&merged&0x800);
		auto initial=initial_case("ST1-masked-memory-write-bit-basis",{0xf4e1});
		initial.initial.push_back({r::STATE_ST1,before|0x400});initial.expected.push_back({r::STATE_ST1,before});
		auto final=observation("reserved-bit-zero-and-INTM-cannot-be-set",{{r::STATE_ST1,expected}},{{7,expected}});
		clocked_register_case c{{{0,std::move(initial)},{3,std::move(final)}},3,10};c.data_events=std::vector<clocked_data_event>{};
		clocked_register_action write{2,{}};write.masked_data.push_back({7,word,mask});c.actions.push_back(std::move(write));cases.push_back(std::move(c));
	}
	return cases;
}
}
